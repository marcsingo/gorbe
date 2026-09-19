#ifndef GORBE_UI_VIEWPORTPANEL_HPP
#define GORBE_UI_VIEWPORTPANEL_HPP

#include <algorithm>
#include <list>

#include "imgui.h"
#include "../App.hpp"
#include "../app/Scene.hpp"
#include "../model/Viewport.hpp"
#include "Layout.hpp"
#include "Requests.hpp"

namespace Ui {

    // Középső ablak: a 3D nézet (a jelenet textúrája), fülekkel. A fülváltást, az
    // új fület és a bezárást csak kéri — a fülsáv ciklusa közben nem változhat a lista.
    inline void viewport_panel(App& app, std::list<Scene>& scenes, UiState& ui,
                               Requests& rq, Geometry const& g) {
        ImGui::SetNextWindowPos({g.cx, g.O.y + PAD});
        ImGui::SetNextWindowSize({g.cw, g.col_h});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("##nezet", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);

        bool image_hovered = false;

        if (ImGui::BeginTabBar("##jelenetek", ImGuiTabBarFlags_Reorderable |
                                              ImGuiTabBarFlags_AutoSelectNewTabs)) {
            for (auto& one : scenes) {
                bool open = true;
                // Csak akkor adunk bezáró gombot, ha van mit bezárni.
                bool* p_open = (scenes.size() > 1) ? &open : nullptr;
                ImGui::PushID(&one);
                if (ImGui::BeginTabItem(one.name, p_open)) {
                    rq.want_scene = &one;

                    ImVec2 avail = ImGui::GetContentRegionAvail();
                    ImVec2 pos   = ImGui::GetCursorScreenPos();
                    int const iw = std::max(16, static_cast<int>(avail.x));
                    int const ih = std::max(16, static_cast<int>(avail.y));

                    // A KÖVETKEZŐ frame-re kérjük a méretet (lásd App::request_viewport_size).
                    app.request_viewport_size(iw, ih);
                    Vp::set_current({pos.x, pos.y, static_cast<float>(iw), static_cast<float>(ih)});

                    // A GL-textúra alulról felfelé áll, ezért az UV-t megfordítjuk.
                    ImGui::Image(static_cast<ImTextureID>(app.viewport_texture()),
                                 ImVec2(static_cast<float>(iw), static_cast<float>(ih)),
                                 ImVec2(0, 1), ImVec2(1, 0));
                    image_hovered = ImGui::IsItemHovered();
                    ImGui::EndTabItem();
                }
                ImGui::PopID();
                if (!open) rq.close_scene = &one;
            }
            // "+" fül: új jelenet
            if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing |
                                          ImGuiTabItemFlags_NoTooltip))
                rq.new_scene = true;
            ImGui::EndTabBar();
        }
        ImGui::End();
        ImGui::PopStyleVar();

        // --- Bemenet-kapu ---------------------------------------------------
        // A jelenet akkor kap egeret, ha a kurzor a képen van. HÚZÁS-RETESZ: ha a
        // húzás a képen indult, a gomb elengedéséig akkor is oda megy, ha az egér
        // kicsúszik — különben forgatás közben a panel fölé érve megállna a nézet.
        ImGuiIO& io = ImGui::GetIO();
        bool const any_down = ImGui::IsMouseDown(ImGuiMouseButton_Left)  ||
                              ImGui::IsMouseDown(ImGuiMouseButton_Right) ||
                              ImGui::IsMouseDown(ImGuiMouseButton_Middle);
        if (image_hovered && any_down) ui.drag_latch = true;
        if (!any_down)                 ui.drag_latch = false;

        Vp::set_scene_mouse(image_hovered || ui.drag_latch);
        Vp::set_scene_keyboard(!io.WantCaptureKeyboard);
    }

}

#endif //GORBE_UI_VIEWPORTPANEL_HPP
