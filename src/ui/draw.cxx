#include "egui.hxx"
#include "layout.hxx"

namespace egui {
    ImDrawList* Draw::get_list(ImDrawList* l) {
        return l ? l : ImGui::GetWindowDrawList();
    }

    static ImColor ApplyAlpha(ImColor col) {
        col.Value.w *= ImGui::GetStyle().Alpha;
        return col;
    }

    void Draw::rect_filled(ImVec2 p_min, ImVec2 p_max, ImColor col, float rounding, ImDrawFlags flags) {
        get_list(list)->AddRectFilled(p_min, p_max, ApplyAlpha(col), rounding, flags);
    }

    void Draw::rect_filled_background(ImVec2 p_min, ImVec2 p_max, ImColor col, float rounding, ImDrawFlags flags) {
        ImGui::GetBackgroundDrawList()->AddRectFilled(p_min, p_max, ApplyAlpha(col), rounding, flags);
    }

    void Draw::rect(ImVec2 p_min, ImVec2 p_max, ImColor col, float rounding, ImDrawFlags flags, float thickness) {
        get_list(list)->AddRect(p_min, p_max, ApplyAlpha(col), rounding, flags, thickness);
    }

    void Draw::line(ImVec2 p1, ImVec2 p2, ImColor col, float thickness) {
        get_list(list)->AddLine(p1, p2, ApplyAlpha(col), thickness);
    }

    void Draw::polyline(const ImVec2* points, int num_points, ImColor col, ImDrawFlags flags, float thickness) {
        get_list(list)->AddPolyline(points, num_points, ApplyAlpha(col), flags, thickness);
    }

    void Draw::circle(ImVec2 center, float radius, ImColor col, int num_segments, float thickness) {
        get_list(list)->AddCircle(center, radius, ApplyAlpha(col), num_segments, thickness);
    }

    void Draw::circle_filled(ImVec2 center, float radius, ImColor col, int num_segments) {
        get_list(list)->AddCircleFilled(center, radius, ApplyAlpha(col), num_segments);
    }

    void Draw::text(ImVec2 pos, ImColor col, const char* text_begin, const char* text_end) {
        ImFont* font = ImGui::GetFont();
        if (font)
            get_list(list)->AddText(font, ImGui::GetFontSize(), pos, ApplyAlpha(col), text_begin, text_end);
        else
            get_list(list)->AddText(pos, ApplyAlpha(col), text_begin, text_end);
    }

    void Draw::text(ImVec2 pos, ImColor col, const char* text, ImFont* font) {
        if (font)
            get_list(list)->AddText(font, font->FontSize, pos, ApplyAlpha(col), text);
        else
            get_list(list)->AddText(pos, ApplyAlpha(col), text);
    }

    void Draw::text(ImFont* font, float font_size, ImVec2 pos, ImColor col, const char* text_begin, const char* text_end, float wrap_width, const ImVec4* cpu_fine_clip_rect) {
        get_list(list)->AddText(font, font_size, pos, ApplyAlpha(col), text_begin, text_end, wrap_width, cpu_fine_clip_rect);
    }

    void Draw::text_gradient(ImVec2 pos, ImColor col_left, ImColor col_right, const char* text, ImFont* font) {
        if (!text) return;
        ImFont* f = font ? font : ImGui::GetFont();
        float font_size = f->FontSize;
        ImVec2 size = f->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text);

