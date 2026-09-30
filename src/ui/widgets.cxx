#include "egui.hxx"
#include "layout.hxx"
#include "theme.hxx"
#include "keys.hxx"
#include "imgui_internal.h"
#include <map>
#include <vector>
#include <string>
#include <cstdio>
#include <cstring>
#include <Windows.h>

namespace egui {

static std::vector<std::string> split_items(const char* items_separated_by_zeros) {
    std::vector<std::string> items;
    const char* p = items_separated_by_zeros;
    while (p && *p) { items.push_back(p); p += strlen(p) + 1; }
    return items;
}

static float r(float v) { return ImMax(1.0f, dpi(v)); }

static void draw_caret(ImVec2 center, float size, const ImVec4& col) {
    ImVec4 c = col;
    c.w *= ImGui::GetStyle().Alpha;
    const float w = size, h = size * 0.55f;
    ImGui::GetWindowDrawList()->AddTriangleFilled(
        ImVec2(center.x - w * 0.5f, center.y - h * 0.5f),
        ImVec2(center.x + w * 0.5f, center.y - h * 0.5f),
        ImVec2(center.x,            center.y + h * 0.5f),
        ImColor(c));
}

static void widget_label(const char* text, const char* text_end) {
    ImFont* f = fnt(efonts.inter.Text11);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float h = layout.widget_top + fsize(f) + layout.label_gap;
    draw_text(f, ImVec2(pos.x, pos.y + layout.widget_top), colors.section_header, text, text_end);
    ImGui::Dummy(ImVec2(measure(f, text, text_end).x, h));
    ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + h));
}

static void draw_control_box(ImVec2 p_min, ImVec2 p_max, float hover) {
    edraw.rect_filled(p_min, p_max, mix(colors.control, colors.control_hov, hover), layout.control_rounding);
    edraw.rect(p_min, p_max, mix(colors.control_border, colors.accent, hover * 0.45f), layout.control_rounding, 0, layout.border);
}

static bool begin_dropdown(const char* id, ImVec2 anchor, float width) {
    ImGui::SetNextWindowPos(anchor);
    ImGui::SetNextWindowSize(ImVec2(width, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(dpi(6.0f), dpi(6.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, dpi(2.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, layout.control_rounding);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, colors.card);
    ImGui::PushStyleColor(ImGuiCol_Border, colors.control_border);
    const bool open = ImGui::BeginPopup(id, ImGuiWindowFlags_NoMove);
    if (open) push_font_stretch();
    else      { ImGui::PopStyleColor(2); ImGui::PopStyleVar(3); }
    return open;
}

static void end_dropdown() {
    ImGui::EndPopup();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

static bool dropdown_item(const char* text, bool selected, int check) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImFont* f = fnt(efonts.inter.Text12);
    const float w = ImGui::GetContentRegionAvail().x;
    const float h = dpi(24.0f);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImGuiID id = window->GetID(text);
    const ImRect bb(pos, ImVec2(pos.x + w, pos.y + h));

    ImGui::ItemSize(ImVec2(w, h), 0.0f);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered, held;
    const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

    if (hovered || selected)
        edraw.rect_filled(bb.Min, bb.Max, hovered ? colors.control_hov : colors.control, r(4.0f));

    const float pad = dpi(8.0f);
    float tx = pos.x + pad;
    if (check >= 0) {
        const float s = dpi(12.0f);
        const float b = layout.border;
        const ImVec2 c_min(pos.x + pad, pos.y + (h - s) * 0.5f);
        const ImVec2 c_max(c_min.x + s, c_min.y + s);
        edraw.rect_filled(c_min, c_max, colors.control, r(3.0f));
        edraw.rect(c_min, c_max, check ? colors.accent : colors.checkbox_border, r(3.0f), 0, b);
        if (check)
            edraw.rect_filled(ImVec2(c_min.x + b, c_min.y + b), ImVec2(c_max.x - b, c_max.y - b), colors.accent, r(2.0f));
        tx = c_max.x + pad;
    }

    const ImVec2 ts = measure(f, text);
    draw_text(f, ImVec2(tx, pos.y + (h - ts.y) * 0.5f), selected ? colors.text_primary : colors.text, text);
    return pressed;
}

bool checkbox(const char* label, bool* v) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImFont* f = fnt(efonts.inter.Text12);
    const char* label_end = ImGui::FindRenderedTextEnd(label);
    const ImVec2 ts = measure(f, label, label_end);

    const float box = layout.check_box;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 size(box + layout.check_gap + ts.x, layout.row_h);
    const ImGuiID id = window->GetID(label);
    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));

    ImGui::ItemSize(size, 0.0f);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered, held;
    const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    if (pressed) { *v = !*v; ImGui::MarkItemEdited(id); }

    const float on = anim(anim_value(id), *v ? 1.0f : 0.0f);
    const float hv = anim(anim_hover(id), hovered ? 1.0f : 0.0f);

    const ImVec2 b_min(pos.x, pos.y + (layout.row_h - box) * 0.5f);
    const ImVec2 b_max(b_min.x + box, b_min.y + box);
    const float b = layout.border;

    edraw.rect_filled(b_min, b_max, colors.control, r(3.0f));
    edraw.rect(b_min, b_max, mix(colors.checkbox_border, colors.accent, ImMax(on, hv * 0.4f)), r(3.0f), 0, b);
    if (on > 0.01f) {
        const float pad = (1.0f - on) * box * 0.5f;
        edraw.rect_filled(ImVec2(b_min.x + b + pad, b_min.y + b + pad),
                          ImVec2(b_max.x - b - pad, b_max.y - b - pad),
                          fade(colors.accent, on), r(2.0f));
    }

    draw_text(f, ImVec2(pos.x + box + layout.check_gap, pos.y + (layout.row_h - ts.y) * 0.5f),
              ImLerp(colors.text, colors.text_primary, hv), label, label_end);
    return pressed;
}

