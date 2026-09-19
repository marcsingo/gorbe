#ifndef GORBE_APP_CONTROLLER_HPP
#define GORBE_APP_CONTROLLER_HPP

#include <cstdio>
#include <list>
#include <string>

#include "../scene/Build.hpp"
#include "../scene/Names.hpp"
#include "../ui/Requests.hpp"
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

    Controller() {
        auto& s0 = scenes.emplace_back();
        std::snprintf(s0.name, sizeof(s0.name), "Jelenet 1");
        cur_ = &s0;
        cur_->set_active(true);
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
    }

    // A jelenetek (bennük a Model-ek: VAO/VBO) felszabadítása MÉG élő GL-kontextussal.
    void shutdown() { scenes.clear(); cur_ = nullptr; }

private:
    Scene* cur_ = nullptr;

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
