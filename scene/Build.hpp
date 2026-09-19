#ifndef GORBE_SCENE_BUILD_HPP
#define GORBE_SCENE_BUILD_HPP

#include <exception>
#include <list>
#include <memory>
#include <stdexcept>
#include <string>

#include "../matek/Kif.hpp"
#include "../particle_sampling/Transform.hpp"
#include "Document.hpp"
#include "Scope.hpp"
#include "Time.hpp"

// A képletek felépítése: szöveg -> elhelyezett kifejezésfa, alakzatonként.
//
// Tiszta modell-logika: nem tud a mintavételezőkről. A vezérlő (app/Controller.hpp)
// a kész fákat adja tovább a sampler-poolnak.
namespace Build {

    using Matek::Analizis::Kif;
    using Matek::Analizis::make_kif;
    using Matek::Analizis::kif_and;
    using Tree = std::shared_ptr<Matek::Analizis::Kifejezes const>;

    // Névfeloldó a parsernek. HÁROM HATÓKÖR, kifelé haladva; az első találat nyer,
    // tehát a belső ELFEDI a külsőt (mint C++-ban):
    //     alakzat lokálisai -> jelenet paraméterei -> program-szintűek
    // A végén a NÁLA KORÁBBI alakzatok neve (így lehet őket egymásból építeni).
    inline Tree resolve(SceneDoc const& sc, std::list<Param> const& program,
                        Shape const& owner, std::string const& nm) {
        // A `t` (idő) beépített: foglalt név, tehát paraméterként nem vehető fel,
        // viszont bármelyik képletben használható.
        if (nm == "t") return Kif(SceneTime::ptr()).get();
        if (float const* v = Scope::find({&owner.locals, &sc.params, &program}, nm.c_str()))
            return Kif(v).get();
        for (auto const& s : sc.shapes) {
            if (&s == &owner) break;               // csak a nála korábbiakra hivatkozhat
            if (s.name[0] && nm == s.name && s.tree) return s.tree;
        }
        return nullptr;                            // ismeretlen név -> a parser hibát dob
    }

    // A jelenet munkatere alakzat-lokálist nem láthat (nincs "saját" alakzata),
    // de a jelenet- és program-szintű paramétereket igen.
    inline Tree resolve_global(SceneDoc const& sc, std::list<Param> const& program,
                               std::string const& nm) {
        if (nm == "t") return Kif(SceneTime::ptr()).get();
        if (float const* v = Scope::find({&sc.params, &program}, nm.c_str()))
            return Kif(v).get();
        return nullptr;
    }

    // Egy kifejezés "elhelyezése": előbb a warp-lánc (a lista sorrendjében, tehát az
    // első elem hat először az alakzatra), utána az affin transzformáció. Így a warpok
    // az alakzat SAJÁT terében dolgoznak, és a kész, deformált alakzatot mozgatja a
    // pozíció/forgatás/méret — ez az, amit egy modellezőtől elvárunk.
    inline Kif place(Kif f, Shape const& s, SceneDoc const& sc, std::list<Param> const& program) {
        auto r = [&](std::string const& nm) { return resolve(sc, program, s, nm); };
        for (auto const& w : s.warps) {
            if (!w.enabled) continue;
            f = apply_warp(f, make_kif(w.fx, r), make_kif(w.fy, r), make_kif(w.fz, r));
        }
        return apply_transform(f, s.xform);
    }

    inline void drop_trees(SceneDoc& sc) {
        for (auto& s : sc.shapes) { s.tree.reset(); s.dom_tree.reset(); }
    }

    // Az összes alakzat beparseolása (a lista sorrendjében, hogy a későbbiek
    // hivatkozhassanak a korábbiak már kész fájára). Üres szöveg = siker; hibánál
    // az üzenet jön vissza, és MINDEN fa eldobódik (félkész jelenet nem marad).
    inline std::string build(SceneDoc& sc, std::list<Param> const& program) {
        auto global = [&](std::string const& nm) { return resolve_global(sc, program, nm); };
        try {
            // A globális tartományt előbb ÖNMAGÁBAN is beparseoljuk: így az itteni hiba
            // nem egy véletlenszerű alakzat nevével jelenik meg, és egyben ellenőrizzük,
            // hogy tényleg csak globális paramétert használ.
            if (sc.domain[0]) {
                try {
                    make_kif(sc.domain, global);
                } catch (std::exception const& e) {
                    throw std::runtime_error(std::string("globalis tartomany: ") + e.what());
                }
            }

            for (auto& s : sc.shapes) {
                try {
                    auto own = [&](std::string const& nm) { return resolve(sc, program, s, nm); };
                    s.tree = make_kif(s.formula, own).get();

                    // Tér-transzformáció: az alakzat saját képletén ÉS a saját
                    // tartományán is alkalmazzuk (a "véges hosszú henger" végei
                    // együtt mozogjanak a hengerrel), a GLOBÁLIS tartományon viszont
                    // NEM — az a világ munkatere, nem az alakzaté.
                    //
                    // A transzformált alakot tesszük vissza s.tree-be, hogy a rá
                    // HIVATKOZÓ későbbi alakzatok is a már elhelyezett formát lássák
                    // (két elhelyezett gömb uniója a helyükön legyen).
                    s.warped = !s.xform.is_identity();
                    s.tree = place(Kif(s.tree), s, sc, program).get();

                    // Tartomány = GLOBÁLIS és a (transzformált) SAJÁT feltétel ÉS-kapcsolata.
                    Kif dom;
                    bool has_dom = false;
                    if (s.domain[0]) {
                        // A tartomány UGYANAZT a warp-láncot és transzformációt kapja,
                        // mint a képlet — így a levágott rész együtt mozog/deformálódik
                        // az alakzattal.
                        dom = place(make_kif(s.domain, own), s, sc, program);
                        has_dom = true;
                    }
                    if (sc.domain[0]) {
                        Kif g = make_kif(sc.domain, global);
                        dom = has_dom ? kif_and(g, dom) : g;   // ÉS = min
                        has_dom = true;
                    }
                    s.dom_tree = has_dom ? dom.get() : nullptr;
                } catch (std::exception const& e) {
                    throw std::runtime_error(std::string(s.name[0] ? s.name : "(nevtelen)")
                                             + ": " + e.what());
                }
            }
        } catch (std::exception const& e) {
            drop_trees(sc);
            return e.what();
        }
        return {};
    }

}

#endif //GORBE_SCENE_BUILD_HPP