bool slider_float(const char* label, float* v, float v_min, float v_max, const char* format) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    const char* label_end = ImGui::FindRenderedTextEnd(label);
    const bool has_label = (label_end != label);
    const float w = ImGui::CalcItemWidth();

    ImGui::PushID(label);

    if (has_label) {
        ImFont* fl = fnt(efonts.inter.Text11);
        char value[64];
        ImFormatString(value, IM_ARRAYSIZE(value), format, *v);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float h = layout.widget_top + fsize(fl) + layout.label_gap;
        draw_text(fl, ImVec2(pos.x, pos.y + layout.widget_top), colors.section_header, label, label_end);
        const ImVec2 vs = measure(fl, value);
        draw_text(fl, ImVec2(pos.x + w - vs.x, pos.y + layout.widget_top), colors.text, value);
        ImGui::Dummy(ImVec2(w, h));
        ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + h));
    }

    const float item_h = layout.track_h + dpi(10.0f);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImGuiID id = window->GetID("##track");
    const ImRect bb(pos, ImVec2(pos.x + w, pos.y + item_h));

    ImGui::ItemSize(ImVec2(w, item_h), 0.0f);
    if (!ImGui::ItemAdd(bb, id)) { ImGui::PopID(); return false; }

    bool hovered, held;
    const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_PressedOnClick);

    ImGuiStorage* st = ImGui::GetStateStorage();
    const ImGuiID key_x = id ^ 0x3C5A9B17u;
    const ImGuiID key_w = id ^ 0x7E1D4F82u;
    if (pressed) {
        st->SetFloat(key_x, bb.Min.x);
        st->SetFloat(key_w, w);
    }

    bool changed = false;
    if (held) {
        const float ref_x = st->GetFloat(key_x, bb.Min.x);
        const float ref_w = ImMax(st->GetFloat(key_w, w), 1.0f);
        const float t = ImClamp((ImGui::GetIO().MousePos.x - ref_x) / ref_w, 0.0f, 1.0f);
        const float nv = v_min + t * (v_max - v_min);
        if (nv != *v) { *v = nv; changed = true; ImGui::MarkItemEdited(id); }
        g_any_slider_active = true;
    }

    const float hv = anim(anim_hover(id), (hovered || held) ? 1.0f : 0.0f);

    const float frac_target = ImClamp((*v - v_min) / ImMax(v_max - v_min, 0.0001f), 0.0f, 1.0f);
    const float frac = anim(anim_value(id), frac_target, held ? 30.0f : hover_speed);

    const ImVec2 t_min(bb.Min.x, bb.Min.y + (item_h - layout.track_h) * 0.5f);
    const ImVec2 t_max(bb.Max.x, t_min.y + layout.track_h);
    const float half = layout.track_h * 0.5f;

    edraw.rect_filled(t_min, t_max, colors.control, half);
    if (frac > 0.001f)
        edraw.rect_filled(t_min, ImVec2(t_min.x + w * frac, t_max.y),
                          mix(colors.accent, colors.text_primary, hv * 0.15f), half);
    if (hv > 0.01f)
        edraw.circle_filled(ImVec2(t_min.x + w * frac, (t_min.y + t_max.y) * 0.5f), half + dpi(2.0f) * hv,
                            fade(colors.accent, hv), 16);

    ImGui::PopID();
    return changed;
}

