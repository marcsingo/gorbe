#ifndef GORBE_UI_PROPSPANEL_HPP
#define GORBE_UI_PROPSPANEL_HPP

#include <cctype>
#include <cstdio>
#include <list>
#include <string>
#include <utility>

#include "imgui.h"
#include "../app/Scene.hpp"
#include "../particle_sampling/WarpPresets.hpp"
#include "../raytrace/Material.hpp"
#include "../raytrace/Palette.hpp"
#include "../scene/Presets.hpp"
#include "../scene/Validate.hpp"
#include "../script/Compiler.hpp"
#include "Layout.hpp"
#include "ParamsPanel.hpp"
#include "Requests.hpp"

namespace Ui {

    // A súgó a függvénytáblából (matek/Fuggvenyek.hpp) épül: egy új függvény így
    // magától megjelenik itt is.
    inline std::string const& function_list() {
        static std::string const s = [] {
            std::string r;
            for (auto const& f : Matek::Analizis::FUNCS)  (r += f.name) += ' ';
            for (auto const& m : Matek::Analizis::MACROS) (r += m.name) += ' ';
            return r;
        }();
        return s;
    }
    inline std::string const& function_help() {
        static std::string const s = [] {
            std::string r;
            for (auto const& f : Matek::Analizis::FUNCS)
                r += std::string(f.name) + (f.arity() == 2 ? "(u, v)" : "(u)") + "  " + f.help + '\n';
            for (auto const& m : Matek::Analizis::MACROS)
                r += std::string(m.name) + "(" + m.params + ")  " + m.help + '\n';
            return r;
        }();
        return s;
    }

