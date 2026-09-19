#ifndef GORBE_UI_PARAMSPANEL_HPP
#define GORBE_UI_PARAMSPANEL_HPP

#include <cstdio>
#include <list>
#include <vector>

#include "imgui.h"
#include "../app/Scene.hpp"
#include "../scene/Names.hpp"
#include "../scene/Validate.hpp"
#include "Layout.hpp"
#include "Requests.hpp"

namespace Ui {

    // Egy paramétertábla (név | érték | törlés). A program-, a jelenet- és a lokális
    // lista UI-ja ugyanaz. A törlést csak kéri: a vezérlő előbb eldobja a fákat.
    //
    // `levels`: a lista SAJÁT maga, majd kifelé a külsőbb hatókörök — az elfedés
    // jelzéséhez (lásd scene/Validate.hpp, shadows).
    inline void param_table(std::list<Param>& list, char const* prefix,
                            std::vector<std::list<Param> const*> const& levels,
                            Scene const& sc, Problems const& pr, Requests& rq) {
        if (ImGui::Button("Uj parameter")) {
            auto& p = list.emplace_back();
            next_name(list, prefix, p.name, sizeof(p.name));
        }
        for (auto& p : list) {
            ImGui::PushID(&p);
            bool warn = pr.is_bad(&p);
            if (warn) ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.45f, 0.12f, 0.12f, 1.0f));
            ImGui::SetNextItemWidth(90.0f);
            ImGui::InputText("##nev", p.name, sizeof(p.name));
            if (warn) ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::SetNextItemWidth(90.0f);
            ImGui::InputFloat("##ertek", &p.value);
            ImGui::SameLine();
            if (ImGui::Button("X")) { rq.erase_from = &list; rq.erase_param = &p; }
            // Elfedés-jelzés: nem hiba, de ne legyen néma meglepetés.
            if (char const* sh = shadows(p.name, levels, sc.params, sc.shapes)) {
                ImGui::SameLine();
                ImGui::TextDisabled("%s", sh);
            }
            ImGui::PopID();
        }
    }

    // 4. ablak: paraméterek — jelenet- és program-szinten, plusz a jelenet munkatere.
    inline void params_panel(Scene& sc, std::list<Param>& program_params,
                             Problems const& pr, Requests& rq, Layout const& layout,
                             Geometry const& g) {
        fixed_panel("Parameterek", {g.O.x + PAD, g.O.y + PAD + g.h1 + PAD + g.h2 + PAD},
                    {layout.left_w, g.h3});

        ImGui::TextDisabled("Hatokor kifele: alakzat -> jelenet -> program.");
        ImGui::TextDisabled("A belso ELFEDI a kulsot (mint C++-ban).");

        ImGui::SeparatorText("Jelenet parameterei");
        ImGui::TextDisabled("Ebben a fulben minden alakzat latja.");
        param_table(sc.params, "s", {&sc.params, &program_params}, sc, pr, rq);

        ImGui::SeparatorText("Program-szintu parameterek");
        ImGui::TextDisabled("MINDEN fulben lathatok.");
        param_table(program_params, "g", {&program_params}, sc, pr, rq);

        // --- A jelenet munkatere ---
        ImGui::SeparatorText("Jelenet munkatere");
        ImGui::TextWrapped("Az a terresz, amiben egyaltalan ertelmezzuk az alakzatokat. "
                           "Ebben a fulben minden alakzatra ervenyes, a sajat "
                           "tartomanyaval ES-kapcsolatban. Ures = korlatlan.");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##gdom", sc.domain, sizeof(sc.domain));

        // Gyorsgombok: a rács ±8 kiterjedéséhez igazodnak, hogy a beállítás látható legyen.
        if (ImGui::SmallButton("Doboz")) {
            std::snprintf(sc.domain, sizeof(sc.domain),
                          "x > 0 - 8 and x < 8 and y > 0 - 8 and y < 8 and z > 0 - 8 and z < 8");
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Gomb")) {
            std::snprintf(sc.domain, sizeof(sc.domain), "x^2 + y^2 + z^2 < 64");
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Torol")) sc.domain[0] = '\0';
        ImGui::TextDisabled("Jelenet- es program-szintu parametert hasznalhat.");
        ImGui::End();
    }

}

#endif //GORBE_UI_PARAMSPANEL_HPP
