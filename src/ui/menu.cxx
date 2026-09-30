#include "egui.hxx"
#include "layout.hxx"
#include "theme.hxx"
#include "glass.hxx"
#include "keys.hxx"
#include "state.hxx"
#include "imgui_internal.h"
#include <vector>
#include <string>
#include <cstdio>
#include <cstring>
#include <Windows.h>

namespace egui { void preview_window(); }

void egui::menu() {

    egui::hotkey_poll();

    static bool was_key_down = false;
    const int menu_key = vars.gui.menu_key;
    const bool is_key_down = !key_listening() && menu_key > 0
        && (GetAsyncKeyState(menu_key) & 0x8000) != 0;
    if (is_key_down && !was_key_down)
        settings.state.menu_open = !settings.state.menu_open;
    was_key_down = is_key_down;

    if (!settings.state.menu_open) {
        settings.alpha.menu = 0.0f;
        return;
    }

    fade_to(settings.alpha.menu, true);

    if (settings.alpha.menu <= 0.001f)
        return;

    settings.size = ImVec2(
        layout.main_w + layout.shadow_pad * 2.0f,
        layout.header_h + layout.tabbar_h + layout.content_h + layout.footer_h + layout.shadow_pad * 2.0f);

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f),
                            ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));

    static ImVec2 applied_size = ImVec2(0.0f, 0.0f);
    const bool size_changed = (applied_size.x != settings.size.x || applied_size.y != settings.size.y);
    if (size_changed && applied_size.x > 0.0f && settings.state.menu_window_center.x >= 0.0f) {
        ImGui::SetNextWindowPos(ImVec2(settings.state.menu_window_center.x - settings.size.x * 0.5f,
                                       settings.state.menu_window_center.y - settings.size.y * 0.5f),
                                ImGuiCond_Always);
    }
    applied_size = settings.size;

    ImGui::SetNextWindowSize(settings.size, ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, settings.alpha.menu);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

    if (ImGui::Begin(settings.project_name, &settings.state.menu_open, settings.window_flags)) {
        settings.state.menu_window_pos = ImGui::GetWindowPos();
        const ImVec2 win_size = ImGui::GetWindowSize();
        settings.state.menu_window_center = ImVec2(
            settings.state.menu_window_pos.x + win_size.x * 0.5f,
            settings.state.menu_window_pos.y + win_size.y * 0.5f);

        g_any_slider_active = false;
        egui::template_shell();
    }
    ImGui::End();

    ImGui::PopStyleVar(4);
    egui::preview_window();
}

void egui::preview_window()
{
    const ImVec2 menu_pos = settings.state.menu_window_pos;
    static ImVec2 preview_pos(0.0f, 0.0f);
    static ImVec2 last_menu(0.0f, 0.0f);
    static bool placed = false;

    ImGuiContext& g = *ImGui::GetCurrentContext();
    const ImGuiWindow* moving = g.MovingWindow ? g.MovingWindow->RootWindow : nullptr;
    const bool dragging = moving && strcmp(moving->Name, "##egui_preview") == 0;

    if (!placed) {
        preview_pos = ImVec2(menu_pos.x + layout.main_w + layout.panel_gap, menu_pos.y);
        last_menu = menu_pos;
        placed = true;
    } else if (!dragging) {
        preview_pos.x += menu_pos.x - last_menu.x;
        preview_pos.y += menu_pos.y - last_menu.y;
    }

    const ImVec2 sz(layout.preview_w + layout.shadow_pad * 2.0f,
                    layout.preview_h + layout.shadow_pad * 2.0f);
    if (!dragging)
        ImGui::SetNextWindowPos(preview_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(sz, ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, settings.alpha.menu);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

    if (ImGui::Begin("##egui_preview", nullptr, settings.window_flags)) {
        push_font_stretch();
        const ImVec2 origin = ImGui::GetWindowPos();
        ImGui::SetCursorScreenPos(ImVec2(origin.x + layout.shadow_pad, origin.y + layout.shadow_pad));
        panel_preview(layout.preview_w, layout.preview_h);
        preview_pos = ImGui::GetWindowPos();
    }
    ImGui::End();
    ImGui::PopStyleVar(4);
    last_menu = menu_pos;
}

namespace egui {

static float g_ov_alpha = 1.0f;

static ImColor ov(const ImVec4& c, float mul = 1.0f) { return fade(c, g_ov_alpha * mul); }

static void overlay_frame(ImVec2 p_min, ImVec2 p_max, bool accent_bar = false)
{
    edraw.shadow_rect(p_min, p_max, ov(ImVec4(0, 0, 0, 1)), layout.shadow * 0.6f, layout.ov_rounding);
    if (!glass_pane(edraw.list, p_min, p_max, layout.ov_rounding, colors.background, g_ov_alpha))
        edraw.rect_filled(p_min, p_max, ov(colors.background), layout.ov_rounding);
    edraw.rect(p_min, p_max, ov(colors.window_border), layout.ov_rounding, 0, layout.border);

    if (!accent_bar) return;
    const float bar_h = ImMax(1.0f, dpi(2.0f));
    const float rad   = ImMax(0.0f, ImMin(layout.ov_rounding, (p_max.x - p_min.x) * 0.5f - 1.0f));

    ImDrawList* dl = edraw.list ? edraw.list : ImGui::GetWindowDrawList();
    dl->PushClipRect(p_min, ImVec2(p_max.x, p_min.y + bar_h), true);
    edraw.rect_filled(p_min, ImVec2(p_max.x, p_min.y + rad * 2.0f + 1.0f), ov(colors.accent),
                      rad, ImDrawFlags_RoundCornersTop);
    dl->PopClipRect();
}

static ImVec2 viewport_min() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    return ImVec2(vp->Pos.x + layout.ov_margin, vp->Pos.y + layout.ov_margin);
}
static ImVec2 viewport_max() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    return ImVec2(vp->Pos.x + vp->Size.x - layout.ov_margin,
                  vp->Pos.y + vp->Size.y - layout.ov_margin);
}

