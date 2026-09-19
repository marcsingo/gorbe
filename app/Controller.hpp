#ifndef GORBE_APP_CONTROLLER_HPP
#define GORBE_APP_CONTROLLER_HPP

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <list>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "../scene/Build.hpp"
#include "../scene/Names.hpp"
#include "../scene/ProjectFile.hpp"
#include "../ui/Requests.hpp"
#include "../utils/Parallel.hpp"
#include "Photo.hpp"
#include "Scene.hpp"

// Az MVC VEZÉRLŐJE: a jelenetek (fülek), az aktuális fül és a program-szintű
// paraméterek gazdája. A nézetek kéréseit (Ui::Requests) a frame végén hajtja végre.
class Controller {
public:
    // PROGRAM-szintű paraméterek: minden jelenet (fül) látja őket. Ez a legkülső
    // hatókör; a jelenet- és az alakzat-szintű nevek elfedhetik (mint C++-ban).
    std::list<Param> program_params;

    // A jelenetek (fülek). std::list, mert a Scene címe stabil kell legyen: a
    // mintavételezők és a kamera eseménykezelői rá mutatnak.
    std::list<Scene> scenes;

    Photo::Settings photo;

    // A megnyitott/mentett projektfájl (üres = még nincs mentve), és az utolsó
    // fájlművelet eredménye a panelre.
    std::string project_path;
    std::string file_status;

    Controller() {
        auto& s0 = scenes.emplace_back();
        std::snprintf(s0.name, sizeof(s0.name), "Jelenet 1");
        cur_ = &s0;
        cur_->set_active(true);

        // A szimuláció órája: frame-enként egyszer, az aktuális fül esedékes
        // objektumait PÁRHUZAMOSAN lépteti (a háttérben lévők úgysem futnak).
        sim_sub = Window::Subscription(Window::add_time_passed_event([this](auto ev) {
            step_objects(static_cast<float>(ev.dt));
        }));
    }

    // Az esedékes objektumok egy-egy lépése, párhuzamosan (utils/Parallel.hpp).
    //
    // Biztonságos: minden objektumnak saját részecskéi, lefordított programjai (a
    // munkaterületükkel) és véletlenszám-generátora van; a közös adatokat
    // (paraméterek, a `t`, a kifejezésfák) a lépés csak OLVASSA, és a GUI csak a
    // lépések befejezése után írhat újra — ez a hívás megvárja az összeset.
    void step_objects(float dt) {
        if (!cur_) return;
        std::vector<std::pair<SpaceObject*, float>> due;
        for (auto& p : cur_->pool)
            if (float const s = p->controller().advance(dt); s > 0.0f) due.emplace_back(p.get(), s);
        Parallel::for_each(due.size(), [&](std::size_t i) {
            due[i].first->model().step(due[i].second);
        });
    }

    Scene& cur() { return *cur_; }

    // A frame végén, a GUI felépítése UTÁN hívandó. A sorrend számít: előbb a
    // törlések (drop-pal), utána az építés és a fénykép az AKTUÁLIS fülön, és csak
    // legvégül a fülváltás/bezárás — az utóbbi érvénytelenítheti a `cur`-t.
    void apply(Ui::Requests const& rq) {
        Scene& sc = *cur_;

        // A paramétert az alakzat ELŐTT: a listája lehet épp a törlendő alakzat lokálisa.
        if (rq.erase_param) {
            // Program-szintű változás MINDEN jelenetet érint: a már beparseolt fák a
            // törölt float CÍMÉT tartják, ezért mindenhol el kell dobni őket.
            if (rq.erase_from == &program_params)
                for (auto& other : scenes) other.drop();
            else
                sc.drop();
            rq.erase_from->remove_if([&](Param const& p) { return &p == rq.erase_param; });
        }
        if (rq.erase_shape) {
            if (sc.selected == rq.erase_shape) sc.selected = nullptr;
            sc.drop();              // a törölt alakzat lokálisaira mutathatnak fák
            sc.shapes.remove_if([&](Shape const& s) { return &s == rq.erase_shape; });
        }
        if (rq.drop) sc.drop();

        sc.sync_pool();
        if (rq.build) build(sc);
        if (rq.photo) Photo::take(sc, photo);

        switch_scenes(rq);

        // A projekt-műveletek a legvégén: a betöltés és az új projekt MINDEN jelenetet
        // lecserél, tehát a fenti hivatkozások (sc, cur_) utána érvénytelenek.
        if (!rq.save_path.empty()) save_project(rq.save_path);
        if (!rq.load_path.empty()) load_project(rq.load_path);
        if (rq.new_project)        new_project();
    }

