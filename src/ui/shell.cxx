#include "egui.hxx"
#include "layout.hxx"
#include "theme.hxx"
#include "assets/font.hxx"
#include "glass.hxx"
#include "state.hxx"
#include "preview.hxx"
#include "assets/icon.hxx"
#include <d3d11.h>
#include <imgui_freetype.h>
#include <cstdio>
#include <vector>
#include <string>
#include <fstream>
#include <type_traits>
#include <Windows.h>
#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

namespace egui {

    static const ImWchar icomoon_ranges[] = { 0xe000, 0xf8ff, 0 };

    static float scaled_px(float v, float s) { return ImFloor(v * s + 0.5f); }

    static bool g_atlas_full = false;

    static ID3D11ShaderResourceView* g_font_srv = nullptr;

    static void upload_font_texture()
    {
        if (!g_pd3dDevice) return;

        unsigned char* pixels = nullptr;
        int width = 0, height = 0;
        ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        if (!pixels || width <= 0 || height <= 0) return;

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA sub = {};
        sub.pSysMem = pixels;
        sub.SysMemPitch = (UINT)width * 4;

        ID3D11Texture2D* tex = nullptr;
        if (FAILED(g_pd3dDevice->CreateTexture2D(&desc, &sub, &tex)) || !tex)
            return;

        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MipLevels = 1;

        ID3D11ShaderResourceView* srv = nullptr;
        g_pd3dDevice->CreateShaderResourceView(tex, &srv_desc, &srv);
        tex->Release();
        if (!srv) return;

        if (g_font_srv) g_font_srv->Release();
        g_font_srv = srv;

        ImGui::GetIO().Fonts->SetTexID((ImTextureID)srv);

        ImGui::GetIO().Fonts->ClearTexData();
    }

