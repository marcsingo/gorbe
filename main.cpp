#include <string>
#include <cstdio>
#include <list>
#include "App.hpp"

// A felület-típusok (Sphere, Torus, Ellipsoid, Ellipse, ...) itt vannak definiálva:
#include "particle_sampling/Surface.hpp"

// Saját ImGui felülethez:
#include "imgui.h"
#include "libraries/imgui/imgui.h"

int main() {
    App app{800, 800, "Particle sampling"};

    // Állandó életű sampler-pool: MINDEN felvett alakzat egy önálló ImplicitSurface-t kap,
    // saját kezdő részecskékkel (külön mintavételezve). A pool egyszer jön létre és nem
    // semmisül meg (a Window eseménykezelők miatt), itt csak elosztjuk az alakzatokat.
    constexpr int MAX_SHAPES = 16;
    auto pool = app.make_equation_pool(MAX_SHAPES);

    // --- Jelenet-definíció (a GUI szerkeszti) -----------------------------------------
    // Paraméter: NÉV + ÉRTÉK. A value címe STABIL kell legyen (a Parameter float const*-ot
    // tárol rá) -> std::list. Alakzat: NÉV (f1, f2, ...) + KÉPLET + a beparseolt fája.
    struct Param { char name[32] = ""; float value = 0.0f; };
    struct Shape { char name[32] = ""; char formula[256] = ""; bool visible = true; std::shared_ptr<Kifejezes const> tree; };
    std::list<Param> params;
    std::list<Shape> shapes;

    float       d_ui = 2.0f;   // közös méretskála minden samplerre
    std::string error;

    // Névfeloldó: paraméter (skalár) vagy a NÁLA korábbi alakzat részfája (sorrend-függő).
    auto resolve = [&](std::string const& nm, Shape const* limit) -> std::shared_ptr<Kifejezes const> {
        for (auto& p : params)
            if (nm[0] && nm == p.name) return Kif(&p.value).get();
        for (auto& s : shapes) {
            if (&s == limit) break;
            if (s.name[0] && nm == s.name && s.tree) return s.tree;
        }
        return nullptr; // ismeretlen név -> a parser hibát dob
    };

    auto stop_all = [&] { for (auto* p : pool) p->clear(); };

    app.set_gui([&] {
        ImGui::Begin("Alakzatok");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

        // --- Paramétertábla: NÉV | ÉRTÉK | törlés ---
        ImGui::Separator();
        ImGui::Text("Parameterek (nev = ertek):");
        if (ImGui::Button("Uj parameter")) params.push_back({});
        for (auto it = params.begin(); it != params.end(); ) {
            ImGui::PushID(&*it);
            ImGui::SetNextItemWidth(90.0f);  ImGui::InputText("##nev", it->name, sizeof(it->name)); ImGui::SameLine();
            ImGui::SetNextItemWidth(110.0f); ImGui::InputFloat("##ertek", &it->value);              ImGui::SameLine();
            bool del = ImGui::Button("X");
            ImGui::PopID();
            // Paraméter törlése: a futó képletek arra mutathatnak -> minden samplert leállítunk.
            if (del) { it = params.erase(it); stop_all(); error.clear(); }
            else     { ++it; }
        }

        // --- Alakzattábla: NÉV (f1, f2, ...) | KÉPLET (x,y,z, paraméterek) | törlés ---
        // Minden alakzat KÜLÖN, önálló implicit felületként lesz mintavételezve. Egy alakzat
        // hivatkozhat a NÁLA KORÁBBAN definiált alakzatokra is.
        ImGui::Separator();
        ImGui::Text("Alakzatok (nev = keplet) - kulon mintavetelezve:");
        if (ImGui::Button("Uj alakzat") && (int)shapes.size() < MAX_SHAPES) {
            auto& s = shapes.emplace_back();
            std::snprintf(s.name, sizeof(s.name), "f%d", (int)shapes.size());
        }
        for (auto it = shapes.begin(); it != shapes.end(); ) {
            ImGui::PushID(&*it);
            ImGui::Checkbox("##show", &it->visible);                                                         ImGui::SameLine();
            ImGui::SetNextItemWidth(60.0f);  ImGui::InputText("##nev", it->name, sizeof(it->name));          ImGui::SameLine();
            ImGui::SetNextItemWidth(220.0f); ImGui::InputText("##keplet", it->formula, sizeof(it->formula)); ImGui::SameLine();
            bool del = ImGui::Button("X");
            ImGui::PopID();
            if (del) { it = shapes.erase(it); stop_all(); error.clear(); }
            else     { ++it; }
        }
        // A checkboxok élőben hatnak: alakzatonként a megfelelő pool-felület láthatósága.
        { int i = 0; for (auto& s : shapes) { if (i < MAX_SHAPES) pool[i]->set_visible(s.visible); ++i; } }

        ImGui::Separator();
        if (ImGui::Button("Indit")) {
            try {
                int i = 0;
                for (auto& s : shapes) {
                    // alakzat fája (sorrendben: hivatkozhat a korábbiakra és paraméterekre)
                    s.tree = make_kif(s.formula,
                        [&](std::string const& nm) { return resolve(nm, &s); }).get();
                    pool[i]->get_surface().set_tree(s.tree);
                    pool[i]->restart(); // saját kezdő részecskék + futó állapot
                    ++i;
                }
                for (; i < MAX_SHAPES; ++i) pool[i]->clear(); // a fel nem használtak állnak
                error.clear();
            } catch (std::exception const& e) {
                error = e.what();
                stop_all();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Torol")) { stop_all(); error.clear(); }

        if (!error.empty())
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", error.c_str());

        // --- Közös méretskála minden samplerre ---
        ImGui::Separator();
        ImGui::SliderFloat("d (meretskala)", &d_ui, 0.5f, 10.0f);
        for (auto* p : pool) p->d = d_ui;

        ImGui::End();
    });

    app.run();
    return 0;
}
