// A jelenet-modell tiszta logikaja (scene/): a kepletek felepitese, a nevfeloldas
// sorrendje, a tartomanyok ES-kapcsolata es a nevellenorzes. Ez korabban a main.cpp
// GUI-lambdajaban elt, ezert nem volt tesztelheto.
#include <cmath>
#include <cstdio>
#include <string>

#include "scene/Build.hpp"
#include "scene/Presets.hpp"
#include "scene/Validate.hpp"

using Matek::Analizis::Kif;

static int failures = 0;
static void ok(std::string const& what, bool c, std::string const& info = "") {
    if (!c) ++failures;
    std::printf("  %-50s %-8s %s\n", what.c_str(), c ? "[OK]" : "[HIBA]", info.c_str());
}
static void near(std::string const& what, float got, float exp, float tol = 1e-4f) {
    bool c = std::isfinite(got) && std::abs(got - exp) <= tol;
    if (!c) ++failures;
    std::printf("  %-50s %-8s got=%9.5f exp=%9.5f\n", what.c_str(), c ? "[OK]" : "[HIBA]", got, exp);
}

static Param& param(std::list<Param>& l, char const* n, float v) {
    auto& p = l.emplace_back();
    std::snprintf(p.name, sizeof(p.name), "%s", n);
    p.value = v;
    return p;
}
static Shape& shape(SceneDoc& sc, char const* n, char const* f) {
    auto& s = sc.shapes.emplace_back();
    std::snprintf(s.name, sizeof(s.name), "%s", n);
    std::snprintf(s.formula, sizeof(s.formula), "%s", f);
    return s;
}
static float at(Shape const& s, float x, float y, float z) { return Kif(s.tree).at({x, y, z}); }

