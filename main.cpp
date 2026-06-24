#include <string>
#include "App.hpp"

// A felület-típusok (Sphere, Torus, Ellipsoid, Ellipse, ...) itt vannak definiálva:
#include "particle_sampling/Surface.hpp"

// Saját ImGui felülethez:
#include "imgui.h"
#include "libraries/imgui/imgui.h"

int main() {
    App app{800, 800, "Particle sampling"};

    // Stringből, futásidőben megadható egyenlet (F(x,y,z)=0). A felület álló állapotban
    // jön létre; az ImGui-panel "Indit" gombja állítja be az egyenletet és indítja a
    // szimulációt, a "Torol" leállítja és kiüríti a részecskéket.
    auto& sim = app.show_equation();
    sim.set_manual_diameter(true); // a d csúszkáról állítható (különben a felület felülírná)

    // A panel állapota (a lambda a main végéig él, így biztonságos referenciával kapni el).
    char        eq_buf[256] = "x^2 + y^2 + z^2 - 1";
    std::string error;

    app.set_gui([&] {
        ImGui::Begin("Egyenlet");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

        ImGui::InputText("F(x,y,z) = 0", eq_buf, sizeof(eq_buf));

        // --- Paramétertábla: NÉV | ÉRTÉK | törlés -----------------------------------
        // Az x/y/z változó; minden más névre itt felvett paraméterként hivatkozhatsz a
        // képletben (pl. "x^2 + y^2 - r^2", ha felvettél egy "r" paramétert). Az értéket
        // futás közben is állíthatod (élőben hat). Sor törlése leállítja a szimulációt,
        // mert a futó képlet még arra a paraméterre mutathat -> az "Indit"-tal indítsd újra.
        ImGui::Separator();
        ImGui::Text("Parameterek (nev = ertek):");
        auto& params = sim.get_surface().params;
        if (ImGui::Button("Uj parameter")) params.push_back({});
        for (auto it = params.begin(); it != params.end(); ) {
            ImGui::PushID(&*it);
            ImGui::SetNextItemWidth(90.0f);
            ImGui::InputText("##nev", it->name, sizeof(it->name));
            ImGui::SameLine();
            ImGui::SetNextItemWidth(110.0f);
            ImGui::InputFloat("##ertek", &it->value);
            ImGui::SameLine();
            bool del = ImGui::Button("X");
            ImGui::PopID();
            if (del) { it = params.erase(it); sim.clear(); error.clear(); }
            else     { ++it; }
        }
        ImGui::Separator();

        if (ImGui::Button("Indit")) {
            try {
                sim.get_surface().set_equation(eq_buf); // string -> fa (dobhat)
                sim.restart();                          // friss részecskék, futó állapot
                error.clear();
            } catch (std::exception const& e) {
                error = e.what();
                sim.clear();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Torol")) {
            sim.clear();
            error.clear();
        }

        ImGui::Text("Allapot: %s", sim.is_running() ? "fut" : "all");
        if (!error.empty())
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", error.c_str());

        ImGui::Separator();
        ImGui::SliderFloat("d (meretskala)", &sim.d, 0.1f, 10.0f);
        ImGui::Text("sigma_v   = %.3f", sim.sigma_v());
        ImGui::Text("sigma_max = %.3f", sim.sigma_max());
        ImGui::End();
    });

    app.run();
    return 0;
}