bool combo(const char* label, int* current_item, const char* items_separated_by_zeros) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    const std::vector<std::string> items = split_items(items_separated_by_zeros);
    const char* label_end = ImGui::FindRenderedTextEnd(label);

    ImGui::PushID(label);
    if (label_end != label)
        widget_label(label, label_end);

    const float w = ImGui::CalcItemWidth();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImGuiID id = window->GetID("##box");
    const ImRect bb(pos, ImVec2(pos.x + w, pos.y + layout.control_h));

    ImGui::ItemSize(ImVec2(w, layout.control_h), 0.0f);
    if (!ImGui::ItemAdd(bb, id)) { ImGui::PopID(); return false; }

    bool hovered, held;
    if (ImGui::ButtonBehavior(bb, id, &hovered, &held))
        ImGui::OpenPopup("##dd");

    const float hv = anim(anim_hover(id), hovered ? 1.0f : 0.0f);
    draw_control_box(bb.Min, bb.Max, hv);

    ImFont* f = fnt(efonts.inter.Text12);
    const bool valid = (*current_item >= 0 && *current_item < (int)items.size());
    const char* preview = valid ? items[*current_item].c_str() : "";
    const ImVec2 ts = measure(f, preview);
    draw_text(f, ImVec2(bb.Min.x + dpi(11.0f), bb.Min.y + (layout.control_h - ts.y) * 0.5f), colors.text, preview);
    draw_caret(ImVec2(bb.Max.x - dpi(13.0f), bb.Min.y + layout.control_h * 0.5f), dpi(8.0f), colors.text_muted);

    bool changed = false;
    if (begin_dropdown("##dd", ImVec2(bb.Min.x, bb.Max.y + dpi(4.0f)), w)) {
        for (int i = 0; i < (int)items.size(); i++)
            if (dropdown_item(items[i].c_str(), i == *current_item, -1)) {
                *current_item = i;
                changed = true;
                ImGui::CloseCurrentPopup();
            }
        end_dropdown();
    }

    ImGui::PopID();
    return changed;
}

bool multi_combo(const char* label, bool* selected, const char* items_separated_by_zeros) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    const std::vector<std::string> items = split_items(items_separated_by_zeros);
    if (items.empty()) return false;

    std::string preview;
    for (int i = 0; i < (int)items.size(); i++) {
        if (!selected[i]) continue;
        if (!preview.empty()) preview += ", ";
        preview += items[i];
    }
    if (preview.empty()) preview = "None";

    const char* label_end = ImGui::FindRenderedTextEnd(label);

    ImGui::PushID(label);
    if (label_end != label)
        widget_label(label, label_end);

    const float w = ImGui::CalcItemWidth();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImGuiID id = window->GetID("##box");
    const ImRect bb(pos, ImVec2(pos.x + w, pos.y + layout.control_h));

    ImGui::ItemSize(ImVec2(w, layout.control_h), 0.0f);
    if (!ImGui::ItemAdd(bb, id)) { ImGui::PopID(); return false; }

    bool hovered, held;
    if (ImGui::ButtonBehavior(bb, id, &hovered, &held))
        ImGui::OpenPopup("##dd");

    const float hv = anim(anim_hover(id), hovered ? 1.0f : 0.0f);
    draw_control_box(bb.Min, bb.Max, hv);

    ImFont* f = fnt(efonts.inter.Text12);
    const ImVec2 ts = measure(f, preview.c_str());
    const ImVec4 clip(bb.Min.x + dpi(11.0f), bb.Min.y, bb.Max.x - dpi(24.0f), bb.Max.y);
    edraw.text(fnt(f), fsize(f), ImVec2(bb.Min.x + dpi(11.0f), bb.Min.y + (layout.control_h - ts.y) * 0.5f),
               colors.text, preview.c_str(), nullptr, 0.0f, &clip);
    draw_caret(ImVec2(bb.Max.x - dpi(13.0f), bb.Min.y + layout.control_h * 0.5f), dpi(8.0f), colors.text_muted);

    bool changed = false;
    if (begin_dropdown("##dd", ImVec2(bb.Min.x, bb.Max.y + dpi(4.0f)), w)) {
        for (int i = 0; i < (int)items.size(); i++)
            if (dropdown_item(items[i].c_str(), selected[i], selected[i] ? 1 : 0)) {
                selected[i] = !selected[i];
                changed = true;
            }
        end_dropdown();
    }

    ImGui::PopID();
    return changed;
}

static ImVec2 color_picker_size(bool alpha) {
    const float sv = layout.pick_w - (alpha ? layout.pick_bar + layout.pick_gap : 0.0f);
    return ImVec2(layout.pick_w + layout.pick_pad * 2.0f,
                  sv + layout.pick_gap + layout.pick_bar + layout.pick_pad * 2.0f);
}