    // --- Warp-lánc szerkesztő ------------------------------------------------
    // Újraépítést akkor kér, ha a lánc SZERKEZETE vagy egy kifejezés szövege
    // változott: ilyenkor újra kell parseolni. A warp PARAMÉTEREI viszont az
    // alakzat lokálisai, tehát cím szerint épülnek be — azokat a csúszka élőben
    // állítja, újraépítés nélkül.
    //
    // A warpokat a nézet közvetlenül szerkeszti: a fák a SZÖVEGÜKBŐL épülnek, a
    // Warp memóriájára nem hivatkoznak, tehát a törlésük nem veszélyes.
    inline void warp_editor(Shape& s, UiState& ui, Requests& rq) {
        if (!ImGui::CollapsingHeader("Warpok (lancban)", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        auto const& ALL = WarpPresets::ALL;
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::BeginCombo("##warpsablon", ALL[ui.warp_preset_idx].label)) {
            for (int k = 0; k < static_cast<int>(ALL.size()); ++k) {
                bool sel = (k == ui.warp_preset_idx);
                if (ImGui::Selectable(ALL[k].label, sel)) ui.warp_preset_idx = k;
                if (sel) ImGui::SetItemDefaultFocus();
                if (ImGui::IsItemHovered() && ALL[k].hint[0])
                    ImGui::SetTooltip("%s", ALL[k].hint);
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Warp hozzaad")) {
            add_warp(s, ALL[ui.warp_preset_idx]);
            rq.build = true;
        }
        if (s.warps.empty())
            ImGui::TextDisabled("Nincs warp. A lancban az ELSO hat eloszor.");

        int move_from = -1, move_to = -1, erase = -1;
        for (int i = 0; i < static_cast<int>(s.warps.size()); ++i) {
            Warp& w = s.warps[i];
            ImGui::PushID(i);
            if (ImGui::Checkbox("##on", &w.enabled)) rq.build = true;
            ImGui::SameLine();
            bool open = ImGui::TreeNodeEx("##w", ImGuiTreeNodeFlags_DefaultOpen,
                                          "%d. %s", i + 1, w.name);
            ImGui::SameLine();
            if (ImGui::SmallButton("^") && i > 0)                        { move_from = i; move_to = i - 1; }
            ImGui::SameLine();
            if (ImGui::SmallButton("v") && i + 1 < (int)s.warps.size())  { move_from = i; move_to = i + 1; }
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) erase = i;

            if (open) {
                char const* labels[] = {"x' =", "y' =", "z' ="};
                char* fields[] = {w.fx, w.fy, w.fz};
                for (int c = 0; c < 3; ++c) {
                    ImGui::SetNextItemWidth(-40.0f);
                    ImGui::InputText(labels[c], fields[c], sizeof(w.fx));
                    if (ImGui::IsItemDeactivatedAfterEdit()) rq.build = true;
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        if (move_from >= 0) { std::swap(s.warps[move_from], s.warps[move_to]); rq.build = true; }
        if (erase >= 0)     { s.warps.erase(s.warps.begin() + erase);          rq.build = true; }

        ImGui::TextDisabled("A harom kifejezes a ter -> alakzat lekepezes:");
        ImGui::TextDisabled("'hol keressuk ki az alakzatot ehhez a ponthoz'.");
        ImGui::TextDisabled("Ezert a sablonok a deformacio INVERZET tartalmazzak.");
    }

    // 2. ablak (JOBB oldalon, TELJES magasságban): a kijelölt alakzat adatai —
    // képlet, szín/anyag, tartomány, transzformáció, warp-lánc, lokális paraméterek.
    // Az alakzat nevéből egy osztálynév-javaslat: csak betű, szám, _ (az ékezetes betű
    // kimarad), nagy kezdőbetűvel — "gomb 1" -> "Gomb_1".
    inline std::string suggest_class_name(char const* shape_name) {
        std::string r;
        for (char const* p = shape_name; *p; ++p) {
            unsigned char const c = static_cast<unsigned char>(*p);
            if (c >= 0x80) continue;
            r += std::isalnum(c) ? static_cast<char>(c) : '_';
        }
        if (r.empty() || std::isdigit(static_cast<unsigned char>(r[0]))) r = "Alakzat" + r;
        r[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(r[0])));
        return r;
    }

    // --- Az alakzat C++ kódja (script/) -------------------------------------
    // A fájlt a nézet nem írja: a létrehozást és a megnyitást a vezérlő végzi.
    // A leválasztás csak a nevet törli (a fájl marad), és a következő Indításkor hat.
    inline void script_section(Shape& s, std::string const& project_path, UiState& ui, Requests& rq) {
        if (!ImGui::CollapsingHeader("Kod (C++)", ImGuiTreeNodeFlags_DefaultOpen)) return;
        ImGui::PushTextWrapPos(0.0f);
        if (project_path.empty()) {
            ImGui::TextDisabled("A kod a projekt mappajaba kerul, ezert elobb mentsd a "
                                "projektet (Fajl -> Mentes).");
        } else if (s.script[0]) {
            ImGui::Text("kod/%s", s.script);
            if (ImGui::Button("Megnyitas")) rq.script_open = &s;
            ImGui::SameLine();
            if (ImGui::Button("Mappa")) rq.open_project_dir = true;
            ImGui::SameLine();
            if (ImGui::Button("Levalasztas")) s.script[0] = 0;
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("A fajl megmarad, csak ez az alakzat nem futtatja\n"
                                  "(a kovetkezo Inditastol).");
            ImGui::TextDisabled("Az Indit gomb forditja le es inditja el. A feluleten "
                                "beallitott parameterek es a pozicio a kezdoertekek.");
        } else {
            if (ui.script_for != &s) {
                std::snprintf(ui.script_buf, sizeof(ui.script_buf), "%s", suggest_class_name(s.name).c_str());
                ui.script_for = &s;
            }
            ImGui::SetNextItemWidth(200.0f);
            ImGui::InputText("osztaly", ui.script_buf, sizeof(ui.script_buf));
            bool const valid = Script::valid_class_name(ui.script_buf);
            ImGui::BeginDisabled(!valid);
            if (ImGui::Button("Kod letrehozasa")) {
                rq.script_create = &s;
                rq.script_class = ui.script_buf;
            }
            ImGui::EndDisabled();
            if (!valid)
                ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f),
                                   "Az osztalynev betu, szam es _ lehet, nem kezdodhet szammal.");
            ImGui::TextDisabled("kod/<osztaly>.cpp keszul, es megnyilik a szerkesztoben. Ha mar "
                                "van ilyen fajl, azt rendeli hozza (tobb alakzat is futtathatja).");
        }
        ImGui::PopTextWrapPos();
    }

    // --- A jelenet kamerája (az Alakzatok lista "Kamera" sora) ------------------
    // Az értékeket a nézet élőben írja (mint egy alakzat transzformációját): futás közben
    // azonnal látszik, és amit közben a kód átír, az a Töröl után ide áll vissza.
    inline void camera_props(Scene& sc) {
        CameraDoc& c = sc.scene_camera;
        ImGui::Text("Kamera");
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("A jelenet kameraja: futas kozben (Indit utan) ebbol latunk. "
                            "A szerkeszto nezetben a narancs gula mutatja, hol all.");
        ImGui::PopTextWrapPos();
        ImGui::Separator();

        ImGui::DragFloat3("pozicio", c.pos, 0.05f);
        ImGui::SliderAngle("irany (yaw)", &c.rot[0], -180.0f, 180.0f);
        ImGui::SliderAngle("emelkedes (pitch)", &c.rot[1], -89.0f, 89.0f);
        ImGui::SliderAngle("doles (roll)", &c.rot[2], -180.0f, 180.0f);
        ImGui::SliderFloat("latoszog", &c.fov, CameraDoc::FOV_MIN, CameraDoc::FOV_MAX, "%.0f fok");

        static float target[3] = {0.0f, 0.0f, 0.0f};    // csak a nézeté (nem mentődik)
        ImGui::DragFloat3("##cel", target, 0.05f);
        ImGui::SameLine();
        if (ImGui::Button("Ranez")) c.look_towards(glm::vec3(target[0], target[1], target[2]) - c.position());
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("A kamera a megadott pont fele fordul (a helye marad).");

        if (ImGui::Button("Szerkeszto nezetbol")) {
            c.look_at(sc.camera.get_position(), sc.camera.get_position() + sc.camera.get_front());
            c.rot[2] = 0.0f;
            c.fov = sc.camera.get_fov_deg();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("A kamera oda all es arra nez, ahonnan most a szerkeszto kamera.");
        ImGui::SameLine();
        if (ImGui::Button("Alaphelyzet")) c = CameraDoc{};

        ImGui::Separator();
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("Kodbol: camera().position(), rotation(), fov(), look_at(p), "
                            "forward() / right() / up(). A kod altal irt ertekek a Torol utan "
                            "visszaallnak ide.");
        ImGui::PopTextWrapPos();
    }

    inline void props_panel(Scene& sc, std::list<Param> const& program_params,
                            std::string const& project_path,
                            Problems const& pr, UiState& ui, Requests& rq,
                            Geometry const& g) {
        fixed_panel("Tulajdonsagok", {g.rx, g.O.y + PAD}, {ui.layout.right_w, g.col_h});
        if (sc.selected == nullptr && sc.camera_selected && !sc.editor) {
            camera_props(sc);
            ImGui::End();
            return;
        }
        if (sc.selected == nullptr) {
            ImGui::TextDisabled("Valassz egy alakzatot az \"Alakzatok\" listabol.");
            ImGui::End();
            return;
        }
        Shape& s = *sc.selected;

        bool name_warn = pr.is_bad(&s);
        if (name_warn) ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.45f, 0.12f, 0.12f, 1.0f));
        ImGui::SetNextItemWidth(200.0f);
        ImGui::InputText("nev", s.name, sizeof(s.name));
        if (name_warn) ImGui::PopStyleColor();

        // Variációs szerkesztő (Turk–O'Brien): képletes alakzatnál előbb átalakít a
        // részecskékből, és külön fület nyit, az alakzat közepével az origóban.
        if (!sc.editor) {
            if (ImGui::Button(s.vari ? "Szerkesztes" : "Atalakitas es szerkesztes")) rq.edit_shape = &s;
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(s.vari ? "Kulon fulon, az alakzat kozepe az origoban."
                                         : "A reszecskekbol variacios feluletet kesz (a kepletet,\n"
                                           "a warpokat es a tartomanyt felvaltja), es kulon\n"
                                           "fulon megnyitja. Elotte inditsd el az alakzatot.");
        }
        if (s.vari) {
            int bound = 0;
            for (float v : s.vari->values) bound += (v == 0.0f);
            ImGui::TextDisabled("Variacios alakzat: %d feluleti pont, %d normalis", bound,
                                static_cast<int>(s.vari->values.size()) - bound);
            if (sc.editor) {
                ImGui::PushTextWrapPos(0.0f);
                ImGui::TextDisabled("Huzz egy kockat: a felulet atmegy rajta. Shift + kattintas "
                                    "a feluletre: uj pont (a felulet nem valtozik). "
                                    "Ctrl + kattintas: torles.");
                ImGui::PopTextWrapPos();
            }
        }

        // A panel szekciói összecsukhatók: különben a lentebbi részek (warpok,
        // paraméterek) lelógnának a panel aljáról és észrevehetetlenek lennének.
        if (!s.vari && ImGui::CollapsingHeader("Keplet", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("F(x, y, z) =");
            ImGui::InputTextMultiline("##keplet", s.formula, sizeof(s.formula),
                                      ImVec2(-1.0f, ImGui::GetTextLineHeight() * 3.5f));
            ImGui::TextDisabled("Valtozok: x y z | t = ido (mp) | allandok: pi e");
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled("Fuggvenyek (reszletek: vidd ide az egeret): %s",
                                function_list().c_str());
            ImGui::PopTextWrapPos();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", function_help().c_str());
            ImGui::TextDisabled("Rovidites: 2x  3(x+1)  x**2  -> 2*x  3*(x+1)  x^2");
            ImGui::TextDisabled("Hivatkozhatsz a listaban ELOTTE allo alakzatok nevere is.");
        }

        // Szín és anyag (mindkettő a sugárkövetett FÉNYKÉPHEZ). Az anyag a szín
        // MELLÉ jön, nem helyette: ugyanaz a piros lehet matt gumi vagy üveg.
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::BeginCombo("szin", Raytrace::palette_name(s.color_idx))) {
            for (int k = 0; k < Raytrace::PALETTE_COUNT; ++k) {
                glm::vec3 c = Raytrace::palette_color(k);
                ImGui::ColorButton("##c", ImVec4(c.r, c.g, c.b, 1.0f),
                                   ImGuiColorEditFlags_NoTooltip, ImVec2(14, 14));
                ImGui::SameLine();
                if (ImGui::Selectable(Raytrace::palette_name(k), k == s.color_idx))
                    s.color_idx = k;
            }
            ImGui::EndCombo();
        }

        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::BeginCombo("anyag", Raytrace::material_name(s.material_idx))) {
            for (int k = 0; k < Raytrace::MATERIAL_COUNT; ++k)
                if (ImGui::Selectable(Raytrace::material_name(k), k == s.material_idx))
                    s.material_idx = k;
            ImGui::EndCombo();
        }
        ImGui::TextDisabled("A szin es az anyag a FENYKEPRE hat (nem a nezetre).");