    // --- projekt (scene/ProjectFile.hpp) ------------------------------------------

    void new_project() {
        ProjectFile::Project pr;
        std::snprintf(pr.scenes.emplace_back().name, 32, "Jelenet 1");
        replace_all(std::move(pr));
        project_path.clear();
        file_status = "Uj projekt";
    }

    bool save_project(std::string const& path) {
        // A futó kamerák állása a dokumentumba, hogy a nézőpont is mentődjön.
        for (auto& s : scenes) {
            s.view.eye    = s.camera.get_position();
            s.view.target = s.view.eye + s.camera.get_front();
            s.view.fov    = s.camera.get_fov_deg();
        }
        std::ofstream f(path, std::ios::binary);
        if (f) f << ProjectFile::to_text(program_params, scenes);
        if (!f) {
            file_status = "HIBA: nem sikerult menteni: " + path;
            return false;
        }
        project_path = path;
        file_status = "Mentve: " + path;
        return true;
    }

    bool load_project(std::string const& path) {
        std::ifstream f(path, std::ios::binary);
        if (!f) {
            file_status = "HIBA: nem sikerult megnyitni: " + path;
            return false;
        }
        std::stringstream text;
        text << f.rdbuf();
        std::vector<std::string> warnings;
        try {
            replace_all(ProjectFile::from_text(text.str(), warnings));
        } catch (std::exception const& e) {
            file_status = std::string("HIBA: ") + e.what();   // a régi projekt megmarad
            return false;
        }
        project_path = path;
        file_status = "Megnyitva: " + path;
        for (auto const& w : warnings) file_status += "\n  figyelmeztetes: " + w;
        return true;
    }

    // A jelenetek (bennük a Model-ek: VAO/VBO) felszabadítása MÉG élő GL-kontextussal.
    void shutdown() { scenes.clear(); cur_ = nullptr; }

private:
    Scene* cur_ = nullptr;
    Window::Subscription sim_sub;

    // Minden jelenet lecserélése egy betöltött projektre, és a felépítésük, hogy
    // rögtön lássuk is. SORREND: előbb a régi jelenetek szűnnek meg (a fáik a régi
    // program-paraméterek címére mutatnak), csak utána cserélődnek a paraméterek.
    void replace_all(ProjectFile::Project&& pr) {
        scenes.clear();
        cur_ = nullptr;
        program_params = std::move(pr.program_params);
        for (auto& doc : pr.scenes) {
            auto& ns = scenes.emplace_back();
            static_cast<SceneDoc&>(ns) = std::move(doc);
            ns.camera.look_at(ns.view.eye, ns.view.target);
            ns.camera.set_fov_deg(ns.view.fov);
            ns.sync_pool();
            build(ns);
            ns.set_active(false);
        }
        cur_ = &scenes.front();
        cur_->set_active(true);
    }

    // Az összes alakzat beparseolása (scene/Build.hpp) és a kész fák átadása a
    // mintavételezőknek.
    void build(Scene& sc) {
        std::string err = Build::build(sc, program_params);
        if (!err.empty()) {
            sc.drop();
            sc.error = err;
            return;
        }
        sc.error.clear();
        std::size_t i = 0;
        for (auto& s : sc.shapes) {
            auto& p = *sc.pool[i++];
            auto& surf = p.model().surface();
            surf.set_tree(s.tree);
            if (s.dom_tree) surf.set_domain(s.dom_tree);
            else            surf.clear_domain();
            p.start();     // saját kezdő részecskék + futó állapot
        }
    }

    void switch_scenes(Ui::Requests const& rq) {
        Scene* want = rq.want_scene ? rq.want_scene : cur_;

        // Új jelenet (a "+" fülről). Csak itt, a fülsáv ciklusa után: a ciklus
        // minden frame-ben a KIVÁLASZTOTT fülre állítja a want_scene-t.
        if (rq.new_scene) {
            auto& ns = scenes.emplace_back();
            next_name(scenes, "Jelenet ", ns.name, sizeof(ns.name));
            ns.set_active(false);
            want = &ns;
        }

        if (rq.close_scene) {
            bool const closing_current = (rq.close_scene == cur_);
            scenes.remove_if([&](Scene const& s) { return &s == rq.close_scene; });
            if (closing_current || want == rq.close_scene) {
                cur_ = &scenes.front();
                cur_->set_active(true);
            }
        } else if (want != cur_) {
            cur_->set_active(false);      // a részecskék állapota megmarad
            cur_ = want;
            cur_->set_active(true);
        }
    }
};

#endif //GORBE_APP_CONTROLLER_HPP