static bool color_picker_body(float* col, bool alpha, float width) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGuiStorage* st = ImGui::GetStateStorage();
    const ImGuiID k_h = ImGui::GetID("##hue");
    const ImGuiID k_s = ImGui::GetID("##sat");

    float h, s, v;
    ImGui::ColorConvertRGBtoHSV(col[0], col[1], col[2], h, s, v);
    if (s <= 0.0f) h = st->GetFloat(k_h, h);
    if (v <= 0.0f) { h = st->GetFloat(k_h, h); s = st->GetFloat(k_s, s); }

    const float gap = layout.pick_gap;
    const float bar = layout.pick_bar;
    const float rad  = ImMax(1.0f, layout.card_rounding);
    const float radb = ImMax(1.0f, bar * 0.5f);
    const float sv_w = width - (alpha ? bar + gap : 0.0f);
    const float sv_h = sv_w;

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 sv_min = origin;
    const ImVec2 sv_max(sv_min.x + sv_w, sv_min.y + sv_h);
    const ImVec2 hue_min(sv_min.x, sv_max.y + gap);
    const ImVec2 hue_max(origin.x + width, hue_min.y + bar);
    const ImVec2 al_min(sv_max.x + gap, sv_min.y);
    const ImVec2 al_max(al_min.x + bar, sv_max.y);

    auto zone = [&](const char* id, ImVec2 p_min, ImVec2 p_max, float& tx, float& ty) -> bool {
        ImGui::SetCursorScreenPos(p_min);
        ImGui::InvisibleButton(id, ImVec2(p_max.x - p_min.x, p_max.y - p_min.y));
        if (!ImGui::IsItemActive()) return false;
        const ImVec2 m = ImGui::GetIO().MousePos;
        tx = ImClamp((m.x - p_min.x) / ImMax(p_max.x - p_min.x, 1.0f), 0.0f, 1.0f);
        ty = ImClamp((m.y - p_min.y) / ImMax(p_max.y - p_min.y, 1.0f), 0.0f, 1.0f);
        g_any_slider_active = true;
        return true;
    };

    bool changed = false;
    float tx = 0.0f, ty = 0.0f;
    if (zone("##sv", sv_min, sv_max, tx, ty)) { s = tx; v = 1.0f - ty; changed = true; }
    if (zone("##hue", hue_min, hue_max, tx, ty)) { h = ImMin(tx, 0.9999f); changed = true; }
    if (alpha && zone("##alpha", al_min, al_max, tx, ty)) { col[3] = 1.0f - ty; changed = true; }

    float pr, pg, pb;
    ImGui::ColorConvertHSVtoRGB(h, 1.0f, 1.0f, pr, pg, pb);
    const ImColor pure(pr, pg, pb, 1.0f);
    const ImColor white(1.0f, 1.0f, 1.0f, 1.0f);
    const ImColor clear(0.0f, 0.0f, 0.0f, 0.0f);
    const ImColor black(0.0f, 0.0f, 0.0f, 1.0f);
    edraw.rect_filled_multi_color_rounding(sv_min, sv_max, white, pure, pure, white, rad, ImDrawFlags_RoundCornersAll);
    edraw.rect_filled_multi_color_rounding(sv_min, sv_max, clear, clear, black, black, rad, ImDrawFlags_RoundCornersAll);
    edraw.rect(sv_min, sv_max, colors.control_border, rad, 0, layout.border);

    static const ImU32 hue_stops[7] = {
        IM_COL32(255,   0,   0, 255), IM_COL32(255, 255,   0, 255),
        IM_COL32(  0, 255,   0, 255), IM_COL32(  0, 255, 255, 255),
        IM_COL32(  0,   0, 255, 255), IM_COL32(255,   0, 255, 255),
        IM_COL32(255,   0,   0, 255)
    };
    const float seg = (hue_max.x - hue_min.x) / 6.0f;
    for (int i = 0; i < 6; i++) {
        const ImVec2 a(hue_min.x + seg * i, hue_min.y);
        const ImVec2 b(hue_min.x + seg * (i + 1), hue_max.y);
        const ImDrawFlags fl = (i == 0) ? ImDrawFlags_RoundCornersLeft
                             : (i == 5) ? ImDrawFlags_RoundCornersRight
                                        : ImDrawFlags_RoundCornersNone;
        edraw.rect_filled_multi_color_rounding(a, b, ImColor(hue_stops[i]), ImColor(hue_stops[i + 1]),
                                               ImColor(hue_stops[i + 1]), ImColor(hue_stops[i]), radb, fl);
    }
    edraw.rect(hue_min, hue_max, colors.control_border, radb, 0, layout.border);

    if (alpha) {
        edraw.rect_filled(al_min, al_max, ImColor(0.8f, 0.8f, 0.8f, 1.0f), radb);
        ImGui::PushClipRect(ImVec2(al_min.x, al_min.y + radb), ImVec2(al_max.x, al_max.y - radb), true);
        ImGui::RenderColorRectWithAlphaCheckerboard(dl, al_min, al_max, IM_COL32(0, 0, 0, 0),
                                                    dpi(5.0f), ImVec2(0, 0), 0.0f);
        ImGui::PopClipRect();
        const ImColor a_top(col[0], col[1], col[2], 1.0f);
        const ImColor a_bot(col[0], col[1], col[2], 0.0f);
        edraw.rect_filled_multi_color_rounding(al_min, al_max, a_top, a_top, a_bot, a_bot, radb, ImDrawFlags_RoundCornersAll);
        edraw.rect(al_min, al_max, colors.control_border, radb, 0, layout.border);
    }

    const ImVec2 sv_cur(sv_min.x + s * (sv_max.x - sv_min.x),
                        sv_min.y + (1.0f - v) * (sv_max.y - sv_min.y));
    edraw.circle(sv_cur, dpi(5.0f), ImColor(1.f, 1.f, 1.f, 1.f), 24, dpi(2.0f));
    edraw.circle(sv_cur, dpi(6.5f), ImColor(0.f, 0.f, 0.f, 0.45f), 24, layout.border);

    auto marker = [&](ImVec2 p_min, ImVec2 p_max, float t, bool vertical) {
        const float thick = dpi(3.0f);
        const float over = dpi(2.0f);
        ImVec2 a, b;
        if (vertical) {
            const float y = ImLerp(p_min.y + thick * 0.5f, p_max.y - thick * 0.5f, t);
            a = ImVec2(p_min.x - over, y - thick * 0.5f);
            b = ImVec2(p_max.x + over, y + thick * 0.5f);
        } else {
            const float x = ImLerp(p_min.x + thick * 0.5f, p_max.x - thick * 0.5f, t);
            a = ImVec2(x - thick * 0.5f, p_min.y - over);
            b = ImVec2(x + thick * 0.5f, p_max.y + over);
        }
        edraw.rect_filled(a, b, ImColor(1.f, 1.f, 1.f, 1.f), r(2.0f));
        edraw.rect(a, b, ImColor(0.f, 0.f, 0.f, 0.35f), r(2.0f), 0, layout.border);
    };
    marker(hue_min, hue_max, ImClamp(h, 0.0f, 1.0f), false);
    if (alpha) marker(al_min, al_max, 1.0f - ImClamp(col[3], 0.0f, 1.0f), true);

    st->SetFloat(k_h, h);
    st->SetFloat(k_s, s);
    if (changed)
        ImGui::ColorConvertHSVtoRGB(h, s, v, col[0], col[1], col[2]);

    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(ImVec2(width, hue_max.y - sv_min.y));
    return changed;
}