static float g_watermark_bottom = 0.0f;
static float g_watermark_alpha  = 0.0f;

static float digit_slot(ImFont* f, int n)
{
    static ImFont* c_font = nullptr;
    static float   c_size = 0.0f;
    static float   c_w    = 0.0f;

    const float size = fsize(f);
    if (c_font != f || c_size != size) {
        c_font = f; c_size = size; c_w = 0.0f;
        for (char c = '0'; c <= '9'; c++)
            c_w = ImMax(c_w, f->CalcTextSizeA(size, FLT_MAX, 0.0f, &c, &c + 1).x);
    }
    return c_w * (float)n;
}

static void draw_slot(ImFont* f, float right_x, float cy, const ImVec4& col, const char* txt,
                      float dy = 0.0f)
{
    const ImVec2 ts = measure(f, txt);
    draw_text(f, ImVec2(right_x - ts.x, cy - ts.y * 0.5f + dy), col, txt);
}

void watermark()
{
    fade_to(g_watermark_alpha, settings.overlay.watermark);

    if (g_watermark_alpha <= 0.001f) { g_watermark_bottom = viewport_min().y; return; }

    ImFont* f_name = fnt(efonts.inter.Bold12);
    ImFont* f_val  = fnt(efonts.inter.Bold11);
    ImFont* f_unit = fnt(efonts.inter.Text10);

    SYSTEMTIME st;
    ::GetLocalTime(&st);

    char v_fps[12], v_ms[12], v_time[12];
    snprintf(v_fps,  sizeof(v_fps),  "%d", (int)ImMin(ImGui::GetIO().Framerate + 0.5f, 999.0f));
    snprintf(v_ms,   sizeof(v_ms),   "%d", 12);
    snprintf(v_time, sizeof(v_time), "%02d:%02d", st.wHour, st.wMinute);

    const float gap    = dpi(13.0f);
    const float gap_u  = dpi(3.0f);

    const ImVec2 name_sz = measure(f_name, settings.project_name);
    const float slot   = digit_slot(f_val, 3);
    const float u_fps  = measure(f_unit, "fps").x;
    const float u_ms   = measure(f_unit, "ms").x;
    const float w_time = measure(f_val, "00:00").x;

    const float w = layout.ov_pad * 2.0f
                  + name_sz.x
                  + gap + slot + gap_u + u_fps
                  + gap + slot + gap_u + u_ms
                  + gap + w_time;
    const float h = layout.ov_pad * 2.0f + ImMax(name_sz.y, fsize(f_val));

    static ImVec2 wm_pos(0.0f, 0.0f);
    static bool wm_placed = false;
    if (!wm_placed) {
        wm_pos = ImVec2(viewport_max().x - w, viewport_min().y);
        wm_placed = true;
    }

    ImGuiContext& g = *ImGui::GetCurrentContext();
    const ImGuiWindow* moving = g.MovingWindow ? g.MovingWindow->RootWindow : nullptr;
    const bool dragging = moving && strcmp(moving->Name, "##egui_watermark") == 0;
    if (!dragging)
        ImGui::SetNextWindowPos(wm_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_watermark_alpha);

    const bool open = ImGui::Begin("##egui_watermark", nullptr, settings.window_flags);
    if (open) {
        push_font_stretch();
        wm_pos = ImGui::GetWindowPos();
        const ImVec2 p_min = wm_pos;
        const ImVec2 p_max(p_min.x + w, p_min.y + h);

        g_ov_alpha = g_watermark_alpha;
        edraw.list = ImGui::GetForegroundDrawList();
        overlay_frame(p_min, p_max);

    const float cy = (p_min.y + p_max.y) * 0.5f;
    float x = p_min.x + layout.ov_pad;

    draw_text(f_name, ImVec2(x, cy - name_sz.y * 0.5f - dpi(1.0f)), ov(colors.accent), settings.project_name);
    x += name_sz.x + gap;

    draw_slot(f_val, x + slot, cy, ov(colors.text_primary), v_fps);
    x += slot + gap_u;
    draw_text(f_unit, ImVec2(x, cy - fsize(f_unit) * 0.5f - dpi(1.0f)), ov(colors.text_muted), "fps");
    x += u_fps + gap;

    draw_slot(f_val, x + slot, cy, ov(colors.text_primary), v_ms);
    x += slot + gap_u;
    draw_text(f_unit, ImVec2(x, cy - fsize(f_unit) * 0.5f - dpi(1.0f)), ov(colors.text_muted), "ms");
    x += u_ms + gap;

    draw_slot(f_val, x + w_time, cy, ov(colors.text_sec), v_time);

        edraw.list = nullptr;
        g_ov_alpha = 1.0f;
    }
    ImGui::End();
    ImGui::PopStyleVar(4);

    g_watermark_bottom = viewport_min().y + h;
}

