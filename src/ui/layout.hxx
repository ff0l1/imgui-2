#pragma once
#include "imgui.h"
#include "theme.hxx"
#include <d3d11.h>

inline ID3D11Device* g_pd3dDevice = nullptr;
inline ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
inline IDXGISwapChain* g_pSwapChain = nullptr;
inline UINT            g_ResizeWidth = 0, g_ResizeHeight = 0;
inline ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

namespace egui {

    struct Layout {

        float window_rounding  = 10.0f;
        float card_rounding    = 8.0f;
        float control_rounding = 6.0f;

        float shadow_pad = 26.0f;

        float main_w    = 640.0f;
        float preview_w = 270.0f;
        float preview_h = 360.0f;
        float panel_gap = 20.0f;

        float header_h  = 43.0f;
        float tabbar_h  = 46.0f;
        float content_h = 400.0f;
        float footer_h  = 36.0f;

        float tab_h     = 26.0f;
        float tab_pad_x = 12.0f;
        float tab_gap   = 4.0f;

        float content_pad = 14.0f;
        float col_gap     = 14.0f;

        float card_gap   = 12.0f;
        float card_pad_x = 14.0f;
        float card_pad_t = 12.0f;
        float card_pad_b = 14.0f;

        float row_gap    = 7.0f;
        float row_h      = 18.0f;
        float check_box  = 14.0f;
        float check_gap  = 10.0f;
        float control_h  = 32.0f;
        float track_h    = 6.0f;
        float swatch     = 16.0f;
        float pill_w     = 40.0f;
        float pill_pad_x = 10.0f;
        float pill_h     = 20.0f;
        float label_gap  = 6.0f;
        float widget_top = 7.0f;

        float pick_w   = 176.0f;
        float pick_bar = 12.0f;
        float pick_gap = 6.0f;
        float pick_pad = 8.0f;

        float ov_pad       = 10.0f;
        float ov_gap       = 6.0f;
        float ov_margin    = 14.0f;
        float ov_rounding  = 8.0f;
        float ov_row_h     = 22.0f;

        float scroll_w      = 4.0f;
        float scroll_margin = 5.0f;
        float scroll_min    = 28.0f;

        float border   = 1.0f;
        float shadow   = 24.0f;
    };

    inline Layout layout;
    inline const Layout layout_design;

    static_assert(sizeof(Layout) % sizeof(float) == 0, "Layout is only floats");

    struct Settings {
        ImVec2 size = ImVec2(1062, 777);
        const char* project_name = "FF0L";
        int rounding = 10;

        Theme theme = Theme::Dark;

        float dpi_scale = 1.0f;
        float current_dpi_scale = 1.0f;
        float scale_current = 1.0f;

        struct Alpha { float menu = 0.0f; } alpha;

        struct Overlays {
            bool watermark  = true;
            bool keybinds   = true;
            bool spectators = true;
        } overlay;

        struct State {
            bool menu_open = true;
            ImVec2 menu_window_center = ImVec2(-1.f, -1.f);
            ImVec2 menu_window_pos = ImVec2(0, 0);
        } state;

        ImGuiWindowFlags window_flags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoSavedSettings;
    };

    inline Settings settings;

    inline float dpi(float v) { return v * settings.current_dpi_scale; }

    inline float font_stretch() {
        return settings.scale_current > 0.0f
            ? settings.current_dpi_scale / settings.scale_current
            : 1.0f;
    }
}