int main() {
    std::printf("=== 1. Hatokorok: a belso elfedi a kulsot ===\n");
    {
        std::list<Param> program;
        SceneDoc sc;
        param(program,   "r", 1.0f);
        param(sc.params, "r", 2.0f);
        auto& a = shape(sc, "a", "r");            // jelenet-szintu r
        auto& b = shape(sc, "b", "r");
        param(b.locals, "r", 3.0f);               // lokalis r
        auto& c = shape(sc, "c", "g");
        param(program, "g", 7.0f);                // csak program-szinten van

        ok("build sikeres", Build::build(sc, program).empty());
        near("a: jelenet elfedi a programot", at(a, 0, 0, 0), 2.0f);
        near("b: lokalis elfedi a jelenetet", at(b, 0, 0, 0), 3.0f);
        near("c: program-szint latszik",      at(c, 0, 0, 0), 7.0f);

        // A parameter CIM szerint epul be: ujraepites nelkul kovetjuk.
        sc.params.front().value = 5.0f;
        near("a: az ertek elo (cim szerint)", at(a, 0, 0, 0), 5.0f);
    }

    std::printf("\n=== 2. Hivatkozas masik alakzatra ===\n");
    {
        std::list<Param> program;
        SceneDoc sc;
        auto& g = shape(sc, "g", "x^2 + y^2 + z^2 - 1");
        g.xform.pos[0] = 3.0f;                    // elhelyezett gomb
        auto& u = shape(sc, "u", "g");
        ok("build sikeres", Build::build(sc, program).empty());
        near("a hivatkozo az ELHELYEZETT format latja", at(u, 3, 0, 0), -1.0f);
        ok("warped jelzo beallt", g.warped && !u.warped);

        SceneDoc hibas;
        auto& elore = shape(hibas, "e", "k");     // k csak KESOBB jon
        shape(hibas, "k", "x");
        std::string err = Build::build(hibas, program);
        ok("kesobbi alakzatra nem hivatkozhat", !err.empty(), err);
        ok("hiba utan nem marad felkesz fa", !elore.tree && !hibas.shapes.back().tree);
    }

    std::printf("\n=== 3. Tartomany: sajat ES munkater ===\n");
    {
        std::list<Param> program;
        SceneDoc sc;
        std::snprintf(sc.domain, sizeof(sc.domain), "x < 4");
        auto& s = shape(sc, "s", "z");
        std::snprintf(s.domain, sizeof(s.domain), "x > 1");
        auto& n = shape(sc, "n", "z");            // nincs sajat tartomany
        ok("build sikeres", Build::build(sc, program).empty());
        ok("mindketto kapott tartomanyt", s.dom_tree && n.dom_tree);
        ok("x=2: bent (sajat ES munkater)", Kif(s.dom_tree).at({2, 0, 0}) > 0.0f);
        ok("x=0: kint (sajat nem teljesul)", Kif(s.dom_tree).at({0, 0, 0}) < 0.0f);
        ok("x=5: kint (munkater nem teljesul)", Kif(s.dom_tree).at({5, 0, 0}) < 0.0f);
        ok("csak munkater: x=0 bent", Kif(n.dom_tree).at({0, 0, 0}) > 0.0f);

        SceneDoc lokalis;
        auto& l = shape(lokalis, "l", "z");
        param(l.locals, "q", 1.0f);
        std::snprintf(lokalis.domain, sizeof(lokalis.domain), "x < q");
        std::string err = Build::build(lokalis, program);
        ok("munkater nem lat lokalist", err.rfind("globalis tartomany", 0) == 0, err);
    }

    std::printf("\n=== 4. Nevellenorzes es elfedes-jelzes ===\n");
    {
        std::list<Param> program;
        SceneDoc sc;
        param(program, "k", 1.0f);
        auto& d1 = param(sc.params, "d", 1.0f);
        auto& d2 = param(sc.params, "d", 2.0f);
        auto& rossz = param(sc.params, "sin", 0.0f);
        auto& s = shape(sc, "f1", "x");
        auto& k = param(s.locals, "k", 2.0f);

        Problems pr = validate(program, sc);
        ok("ketszeres nev hiba", pr.is_bad(&d1) && pr.is_bad(&d2));
        ok("foglalt nev hiba", pr.is_bad(&rossz));
        ok("elfedes NEM hiba", !pr.is_bad(&k) && !pr.is_bad(&s));
        ok("2 uzenet (ismetles + foglalt)", pr.messages.size() == 2, std::to_string(pr.messages.size()));

        char const* sh = shadows("k", {&s.locals, &sc.params, &program}, sc.params, sc.shapes);
        ok("lokalis k elfedi a program k-t", sh && std::string(sh) == "elfedi: program");
        sh = shadows("k", {&sc.params, &program}, sc.params, sc.shapes);
        ok("jelenet-szintu k elfedi a program k-t", sh && std::string(sh) == "elfedi: program");
        sh = shadows("d", {&sc.params, &program}, sc.params, sc.shapes);
        ok("jelenet d: kint nincs d, nincs elfedes", sh == nullptr);
        sh = shadows("f1", {&program}, sc.params, sc.shapes);
        ok("parameter elfedi az azonos nevu alakzatot", sh && std::string(sh) == "elfedi: alakzat");
    }

    std::printf("\n=== 5. Sablonok ===\n");
    {
        SceneDoc sc;
        auto& a = add_preset(sc.shapes, PRESETS[0]);
        auto& b = add_preset(sc.shapes, PRESETS[0]);
        ok("szabad nevek", std::string(a.name) == "gomb1" && std::string(b.name) == "gomb2",
           std::string(a.name) + " " + b.name);
        ok("a lokalisok bemasolodtak", a.locals.size() == 1);
        add_warp(a, WarpPresets::ALL[0]);
        ok("warp hozzaadva", a.warps.size() == 1);
        ok("build sikeres", Build::build(sc, {}).empty());
        near("gomb1 felszinen", at(a, 1, 0, 0), 0.0f);
    }

    std::printf("\n%s (%d hiba)\n", failures ? "SIKERTELEN" : "MINDEN RENDBEN", failures);
    return failures ? 1 : 0;
}