struct ListEntry {
    std::string left;
    std::string right;
    std::string extra;
    float t      = 0.0f;
    float act    = 0.0f;
    bool  live   = false;
    bool  active = false;
};

struct ListState {
    std::vector<ListEntry> rows;
    float alpha = 0.0f;
};
static ListState g_keybinds, g_spectators;

static void list_begin(ListState& s)
{
    for (size_t i = 0; i < s.rows.size(); i++) s.rows[i].live = false;
}

static ListEntry& list_row(ListState& s, const char* name)
{
    for (size_t i = 0; i < s.rows.size(); i++)
        if (!s.rows[i].live && s.rows[i].left == name) return s.rows[i];

    s.rows.push_back(ListEntry());
    s.rows.back().left = name;
    return s.rows.back();
}

static int list_advance(ListState& s, bool show, float& rows_h)
{
    int live = 0;
    rows_h = 0.0f;
    for (size_t i = 0; i < s.rows.size(); ) {
        ListEntry& r = s.rows[i];
        fade_to(r.t, r.live && show);
        fade_to(r.act, r.active && r.live, hover_speed);

        if (!r.live && r.t <= 0.0f) { s.rows.erase(s.rows.begin() + (int)i); continue; }

        rows_h += layout.ov_row_h * r.t;
        if (r.live) live++;
        i++;
    }
    return live;
}

static void draw_text_sh(ImFont* f, ImVec2 pos, const ImVec4& col, const char* s, float a = 1.0f)
{
    draw_text(f, ImVec2(pos.x + dpi(1.0f), pos.y + dpi(1.0f)), ov(ImVec4(0, 0, 0, 0.75f), a), s);
    draw_text(f, pos, ov(col, a), s);
}

static float tracked_w(ImFont* f, const char* s, float track)
{
    float w = 0.0f;
    for (const char* p = s; *p; p++) {
        const char c = (char)ImToUpper(*p);
        w += f->CalcTextSizeA(fsize(f), FLT_MAX, 0.0f, &c, &c + 1).x + track;
    }
    return w > 0.0f ? w - track : 0.0f;
}

