#ifndef GORBE_UI_CONSOLEPANEL_HPP
#define GORBE_UI_CONSOLEPANEL_HPP

#include <cstdio>
#include <string>

#include "imgui.h"
#include "../utils/Log.hpp"
#include "Layout.hpp"
#include "Requests.hpp"

namespace Ui {

    // Egy konzolsor szövegként: "[  12.34] Gomb1: szoveg (x37)".
    inline std::string console_line_text(Log::Line const& l) {
        char t[32];
        std::snprintf(t, sizeof(t), "[%8.2f] ", l.t);
        std::string s = t;
        if (!l.source.empty()) (s += l.source) += ": ";
        s += l.text;
        if (l.repeat > 1) s += " (x" + std::to_string(l.repeat) + ")";
        return s;
    }

    // Középen alul, a 3D nézet alatt: a konzol (utils/Log.hpp). Az alakzatok kódja a
    // print()-tel ír ide; a fordítás eredménye és a futás hibái is ide kerülnek.
    // Ha az automatikus görgetés be van kapcsolva, és a lista alján állunk, követi az új
    // sorokat; feljebb görgetve megáll, hogy a régebbi sorok olvashatók maradjanak.
    inline void console_panel(UiState& ui, Geometry const& g) {
        fixed_panel("Konzol", {g.cx, g.con_y}, {g.cw, ui.layout.console_h});

        if (ImGui::SmallButton("Torles")) Log::clear();
        ImGui::SameLine();
        if (ImGui::SmallButton("Masolas")) {
            std::string all;
            for (auto const& l : Log::lines) (all += console_line_text(l)) += '\n';
            ImGui::SetClipboardText(all.c_str());
        }
        ImGui::SameLine();
        ImGui::Checkbox("automatikus gorgetes", &ui.console_autoscroll);
        ImGui::SameLine();
        ImGui::TextDisabled("(%d sor)", static_cast<int>(Log::lines.size()));
        ImGui::Separator();

        ImGui::BeginChild("##sorok", ImVec2(0, 0), ImGuiChildFlags_None,
                          ImGuiWindowFlags_HorizontalScrollbar);
        bool const at_bottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;
        if (Log::lines.empty())
            ImGui::TextDisabled("Ures. Az alakzatok kodja a print(\"...\")-tel ir ide.");

        // Csak a látható sorok rajzolódnak (sok ezer sornál is gyors).
        ImGuiListClipper clip;
        clip.Begin(static_cast<int>(Log::lines.size()));
        while (clip.Step())
            for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i) {
                Log::Line const& l = Log::lines[static_cast<std::size_t>(i)];
                std::string const s = console_line_text(l);
                switch (l.level) {
                    case Log::Level::Error:
                        ImGui::TextColored(ImVec4(1.0f, 0.40f, 0.40f, 1.0f), "%s", s.c_str()); break;
                    case Log::Level::Warning:
                        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.30f, 1.0f), "%s", s.c_str()); break;
                    default:
                        ImGui::TextUnformatted(s.c_str());
                }
            }
        if (ui.console_autoscroll && at_bottom) ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
        ImGui::End();
    }

}

#endif //GORBE_UI_CONSOLEPANEL_HPP