        float current_x = pos.x;
        const char* p = text;
        while (*p) {
            char buf[2] = { *p, '\0' };
            float char_w = f->CalcTextSizeA(font_size, FLT_MAX, 0.0f, buf).x;

            float t = (current_x - pos.x) / (size.x > 0 ? size.x : 1.0f);
            ImColor col = ImColor(ImLerp((ImVec4)col_left, (ImVec4)col_right, t));

            get_list(list)->AddText(f, font_size, ImVec2(current_x, pos.y), ApplyAlpha(col), buf);
            current_x += char_w;
            p++;
        }
    }

    void Draw::rect_filled_multi_color(ImVec2 p_min, ImVec2 p_max, ImColor col_upr_left, ImColor col_upr_right, ImColor col_bot_right, ImColor col_bot_left) {
        get_list(list)->AddRectFilledMultiColor(p_min, p_max, ApplyAlpha(col_upr_left), ApplyAlpha(col_upr_right), ApplyAlpha(col_bot_right), ApplyAlpha(col_bot_left));
    }

    void Draw::rect_filled_multi_color_rounding(ImVec2 p_min, ImVec2 p_max, ImColor col_upr_left, ImColor col_upr_right, ImColor col_bot_right, ImColor col_bot_left, float rounding, ImDrawFlags flags) {
        ImDrawList* dl = get_list(list);
        ImU32 c_ul = ApplyAlpha(col_upr_left);
        ImU32 c_ur = ApplyAlpha(col_upr_right);
        ImU32 c_br = ApplyAlpha(col_bot_right);
        ImU32 c_bl = ApplyAlpha(col_bot_left);

        if (rounding <= 0.0f || (flags & ImDrawFlags_RoundCornersNone) == ImDrawFlags_RoundCornersNone) {
            dl->AddRectFilledMultiColor(p_min, p_max, c_ul, c_ur, c_br, c_bl);
            return;
        }

        float w = ImMax(p_max.x - p_min.x, 1.0f);
        float h = ImMax(p_max.y - p_min.y, 1.0f);
        auto bilerp = [&](const ImVec2& p) -> ImU32 {
            float tx = ImClamp((p.x - p_min.x) / w, 0.0f, 1.0f);
            float ty = ImClamp((p.y - p_min.y) / h, 0.0f, 1.0f);
            ImVec4 top = ImLerp((ImVec4)ImColor(c_ul), (ImVec4)ImColor(c_ur), tx);
            ImVec4 bot = ImLerp((ImVec4)ImColor(c_bl), (ImVec4)ImColor(c_br), tx);
            return ImGui::ColorConvertFloat4ToU32(ImLerp(top, bot, ty));
        };

        dl->PathRect(p_min, p_max, rounding, flags);
        const int vtx_count = dl->_Path.Size;
        if (vtx_count < 3) { dl->PathClear(); return; }

        ImVec2 center((p_min.x + p_max.x) * 0.5f, (p_min.y + p_max.y) * 0.5f);
        dl->PrimReserve(vtx_count * 3, vtx_count + 1);
        for (int i = 0; i < vtx_count; i++)
            dl->PrimWriteVtx(dl->_Path[i], dl->_Data->TexUvWhitePixel, bilerp(dl->_Path[i]));
        const unsigned int center_idx = dl->_VtxCurrentIdx;
        const unsigned int base_idx = center_idx - (unsigned int)vtx_count;
        dl->PrimWriteVtx(center, dl->_Data->TexUvWhitePixel, bilerp(center));
        for (int i = 0; i < vtx_count; i++) {
            int i2 = (i + 1) % vtx_count;
            dl->PrimWriteIdx((ImDrawIdx)center_idx);
            dl->PrimWriteIdx((ImDrawIdx)(base_idx + i));
            dl->PrimWriteIdx((ImDrawIdx)(base_idx + i2));
        }
        dl->PathClear();
    }

    void Draw::image(ImTextureID user_texture_id, ImVec2 p_min, ImVec2 p_max, ImVec2 uv_min, ImVec2 uv_max, ImColor col) {
        get_list(list)->AddImage(user_texture_id, p_min, p_max, uv_min, uv_max, ApplyAlpha(col));
    }

    void Draw::shadow_rect(ImVec2 p_min, ImVec2 p_max, ImColor col, float shadow_thickness, float rounding) {
        auto dl = get_list(list);
        ImU32 col_u32 = col;
        for (int i = 1; i <= (int)shadow_thickness; i++) {
            float alpha = (1.0f - (float)i / shadow_thickness) * 0.15f * ImGui::GetStyle().Alpha;
            ImU32 shadow_col = (col_u32 & 0x00FFFFFF) | ((ImU32)(alpha * 255) << 24);
            dl->AddRect(ImVec2(p_min.x - i, p_min.y - i), ImVec2(p_max.x + i, p_max.y + i), shadow_col, rounding + i);
        }
    }

    void Draw::shadow_circle(ImVec2 center, float radius, ImColor col, float shadow_thickness) {
        if (shadow_thickness < 0.01f || radius < 0.1f) return;
        auto dl = get_list(list);
        ImU32 col_u32 = col;
        for (int i = 1; i <= (int)shadow_thickness; i++) {
            float alpha = (1.0f - (float)i / shadow_thickness) * 0.15f * ImGui::GetStyle().Alpha;
            ImU32 shadow_col = (col_u32 & 0x00FFFFFF) | ((ImU32)(alpha * 255) << 24);
            dl->AddCircleFilled(center, radius + i, shadow_col, 0);
        }
    }
}
