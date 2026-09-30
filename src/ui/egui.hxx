#pragma once
#include "imgui.h"
#include "imgui_internal.h"
#include "theme.hxx"
#include "layout.hxx"
#include <d3d11.h>
#include <cmath>

namespace egui {

    struct Draw {
        ImDrawList* list = nullptr;
        ImDrawList* get_list(ImDrawList* l);

        void rect_filled(ImVec2 p_min, ImVec2 p_max, ImColor col, float rounding = 0, ImDrawFlags flags = 0);
        void rect_filled_background(ImVec2 p_min, ImVec2 p_max, ImColor col, float rounding = 0, ImDrawFlags flags = 0);
        void rect(ImVec2 p_min, ImVec2 p_max, ImColor col, float rounding = 0, ImDrawFlags flags = 0, float thickness = 1.0f);
        void line(ImVec2 p1, ImVec2 p2, ImColor col, float thickness = 1.0f);
        void polyline(const ImVec2* points, int num_points, ImColor col, ImDrawFlags flags, float thickness);
        void circle(ImVec2 center, float radius, ImColor col, int num_segments = 0, float thickness = 1.0f);
        void circle_filled(ImVec2 center, float radius, ImColor col, int num_segments = 0);

        void text(ImVec2 pos, ImColor col, const char* text_begin, const char* text_end = nullptr);
        void text(ImVec2 pos, ImColor col, const char* text, ImFont* font);
        void text(ImFont* font, float font_size, ImVec2 pos, ImColor col, const char* text_begin, const char* text_end = nullptr, float wrap_width = 0.0f, const ImVec4* cpu_fine_clip_rect = nullptr);
        void text_gradient(ImVec2 pos, ImColor col_left, ImColor col_right, const char* text, ImFont* font = nullptr);

        void rect_filled_multi_color(ImVec2 p_min, ImVec2 p_max, ImColor col_upr_left, ImColor col_upr_right, ImColor col_bot_right, ImColor col_bot_left);
        void rect_filled_multi_color_rounding(ImVec2 p_min, ImVec2 p_max, ImColor col_upr_left, ImColor col_upr_right, ImColor col_bot_right, ImColor col_bot_left, float rounding, ImDrawFlags flags);
        void image(ImTextureID user_texture_id, ImVec2 p_min, ImVec2 p_max, ImVec2 uv_min = ImVec2(0, 0), ImVec2 uv_max = ImVec2(1, 1), ImColor col = ImColor(255, 255, 255));

        void shadow_rect(ImVec2 p_min, ImVec2 p_max, ImColor col, float shadow_thickness, float rounding = 0.0f);
        void shadow_circle(ImVec2 center, float radius, ImColor col, float shadow_thickness);
    };

    struct Fonts {
        struct Inter {
            ImFont* Regular = nullptr;
            ImFont* Medium = nullptr;
            ImFont* MediumSmall = nullptr;
            ImFont* SemiBold = nullptr;
            ImFont* SemiBoldSmall = nullptr;
            ImFont* SemiBold13 = nullptr;
            ImFont* Black = nullptr;
            ImFont* Bold = nullptr;
            ImFont* ExtraBold = nullptr;

            ImFont* Text10 = nullptr;
            ImFont* Text11 = nullptr;
            ImFont* Text12 = nullptr;
            ImFont* Text13 = nullptr;
            ImFont* Bold10 = nullptr;
            ImFont* Bold11 = nullptr;
            ImFont* Bold12 = nullptr;
            ImFont* Bold13 = nullptr;
        } inter;
        struct Icomoon {
            ImFont* regular = nullptr;
            ImFont* regular_small = nullptr;
            ImFont* regular_large = nullptr;
        } icomoon;
    };

    struct Images {
        ID3D11ShaderResourceView* background = nullptr;
        int background_w = 0, background_h = 0;
        ID3D11ShaderResourceView* logo = nullptr;

        ID3D11ShaderResourceView* character = nullptr;
        int character_w = 0, character_h = 0;
    };

    inline Draw edraw;
    inline Fonts efonts;
    inline Images eimages;
    inline bool g_any_slider_active = false;

    void initialize();

    bool create_texture_from_memory(ID3D11Device* dev, const void* data, size_t size,
                                    ID3D11ShaderResourceView** out_srv,
                                    int* out_w = nullptr, int* out_h = nullptr);

    float system_dpi_scale();
    void  apply_scale(float scale);
    void  update_dpi();

    void  push_font_stretch();

    void set_theme(Theme t);
    void update_theme();

    void template_shell();
    void begin_child(const char* name, const ImVec2& size);
    void end_child();
    void panel_left(float width, float height);
    void panel_right(float width, float height, const char* title = "Information");
    void panel_preview(float width, float height);