bool color_button(const char* id, float* col, bool alpha) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGui::PushID(id);

    const float s = layout.swatch;
    const ImVec2 cur = ImGui::GetCursorScreenPos();
    const ImGuiID iid = window->GetID("##swatch");
    const ImRect item(cur, ImVec2(cur.x + s, cur.y + layout.row_h));
    const ImVec2 p_min(cur.x, cur.y + (layout.row_h - s) * 0.5f);
    const ImVec2 p_max(p_min.x + s, p_min.y + s);

    ImGui::ItemSize(ImVec2(s, layout.row_h), 0.0f);
    if (!ImGui::ItemAdd(item, iid)) { ImGui::PopID(); return false; }

    bool hovered, held;
    if (ImGui::ButtonBehavior(ImRect(p_min, p_max), iid, &hovered, &held))
        ImGui::OpenPopup("##picker");

    edraw.rect_filled(p_min, p_max, ImColor(col[0], col[1], col[2], 1.0f), r(4.0f));
    edraw.rect(p_min, p_max, colors.swatch_border, r(4.0f), 0, layout.border);

    const float pad = layout.pick_pad;
    const ImVec2 pop = color_picker_size(alpha);
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float m = dpi(4.0f);
    float px = p_max.x - pop.x;
    float py = p_max.y + m;
    if (py + pop.y > vp->WorkPos.y + vp->WorkSize.y) py = p_min.y - m - pop.y;
    px = ImClamp(px, vp->WorkPos.x + m, ImMax(vp->WorkPos.x + m, vp->WorkPos.x + vp->WorkSize.x - pop.x - m));
    py = ImClamp(py, vp->WorkPos.y + m, ImMax(vp->WorkPos.y + m, vp->WorkPos.y + vp->WorkSize.y - pop.y - m));

    bool changed = false;
    ImGui::SetNextWindowPos(ImVec2(px, py));
    ImGui::SetNextWindowSize(ImVec2(pop.x, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad, pad));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(dpi(6.0f), dpi(6.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, layout.control_rounding);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, colors.card);
    ImGui::PushStyleColor(ImGuiCol_Border, colors.control_border);
    if (ImGui::BeginPopup("##picker", ImGuiWindowFlags_NoMove)) {
        push_font_stretch();
        changed = color_picker_body(col, alpha, layout.pick_w);
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);

    ImGui::PopID();
    return changed;
}

bool colorpicker(const char* label, float* col, bool alpha) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGui::PushID(label);

    ImFont* f = fnt(efonts.inter.Text12);
    const char* label_end = ImGui::FindRenderedTextEnd(label);
    const ImVec2 ts = measure(f, label, label_end);
    const ImVec2 pos = ImGui::GetCursorScreenPos();

    ImGui::ItemSize(ImVec2(ts.x, layout.row_h), 0.0f);
    ImGui::ItemAdd(ImRect(pos, ImVec2(pos.x + ts.x, pos.y + layout.row_h)), 0);
    draw_text(f, ImVec2(pos.x, pos.y + (layout.row_h - ts.y) * 0.5f), colors.text, label, label_end);

    same_line_right(layout.swatch);
    const bool changed = color_button("##col", col, alpha);

    ImGui::PopID();
    return changed;
}