        // Méretskála: a globálisat követi, vagy saját értéke van.
        if (ImGui::CollapsingHeader("Mintavetelezes", ImGuiTreeNodeFlags_DefaultOpen)) {
            // Bekapcsoláskor a pillanatnyi globális értékről indul, hogy a
            // mintavétel sűrűsége ne ugorjon.
            if (ImGui::Checkbox("sajat d", &s.own_d) && s.own_d) s.d = sc.d_ui;
            ImGui::SameLine();
            ImGui::BeginDisabled(!s.own_d);
            ImGui::SetNextItemWidth(-1.0f);
            float shown = effective_d(s, sc);
            if (ImGui::SliderFloat("##sajatd", &shown, 0.5f, 10.0f, "d = %.2f")) s.d = shown;
            ImGui::EndDisabled();
            ImGui::TextDisabled(s.own_d ? "A globalis d nem hat erre az alakzatra."
                                        : "A globalis d-t koveti (Alakzatok panel).");
        }

        if (ImGui::CollapsingHeader("Tartomany")) {
            ImGui::Text("Csak itt jelenjen meg (opcionalis):");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputText("##tartomany", s.domain, sizeof(s.domain));
            ImGui::TextDisabled("pl. x > 2 and x < 6 and y > -2 and y < 2");
            ImGui::TextDisabled("Operatorok: > < >= <= and or not (&& || ! is jo)");
            ImGui::TextDisabled("A reszecskek a FELULETEN csusznak be a jo terreszbe,");
            ImGui::TextDisabled("es a peremen nem lepnek at. Ures = korlatlan.");
        }

