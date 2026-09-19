#ifndef GORBE_UI_LAYOUT_HPP
#define GORBE_UI_LAYOUT_HPP

#include <algorithm>

#include "imgui.h"
#include "Requests.hpp"

// ---------------------------------------------------------------------------
// Fix elrendezés: a panelek nem lebegnek, hanem a főablak méretéhez igazodnak.
// A két oldalsó sáv szélessége és a bennük lévő vízszintes osztás húzható.
// ---------------------------------------------------------------------------
namespace Ui {

    constexpr float PAD  = 6.0f;    // panelek közti rés (ez egyben az elválasztó is)
    constexpr float MINW = 220.0f;

    // Az adott frame panel-téglalapjai, a főablak méretéből.
    struct Geometry {
        ImVec2 O;                   // a munkaterület bal felső sarka
        float  col_h;               // egy sáv teljes magassága
        float  cx, cw;              // a középső (3D) ablak bal széle és szélessége
        float  rx;                  // a jobb sáv bal széle
        float  left_avail;          // a bal sáv három panelje együtt
        float  h1, h2, h3;          // Alakzatok / Nezet es sugo / Parameterek
        float  max_side;
    };

    inline Geometry compute_geometry(Layout& layout) {
        ImGuiViewport const* vp = ImGui::GetMainViewport();
        ImVec2 const S = vp->WorkSize;
        Geometry g{};
        g.O = vp->WorkPos;
        g.max_side = std::max(MINW, (S.x - 3.0f * PAD - 320.0f) * 0.5f);
        layout.left_w  = std::clamp(layout.left_w,  MINW, g.max_side);
        layout.right_w = std::clamp(layout.right_w, MINW, g.max_side);

        g.col_h = S.y - 2.0f * PAD;
        g.cx = g.O.x + PAD + layout.left_w + PAD;
        g.cw = S.x - layout.left_w - layout.right_w - 4.0f * PAD;
        g.rx = g.O.x + S.x - PAD - layout.right_w;

        // A bal sávban három panel és köztük két rés van.
        g.left_avail = std::max(g.col_h - 2.0f * PAD, 3.0f);
        layout.left_f1 = std::clamp(layout.left_f1, 0.12f, 0.70f);
        layout.left_f2 = std::clamp(layout.left_f2, 0.12f, 0.88f - layout.left_f1);
        g.h1 = g.left_avail * layout.left_f1;
        g.h2 = g.left_avail * layout.left_f2;
        g.h3 = g.left_avail - g.h1 - g.h2;
        return g;
    }

    // Fix panel: nem mozgatható, nem átméretezhető, nem csukható össze.
    inline void fixed_panel(char const* title, ImVec2 pos, ImVec2 size) {
        ImGui::SetNextWindowPos(pos);
        ImGui::SetNextWindowSize(size);
        ImGui::Begin(title, nullptr,
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);
    }

    // Egy vékony, húzható elválasztó sáv. Saját, keret nélküli ImGui-ablak a résben:
    // így pontosan ott fogja az egeret, ahol a hézag van, és nem zavarja a paneleket.
    inline void splitter(char const* id, ImVec2 pos, ImVec2 size, bool vertical,
                         float* value, float lo, float hi, float sign) {
        if (size.x < 1.0f || size.y < 1.0f) return;
        ImGui::SetNextWindowPos(pos);
        ImGui::SetNextWindowSize(size);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin(id, nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);
        ImGui::InvisibleButton("##grip", size);
        bool const hot = ImGui::IsItemHovered() || ImGui::IsItemActive();
        if (hot) ImGui::SetMouseCursor(vertical ? ImGuiMouseCursor_ResizeEW
                                                : ImGuiMouseCursor_ResizeNS);
        if (ImGui::IsItemActive()) {
            float d = vertical ? ImGui::GetIO().MouseDelta.x : ImGui::GetIO().MouseDelta.y;
            *value = std::clamp(*value + d * sign, lo, hi);
        }
        ImU32 col = hot ? ImGui::GetColorU32(ImGuiCol_SeparatorHovered)
                        : ImGui::GetColorU32(ImGuiCol_Separator);
        ImGui::GetWindowDrawList()->AddRectFilled(
            pos, ImVec2(pos.x + size.x, pos.y + size.y), col, 2.0f);
        ImGui::End();
        ImGui::PopStyleVar();
    }

    inline void splitters(Layout& layout, Geometry const& g) {
        ImVec2 const O = g.O;
        splitter("##split_left",  {O.x + PAD + layout.left_w, O.y + PAD},
                 {PAD, g.col_h}, true, &layout.left_w,  MINW, g.max_side, +1.0f);
        splitter("##split_right", {g.rx - PAD, O.y + PAD},
                 {PAD, g.col_h}, true, &layout.right_w, MINW, g.max_side, -1.0f);

        // A bal sáv két vízszintes osztása. Pixelben húzzuk, arányban tároljuk:
        // így ablak-átméretezéskor együtt mozognak a panelekkel.
        float p1 = g.h1, p2 = g.h2;
        splitter("##split_l1", {O.x + PAD, O.y + PAD + g.h1},
                 {layout.left_w, PAD}, false, &p1, 90.0f, g.left_avail - 180.0f, +1.0f);
        splitter("##split_l2", {O.x + PAD, O.y + PAD + g.h1 + PAD + g.h2},
                 {layout.left_w, PAD}, false, &p2, 90.0f, g.left_avail - 180.0f, +1.0f);
        layout.left_f1 = p1 / g.left_avail;
        layout.left_f2 = p2 / g.left_avail;
    }

}

#endif //GORBE_UI_LAYOUT_HPP