bool button(const char* label, ImVec2 size) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImFont* f = fnt(efonts.inter.Text12);
    const char* label_end = ImGui::FindRenderedTextEnd(label);
    const ImVec2 ts = measure(f, label, label_end);

    if (size.x <= 0.0f) size.x = ImGui::CalcItemWidth();
    if (size.y <= 0.0f) size.y = layout.control_h;

    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImGuiID id = window->GetID(label);
    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));

    ImGui::ItemSize(size, 0.0f);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered, held;
    const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

    const float hv = anim(anim_hover(id), (hovered || held) ? 1.0f : 0.0f);
    draw_control_box(bb.Min, bb.Max, held ? 1.0f : hv);
    draw_text(f, ImVec2(bb.Min.x + (size.x - ts.x) * 0.5f, bb.Min.y + (size.y - ts.y) * 0.5f),
              ImLerp(colors.text, colors.text_primary, hv), label, label_end);
    return pressed;
}

bool icon_button(const char* label, const char* icon, ImVec2 size) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImFont* f  = fnt(efonts.inter.Text12);
    ImFont* fi = fnt(efonts.icomoon.regular_small);
    const char* label_end = label ? ImGui::FindRenderedTextEnd(label) : nullptr;

    const ImVec2 ts = label ? measure(f, label, label_end) : ImVec2(0, 0);
    const ImVec2 is = icon ? measure(fi, icon) : ImVec2(0, 0);
    const float gap_x = (icon && label && *label) ? 8.0f : 0.0f;

    if (size.x <= 0.0f) size.x = ImGui::CalcItemWidth();
    if (size.y <= 0.0f) size.y = layout.control_h;

    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImGuiID id = window->GetID(label ? label : icon);
    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));

    ImGui::ItemSize(size, 0.0f);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered, held;
    const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

    const float hv = anim(anim_hover(id), (hovered || held) ? 1.0f : 0.0f);
    draw_control_box(bb.Min, bb.Max, held ? 1.0f : hv);

    float x = bb.Min.x + (size.x - (is.x + gap_x + ts.x)) * 0.5f;
    const ImVec4 col = ImLerp(colors.text, colors.text_primary, hv);
    if (icon)  { draw_text(fi, ImVec2(x, bb.Min.y + (size.y - is.y) * 0.5f), col, icon); x += is.x + gap_x; }
    if (label) { draw_text(f,  ImVec2(x, bb.Min.y + (size.y - ts.y) * 0.5f), col, label, label_end); }
    return pressed;
}

struct HotkeyReg {
    std::string label;
    int*  key   = nullptr;
    int*  mode  = nullptr;
    bool* value = nullptr;
    bool  prev    = false;
    bool  toggled = false;
    bool  active  = false;
};
static std::vector<HotkeyReg> g_hotkeys;

static int g_key_capture_frame = -1;

static bool g_pill_capturing = false;

bool key_listening() {
    return g_key_capture_frame >= ImGui::GetFrameCount() - 1;
}

static HotkeyReg* hotkey_find(const char* label) {
    for (size_t i = 0; i < g_hotkeys.size(); i++)
        if (g_hotkeys[i].label == label) return &g_hotkeys[i];
    return nullptr;
}

void hotkey_register(const char* label, int* key, int* mode, bool* value) {
    if (!label || !*label) return;
    HotkeyReg* h = hotkey_find(label);
    if (!h) {
        g_hotkeys.push_back(HotkeyReg());
        h = &g_hotkeys.back();
        h->label   = label;
        h->toggled = (value && *value);
    }
    h->key  = key;
    h->mode = mode;
    h->value = value;
}

void hotkey_poll() {
    static int last_frame = -1;
    const int frame = ImGui::GetFrameCount();
    if (frame == last_frame) return;
    last_frame = frame;

    const bool blocked = (g_key_capture_frame >= frame - 1) || ImGui::GetIO().WantTextInput;

    for (size_t i = 0; i < g_hotkeys.size(); i++) {
        HotkeyReg& h = g_hotkeys[i];
        const int key  = h.key  ? *h.key  : 0;
        const int mode = h.mode ? *h.mode : 0;
        const bool down = !blocked && key > 0 && ((::GetAsyncKeyState(key) & 0x8000) != 0);

        if (mode == 2 && down && !h.prev) h.toggled = !h.toggled;
        h.prev   = down;
        h.active = (mode == 1) ? down : (mode == 2) ? h.toggled : true;

        if (h.value && key > 0 && mode != 0) *h.value = h.active;
    }
}

