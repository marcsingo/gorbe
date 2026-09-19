#ifndef GORBE_SCENE_BUILD_HPP
#define GORBE_SCENE_BUILD_HPP

#include <exception>
#include <list>
#include <memory>
#include <stdexcept>
#include <string>

#include "../matek/Kif.hpp"
#include "../matek/Parser.hpp"
#include "../particle_sampling/Transform.hpp"
#include "Document.hpp"
#include "Scope.hpp"
#include "Time.hpp"

// A képletek felépítése: szöveg -> elhelyezett kifejezésfa, alakzatonként.
//
// Maga a string <-> fa átalakítás a matek könyvtárban van (matek/Parser.hpp); itt
// csak a JELENET szabályai vannak: melyik név mit jelent (resolve), és visszafelé,
// melyik paraméternek mi a neve (namer). A vezérlő (app/Controller.hpp) a kész
// fákat adja tovább a sampler-poolnak.
namespace Build {

    using Matek::Analizis::Kif;
    using Matek::Analizis::make_kif;
    using Matek::Analizis::kif_and;
    using Matek::Analizis::FuncResolver;
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

    // A resolve INVERZE a kiíráshoz (Matek::Analizis::kif_text): paraméter-cím -> név,
    // az `owner` alakzat szemszögéből. Csak olyan nevet ad, ami ott VISSZA is erre a
    // címre oldódik fel. Ha a név el van fedve (pl. egy beépült korábbi alakzat a
    // jelenet `r`-jét használja, de az owner-nek saját `r`-je van), üreset ad — ilyenkor
    // a kiírás a pillanatnyi értéket írja, tehát az érték helyes, csak nem él tovább.
    inline Matek::Analizis::ParamNamer namer(SceneDoc const& sc, std::list<Param> const& program,
                                             Shape const& owner) {
        return [&sc, &program, &owner](float const* p) -> std::string {
            if (p == SceneTime::ptr()) return "t";
            for (auto const* lvl : {&owner.locals, &sc.params, &program})
                for (auto const& q : *lvl)
                    if (&q.value == p)
                        return Scope::find({&owner.locals, &sc.params, &program}, q.name) == p
                               ? q.name : "";
            return "";
        };
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

    // A jelenet saját függvényei a parsernek. A `upto` előtti függvényeket látja
    // (egy függvény törzse így csak a nála korábbiakat hívhatja — nincs rekurzió).
    // A törzs a jelenet munkaterével azonos neveket lát: jelenet- és program-szintű
    // paramétert és `t`-t; alakzat-lokálist nem, mert a függvény nem egy alakzaté.
    inline FuncResolver funcs(SceneDoc const& sc, std::list<Param> const& program,
                              UserFunc const* upto = nullptr) {
        return [&sc, &program, upto](std::string const& nm,
                                     std::vector<Kif> const& args) -> Tree {
            for (auto const& f : sc.funcs) {
                if (&f == upto) break;
                if (!f.name[0] || nm != f.name) continue;
                return Matek::Analizis::expand_user_function(
                    f.name, f.params, f.body, args,
                    [&](std::string const& n) { return resolve_global(sc, program, n); },
                    funcs(sc, program, &f)).get();
            }
            return nullptr;
        };
    }

    // Egy kifejezés "elhelyezése": előbb a warp-lánc (a lista sorrendjében, tehát az
    // első elem hat először az alakzatra), utána az affin transzformáció. Így a warpok
    // az alakzat SAJÁT terében dolgoznak, és a kész, deformált alakzatot mozgatja a
    // pozíció/forgatás/méret — ez az, amit egy modellezőtől elvárunk.
    inline Kif place(Kif f, Shape const& s, SceneDoc const& sc, std::list<Param> const& program) {
        auto r = [&](std::string const& nm) { return resolve(sc, program, s, nm); };
        for (auto const& w : s.warps) {
            if (!w.enabled) continue;
            auto fr = funcs(sc, program);
            f = apply_warp(f, make_kif(w.fx, r, fr), make_kif(w.fy, r, fr), make_kif(w.fz, r, fr));
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
        auto fr = funcs(sc, program);
        try {
            // A globális tartományt előbb ÖNMAGÁBAN is beparseoljuk: így az itteni hiba
            // nem egy véletlenszerű alakzat nevével jelenik meg, és egyben ellenőrizzük,
            // hogy tényleg csak globális paramétert használ.
            if (sc.domain[0]) {
                try {
                    make_kif(sc.domain, global, fr);
                } catch (std::exception const& e) {
                    throw std::runtime_error(std::string("globalis tartomany: ") + e.what());
                }
            }

            for (auto& s : sc.shapes) {
                try {
                    auto own = [&](std::string const& nm) { return resolve(sc, program, s, nm); };
                    s.tree = make_kif(s.formula, own, fr).get();

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
                        dom = place(make_kif(s.domain, own, fr), s, sc, program);
                        has_dom = true;
                    }
                    if (sc.domain[0]) {
                        Kif g = make_kif(sc.domain, global, fr);
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