static void draw_tracked_sh(ImFont* f, ImVec2 pos, const ImVec4& col, const char* s,
                            float track, float a = 1.0f)
{
    for (const char* p = s; *p; p++) {
        const char c = (char)ImToUpper(*p);
        draw_text(f, ImVec2(pos.x + dpi(1.0f), pos.y + dpi(1.0f)), ov(ImVec4(0, 0, 0, 0.75f), a), &c, &c + 1);
        draw_text(f, pos, ov(col, a), &c, &c + 1);
        pos.x += f->CalcTextSizeA(fsize(f), FLT_MAX, 0.0f, &c, &c + 1).x + track;
    }
}

static void keybind_push(const char* name, int key, int mode, bool active)
{
    if (!name || !*name) return;

    if (key <= 0 && mode != 0) return;

    static const char* modes[] = { "always", "hold", "toggle" };

    ListEntry& e = list_row(g_keybinds, name);
    e.live   = true;
    e.right  = (key > 0 && key < IM_ARRAYSIZE(::keys)) ? ::keys[key] : "on";
    e.extra  = (mode >= 0 && mode < IM_ARRAYSIZE(modes)) ? modes[mode] : modes[0];
    e.active = active;
}

void keybind_list_begin()
{
    hotkey_poll();
    list_begin(g_keybinds);

    for (int i = 0, n = hotkey_count(); i < n; i++) {
        const char* name = nullptr;
        int key = 0, mode = 0;
        bool active = false, enabled = false;
        if (!hotkey_info(i, &name, &key, &mode, &active, &enabled)) continue;

        if (key > 0 || (mode == 0 && enabled))
            keybind_push(name, key, mode, active);
    }
}

void keybind_list_add(const char* name, int key, int mode)
{
    keybind_push(name, key, mode, (mode == 0) || ((::GetAsyncKeyState(key) & 0x8000) != 0));
}

struct FloatPos {
    ImVec2 pos = {};
    bool placed = false;
};

