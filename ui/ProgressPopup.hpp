#ifndef GORBE_UI_PROGRESSPOPUP_HPP
#define GORBE_UI_PROGRESSPOPUP_HPP

#include "imgui.h"

#include "../app/Job.hpp"
#include "Requests.hpp"

namespace Ui {

    // Folyamatjelző egy hosszú munkához (app/Job.hpp): a cím, a százalék és az épp
    // futó lépés. Modális, tehát amíg látszik, a panelek és a 3D nézet nem kapnak
    // bevitelt. A gyors munka a saját frame-jében befejeződik, azt ez nem is látja.
    //
    // A méret RÖGZÍTETT: az automatikus méretű ablakot az ImGui az első frame-ben
    // elrejti (akkor méri ki), és egy rövid munkánál ennyi késés is elég ahhoz, hogy
    // a jelző ne látsszon.
    inline void progress_popup(Job const* job, Requests& rq) {
        constexpr char const* ID = "##folyamat";
        bool const show = job != nullptr;
        if (show && !ImGui::IsPopupOpen(ID)) ImGui::OpenPopup(ID);

        ImGuiStyle const& st = ImGui::GetStyle();
        float const h = 2.0f * ImGui::GetTextLineHeightWithSpacing() + 2.0f * ImGui::GetFrameHeightWithSpacing() +
                        2.0f * st.WindowPadding.y;
        ImGui::SetNextWindowSize(ImVec2(380.0f, h), ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        if (!ImGui::BeginPopupModal(ID, nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar))
            return;
        if (!show) {
            ImGui::CloseCurrentPopup();
        } else {
            ImGui::TextUnformatted(job->title.c_str());
            ImGui::ProgressBar(job->progress(), ImVec2(-1.0f, 0.0f));
            ImGui::TextDisabled("%s", job->current().c_str());
            // A megszakítás az épp futó lépés UTÁN hat (egy lépés nem szakítható félbe).
            if (ImGui::Button("Megszakitas", ImVec2(-1.0f, 0.0f))) rq.cancel_job = true;
        }
        ImGui::EndPopup();
    }

}

#endif //GORBE_UI_PROGRESSPOPUP_HPP
