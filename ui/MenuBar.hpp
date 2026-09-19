#ifndef GORBE_UI_MENUBAR_HPP
#define GORBE_UI_MENUBAR_HPP

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "imgui.h"
#include "../model/Window.hpp"
#include "../scene/ProjectFile.hpp"
#include "Requests.hpp"

// A bináris könyvtára (a CMake adja meg); ide javasoljuk az első mentést.
#ifndef BINARY_DIR
#define BINARY_DIR "."
#endif

namespace Ui {

    namespace fs = std::filesystem;

    inline void open_file_dialog(UiState& ui, UiState::FileDialog kind, std::string const& current) {
        ui.file_dialog = kind;
        std::string start = current.empty()
            ? (fs::path(BINARY_DIR) / "projektek" / ("projekt" + std::string(ProjectFile::EXTENSION))).string()
            : current;
        std::snprintf(ui.path_buf, sizeof(ui.path_buf), "%s", start.c_str());
    }

    // Az útvonal-ablak. ImGui-ban nincs beépített fájlválasztó, a natív ablak pedig
    // platformhoz kötne — ezért egy egyszerű böngésző: a mappa projektfájljai és
    // almappái kattinthatók, felfelé a "..", és az útvonal kézzel is írható.
    inline void file_dialog(UiState& ui, Requests& rq) {
        if (ui.file_dialog == UiState::FileDialog::None) return;
        bool const saving = ui.file_dialog == UiState::FileDialog::SaveAs;
        char const* const title = saving ? "Mentes maskent##fajl" : "Megnyitas##fajl";
        if (!ImGui::IsPopupOpen(title)) ImGui::OpenPopup(title);

        if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

        ImGui::SetNextItemWidth(560.0f);
        bool const enter = ImGui::InputText("##ut", ui.path_buf, sizeof(ui.path_buf),
                                            ImGuiInputTextFlags_EnterReturnsTrue);

        fs::path const p(ui.path_buf);
        fs::path const dir = fs::is_directory(p) ? p : p.parent_path();
        std::error_code ec;
        if (ImGui::BeginListBox("##lista", ImVec2(560.0f, 220.0f))) {
            auto set = [&](fs::path const& q) {
                std::snprintf(ui.path_buf, sizeof(ui.path_buf), "%s", q.string().c_str());
            };
            if (dir.has_parent_path() && dir.parent_path() != dir &&
                ImGui::Selectable(".. (szulo mappa)"))
                set(dir.parent_path() / p.filename());
            std::vector<fs::directory_entry> items;
            for (auto const& e : fs::directory_iterator(dir, ec)) items.push_back(e);
            std::sort(items.begin(), items.end(),
                      [](auto const& a, auto const& b) { return a.path() < b.path(); });
            for (auto const& e : items) {
                std::string const name = e.path().filename().string();
                if (e.is_directory(ec)) {
                    if (ImGui::Selectable(("[mappa] " + name).c_str()))
                        set(e.path() / (saving ? p.filename() : fs::path()));
                } else if (name.size() > std::strlen(ProjectFile::EXTENSION) &&
                           name.ends_with(ProjectFile::EXTENSION)) {
                    if (ImGui::Selectable(name.c_str(), e.path() == p)) set(e.path());
                }
            }
            ImGui::EndListBox();
        }

        bool const exists = fs::is_regular_file(p, ec);
        if (saving) {
            ImGui::TextDisabled(exists ? "Ez a fajl mar letezik - a mentes felulirja."
                                       : "Uj fajl keszul (a hianyzo mappak is).");
        } else if (!exists) {
            ImGui::TextDisabled("Valassz egy %s fajlt.", ProjectFile::EXTENSION);
        }

        bool const can = ui.path_buf[0] && (saving || exists);
        ImGui::BeginDisabled(!can);
        bool const ok = ImGui::Button(saving ? "Mentes" : "Megnyitas", ImVec2(140.0f, 0.0f)) ||
                        (enter && can);
        ImGui::EndDisabled();
        ImGui::SameLine();
        bool const cancel = ImGui::Button("Megse", ImVec2(140.0f, 0.0f)) ||
                            ImGui::IsKeyPressed(ImGuiKey_Escape);

        if (ok) {
            std::string path = ui.path_buf;
            if (saving) {
                // A kiterjesztés hiányzik? Pótoljuk, hogy a Megnyitás listája megtalálja.
                if (!path.ends_with(ProjectFile::EXTENSION)) path += ProjectFile::EXTENSION;
                fs::create_directories(fs::path(path).parent_path(), ec);
                rq.save_path = path;
            } else {
                rq.load_path = path;
            }
        }
        if (ok || cancel) {
            ui.file_dialog = UiState::FileDialog::None;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // A felső menüsor. A magasságát az ImGui a munkaterületből levonja, ezért a
    // panelek (Layout.hpp) magától lejjebb kerülnek — ezt a menüt kell ELŐSZÖR
    // rajzolni a frame-ben.
    inline void menu_bar(std::string const& project_path, UiState& ui, Requests& rq) {
        using FD = UiState::FileDialog;

        // Esc = kilépés, de csak ha nem egy felugró ablakot vagy szövegbevitelt zár
        // be. Ez a frame ELEJÉN fut, mielőtt a felugró ablakok maguk feldolgoznák az
        // Esc-et — így egy most bezáródó ablak még nyitottnak látszik.
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !ImGui::GetIO().WantTextInput &&
            !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId) &&
            ui.file_dialog == FD::None)
            glfwSetWindowShouldClose(Window::handle(), true);

        bool const save_shortcut = ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S);
        bool const open_shortcut = ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_O);

        auto save = [&] {
            if (project_path.empty()) open_file_dialog(ui, FD::SaveAs, project_path);
            else                      rq.save_path = project_path;
        };
        if (save_shortcut) save();
        if (open_shortcut) open_file_dialog(ui, FD::Open, project_path);

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Fajl")) {
                if (ImGui::MenuItem("Uj projekt")) rq.new_project = true;
                if (ImGui::MenuItem("Megnyitas...", "Ctrl+O")) open_file_dialog(ui, FD::Open, project_path);
                if (ImGui::MenuItem("Mentes", "Ctrl+S")) save();
                if (ImGui::MenuItem("Mentes maskent...")) open_file_dialog(ui, FD::SaveAs, project_path);
                ImGui::Separator();
                if (ImGui::MenuItem("Kilepes", "Esc")) glfwSetWindowShouldClose(Window::handle(), true);
                ImGui::EndMenu();
            }
            // A projekt neve a menüsoron, hogy mindig lássuk, mit szerkesztünk.
            ImGui::TextDisabled("  %s", project_path.empty()
                                            ? "(mentetlen projekt)"
                                            : fs::path(project_path).filename().string().c_str());
            ImGui::EndMainMenuBar();
        }
        file_dialog(ui, rq);
    }

}

#endif //GORBE_UI_MENUBAR_HPP