bool hotkey_active(const char* label) {
    const HotkeyReg* h = hotkey_find(label);
    return h && h->active;
}

int hotkey_count() { return (int)g_hotkeys.size(); }

bool hotkey_info(int index, const char** label, int* key, int* mode, bool* active, bool* enabled) {
    if (index < 0 || index >= (int)g_hotkeys.size()) return false;
    const HotkeyReg& h = g_hotkeys[index];
    if (label)   *label   = h.label.c_str();
    if (key)     *key     = h.key  ? *h.key  : 0;
    if (mode)    *mode    = h.mode ? *h.mode : 0;
    if (active)  *active  = h.active;
    if (enabled) *enabled = h.value ? *h.value : true;
    return true;
}

static const char* key_label(int key) {
    return (key > 0 && key < IM_ARRAYSIZE(::keys)) ? ::keys[key] : "none";
}

float key_pill_width(int key) {
    ImFont* f = fnt(efonts.inter.Text10);
    return ImMax(layout.pill_w, measure(f, key_label(key)).x + layout.pill_pad_x * 2.0f);
}

bool key_pill(const char* id, int* key) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    struct PillState { bool waiting = false; bool swallow = false; };
    static std::map<ImGuiID, PillState> state_map;

    ImGui::PushID(id);

    const ImGuiID iid = window->GetID("##pill");
    PillState& st = state_map[iid];
    bool& waiting = st.waiting;
    const bool was_waiting = waiting;

    const float w = key_pill_width(*key);
    const float h = layout.pill_h;
    const float row_h = layout.row_h;
    const ImVec2 cur = ImGui::GetCursorScreenPos();
    const ImVec2 p_min(cur.x, cur.y + (row_h - h) * 0.5f);
    const ImVec2 p_max(p_min.x + w, p_min.y + h);

    ImGui::ItemSize(ImVec2(w, row_h), 0.0f);
    if (!ImGui::ItemAdd(ImRect(cur, ImVec2(cur.x + w, cur.y + row_h)), iid)) { ImGui::PopID(); return false; }

    bool hovered, held;
    const bool pressed = ImGui::ButtonBehavior(ImRect(p_min, p_max), iid, &hovered, &held);
    if (pressed) {
        if (st.swallow) st.swallow = false;
        else            waiting = !waiting;
    }
    else if (st.swallow && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        st.swallow = false;
    }

    bool changed = false;
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !waiting && *key != 0) {
        *key = 0;
        changed = true;
    }

    if (waiting) {
        g_key_capture_frame = ImGui::GetFrameCount();
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            *key = 0; waiting = false; changed = true;
        }
        else {

            const int vk_map[] = { VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2 };
            for (int i = 0; i < IM_ARRAYSIZE(vk_map) && waiting; i++)
                if (ImGui::IsMouseClicked(i)) {
                    *key = vk_map[i]; waiting = false; changed = true;
                    if (hovered && i == ImGuiMouseButton_Left) st.swallow = true;
                }
            for (int i = 8; i < 256 && waiting; i++)
                if ((GetAsyncKeyState(i) & 0x8000) && i != VK_ESCAPE) {
                    *key = i; waiting = false; changed = true;
                }
        }
    }

    g_pill_capturing = waiting || was_waiting;

    const bool bound = (*key != 0);
    const char* name = waiting ? "..." : key_label(*key);

    const float hv  = anim(anim_hover(iid), hovered ? 1.0f : 0.0f);
    const float cap = anim(anim_value(iid), waiting ? 1.0f : 0.0f, 16.0f);

    const float rounding = ImMin(r(6.0f), h * 0.5f);

    const ImVec4 bg_idle = bound ? colors.control : colors.surface_hov;
    ImVec4 bg = ImLerp(bg_idle, colors.control_hov, hv);
    bg = ImLerp(bg, ImVec4(colors.accent.x, colors.accent.y, colors.accent.z, 0.16f), cap);
    edraw.rect_filled(p_min, p_max, bg, rounding);

    const ImVec4 bd = ImLerp(ImLerp(colors.control_border, colors.checkbox_border, hv), colors.accent, cap);
    edraw.rect(p_min, p_max, bd, rounding, 0, layout.border);

    ImFont* f = fnt(efonts.inter.Text10);
    const ImVec2 ts = measure(f, name);
    const ImVec4 txt = ImLerp(bound ? colors.text_sec : colors.text_disabled, colors.accent, ImMax(cap, hv * 0.5f));
    const ImVec4 clip(p_min.x + dpi(3.0f), p_min.y, p_max.x - dpi(3.0f), p_max.y);
    edraw.text(f, fsize(f), ImVec2(p_min.x + (w - ts.x) * 0.5f,
                                   p_min.y + (h - ts.y) * 0.5f - dpi(1.0f)),
               txt, name, nullptr, 0.0f, &clip);

    ImGui::PopID();
    return changed;
}

