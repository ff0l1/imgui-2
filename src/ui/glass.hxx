#pragma once

#include "imgui.h"
#include "layout.hxx"
#include <d3d11.h>

namespace egui {

    struct Glass {
        bool  enabled    = true;
        bool  cards      = true;

        float blur       = 0.18f;
        float tint       = 0.94f;

        float tint_light = 0.45f;
        float thickness  = 1.5f;
        float refraction = 4.0f;
        float chroma     = 0.08f;
        float grain      = 0.00f;

        float sheen      = 0.00f;
        float specular   = 0.00f;

        float light_x    = -0.55f;
        float light_y    = -0.83f;
    };
    inline Glass glass;

    bool glass_pane(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float rounding,
                    const ImVec4& tint_col, float alpha, bool inner = false);

    void glass_capture();

    void glass_invalidate();

    void glass_shutdown();
}