    void begin_group(const char* id, float width = 0.0f);
    void end_group();
    void group_title(const char* text);
    bool group_tabs(const char* id, const char* const* items, int count, int* current);
    void section_label(const char* text, const char* text_end = nullptr);
    void gap(float height = 7.0f);
    void same_line_right(float width);

    void menu();

    void watermark();

    void keybind_list_begin();
    void keybind_list_add(const char* name, int key, int mode);
    void keybind_list_end();

    void spectator_list_begin();
    void spectator_list_add(const char* name);
    void spectator_list_end();

    bool checkbox(const char* label, bool* v);
    bool slider_float(const char* label, float* v, float v_min, float v_max, const char* format = "%.0f%%");
    bool combo(const char* label, int* current_item, const char* items_separated_by_zeros);
    bool multi_combo(const char* label, bool* selected, const char* items_separated_by_zeros);
    bool colorpicker(const char* label, float* col, bool alpha = true);
    bool color_button(const char* id, float* col, bool alpha = true);
    bool button(const char* label, ImVec2 size = ImVec2(0, 0));
    bool icon_button(const char* label, const char* icon, ImVec2 size = ImVec2(0, 0));
    float key_pill_width(int key);
    bool key_pill(const char* id, int* key);
    bool keybind(const char* label, int* key);
    bool key_listening();
    bool key_pill_mode(const char* id, int* key, int* mode);
    bool hotkey(const char* label, int* key, int* mode);
    bool hotkey_bind(const char* label, int* key, int* mode, bool* value);

    void hotkey_register(const char* label, int* key, int* mode, bool* value = nullptr);
    void hotkey_poll();
    bool hotkey_active(const char* label);
    int  hotkey_count();
    bool hotkey_info(int index, const char** label, int* key, int* mode, bool* active,
                     bool* enabled = nullptr);
    bool input_text(const char* label, const char* hint, char* buf, size_t buf_size, ImGuiInputTextFlags flags = 0);
    bool input_text_hint(const char* hint, char* buf, size_t buf_size, ImGuiInputTextFlags flags = 0);
    bool text_editor(const char* label, char* buf, size_t buf_size, ImVec2 size = ImVec2(0, 0), ImGuiInputTextFlags flags = 0);
}

namespace egui {
    inline constexpr float fade_speed  = 12.0f;
    inline constexpr float hover_speed = 14.0f;
    inline constexpr float swap_time   = 0.125f;

    inline float anim_step(float speed) {
        return 1.0f - expf(-speed * ImMax(ImGui::GetIO().DeltaTime, 0.0f));
    }

    inline float fade_to(float& cur, bool on, float speed = fade_speed) {
        const float target = on ? 1.0f : 0.0f;
        cur = ImLerp(cur, target, anim_step(speed));
        if (ImFabs(cur - target) < 0.002f) cur = target;
        return cur;
    }

    inline float ramp_to(float& cur, bool up, float time = swap_time) {
        const float step = ImGui::GetIO().DeltaTime / ImMax(time, 0.0001f);
        cur = ImClamp(cur + (up ? step : -step), 0.0f, 1.0f);
        return cur;
    }
}

inline float anim(ImGuiID id, float target, float speed = egui::hover_speed) {
    ImGuiStorage* st = ImGui::GetStateStorage();
    float cur = st->GetFloat(id, target);
    cur = ImLerp(cur, target, egui::anim_step(speed));
    if (ImFabs(cur - target) < 0.001f) cur = target;
    st->SetFloat(id, cur);
    return cur;
}
inline ImGuiID anim_hover(ImGuiID id) { return id ^ 0xA1B2C3D4u; }
inline ImGuiID anim_value(ImGuiID id) { return id ^ 0x5E6F7A8Bu; }

inline ImColor mix(const ImVec4& a, const ImVec4& b, float t) { return ImColor(ImLerp(a, b, t)); }
inline ImColor fade(const ImVec4& c, float a) { return ImColor(c.x, c.y, c.z, c.w * a); }

inline ImFont* fnt(ImFont* f) { return f ? f : ImGui::GetFont(); }

inline float fsize(ImFont* f) { return fnt(f)->FontSize * egui::font_stretch(); }

inline ImVec2 measure(ImFont* f, const char* text, const char* text_end = nullptr) {
    f = fnt(f);
    return f->CalcTextSizeA(fsize(f), FLT_MAX, 0.0f, text, text_end);
}

inline void draw_text(ImFont* f, ImVec2 pos, const ImVec4& col, const char* text, const char* text_end = nullptr) {
    f = fnt(f);
    egui::edraw.text(f, fsize(f), pos, col, text, text_end);
}