bool key_pill_mode(const char* id, int* key, int* mode) {
    ImGui::PushID(id);

    bool changed = key_pill("##key", key);
    const bool capturing = g_pill_capturing;

    const ImVec2 pill_max = ImGui::GetItemRectMax();
    if (!capturing && ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        ImGui::OpenPopup("##mode");

    static const char* modes[] = { "Always On", "Hold", "Toggle" };
    if (begin_dropdown("##mode", ImVec2(pill_max.x - dpi(96.0f), pill_max.y + dpi(4.0f)), dpi(96.0f))) {
        for (int i = 0; i < IM_ARRAYSIZE(modes); i++)
            if (dropdown_item(modes[i], i == *mode, -1)) {
                *mode = i; changed = true;
                ImGui::CloseCurrentPopup();
            }
        end_dropdown();
    }

    ImGui::PopID();
    return changed;
}

bool keybind(const char* label, int* key) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGui::PushID(label);

    ImFont* f = fnt(efonts.inter.Text12);
    const char* label_end = ImGui::FindRenderedTextEnd(label);
    const ImVec2 ts = measure(f, label, label_end);
    const ImVec2 pos = ImGui::GetCursorScreenPos();

    ImGui::ItemSize(ImVec2(ts.x, layout.row_h), 0.0f);
    ImGui::ItemAdd(ImRect(pos, ImVec2(pos.x + ts.x, pos.y + layout.row_h)), 0);
    draw_text(f, ImVec2(pos.x, pos.y + (layout.row_h - ts.y) * 0.5f), colors.text, label, label_end);

    same_line_right(key_pill_width(*key));
    const bool changed = key_pill("##key", key);

    ImGui::PopID();
    return changed;
}

bool hotkey(const char* label, int* key, int* mode) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGui::PushID(label);

    ImFont* f = fnt(efonts.inter.Text12);
    const char* label_end = ImGui::FindRenderedTextEnd(label);
    const ImVec2 ts = measure(f, label, label_end);
    const ImVec2 pos = ImGui::GetCursorScreenPos();

    ImGui::ItemSize(ImVec2(ts.x, layout.row_h), 0.0f);
    ImGui::ItemAdd(ImRect(pos, ImVec2(pos.x + ts.x, pos.y + layout.row_h)), 0);
    draw_text(f, ImVec2(pos.x, pos.y + (layout.row_h - ts.y) * 0.5f), colors.text, label, label_end);

    same_line_right(key_pill_width(*key));
    const bool changed = key_pill_mode("##key", key, mode);

    hotkey_register(label, key, mode);

    ImGui::PopID();
    return changed;
}

bool hotkey_bind(const char* label, int* key, int* mode, bool* value) {
    const bool changed = hotkey(label, key, mode);
    hotkey_register(label, key, mode, value);
    return changed;
}

bool input_text(const char* label, const char* hint, char* buf, size_t buf_size, ImGuiInputTextFlags flags) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    const char* label_end = ImGui::FindRenderedTextEnd(label);

    ImGui::PushID(label);
    if (label_end != label)
        widget_label(label, label_end);

    ImGui::SetNextItemWidth(ImGui::CalcItemWidth());
    const bool changed = (hint && *hint)
        ? ImGui::InputTextWithHint("##in", hint, buf, buf_size, flags)
        : ImGui::InputText("##in", buf, buf_size, flags);
    if (ImGui::IsItemActive()) g_any_slider_active = true;

    ImGui::PopID();
    return changed;
}

bool input_text_hint(const char* hint, char* buf, size_t buf_size, ImGuiInputTextFlags flags) {
    return input_text("##hint_input", hint, buf, buf_size, flags);
}

bool text_editor(const char* label, char* buf, size_t buf_size, ImVec2 size, ImGuiInputTextFlags flags) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    const char* label_end = ImGui::FindRenderedTextEnd(label);

    ImGui::PushID(label);
    if (label_end != label)
        widget_label(label, label_end);

    if (size.x <= 0.0f) size.x = ImGui::CalcItemWidth();
    if (size.y <= 0.0f) size.y = ImGui::GetTextLineHeight() * 8.0f;

    const bool changed = ImGui::InputTextMultiline("##edit", buf, buf_size, size, flags | ImGuiInputTextFlags_AllowTabInput);
    if (ImGui::IsItemActive()) g_any_slider_active = true;

    int lines = 1;
    for (const char* p = buf; *p; p++) if (*p == '\n') lines++;

    char info[64];
    snprintf(info, sizeof(info), "%d lines | UTF-8", lines);

    ImFont* f = fnt(efonts.inter.Text10);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    draw_text(f, ImVec2(pos.x, pos.y + dpi(2.0f)), colors.text_faint, info);
    ImGui::Dummy(ImVec2(measure(f, info).x, fsize(f) + dpi(2.0f)));

    ImGui::PopID();
    return changed;
}

}
