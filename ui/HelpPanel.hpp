#ifndef GORBE_UI_HELPPANEL_HPP
#define GORBE_UI_HELPPANEL_HPP

#include "imgui.h"
#include "../App.hpp"
#include "../particle_sampling/ParticleView.hpp"
#include "Layout.hpp"

namespace Ui {

    // 3. ablak: nézet, jelmagyarázat és irányítás — a program használata közben
    // végig látható súgó. Csak értékeket állít (kamera, rács, korong-hézag).
    inline void help_panel(Camera3D& cam, Axes& axes, Layout const& layout, Geometry const& g) {
        fixed_panel("Nezet es sugo", {g.O.x + PAD, g.O.y + PAD + g.h1 + PAD},
                    {layout.left_w, g.h2});

        auto swatch = [](ImVec4 c, char const* text) {
            ImGui::ColorButton("##sw", c,
                               ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
                               ImVec2(14, 14));
            ImGui::SameLine();
            ImGui::TextUnformatted(text);
        };

        if (ImGui::CollapsingHeader("Jelmagyarazat", ImGuiTreeNodeFlags_DefaultOpen)) {
            swatch(ImVec4(0.85f, 0.22f, 0.26f, 1.0f), "X tengely");
            swatch(ImVec4(0.20f, 0.62f, 0.28f, 1.0f), "Y tengely");
            swatch(ImVec4(0.20f, 0.42f, 0.85f, 1.0f), "Z tengely  (ez a 'fuggoleges')");
            ImGui::Spacing();
            swatch(ImVec4(0.00f, 0.00f, 1.00f, 1.0f), "mintavetelezo reszecskek");
            swatch(ImVec4(1.00f, 0.00f, 0.00f, 1.0f), "kontrollpontok (kockak)");
            ImGui::Spacing();
            ImGui::TextDisabled("A racs a z = 0 sikban van, 1 egyseg osztassal");
            ImGui::TextDisabled("(minden 5. vonal es osztas hangsulyos).");
            ImGui::Checkbox("Racs mutatasa", &axes.show_grid);
        }

        if (ImGui::CollapsingHeader("Nezet", ImGuiTreeNodeFlags_DefaultOpen)) {
            // A nézetváltás a kamera aktuális origótól mért távolságát megtartja.
            float dist = glm::length(cam.get_position());
            if (dist < 1.0f) dist = 15.0f;

            // Z-up kameránál a SZINTE FÜGGŐLEGES nézés a határeset (pitch -> -90), ezért a
            // felülnézet kap egy pici y-eltolást: így a képernyőn +x jobbra, +y felfelé áll.
            if (ImGui::Button("Felulnezet")) cam.look_at({0.0f, -0.02f * dist, dist});
            ImGui::SameLine();
            if (ImGui::Button("3/4 nezet"))  cam.look_at(glm::normalize(App::DEFAULT_EYE) * dist);
            ImGui::SameLine();
            if (ImGui::Button("Oldalrol"))   cam.look_at({dist, 0.0f, 0.0f});

            if (ImGui::Button("Elolrol"))    cam.look_at({0.0f, -dist, 0.0f});
            ImGui::SameLine();
            if (ImGui::Button("Alapnezet"))  cam.look_at(App::DEFAULT_EYE);

            ImGui::SetNextItemWidth(150.0f);
            if (ImGui::SliderFloat("tavolsag", &dist, 3.0f, 60.0f))
                cam.look_at(glm::normalize(cam.get_position()) * dist);

            // A korongok megjelenítési mérete. NEM a szimuláció: a részecskék helye
            // és a `d` által beállított tényleges távolságuk változatlan marad.
            ImGui::SeparatorText("Reszecskek");
            ImGui::SetNextItemWidth(150.0f);
            ImGui::SliderFloat("hezag", &ParticleView::gap,
                               ParticleView::GAP_MIN, ParticleView::GAP_MAX, "%.2f");
            ImGui::SameLine();
            if (ImGui::SmallButton("0")) ParticleView::gap = 0.0f;
            ImGui::TextDisabled("0 = a korongok eppen osszeernek, negativ = atfedok.");
            ImGui::TextDisabled("Csak a rajzolast allitja; a tenyleges suruseg a `d`.");
        }

        if (ImGui::CollapsingHeader("Iranyitas", ImGuiTreeNodeFlags_DefaultOpen)) {
            struct Row { char const* input; char const* effect; };
            static Row const camera_rows[] = {
                {"jobb egergomb + huzas", "nezet forgatasa"},
                {"Alt + bal gomb + huzas", "nezet forgatasa"},
                {"W / S",                  "kamera elore / hatra"},
                {"A / D",                  "kamera balra / jobbra"},
                {"egergorgo",              "zoom (latoszog 1-45 fok)"},
                {"Esc",                    "kilepes"},
            };
            static Row const point_rows[] = {
                {"Shift + bal kattintas",  "uj pont az alakzatra"},
                {"bal gomb + huzas",       "pont huzasa: az alakzat koveti"},
                {"Ctrl + bal kattintas",   "pont torlese"},
            };
            // Fix oszlop-eltolás, nem ImGui-tábla: a monospace alapfonttal így biztosan
            // nem vágódik el a hosszabb bevitel-leírás (a táblás arányos osztás elvágta).
            auto table = [](Row const* rows, int n) {
                for (int i = 0; i < n; ++i) {
                    ImGui::TextUnformatted(rows[i].input);
                    ImGui::SameLine(178.0f);
                    ImGui::TextDisabled("%s", rows[i].effect);
                }
            };
            ImGui::SeparatorText("Kamera");
            table(camera_rows, IM_ARRAYSIZE(camera_rows));
            ImGui::SeparatorText("Kontrollpontok");
            table(point_rows, IM_ARRAYSIZE(point_rows));
            ImGui::TextDisabled("A UI folott az eger/billentyu a panelt vezerli.");
        }
        ImGui::End();
    }

}

#endif //GORBE_UI_HELPPANEL_HPP