    bool create_texture_from_memory(ID3D11Device* dev, const void* data, size_t size,
                                    ID3D11ShaderResourceView** out_srv, int* out_w, int* out_h)
    {
        if (!dev || !data || size == 0 || !out_srv) return false;
        *out_srv = nullptr;
        if (out_w) *out_w = 0;
        if (out_h) *out_h = 0;

        const HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        const bool co_owned = SUCCEEDED(co);

        IWICImagingFactory* factory = nullptr;
        IWICStream* stream = nullptr;
        IWICBitmapDecoder* decoder = nullptr;
        IWICBitmapFrameDecode* frame = nullptr;
        IWICFormatConverter* conv = nullptr;
        bool ok = false;

        if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                       IID_PPV_ARGS(&factory))) &&
            SUCCEEDED(factory->CreateStream(&stream)) &&
            SUCCEEDED(stream->InitializeFromMemory((WICInProcPointer)const_cast<void*>(data), (DWORD)size)) &&
            SUCCEEDED(factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &decoder)) &&
            SUCCEEDED(decoder->GetFrame(0, &frame)) &&
            SUCCEEDED(factory->CreateFormatConverter(&conv)) &&
            SUCCEEDED(conv->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
                                       nullptr, 0.0, WICBitmapPaletteTypeCustom)))
        {
            UINT w = 0, h = 0;
            if (SUCCEEDED(conv->GetSize(&w, &h)) && w > 0 && h > 0) {
                std::vector<unsigned char> pixels((size_t)w * h * 4);
                if (SUCCEEDED(conv->CopyPixels(nullptr, w * 4, (UINT)pixels.size(), pixels.data()))) {
                    D3D11_TEXTURE2D_DESC desc = {};
                    desc.Width = w;
                    desc.Height = h;
                    desc.MipLevels = 1;
                    desc.ArraySize = 1;
                    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                    desc.SampleDesc.Count = 1;
                    desc.Usage = D3D11_USAGE_DEFAULT;
                    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

                    D3D11_SUBRESOURCE_DATA sub = {};
                    sub.pSysMem = pixels.data();
                    sub.SysMemPitch = w * 4;

                    ID3D11Texture2D* tex = nullptr;
                    if (SUCCEEDED(dev->CreateTexture2D(&desc, &sub, &tex)) && tex) {
                        ok = SUCCEEDED(dev->CreateShaderResourceView(tex, nullptr, out_srv));
                        tex->Release();
                        if (ok) {
                            if (out_w) *out_w = (int)w;
                            if (out_h) *out_h = (int)h;
                        }
                    }
                }
            }
        }

        auto rel = [](auto*& p) { if (p) { p->Release(); p = nullptr; } };
        rel(conv); rel(frame); rel(decoder); rel(stream); rel(factory);
        if (co_owned) CoUninitialize();
        return ok;
    }

    static void load_fonts(float s, bool with_extras)
    {
        ImGuiIO& io = ImGui::GetIO();

        io.Fonts->Clear();
        io.FontDefault = nullptr;

        ImFontConfig inter_config;
        inter_config.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_LoadColor;
        inter_config.FontDataOwnedByAtlas = false;

        auto px = [s](float v) { return v * s; };

        efonts.inter.Text12        = io.Fonts->AddFontFromMemoryTTF(inter_medium,    sizeof(inter_medium),    px(12.f), &inter_config);
        efonts.inter.Text10        = io.Fonts->AddFontFromMemoryTTF(inter_medium,    sizeof(inter_medium),    px(10.f), &inter_config);
        efonts.inter.Text11        = io.Fonts->AddFontFromMemoryTTF(inter_medium,    sizeof(inter_medium),    px(11.f), &inter_config);
        efonts.inter.Text13        = io.Fonts->AddFontFromMemoryTTF(inter_medium,    sizeof(inter_medium),    px(13.f), &inter_config);
        efonts.inter.Bold10        = io.Fonts->AddFontFromMemoryTTF(inter_semibold,  sizeof(inter_semibold),  px(10.f), &inter_config);
        efonts.inter.Bold11        = io.Fonts->AddFontFromMemoryTTF(inter_semibold,  sizeof(inter_semibold),  px(11.f), &inter_config);
        efonts.inter.Bold12        = io.Fonts->AddFontFromMemoryTTF(inter_semibold,  sizeof(inter_semibold),  px(12.f), &inter_config);
        efonts.inter.Bold13        = io.Fonts->AddFontFromMemoryTTF(inter_bold,      sizeof(inter_bold),      px(13.f), &inter_config);

        if (with_extras) {
            efonts.inter.Regular       = io.Fonts->AddFontFromMemoryTTF(inter_regular,   sizeof(inter_regular),   px(18.f), &inter_config);
            efonts.inter.Medium        = io.Fonts->AddFontFromMemoryTTF(inter_medium,    sizeof(inter_medium),    px(18.f), &inter_config);
            efonts.inter.MediumSmall   = io.Fonts->AddFontFromMemoryTTF(inter_medium,    sizeof(inter_medium),    px(14.f), &inter_config);
            efonts.inter.SemiBold      = io.Fonts->AddFontFromMemoryTTF(inter_semibold,  sizeof(inter_semibold),  px(16.f), &inter_config);
            efonts.inter.SemiBoldSmall = io.Fonts->AddFontFromMemoryTTF(inter_semibold,  sizeof(inter_semibold),  px(11.f), &inter_config);
            efonts.inter.SemiBold13    = io.Fonts->AddFontFromMemoryTTF(inter_semibold,  sizeof(inter_semibold),  px(13.f), &inter_config);
            efonts.inter.Bold          = io.Fonts->AddFontFromMemoryTTF(inter_bold,      sizeof(inter_bold),      px(18.f), &inter_config);
            efonts.inter.Black         = io.Fonts->AddFontFromMemoryTTF(inter_black,     sizeof(inter_black),     px(18.f), &inter_config);
            efonts.inter.ExtraBold     = io.Fonts->AddFontFromMemoryTTF(inter_extrabold, sizeof(inter_extrabold), px(18.f), &inter_config);

            ImFontConfig icomoon_config;
            icomoon_config.OversampleH = icomoon_config.OversampleV = 1;
            icomoon_config.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;

            efonts.icomoon.regular       = io.Fonts->AddFontFromMemoryCompressedBase85TTF(icomoon_compressed_data_base85, px(20.f), &icomoon_config, icomoon_ranges);
            efonts.icomoon.regular_small = io.Fonts->AddFontFromMemoryCompressedBase85TTF(icomoon_compressed_data_base85, px(16.f), &icomoon_config, icomoon_ranges);
            efonts.icomoon.regular_large = io.Fonts->AddFontFromMemoryCompressedBase85TTF(icomoon_compressed_data_base85, px(28.f), &icomoon_config, icomoon_ranges);
        }
        else {
            efonts.inter.Regular = efonts.inter.Medium = efonts.inter.MediumSmall = nullptr;
            efonts.inter.SemiBold = efonts.inter.SemiBoldSmall = efonts.inter.SemiBold13 = nullptr;
            efonts.inter.Bold = efonts.inter.Black = efonts.inter.ExtraBold = nullptr;
            efonts.icomoon.regular = efonts.icomoon.regular_small = efonts.icomoon.regular_large = nullptr;
        }
        g_atlas_full = with_extras;

        settings.scale_current = s;
    }

    float system_dpi_scale()
    {
        HDC dc = ::GetDC(nullptr);
        if (!dc) return 1.0f;
        const int dpi_x = ::GetDeviceCaps(dc, LOGPIXELSX);
        ::ReleaseDC(nullptr, dc);
        return dpi_x > 0 ? (float)dpi_x / 96.0f : 1.0f;
    }

    static void apply_style_colors();

    void apply_scale(float scale)
    {
        scale = ImClamp(scale, 0.5f, 4.0f);

        const float* src = reinterpret_cast<const float*>(&layout_design);
        float* dst = reinterpret_cast<float*>(&layout);
        for (size_t i = 0; i < sizeof(Layout) / sizeof(float); i++)
            dst[i] = scaled_px(src[i], scale);

        layout.border = ImMax(1.0f, layout.border);

        settings.current_dpi_scale = scale;

        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowPadding      = ImVec2(0.f, 0.f);
        style.WindowBorderSize   = 0.f;
        style.WindowRounding     = 0.f;
        style.ChildBorderSize    = 0.f;
        style.ChildRounding      = 0.f;
        style.PopupRounding      = layout.control_rounding;
        style.PopupBorderSize    = layout.border;
        style.FrameBorderSize    = layout.border;
        style.FrameRounding      = layout.control_rounding;
        style.FramePadding       = ImVec2(dpi(11.f), (layout.control_h - dpi(12.f)) * 0.5f);
        style.ItemSpacing        = ImVec2(0.f, layout.row_gap);
        style.ItemInnerSpacing   = ImVec2(dpi(8.f), dpi(4.f));
        style.ScrollbarSize      = dpi(6.f);
        style.ScrollbarRounding  = dpi(3.f);
        style.GrabRounding       = dpi(3.f);
        style.WindowMenuButtonPosition = ImGuiDir_None;

        apply_style_colors();
    }

    static void apply_style_colors()
    {
        ImVec4* c = ImGui::GetStyle().Colors;
        c[ImGuiCol_Text]                  = colors.text;
        c[ImGuiCol_TextDisabled]          = colors.text_muted;
        c[ImGuiCol_WindowBg]              = colors.background;
        c[ImGuiCol_ChildBg]               = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_PopupBg]               = colors.surface_sec;
        c[ImGuiCol_Border]                = colors.control_border;
        c[ImGuiCol_BorderShadow]          = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_FrameBg]               = colors.control;
        c[ImGuiCol_FrameBgHovered]        = colors.control_hov;
        c[ImGuiCol_FrameBgActive]         = colors.control_hov;
        c[ImGuiCol_ScrollbarBg]           = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_ScrollbarGrab]         = colors.scroll;
        c[ImGuiCol_ScrollbarGrabHovered]  = colors.surface_sel;
        c[ImGuiCol_ScrollbarGrabActive]   = colors.accent;
        c[ImGuiCol_CheckMark]             = colors.accent;
        c[ImGuiCol_SliderGrab]            = colors.accent;
        c[ImGuiCol_SliderGrabActive]      = colors.accent;
        c[ImGuiCol_Button]                = colors.control;
        c[ImGuiCol_ButtonHovered]         = colors.control_hov;
        c[ImGuiCol_ButtonActive]          = colors.control_hov;
        c[ImGuiCol_Header]                = colors.surface_sel;
        c[ImGuiCol_HeaderHovered]         = colors.surface_hov;
        c[ImGuiCol_HeaderActive]          = colors.surface_sel;
        c[ImGuiCol_Separator]             = colors.divider;
        c[ImGuiCol_TextSelectedBg]        = ImVec4(colors.accent.x, colors.accent.y, colors.accent.z, 0.35f);
        c[ImGuiCol_NavHighlight]          = ImVec4(0, 0, 0, 0);
    }

    void set_theme(Theme t) { settings.theme = t; }

    void update_theme()
    {
        const DesignTokens target = theme_tokens(settings.theme);

        const float* dst = reinterpret_cast<const float*>(&target);
        float* cur = reinterpret_cast<float*>(&colors);
        const int n = (int)(sizeof(DesignTokens) / sizeof(float));

        const float k = anim_step(fade_speed);
        bool moving = false;
        for (int i = 0; i < n; i++) {
            if (ImFabs(cur[i] - dst[i]) < 0.001f) { cur[i] = dst[i]; continue; }
            cur[i] = ImLerp(cur[i], dst[i], k);
            moving = true;
        }

        static bool was_moving = true;
        if (moving || was_moving)
            apply_style_colors();
        was_moving = moving;
    }

    static int font_signature(float s)
    {
        static const float bases[] = { 10.f, 11.f, 12.f, 13.f };
        int sig = 0;
        for (int i = 0; i < IM_ARRAYSIZE(bases); i++)
            sig = sig * 131 + (int)scaled_px(bases[i], s);
        return sig;
    }

    void update_dpi()
    {
        IM_ASSERT((ImGui::GetCurrentContext() == nullptr || !ImGui::GetCurrentContext()->WithinFrameScope)
                  && "egui::update_dpi() debe llamarse fuera del frame, antes de NewFrame");

        ImGuiIO& io = ImGui::GetIO();
        settings.dpi_scale = system_dpi_scale();
        const float target = ImClamp(settings.dpi_scale, 0.5f, 4.0f);
        settings.dpi_scale = target;

        if (settings.current_dpi_scale != target) {
            float cur = ImLerp(settings.current_dpi_scale, target, anim_step(fade_speed));
            if (ImFabs(cur - target) < 0.002f) cur = target;
            apply_scale(cur);
        }
        const bool settled = (settings.current_dpi_scale == target);

        const bool sizes_changed = font_signature(settings.current_dpi_scale) != font_signature(settings.scale_current);
        const bool extras_missing = settled && !g_atlas_full;
        if (!sizes_changed && !extras_missing)
            return;

        load_fonts(settings.current_dpi_scale, settled);
        io.Fonts->Build();
        upload_font_texture();
    }

    void initialize()
    {
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = NULL;

        ImGui::StyleColorsDark();

        apply_scale(settings.dpi_scale);
        load_fonts(settings.current_dpi_scale, true);

        hotkey_register("Aimbot",  &vars.aimbot.aim_key,   &vars.aimbot.aim_key_mode,   &vars.aimbot.aim_key_active);
        hotkey_register("Trigger", &vars.aimbot.trigger_key, &vars.aimbot.trigger_key_mode, &vars.aimbot.trigger_active);
        hotkey_register("Fly Key", &vars.misc.pf_fly_key,  &vars.misc.pf_fly_key_mode,  nullptr);
    }

    static const char* header_tabs[] = { "Aimbot", "ESP", "World", "Misc", "Config", "Settings" };
    static constexpr int TAB_COUNT = IM_ARRAYSIZE(header_tabs);
    static_assert(TAB_COUNT == IM_ARRAYSIZE(vars.gui.active_subtab),
                  "header tab count must match active_subtab");

    static int shown_tab = 0;

    static bool g_scroll_reset_next = false;

    struct GroupCtx {
        ImDrawListSplitter splitter;
        ImVec2 start = ImVec2(0, 0);
        float  width = 0.f;
    };
    static GroupCtx g_groups[8];
    static int      g_group_depth = 0;

    void begin_group(const char* id, float width) {
        IM_ASSERT(g_group_depth < IM_ARRAYSIZE(g_groups));
        GroupCtx& g = g_groups[g_group_depth++];

        ImDrawList* dl = ImGui::GetWindowDrawList();
        g.start = ImGui::GetCursorScreenPos();
        g.width = (width > 0.f) ? width : ImGui::GetContentRegionAvail().x;

        g.splitter.Split(dl, 2);
        g.splitter.SetCurrentChannel(dl, 1);

        ImGui::PushID(id);
        ImGui::Indent(layout.card_pad_x);
        ImGui::BeginGroup();
        ImGui::PushItemWidth(g.width - layout.card_pad_x * 2.f);
        ImGui::Dummy(ImVec2(0.f, ImMax(0.f, layout.card_pad_t - ImGui::GetStyle().ItemSpacing.y)));
    }

    void end_group() {
        IM_ASSERT(g_group_depth > 0);
        GroupCtx& g = g_groups[--g_group_depth];

        ImGui::PopItemWidth();
        ImGui::EndGroup();
        ImGui::Unindent(layout.card_pad_x);
        ImGui::PopID();

        const ImVec2 p_min = g.start;
        const ImVec2 p_max(g.start.x + g.width, ImGui::GetItemRectMax().y + layout.card_pad_b);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        g.splitter.SetCurrentChannel(dl, 0);
        if (!glass_pane(dl, p_min, p_max, layout.card_rounding, colors.card,
                        ImGui::GetStyle().Alpha, true))
            edraw.rect_filled(p_min, p_max, colors.card, layout.card_rounding);
        edraw.rect(p_min, p_max, colors.card_border, layout.card_rounding, 0, layout.border);
        g.splitter.Merge(dl);

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        window->DC.CursorMaxPos.x = ImMax(window->DC.CursorMaxPos.x, p_max.x);
        window->DC.CursorMaxPos.y = ImMax(window->DC.CursorMaxPos.y, p_max.y);

        ImGui::SetCursorScreenPos(ImVec2(g.start.x, p_max.y + layout.card_gap));
    }

    void group_title(const char* text) {
        ImFont* f = fnt(efonts.inter.Bold11);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        draw_text(f, ImVec2(pos.x, pos.y - dpi(1.f)), colors.section_header, text);
        ImGui::Dummy(ImVec2(measure(f, text).x, fsize(f) + dpi(4.f)));
    }

    bool group_tabs(const char* id, const char* const* items, int count, int* current) {
        ImFont* f_on  = fnt(efonts.inter.Bold11);
        ImFont* f_off = fnt(efonts.inter.Text11);

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float  h   = fsize(f_on);
        bool changed = false;
        float x = pos.x;

        ImGui::PushID(id);
        for (int i = 0; i < count; i++) {
            const ImVec2 ts_on  = measure(f_on,  items[i]);
            const ImVec2 ts_off = measure(f_off, items[i]);

            const float w = ts_on.x;

            ImGui::SetCursorScreenPos(ImVec2(x, pos.y));
            ImGui::InvisibleButton(items[i], ImVec2(w, h));
            const bool hovered = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked()) { *current = i; changed = true; }

            const ImGuiID iid = ImGui::GetItemID();
            const float sel_t = anim(anim_value(iid), (*current == i) ? 1.f : 0.f, 14.f);
            const float hov_t = anim(anim_hover(iid), hovered ? 1.f : 0.f, 14.f);

            if (sel_t < 0.999f)
                draw_text(f_off, ImVec2(x, pos.y + (h - ts_off.y) * 0.5f - dpi(1.f)),
                          fade(ImLerp(colors.text_disabled, colors.text_sec, hov_t), 1.f - sel_t),
                          items[i]);
            if (sel_t > 0.001f)
                draw_text(f_on, ImVec2(x, pos.y + (h - ts_on.y) * 0.5f - dpi(1.f)),
                          fade(colors.accent, sel_t), items[i]);

            x += w + dpi(16.f);
        }
        ImGui::PopID();

        ImGui::SetCursorScreenPos(pos);
        ImGui::Dummy(ImVec2(ImMin(ImMax(0.f, x - pos.x - dpi(16.f)), ImGui::CalcItemWidth()), h + dpi(4.f)));
        return changed;
    }

    void section_label(const char* text, const char* text_end) {
        ImFont* f = fnt(efonts.inter.Text11);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        draw_text(f, pos, colors.section_header, text, text_end);
        ImGui::Dummy(ImVec2(measure(f, text, text_end).x, fsize(f)));
    }

    void gap(float height) {
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + height));
    }

    void same_line_right(float width) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        const float row_w = ImGui::CalcItemWidth();
        const float row_l = window->Pos.x + window->DC.Indent.x + window->DC.ColumnsOffset.x;
        const ImVec2 last_min = ImGui::GetItemRectMin();
        ImGui::SameLine();
        ImGui::SetCursorScreenPos(ImVec2(row_l + row_w - width, last_min.y));
    }

    static bool g_glass_frame = false;

    static void header_bar(ImVec2 p, float width) {
        ImFont* f_logo = fnt(efonts.inter.Bold13);
        ImFont* f_icon = fnt(efonts.icomoon.regular_small);

        const float pad = dpi(16.f);
        const float cy = p.y + layout.header_h * 0.5f;

        const ImVec2 logo_sz = measure(f_logo, settings.project_name);
        draw_text(f_logo, ImVec2(p.x + pad, cy - logo_sz.y * 0.5f - dpi(1.f)),
                  colors.accent, settings.project_name);

        const float btn = dpi(22.f);
        const ImVec2 bmin(p.x + width - pad - btn, cy - btn * 0.5f);
        ImGui::SetCursorScreenPos(bmin);
        ImGui::InvisibleButton("##close_menu", ImVec2(btn, btn));
        const bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked())
            ::PostQuitMessage(0);

        const char* mark = (f_icon ? ICON_CLOSE_LINE : "x");
        ImFont* fm = f_icon ? f_icon : fnt(efonts.inter.Text12);
        const ImVec2 ms = measure(fm, mark);
        draw_text(fm, ImVec2(bmin.x + (btn - ms.x) * 0.5f, bmin.y + (btn - ms.y) * 0.5f - dpi(1.f)),
                  hov ? colors.text_primary : colors.text_sec, mark);
    }

    static void nav_bar(ImVec2 p, float width) {
        (void)width;
        ImFont* f = fnt(efonts.inter.Text12);
        const float h = layout.tab_h;
        const float y = p.y + (layout.tabbar_h - h) * 0.5f;
        float x = p.x + layout.tab_pad_x;

        for (int i = 0; i < TAB_COUNT; i++) {
            const ImVec2 ts = measure(f, header_tabs[i]);
            const float w = ts.x + layout.tab_pad_x * 2.f;

            ImGui::SetCursorScreenPos(ImVec2(x, y));
            ImGui::InvisibleButton(header_tabs[i], ImVec2(w, h));
            const bool hovered = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked()) vars.gui.active_tab = i;

            const ImGuiID id = ImGui::GetItemID();
            const float sel_t = anim(anim_value(id), (vars.gui.active_tab == i) ? 1.f : 0.f, 14.f);
            const float hov_t = anim(anim_hover(id), hovered ? 1.f : 0.f, 14.f);

            const float bg_a = ImMax(sel_t, hov_t * 0.6f);
            if (bg_a > 0.001f)
                edraw.rect_filled(ImVec2(x, y), ImVec2(x + w, y + h),
                                  fade(ImLerp(colors.surface_hov, colors.tab_sel, sel_t), bg_a),
                                  layout.control_rounding);

            const ImVec4 c_idle = g_glass_frame ? colors.text_sec     : colors.text_muted;
            const ImVec4 c_hov  = g_glass_frame ? colors.text_primary : colors.text_sec;
            const ImVec4 col = ImLerp(ImLerp(c_idle, c_hov, hov_t),
                                      colors.text_primary, sel_t);
            const ImVec2 tp(x + layout.tab_pad_x, y + (h - ts.y) * 0.5f - dpi(1.f));
            if (g_glass_frame)
                draw_text(f, ImVec2(tp.x + dpi(1.f), tp.y + dpi(1.f)),
                          ImVec4(0.f, 0.f, 0.f, 0.75f), header_tabs[i]);
            draw_text(f, tp, col, header_tabs[i]);
            x += w + layout.tab_gap;
        }
    }

    static const char* build_date() {
        static char buf[24] = {};
        if (!buf[0]) {
            const char* d = __DATE__;
            int j = 0;
            for (int i = 0; d[i] && j < (int)sizeof(buf) - 1; i++) {
                if (d[i] == ' ' && j > 0 && buf[j - 1] == ' ') continue;
                buf[j++] = d[i];
            }
            buf[j] = '\0';
        }
        return buf;
    }

    static void footer_bar(ImVec2 p, float width) {
        ImFont* f = fnt(efonts.inter.Text10);
        const float cy = p.y + layout.footer_h * 0.5f;

        const char* lbl = "build ";
        const char* date = build_date();
        const ImVec2 lbl_sz = measure(f, lbl);
        const ImVec2 date_sz = measure(f, date);

        const float x = p.x + (width - (lbl_sz.x + date_sz.x)) * 0.5f;
        const float y = cy - lbl_sz.y * 0.5f - dpi(1.0f);
        draw_text(f, ImVec2(x, y), colors.text_muted, lbl);
        draw_text(f, ImVec2(x + lbl_sz.x, y), colors.accent, date);
    }

    void template_shell() {

        update_theme();

        settings.overlay.watermark  = vars.gui.show_watermark;
        settings.overlay.keybinds   = vars.gui.show_active_hotkeys;
        settings.overlay.spectators = vars.gui.show_playerlist;

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        const ImVec2 org(window->Pos.x + layout.shadow_pad, window->Pos.y + layout.shadow_pad);

        const float main_h = layout.header_h + layout.tabbar_h + layout.content_h + layout.footer_h;
        const ImVec2 m_min = org;
        const ImVec2 m_max(org.x + layout.main_w, org.y + main_h);

        edraw.shadow_rect(m_min, m_max, ImColor(0, 0, 0, 255), layout.shadow, layout.window_rounding);
        g_glass_frame = glass_pane(ImGui::GetWindowDrawList(), m_min, m_max, layout.window_rounding,
                                   colors.background, ImGui::GetStyle().Alpha);
        if (!g_glass_frame)
            edraw.rect_filled(m_min, m_max, colors.background, layout.window_rounding);
        edraw.rect(m_min, m_max, colors.window_border, layout.window_rounding, 0, layout.border);

        float y = m_min.y;

        const ImColor div = g_glass_frame ? fade(colors.divider, 0.55f)
                                          : ImColor(colors.divider);

        header_bar(ImVec2(m_min.x, y), layout.main_w);
        y += layout.header_h;
        edraw.line(ImVec2(m_min.x, y), ImVec2(m_max.x, y), div, layout.border);

        nav_bar(ImVec2(m_min.x, y), layout.main_w);
        y += layout.tabbar_h;
        edraw.line(ImVec2(m_min.x, y), ImVec2(m_max.x, y), div, layout.border);

        {
            float& tab_alpha = vars.gui.tab_transition_alpha;
            const bool swapping = (shown_tab != vars.gui.active_tab);
            ramp_to(tab_alpha, !swapping);
            if (swapping && tab_alpha <= 0.0f) {
                shown_tab = vars.gui.active_tab;
                g_scroll_reset_next = true;
            }
        }

        ImGui::SetCursorScreenPos(ImVec2(m_min.x, y));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(layout.content_pad, layout.content_pad));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * vars.gui.tab_transition_alpha);
        begin_child("##content", ImVec2(layout.main_w, layout.content_h));
        {
            const float avail = ImGui::GetContentRegionAvail().x;
            const float col_w = (avail - layout.col_gap) * 0.5f;

            ImGui::BeginGroup();
            panel_left(col_w, layout.content_h);
            ImGui::EndGroup();

            ImGui::SameLine(0.f, layout.col_gap);

            ImGui::BeginGroup();
            panel_right(col_w, layout.content_h, "Indicators");
            ImGui::EndGroup();
        }
        end_child();
        ImGui::PopStyleVar(2);

        const float footer_y = m_max.y - layout.footer_h;
        edraw.line(ImVec2(m_min.x, footer_y), ImVec2(m_max.x, footer_y), div, layout.border);
        footer_bar(ImVec2(m_min.x, footer_y), layout.main_w);
    }

    static int g_shown_subtab[TAB_COUNT] = {};

    static int subtab_begin() {
        int&   shown = g_shown_subtab[shown_tab];
        const int want = vars.gui.active_subtab[shown_tab];
        float& a = vars.gui.subtab_transition_alpha;

        const bool swapping = (shown != want);
        ramp_to(a, !swapping);
        if (swapping && a <= 0.0f) shown = want;

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * a);
        return shown;
    }
    static void subtab_end() { ImGui::PopStyleVar(); }

    static char g_config_name[48] = "default";
    static char g_config_status[80] = "";

    static bool config_name_ok(const char* s)
    {
        if (!s || !*s) return false;
        for (const char* p = s; *p; ++p) {
            const unsigned char c = (unsigned char)*p;
            const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                         || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '_';
            if (!ok) return false;
        }
        return true;
    }

    static std::string config_dir()
    {
        char path[MAX_PATH] = {};
        const DWORD n = ::GetModuleFileNameA(nullptr, path, MAX_PATH);
        std::string dir = (n > 0 && n < MAX_PATH) ? std::string(path, n) : std::string(".");
        const size_t slash = dir.find_last_of("\\/");
        if (slash != std::string::npos) dir.resize(slash);
        dir += "\\configs";
        ::CreateDirectoryA(dir.c_str(), nullptr);
        return dir;
    }

    static std::string config_path(const char* name)
    {
        return config_dir() + "\\" + name + ".cfg";
    }

    static void config_list(std::vector<std::string>& out)
    {
        out.clear();
        const std::string pattern = config_dir() + "\\*.cfg";
        WIN32_FIND_DATAA fd = {};
        HANDLE h = ::FindFirstFileA(pattern.c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) return;
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            std::string name = fd.cFileName;
            const size_t dot = name.rfind('.');
            if (dot != std::string::npos) name.resize(dot);
            out.push_back(name);
        } while (::FindNextFileA(h, &fd));
        ::FindClose(h);
    }

    static void config_save()
    {
        if (!config_name_ok(g_config_name)) {
            snprintf(g_config_status, sizeof(g_config_status), "use letters, numbers, space, - or _");
            return;
        }
        static_assert(std::is_trivially_copyable<c_variables>::value, "config blob");
        const uint32_t magic = 0x31474643u;
        const uint32_t version = 2u;
        const uint32_t bytes = (uint32_t)sizeof(c_variables);
        std::ofstream out(config_path(g_config_name), std::ios::binary | std::ios::trunc);
        if (!out) {
            snprintf(g_config_status, sizeof(g_config_status), "could not save");
            return;
        }
        out.write((const char*)&magic, sizeof(magic));
        out.write((const char*)&version, sizeof(version));
        out.write((const char*)&bytes, sizeof(bytes));
        out.write((const char*)&vars, sizeof(vars));
        snprintf(g_config_status, sizeof(g_config_status), "saved %s", g_config_name);
    }

    static void config_load()
    {
        if (!config_name_ok(g_config_name)) {
            snprintf(g_config_status, sizeof(g_config_status), "use letters, numbers, space, - or _");
            return;
        }
        std::ifstream in(config_path(g_config_name), std::ios::binary);
        if (!in) {
            snprintf(g_config_status, sizeof(g_config_status), "not found");
            return;
        }
        uint32_t magic = 0, version = 0, bytes = 0;
        in.read((char*)&magic, sizeof(magic));
        in.read((char*)&version, sizeof(version));
        in.read((char*)&bytes, sizeof(bytes));
        if (!in || magic != 0x31474643u || version != 2u || bytes != sizeof(c_variables)) {
            snprintf(g_config_status, sizeof(g_config_status), "unreadable config");
            return;
        }
        c_variables loaded = {};
        in.read((char*)&loaded, sizeof(loaded));
        if (!in) {
            snprintf(g_config_status, sizeof(g_config_status), "unreadable config");
            return;
        }
        const int tab_count = (int)(sizeof(loaded.gui.active_subtab) / sizeof(loaded.gui.active_subtab[0]));
        if (loaded.gui.active_tab < 0 || loaded.gui.active_tab >= tab_count)
            loaded.gui.active_tab = 0;
        vars = loaded;
        snprintf(g_config_status, sizeof(g_config_status), "loaded %s", g_config_name);
    }

    static void config_delete()
    {
        if (!config_name_ok(g_config_name)) {
            snprintf(g_config_status, sizeof(g_config_status), "use letters, numbers, space, - or _");
            return;
        }
        const std::string path = config_path(g_config_name);
        if (::DeleteFileA(path.c_str()))
            snprintf(g_config_status, sizeof(g_config_status), "deleted %s", g_config_name);
        else
            snprintf(g_config_status, sizeof(g_config_status), "not found");
    }

    void panel_left(float width, float height) {
        (void)height;

        if (shown_tab == 0) {
            static const char* tabs[] = { "Aimbot", "Silent", "FOV", "Trigger" };

            begin_group("##aim", width);
            group_tabs("##aim_tabs", tabs, IM_ARRAYSIZE(tabs), &vars.gui.active_subtab[0]);
            switch (subtab_begin()) {
            case 0:
                checkbox("Enabled", &vars.aimbot.enabled);
                same_line_right(key_pill_width(vars.aimbot.aim_key));
                key_pill_mode("##aim_key", &vars.aimbot.aim_key, &vars.aimbot.aim_key_mode);
                checkbox("Team Check", &vars.aimbot.teamcheck);
                checkbox("Target Team", &vars.aimbot.target_team);
                checkbox("Target Knocked", &vars.aimbot.target_knocked);
                checkbox("Aim At Head", &vars.aimbot.aim_at_head);
                checkbox("Aim At Torso", &vars.aimbot.aim_at_torso);
                if (vars.aimbot.enabled) {
                    slider_float("Smoothness", &vars.aimbot.smoothness, 1.f, 30.f, "%.1f");
                    slider_float("Max Distance", &vars.aimbot.max_distance, 50.f, 2000.f, "%.0f");
                }
                break;
            case 1:
                checkbox("Silent Aim", &vars.aimbot.pf_silent_enabled);
                checkbox("Auto Shoot", &vars.aimbot.pf_silent_auto_shoot);
                checkbox("Prediction", &vars.aimbot.pf_silent_prediction);
                if (vars.aimbot.pf_silent_enabled)
                    slider_float("Smoothness", &vars.aimbot.pf_silent_smoothness, 0.f, 10.f, "%.1f");
                if (vars.aimbot.pf_silent_prediction)
                    slider_float("Prediction Amount", &vars.aimbot.pf_silent_prediction_amount, 0.f, 2.f, "%.2f");
                break;
            case 2:
                checkbox("FOV Check", &vars.aimbot.fov_check);
                checkbox("Draw Circle", &vars.aimbot.fov_circle_enabled);
                same_line_right(layout.swatch);
                color_button("##fov_col", vars.aimbot.fov_color);
                checkbox("Filled", &vars.aimbot.fov_filled);
                if (vars.aimbot.fov_check || vars.aimbot.fov_circle_enabled)
                    slider_float("FOV Radius", &vars.aimbot.fov_radius, 10.f, 600.f, "%.0f");
                combo("Circle Style", &vars.aimbot.fov_style, "Solid\0Dashed\0Dotted\0\0");
                break;
            default:
                checkbox("Enabled", &vars.aimbot.trigger_enabled);
                if (vars.aimbot.trigger_enabled) {
                    checkbox("Always On", &vars.aimbot.trigger_always);
                    if (!vars.aimbot.trigger_always)
                        keybind("Key", &vars.aimbot.trigger_key);
                    slider_float("Delay", &vars.aimbot.trigger_delay_ms, 0.f, 300.f, "%.0f ms");
                }
                break;
            }
            subtab_end();
            end_group();
            return;
        }

        if (shown_tab == 1) {
            static const char* tabs[] = { "Players", "Box" };
            if (vars.gui.active_subtab[1] > 1)
                vars.gui.active_subtab[1] = 0;

            begin_group("##esp", width);
            group_tabs("##esp_tabs", tabs, IM_ARRAYSIZE(tabs), &vars.gui.active_subtab[1]);
            switch (subtab_begin()) {
            case 0:
                checkbox("Enabled", &vars.esp.enabled);
                checkbox("Name", &vars.esp.name);
                same_line_right(layout.swatch); color_button("##name_col", vars.esp.name_color);
                checkbox("Health Bar", &vars.esp.health);
                same_line_right(layout.swatch); color_button("##hp_col", vars.esp.health_color);
                checkbox("Health Text", &vars.esp.health_text);
                same_line_right(layout.swatch); color_button("##hp_text_col", vars.esp.health_text_color);
                checkbox("Weapon", &vars.esp.weapon);
                same_line_right(layout.swatch); color_button("##weapon_col", vars.esp.weapon_color);
                checkbox("Distance", &vars.esp.distance);
                same_line_right(layout.swatch); color_button("##dist_col", vars.esp.distance_color);
                combo("Name Style", &vars.esp.name_flags, "Plain\0Shadow\0Outline\0\0");
                if (vars.esp.enabled) {
                    slider_float("Name Size", &vars.esp.name_size, 8.f, 24.f, "%.0f");
                    slider_float("Health Width", &vars.esp.health_width, 1.f, 10.f, "%.0f");
                }
                break;
            case 1:
                checkbox("Box", &vars.esp.box);
                same_line_right(layout.swatch * 2.f + dpi(6.f)); color_button("##box_col", vars.esp.box_color);
                ImGui::SameLine(0.f, dpi(6.f));                  color_button("##box_col2", vars.esp.box_color_secondary);
                checkbox("Gradient", &vars.esp.box_gradient);
                checkbox("Filled", &vars.esp.box_filled);
                same_line_right(layout.swatch * 2.f + dpi(6.f)); color_button("##fill_top", vars.esp.box_filled_top_color);
                ImGui::SameLine(0.f, dpi(6.f));                  color_button("##fill_bot", vars.esp.box_filled_bottom_color);
                checkbox("Glow", &vars.esp.box_glow);
                same_line_right(layout.swatch); color_button("##glow_col", vars.esp.box_glow_color);
                combo("Box Style", &vars.esp.box_style, "Full\0Corner\0" "3D\0\0");
                if (vars.esp.box)
                    slider_float("Thickness", &vars.esp.box_thickness, 1.f, 6.f, "%.1f");
                if (vars.esp.box && vars.esp.box_glow)
                    slider_float("Glow Strength", &vars.esp.box_glow_strength, 0.f, 5.f, "%.1f");
                break;
            default:
                break;
            }
            subtab_end();
            end_group();

            begin_group("##snaplines", width);
            group_title("Snaplines");
            checkbox("Enabled", &vars.esp.snaplines);
            same_line_right(layout.swatch); color_button("##snap_col", vars.esp.snaplines_color);
            checkbox("Gradient", &vars.esp.snaplines_gradient);
            same_line_right(layout.swatch); color_button("##snap_col2", vars.esp.snaplines_gradient_color);
            combo("Origin", &vars.esp.snaplines_position, "Top\0Center\0Bottom\0\0");
            if (vars.esp.snaplines)
                slider_float("Thickness", &vars.esp.snaplines_thickness, 1.f, 6.f, "%.1f");
            end_group();
            return;
        }

        if (shown_tab == 2) {
            static const char* tabs[] = { "Lighting", "Atmosphere", "Particles" };

            begin_group("##world", width);
            group_tabs("##world_tabs", tabs, IM_ARRAYSIZE(tabs), &vars.gui.active_subtab[2]);
            switch (subtab_begin()) {
            case 0:
                checkbox("Ambient", &vars.world.world_ambient_enabled);
                same_line_right(layout.swatch * 2.f + dpi(6.f)); color_button("##amb_col", vars.world.world_ambient_color, false);
                ImGui::SameLine(0.f, dpi(6.f));                  color_button("##amb_out", vars.world.world_outdoor_ambient_color, false);
                checkbox("Brightness", &vars.world.world_brightness_enabled);
                checkbox("Exposure", &vars.world.world_exposure_enabled);
                checkbox("No Shadows", &vars.world.world_shadows_disabled);
                checkbox("Clock Time", &vars.world.world_clocktime_enabled);
                if (vars.world.world_brightness_enabled)
                    slider_float("Brightness", &vars.world.world_brightness, 0.f, 20.f, "%.1f");
                if (vars.world.world_exposure_enabled)
                    slider_float("Exposure", &vars.world.world_exposure, -5.f, 5.f, "%.2f");
                if (vars.world.world_clocktime_enabled)
                    slider_float("Time Of Day", &vars.world.world_clocktime_value, 0.f, 24.f, "%.1f");
                break;
            case 1:
                checkbox("Atmosphere", &vars.world.world_atmosphere_enabled);
                same_line_right(layout.swatch * 2.f + dpi(6.f)); color_button("##atm_col", vars.world.world_atmos_color, false);
                ImGui::SameLine(0.f, dpi(6.f));                  color_button("##atm_decay", vars.world.world_atmos_decay, false);
                if (vars.world.world_atmosphere_enabled) {
                    slider_float("Density", &vars.world.world_atmos_density, 0.f, 1.f, "%.2f");
                    slider_float("Glare", &vars.world.world_atmos_glare, 0.f, 10.f, "%.1f");
                    slider_float("Haze", &vars.world.world_atmos_haze, 0.f, 10.f, "%.1f");
                    slider_float("Offset", &vars.world.world_atmos_offset, -1.f, 1.f, "%.2f");
                }
                break;
            default: {
                checkbox("Particles", &vars.world.particles_enabled);
                combo("Style", &vars.world.particle_style, "Snow\0Rain\0Ash\0Fireflies\0\0");
                if (vars.world.particles_enabled) {
                    float count = (float)vars.world.particle_count;
                    if (slider_float("Count", &count, 0.f, 1000.f, "%.0f"))
                        vars.world.particle_count = (int)count;

                    slider_float("Speed", &vars.world.particle_speed, 0.f, 5.f, "%.2f");
                    slider_float("Wind", &vars.world.particle_wind, 0.f, 5.f, "%.2f");
                    slider_float("Glow", &vars.world.particle_glow, 0.f, 5.f, "%.2f");
                }
                break;
            }
            }
            subtab_end();
            end_group();
            return;
        }

        if (shown_tab == 3) {
            static const char* tabs[] = { "Movement", "Effects" };

            begin_group("##misc", width);
            group_tabs("##misc_tabs", tabs, IM_ARRAYSIZE(tabs), &vars.gui.active_subtab[3]);
            switch (subtab_begin()) {
            case 0:
                checkbox("Fly", &vars.misc.pf_fly);
                hotkey("Fly Key", &vars.misc.pf_fly_key, &vars.misc.pf_fly_key_mode);
                checkbox("Jump Power", &vars.misc.jump_power);
                checkbox("Third Person", &vars.misc.third_person);
                if (vars.misc.pf_fly)
                    slider_float("Fly Speed", &vars.misc.pf_fly_speed, 1.f, 100.f, "%.0f");
                if (vars.misc.jump_power)
                    slider_float("Jump Power", &vars.misc.jump_power_value, 10.f, 200.f, "%.0f");
                if (vars.misc.third_person)
                    slider_float("Camera Distance", &vars.misc.third_person_distance, 5.f, 40.f, "%.0f");
                break;
            default: {
                checkbox("Kill Effects", &vars.misc.kill_effects_enabled);
                checkbox("Glow", &vars.misc.kill_effects_glow);
                combo("Effect", &vars.misc.kill_effects_type, "Explosion\0Confetti\0Sparks\0Smoke\0\0");
                if (vars.misc.kill_effects_enabled) {
                    float parts = (float)vars.misc.kill_effects_particle_count;
                    if (slider_float("Particles", &parts, 0.f, 200.f, "%.0f"))
                        vars.misc.kill_effects_particle_count = (int)parts;

                    slider_float("Lifetime", &vars.misc.kill_effects_lifetime, 0.5f, 10.f, "%.1f s");
                }
                if (button("Test Effect")) vars.misc.kill_effects_test = true;
                break;
            }
            }
            subtab_end();
            end_group();
            return;
        }

        if (shown_tab == 4) {
            begin_group("##config", width);
            group_title("Config");
            input_text("Name", "name", g_config_name, sizeof(g_config_name));
            if (button("Save")) config_save();
            if (button("Load")) config_load();
            if (button("Delete")) config_delete();
            if (g_config_status[0])
                section_label(g_config_status);
            end_group();
            return;
        }

        if (shown_tab == 5) {
            begin_group("##interface", width);
            group_title("Interface");

            keybind("Menu Key", &vars.gui.menu_key);

            checkbox("Blur", &glass.enabled);
            checkbox("Streamproof", &vars.gui.streamproof);
            end_group();
            return;
        }

        begin_group("##empty_left", width);
        group_title(header_tabs[shown_tab]);
        section_label("no modules loaded");
        end_group();
    }

    void panel_right(float width, float height, const char* title) {
        (void)height;

        if (shown_tab == 0) {
            begin_group("##crosshair", width);
            group_title("Crosshair");
            checkbox("Enabled", &vars.aimbot.crosshair_enabled);
            same_line_right(layout.swatch); color_button("##cross_col", vars.aimbot.crosshair_color);
            if (vars.aimbot.crosshair_enabled)
                slider_float("Size", &vars.aimbot.crosshair_size, 2.f, 30.f, "%.0f");
            end_group();

            begin_group("##target", width);
            group_title("Target");
            checkbox("Target Indicator", &vars.aimbot.show_target_indicator);
            checkbox("Line To Target", &vars.aimbot.draw_line_to_target);
            same_line_right(layout.swatch); color_button("##tline_col", vars.aimbot.target_line_color);
            end_group();
            return;
        }

        if (shown_tab == 1) {
            begin_group("##flags", width);
            group_title("Flags");
            checkbox("Enabled", &vars.esp.flags_enabled);
            multi_combo("Shown Flags", vars.esp.flag_options,
                        "Health\0Armor\0Money\0Weapon\0Distance\0"
                        "Knocked\0Reloading\0Scoped\0Bot\0Friend\0\0");
            if (vars.esp.flags_enabled) {
                slider_float("Spacing", &vars.esp.flags_spacing, 4.f, 30.f, "%.0f");
                slider_float("Font Size", &vars.esp.flags_font_size, 6.f, 20.f, "%.0f");
            }
            end_group();

            begin_group("##radar", width);
            group_title("Radar");
            checkbox("Enabled", &vars.esp.radar_enabled);
            checkbox("Show Names", &vars.esp.radar_show_names);
            checkbox("Show Distance", &vars.esp.radar_show_distance);
            if (vars.esp.radar_enabled) {
                slider_float("Size", &vars.esp.radar_size, 100.f, 400.f, "%.0f");
                slider_float("Range", &vars.esp.radar_range, 25.f, 500.f, "%.0f");
                slider_float("Opacity", &vars.esp.radar_alpha, 0.f, 1.f, "%.2f");
            }
            end_group();

            begin_group("##offscreen", width);
            group_title("Off-screen");
            checkbox("Arrows", &vars.esp.oov_arrows);
            same_line_right(layout.swatch); color_button("##oov_col", vars.esp.oov_arrow_color);
            combo("Style", &vars.esp.oov_arrow_style, "Triangle\0Chevron\0Dot\0\0");
            if (vars.esp.oov_arrows) {
                slider_float("Size", &vars.esp.oov_arrow_size, 5.f, 50.f, "%.0f");
                slider_float("Distance", &vars.esp.oov_arrow_distance, 25.f, 400.f, "%.0f");
            }
            end_group();
            return;
        }

        if (shown_tab == 2) {
            begin_group("##fog", width);
            group_title("Fog");
            checkbox("Enabled", &vars.world.world_fog_enabled);
            same_line_right(layout.swatch); color_button("##fog_col", vars.world.world_fog_color, false);
            if (vars.world.world_fog_enabled) {
                slider_float("Start", &vars.world.world_fog_start, 0.f, 5000.f, "%.0f");
                slider_float("End", &vars.world.world_fog_end, 0.f, 100000.f, "%.0f");
            }
            end_group();

            begin_group("##camera", width);
            group_title("Camera");
            checkbox("Custom FOV", &vars.world.world_fov_enabled);
            if (vars.world.world_fov_enabled)
                slider_float("Field Of View", &vars.world.world_fov, 30.f, 120.f, "%.0f");
            end_group();
            return;
        }

        if (shown_tab == 3) {
            begin_group("##hitbox", width);
            group_title("Hitbox Expander");
            checkbox("Enabled", &vars.misc.hitbox_expander_enabled);
            checkbox("No Collide", &vars.misc.hitbox_expander_no_collide);
            checkbox("Transparent", &vars.misc.hitbox_expander_transparent);
            checkbox("Visualize", &vars.misc.hitbox_expander_visualize);
            if (vars.misc.hitbox_expander_enabled)
                slider_float("Size", &vars.misc.hitbox_expander_size, 1.f, 10.f, "%.1f");
            if (vars.misc.hitbox_expander_enabled && vars.misc.hitbox_expander_transparent)
                slider_float("Transparency", &vars.misc.hitbox_expander_transparency, 0.f, 1.f, "%.2f");
            end_group();
            return;
        }

        if (shown_tab == 4) {
            begin_group("##saved", width);
            group_title("Saved");
            std::vector<std::string> names;
            config_list(names);
            if (names.empty())
                section_label("no configs");
            for (size_t i = 0; i < names.size(); ++i) {
                if (button(names[i].c_str())) {
                    snprintf(g_config_name, sizeof(g_config_name), "%s", names[i].c_str());
                }
            }
            end_group();
            return;
        }

        if (shown_tab == 5) {
            begin_group("##overlays", width);
            group_title("Overlays");
            checkbox("Watermark", &vars.gui.show_watermark);
            checkbox("Watching", &vars.gui.show_playerlist);
            checkbox("Active Hotkeys", &vars.gui.show_active_hotkeys);
            end_group();

            return;
        }

        begin_group("##empty_right", width);
        group_title(title);
        section_label("no modules loaded");
        end_group();
    }

    void panel_preview(float width, float height) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 p_min = ImGui::GetCursorScreenPos();
        const ImVec2 p_max(p_min.x + width, p_min.y + height);

        edraw.shadow_rect(p_min, p_max, ImColor(0, 0, 0, 255), layout.shadow, layout.window_rounding);
        if (!glass_pane(dl, p_min, p_max, layout.window_rounding,
                        colors.background, ImGui::GetStyle().Alpha))
            edraw.rect_filled(p_min, p_max, colors.background, layout.window_rounding);
        edraw.rect(p_min, p_max, colors.window_border, layout.window_rounding, 0, layout.border);

        ImFont* fb12 = fnt(efonts.inter.Bold12);
        ImFont* f10 = fnt(efonts.inter.Text10);

        const float pad = dpi(16.f);
        const float head_y = p_min.y + layout.header_h * 0.5f;
        const float x = p_min.x + pad;
        const ImVec2 t2 = measure(fb12, "Preview");
        draw_text(fb12, ImVec2(x, head_y - t2.y * 0.5f), colors.accent, "Preview");

        const float div_y = p_min.y + layout.header_h;
        edraw.line(ImVec2(p_min.x, div_y), ImVec2(p_max.x, div_y), colors.divider, layout.border);

        const float inner = dpi(12.f);
        const float stat_h = fsize(f10);
        const float viewport_h = ImMax(dpi(80.f), height - layout.header_h - inner * 3.f - stat_h);
        const ImVec2 r_min(p_min.x + inner, div_y + inner);
        const ImVec2 r_max(p_max.x - inner, r_min.y + viewport_h);
        edraw.rect_filled(r_min, r_max,
                          ImColor(g_model3d.bg[0], g_model3d.bg[1], g_model3d.bg[2], g_model3d.bg[3]),
                          layout.control_rounding);

        float orbit_dx = 0.f, orbit_dy = 0.f;
        bool  orbiting = false;
        ImGui::SetCursorScreenPos(r_min);
        ImGui::InvisibleButton("##preview_orbit", ImVec2(r_max.x - r_min.x, r_max.y - r_min.y));
        orbiting = ImGui::IsItemActive();
        if (orbiting) {
            orbit_dx = ImGui::GetIO().MouseDelta.x;
            orbit_dy = ImGui::GetIO().MouseDelta.y;
        }
        if (orbiting || ImGui::IsItemHovered())
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        dl->PushClipRect(r_min, r_max, true);
        g_model3d.render(g_pd3dDevice, g_pd3dDeviceContext,
                         (int)(r_max.x - r_min.x), (int)(r_max.y - r_min.y),
                         orbit_dx, orbit_dy, orbiting);
        if (g_model3d.texture())
            edraw.image(g_model3d.texture(), r_min, r_max);
        dl->PopClipRect();

        char fps[32], ping[32], cpu[32];
        snprintf(fps, sizeof(fps), "fps %d", (int)ImGui::GetIO().Framerate);
        snprintf(ping, sizeof(ping), "ping %dms", 12);
        snprintf(cpu, sizeof(cpu), "cpu %d%%", 3);

        const float stat_y = ImMin(r_max.y + inner, p_max.y - inner - stat_h);
        draw_text(f10, ImVec2(r_min.x, stat_y), colors.text_muted, fps);
        const ImVec2 ps = measure(f10, ping);
        draw_text(f10, ImVec2((r_min.x + r_max.x) * 0.5f - ps.x * 0.5f, stat_y), colors.text_muted, ping);
        const ImVec2 cs = measure(f10, cpu);
        draw_text(f10, ImVec2(r_max.x - cs.x, stat_y), colors.text_muted, cpu);
    }

    void push_font_stretch() { ImGui::SetWindowFontScale(font_stretch()); }

    static void scroll_wheel_update() {
        ImGuiWindow* w = ImGui::GetCurrentWindow();
        ImGuiStorage* st = ImGui::GetStateStorage();
        const ImGuiID k_target = w->GetID("##egui_sc_target");
        const ImGuiID k_drag   = w->GetID("##egui_sc_drag");
        const ImGuiID k_snap   = w->GetID("##egui_sc_snap");

        const float max_y = ImMax(0.0f, w->ScrollMax.y);
        float target = st->GetFloat(k_target, w->Scroll.y);

        if (g_scroll_reset_next) {
            g_scroll_reset_next = false;
            target = 0.0f;
            st->SetBool(k_snap, true);
        }

        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
            const float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f)
                target -= wheel * ImGui::GetFontSize() * 5.0f;
        }
        target = ImClamp(target, 0.0f, max_y);

        const bool dragging = st->GetBool(k_drag, false);
        const bool snap = st->GetBool(k_snap, false);
        st->SetBool(k_snap, false);
        const float speed = (dragging || snap) ? 1e9f : 16.0f;

        float cur = w->Scroll.y;
        if (ImFabs(cur - target) > 0.05f) {
            cur = ImLerp(cur, target, ImClamp(ImGui::GetIO().DeltaTime * speed, 0.0f, 1.0f));
            if (ImFabs(cur - target) < 0.5f) cur = target;
            ImGui::SetScrollY(cur);
        }
        st->SetFloat(k_target, target);
    }

    static void scroll_bar_draw() {
        ImGuiWindow* w = ImGui::GetCurrentWindow();
        ImGuiStorage* st = ImGui::GetStateStorage();
        const ImGuiID k_target = w->GetID("##egui_sc_target");
        const ImGuiID k_drag   = w->GetID("##egui_sc_drag");

        const float max_y = ImMax(0.0f, w->ScrollMax.y);
        st->SetBool(k_drag, false);
        if (max_y <= 0.0f) return;

        const float view_h = w->Size.y;
        const float track_x = w->Pos.x + w->Size.x - layout.scroll_margin - layout.scroll_w;
        const float track_y0 = w->Pos.y + layout.scroll_margin;
        const float track_y1 = w->Pos.y + view_h - layout.scroll_margin;
        const float track_h = ImMax(track_y1 - track_y0, 1.0f);

        const float thumb_h = ImClamp(track_h * (view_h / (view_h + max_y)), layout.scroll_min, track_h);
        const float thumb_y = track_y0 + (track_h - thumb_h) * ImClamp(w->Scroll.y / max_y, 0.0f, 1.0f);

        const ImVec2 backup_cursor = w->DC.CursorPos;
        const ImVec2 backup_max = w->DC.CursorMaxPos;

        const float grab_w = layout.scroll_w + dpi(6.0f);
        ImGui::SetCursorScreenPos(ImVec2(track_x - dpi(3.0f), track_y0));
        ImGui::InvisibleButton("##scrollbar", ImVec2(grab_w, track_h));

        const bool hovered = ImGui::IsItemHovered();
        const bool held = ImGui::IsItemActive();
        const ImGuiID id = ImGui::GetItemID();
        const ImGuiID k_grab = id ^ 0x5C0117B1u;

        if (ImGui::IsItemActivated()) {
            const float my = ImGui::GetIO().MousePos.y;
            const bool on_thumb = (my >= thumb_y && my <= thumb_y + thumb_h);
            st->SetFloat(k_grab, on_thumb ? (my - thumb_y) : thumb_h * 0.5f);
        }
        if (held) {
            const float grab = st->GetFloat(k_grab, thumb_h * 0.5f);
            const float ty = ImGui::GetIO().MousePos.y - grab;
            const float rel = ImClamp((ty - track_y0) / ImMax(track_h - thumb_h, 1.0f), 0.0f, 1.0f);
            st->SetFloat(k_target, rel * max_y);
            st->SetBool(k_drag, true);
        }

        w->DC.CursorPos = backup_cursor;
        w->DC.CursorMaxPos = backup_max;

        const float hv = anim(anim_hover(id), (hovered || held) ? 1.0f : 0.0f);
        const float r_track = layout.scroll_w * 0.5f;

        edraw.rect_filled(ImVec2(track_x, track_y0), ImVec2(track_x + layout.scroll_w, track_y1),
                          fade(colors.control, 0.35f + 0.45f * hv), r_track);
        edraw.rect_filled(ImVec2(track_x, thumb_y), ImVec2(track_x + layout.scroll_w, thumb_y + thumb_h),
                          ImLerp(colors.scroll, colors.text_muted, hv), r_track);
    }

    void begin_child(const char* name, const ImVec2& size) {
        ImGui::BeginChild(name, size, false,
                          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysUseWindowPadding |
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        push_font_stretch();
        scroll_wheel_update();
    }

    void end_child() {
        scroll_bar_draw();
        ImGui::EndChild();
    }

}