static bool begin_float(const char* id, FloatPos& slot, ImVec2 def_pos, ImVec2 size, float alpha)
{
    if (!slot.placed) {
        slot.pos = def_pos;
        slot.placed = true;
    }

    ImGuiContext& g = *ImGui::GetCurrentContext();
    const ImGuiWindow* moving = g.MovingWindow ? g.MovingWindow->RootWindow : nullptr;
    const bool dragging = moving && strcmp(moving->Name, id) == 0;
    if (!dragging)
        ImGui::SetNextWindowPos(slot.pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

    const bool open = ImGui::Begin(id, nullptr, settings.window_flags);
    if (open) {
        push_font_stretch();
        slot.pos = ImGui::GetWindowPos();
    }
    return open;
}

static void end_float()
{
    ImGui::End();
    ImGui::PopStyleVar(4);
}

void keybind_list_end()
{
    ListState& s = g_keybinds;
    const bool show = settings.overlay.keybinds;

    ImFont* f_key  = fnt(efonts.inter.Bold10);
    ImFont* f_name = fnt(efonts.inter.Text12);
    ImFont* f_head = fnt(efonts.inter.Bold11);

    float rows_h = 0.0f;
    const int live = list_advance(s, show, rows_h);

    fade_to(s.alpha, show && live > 0);
    if (s.alpha <= 0.001f) return;

    const float track = dpi(0.9f);
    const float gap   = dpi(12.0f);
    const float pad   = layout.ov_pad;

    float w_key = 0.0f;
    float w_name = 0.0f;
    for (size_t i = 0; i < s.rows.size(); i++) {
        if (!s.rows[i].live) continue;
        w_key = ImMax(w_key, tracked_w(f_key, s.rows[i].right.c_str(), track));
        w_name = ImMax(w_name, measure(f_name, s.rows[i].left.c_str()).x);
    }

    const char* title = "Hotkeys";
    const float head_h = fsize(f_head) + dpi(8.0f);
    const float inner = ImMax(measure(f_head, title).x, w_key + gap + w_name);
    const float w = inner + pad * 2.0f;
    const float h = pad + head_h + rows_h + pad;

    static FloatPos slot;
    g_ov_alpha = s.alpha;
    if (begin_float("##egui_hotkeys", slot,
                    ImVec2(viewport_min().x, viewport_max().y - h),
                    ImVec2(w, h), s.alpha)) {
        const ImVec2 p = ImGui::GetWindowPos();
        edraw.list = ImGui::GetWindowDrawList();
        overlay_frame(p, ImVec2(p.x + w, p.y + h));

        const float x0 = p.x + pad;
        draw_text(f_head, ImVec2(x0, p.y + pad), ov(colors.text_sec), title);

        float y = p.y + pad + head_h;
        for (size_t i = 0; i < s.rows.size(); i++) {
            ListEntry& r = s.rows[i];
            const float rh = layout.ov_row_h * r.t;
            if (rh <= 0.01f) continue;

            const float a = r.t;
            const float cy = y + rh * 0.5f;
            const float key_h = fsize(f_key);
            draw_tracked_sh(f_key, ImVec2(x0, cy - key_h * 0.5f),
                            ImLerp(colors.text_sec, colors.accent, r.act),
                            r.right.c_str(), track, a);

            const ImVec2 ns = measure(f_name, r.left.c_str());
            draw_text_sh(f_name, ImVec2(x0 + w_key + gap, cy - ns.y * 0.5f - dpi(1.0f)),
                         ImLerp(colors.text, colors.text_primary, r.act), r.left.c_str(), a);
            y += rh;
        }

        edraw.list = nullptr;
    }
    end_float();
    g_ov_alpha = 1.0f;
}

void spectator_list_begin() { list_begin(g_spectators); }

void spectator_list_add(const char* name)
{
    if (!name || !*name) return;
    list_row(g_spectators, name).live = true;
}

void spectator_list_end()
{
    ListState& s = g_spectators;
    const bool show = settings.overlay.spectators;

    ImFont* f_head = fnt(efonts.inter.Bold12);
    ImFont* f_name = fnt(efonts.inter.Text12);

    float rows_h = 0.0f;
    const int live = list_advance(s, show, rows_h);

    fade_to(s.alpha, show && live > 0);
    if (s.alpha <= 0.001f) return;

    char count[16];
    snprintf(count, sizeof(count), "%d", live);
    const char* word = "Watching";

    const float pad = layout.ov_pad;
    const float gap = dpi(6.0f);
    const float head_h = fsize(f_head) + dpi(8.0f);

    float w_name = 0.0f;
    for (size_t i = 0; i < s.rows.size(); i++)
        if (s.rows[i].live)
            w_name = ImMax(w_name, measure(f_name, s.rows[i].left.c_str()).x);

    const float head_w = measure(f_head, count).x + gap + measure(f_name, word).x;
    const float inner = ImMax(head_w, w_name);
    const float w = inner + pad * 2.0f;
    const float h = pad + head_h + rows_h + pad;

    static FloatPos slot;
    g_ov_alpha = s.alpha;
    if (begin_float("##egui_watching", slot,
                    ImVec2(viewport_max().x - w, g_watermark_bottom + layout.ov_gap),
                    ImVec2(w, h), s.alpha)) {
        const ImVec2 p = ImGui::GetWindowPos();
        edraw.list = ImGui::GetWindowDrawList();
        overlay_frame(p, ImVec2(p.x + w, p.y + h));

        const float x0 = p.x + pad;
        const float hy = p.y + pad;
        const ImVec2 cs = measure(f_head, count);
        draw_text(f_head, ImVec2(x0, hy), ov(colors.accent), count);
        draw_text(f_name, ImVec2(x0 + cs.x + gap, hy + (cs.y - measure(f_name, word).y) * 0.5f),
                  ov(colors.text_sec), word);

        float y = p.y + pad + head_h;
        for (size_t i = 0; i < s.rows.size(); i++) {
            ListEntry& r = s.rows[i];
            const float rh = layout.ov_row_h * r.t;
            if (rh <= 0.01f) continue;

            const ImVec2 ns = measure(f_name, r.left.c_str());
            draw_text_sh(f_name, ImVec2(x0, y + (rh - ns.y) * 0.5f - dpi(1.0f)),
                         colors.text, r.left.c_str(), r.t);
            y += rh;
        }

        edraw.list = nullptr;
    }
    end_float();
    g_ov_alpha = 1.0f;
}

}