        // --- Tér-transzformáció ---------------------------------------------
        if (ImGui::CollapsingHeader("Transzformacio", ImGuiTreeNodeFlags_DefaultOpen)) {
            bool xf_edited = false;
            xf_edited |= ImGui::DragFloat3("pozicio", s.xform.pos, 0.05f);
            xf_edited |= ImGui::SliderAngle("forgatas x", &s.xform.rot[0], -180.0f, 180.0f);
            xf_edited |= ImGui::SliderAngle("forgatas y", &s.xform.rot[1], -180.0f, 180.0f);
            xf_edited |= ImGui::SliderAngle("forgatas z", &s.xform.rot[2], -180.0f, 180.0f);
            xf_edited |= ImGui::DragFloat3("meret", s.xform.scale, 0.02f, 0.01f, 100.0f);
            if (ImGui::SmallButton("Alaphelyzet")) { s.xform.reset(); xf_edited = true; }
            ImGui::SameLine();
            ImGui::TextDisabled(s.warped ? "(eloben mozog)" : "(elso mozgatasra ujraepul)");

            // Egységtranszformációval a warp NEM épül be (így az egyszerű alakzatok
            // olcsók maradnak: a gömb programja 15 utasítás a warpos 153 helyett).
            // Ezért amikor ELŐSZÖR nyúlsz a vezérlőkhöz, újra kell építeni — utána
            // a paraméterek címe már benne van, és a csúszkák élőben hatnak.
            if (xf_edited && !s.warped && !s.xform.is_identity()) rq.build = true;
        }

