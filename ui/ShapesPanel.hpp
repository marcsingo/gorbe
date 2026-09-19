#ifndef GORBE_UI_SHAPESPANEL_HPP
#define GORBE_UI_SHAPESPANEL_HPP

#include <cstring>

#include "imgui.h"
#include "../app/Photo.hpp"
#include "../app/Scene.hpp"
#include "../scene/Presets.hpp"
#include "../scene/Validate.hpp"
#include "Layout.hpp"
#include "Requests.hpp"

namespace Ui {

    // 1. ablak: a jelenet alakzatai (lista + kijelölés) és a futtatás.
    inline void shapes_panel(Scene& sc, Problems const& pr, Photo::Settings& photo,
                             UiState& ui, Requests& rq, Geometry const& g) {
        fixed_panel("Alakzatok", {g.O.x + PAD, g.O.y + PAD}, {ui.layout.left_w, g.h1});
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Separator();

        if (ImGui::Button("Uj alakzat")) sc.selected = &new_shape(sc.shapes, "f");
        ImGui::SameLine();
        ImGui::TextDisabled("(%d alakzat)", static_cast<int>(sc.shapes.size()));

        // Sablonok: kész alakzat (képlet + lokális paraméterek) hozzáadása egy kattintással.
        ImGui::SetNextItemWidth(190.0f);
        if (ImGui::BeginCombo("##sablon", PRESETS[ui.preset_idx].label)) {
            char const* current_group = nullptr;
            for (int k = 0; k < static_cast<int>(PRESETS.size()); ++k) {
                if (current_group == nullptr || std::strcmp(current_group, PRESETS[k].group) != 0) {
                    current_group = PRESETS[k].group;
                    if (k > 0) ImGui::Separator();
                    ImGui::TextDisabled("%s", current_group);
                }
                bool is_selected = (k == ui.preset_idx);
                if (ImGui::Selectable(PRESETS[k].label, is_selected)) ui.preset_idx = k;
                if (is_selected) ImGui::SetItemDefaultFocus();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("F = %s", PRESETS[k].formula);
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Hozzaad")) sc.selected = &add_preset(sc.shapes, PRESETS[ui.preset_idx]);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("F = %s", PRESETS[ui.preset_idx].formula);
        ImGui::PopTextWrapPos();

        ImGui::Separator();

        for (auto& s : sc.shapes) {
            ImGui::PushID(&s);
            ImGui::Checkbox("##show", &s.visible);          // láthatóság, élőben hat
            ImGui::SameLine();

            bool warn = pr.is_bad(&s);
            if (warn) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.45f, 0.45f, 1.0f));
            if (ImGui::Selectable(s.name[0] ? s.name : "(nevtelen)", sc.selected == &s,
                                  0, ImVec2(180.0f, 0.0f)))
                sc.selected = &s;
            if (warn) ImGui::PopStyleColor();

            ImGui::SameLine();
            if (ImGui::Button("X")) rq.erase_shape = &s;
            ImGui::PopID();
        }

        ImGui::Separator();
        ImGui::BeginDisabled(!pr.empty());
        if (ImGui::Button("Indit")) rq.build = true;
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Torol")) rq.drop = true;

        // --- Sugárkövetett fénykép (a leválasztható raytrace/ komponens) ---
        ImGui::BeginDisabled(sc.shapes.empty());
        if (ImGui::Button("Fenykep keszitese")) rq.photo = true;
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::BeginCombo("##felbontas", Photo::RESOLUTIONS[photo.size_idx].label)) {
            for (int k = 0; k < Photo::RESOLUTION_COUNT; ++k)
                if (ImGui::Selectable(Photo::RESOLUTIONS[k].label, k == photo.size_idx))
                    photo.size_idx = k;
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::Checkbox("arnyek", &photo.shadows);
        if (!photo.status.empty())
            ImGui::TextDisabled("%s", photo.status.c_str());

        if (!sc.error.empty())
            ImGui::TextColored(ImVec4(1.0f, 0.40f, 0.40f, 1.0f), "%s", sc.error.c_str());
        for (auto& m : pr.messages)
            ImGui::TextColored(ImVec4(1.0f, 0.60f, 0.35f, 1.0f), "%s", m.c_str());

        ImGui::Separator();
        ImGui::SliderFloat("d (meretskala)", &sc.d_ui, 0.5f, 10.0f);
        ImGui::SliderFloat("gorbulet-taszitas", &sc.curv_ui, 0.0f, 5.0f);
        ImGui::End();
    }

}

#endif //GORBE_UI_SHAPESPANEL_HPP