        warp_editor(s, ui, rq);

        // Kontrollpontok (a cikk kényszerei). A lerakás és a húzás a 3D nézetben van
        // (app/Controller.hpp); itt csak az áttekintés és a törlés.
        if (!s.vari && ImGui::CollapsingHeader("Kontrollpontok", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("%d pont", static_cast<int>(s.controls.size()));
            ImGui::SameLine();
            ImGui::BeginDisabled(s.controls.empty());
            if (ImGui::SmallButton("Mind torol")) s.controls.clear();
            ImGui::EndDisabled();
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled("Shift + bal kattintas az alakzatra: uj pont. Egy pontot "
                                "huzva az alakzat ugy valtozik, hogy a felulet minden "
                                "ponton atmenjen. Ctrl + kattintas: torles.");
            std::string moves = "Ezeket allitja: pozicio";
            for (auto const& p : s.locals)
                if (!p.derived() && p.name[0]) (moves += ", ") += p.name;
            ImGui::TextDisabled("%s", moves.c_str());
            ImGui::PopTextWrapPos();
        }

        if (!sc.editor) script_section(s, project_path, ui, rq);

        if (ImGui::CollapsingHeader("Lokalis parameterek", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextDisabled("Csak ez az alakzat latja oket.");
            param_table(s.locals, "p", {&s.locals, &sc.params, &program_params}, sc, pr, rq);
        }
        ImGui::End();
    }

}

#endif //GORBE_UI_PROPSPANEL_HPP
