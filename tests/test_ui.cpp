// A UI-reteg TISZTA logikaja: a 3D nezet koordinata-atvaltasa es a harom szintu
// hatokor-feloldas. Mindketto csendben romlik el (elcsuszo eger, rossz nevet lato
// keplet), ezert szamszerusitve van.
#include <cmath>
#include <cstdio>
#include <list>
#include <string>
#include <vector>

#include "model/Viewport.hpp"
#include "scene/Scope.hpp"
#include "scene/Time.hpp"
#include "matek/Kif.hpp"

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

static Param mk(char const* n, float v) {
    Param p; std::snprintf(p.name, sizeof(p.name), "%s", n); p.value = v; return p;
}

int main() {
    std::printf("=== 1. Viewport -> NDC (a nezet EGY PANEL belsejeben van) ===\n");
    {
        // A nezet nem a bal felso sarokban kezdodik es nem tolti ki az ablakot:
        // pont ezert kell a teglalapra szamolni, nem az ablakmeretre.
        Vp::Rect r{300.0f, 100.0f, 800.0f, 400.0f};

        glm::vec2 c = Vp::to_ndc(r, 300.0 + 400.0, 100.0 + 200.0);   // pontosan kozepen
        near("kozep x", c.x, 0.0f);
        near("kozep y", c.y, 0.0f);

        glm::vec2 tl = Vp::to_ndc(r, 300.0, 100.0);                  // bal FELSO sarok
        near("bal felso x = -1", tl.x, -1.0f);
        near("bal felso y = +1 (y megfordul)", tl.y, 1.0f);

        glm::vec2 br = Vp::to_ndc(r, 1100.0, 500.0);                 // jobb ALSO sarok
        near("jobb also x = +1", br.x, 1.0f);
        near("jobb also y = -1", br.y, -1.0f);

        // A REGI (hibas) szamitas az ablakmeretbol indult volna: ha a nezet el van
        // tolva, a kozeppontja NEM a kepernyo kozepe -> elcsuszna az egerelkapas.
        Vp::Rect whole{0.0f, 0.0f, 1400.0f, 700.0f};
        glm::vec2 wrong = Vp::to_ndc(whole, 300.0 + 400.0, 100.0 + 200.0);
        ok("az ablakra szamolt NDC ELTER a viewportra szamolttol",
           std::abs(wrong.x - c.x) > 0.01f || std::abs(wrong.y - c.y) > 0.01f,
           "ablakra=(" + std::to_string(wrong.x) + "," + std::to_string(wrong.y) + ")");
    }

    std::printf("\n=== 2. Kepararany es tartalmazas ===\n");
    {
        near("2:1 arany", Vp::aspect(Vp::Rect{0, 0, 800, 400}), 2.0f);
        near("negyzet",   Vp::aspect(Vp::Rect{0, 0, 500, 500}), 1.0f);
        near("nulla magassag nem oszt nullaval", Vp::aspect(Vp::Rect{0, 0, 800, 0}), 1.0f);

        Vp::Rect r{300, 100, 800, 400};
        ok("bent van",              Vp::contains(r, 700, 300));
        ok("bal szel bent",         Vp::contains(r, 300, 100));
        ok("jobb szel mar kint",   !Vp::contains(r, 1100, 300));
        ok("felette kint",         !Vp::contains(r, 700, 99));
        ok("alatta kint",          !Vp::contains(r, 700, 501));
    }

    std::printf("\n=== 3. Harom szintu hatokor: a BELSO elfedi a kulsot ===\n");
    {
        std::list<Param> program{mk("r", 1.0f), mk("kozos", 100.0f)};
        std::list<Param> scene  {mk("r", 2.0f), mk("csak_jelenet", 50.0f)};
        std::list<Param> local  {mk("r", 3.0f)};

        std::vector<std::list<Param> const*> all{&local, &scene, &program};

        float const* v = Scope::find(all, "r");
        ok("a nev feloldodik", v != nullptr);
        if (v) near("az ALAKZAT 'r'-je nyer (3, nem 2 vagy 1)", *v, 3.0f);

        // Ha az alakzatban nincs, a jelenete jon.
        std::vector<std::list<Param> const*> no_local{&scene, &program};
        v = Scope::find(no_local, "r");
        if (v) near("alakzat nelkul a JELENET 'r'-je (2)", *v, 2.0f);

        // Ha a jelenetben sincs, a program-szintu.
        std::vector<std::list<Param> const*> only_prog{&program};
        v = Scope::find(only_prog, "r");
        if (v) near("csak program-szinttel az 1", *v, 1.0f);

        // Kulso szintrol is latszik, ha belul nincs elfedve.
        v = Scope::find(all, "kozos");
        if (v) near("program-szintu 'kozos' latszik belulrol is", *v, 100.0f);
        v = Scope::find(all, "csak_jelenet");
        if (v) near("jelenet-szintu 'csak_jelenet'", *v, 50.0f);

        ok("ismeretlen nev -> nullptr", Scope::find(all, "nincs_ilyen") == nullptr);
        ok("ures nev -> nullptr",       Scope::find(all, "") == nullptr);

        near("level_of: 'r' a legbelso (0)",          static_cast<float>(Scope::level_of(all, "r")), 0.0f);
        near("level_of: 'csak_jelenet' az 1.",        static_cast<float>(Scope::level_of(all, "csak_jelenet")), 1.0f);
        near("level_of: 'kozos' a 2.",                static_cast<float>(Scope::level_of(all, "kozos")), 2.0f);
        near("level_of: ismeretlen -> -1",            static_cast<float>(Scope::level_of(all, "x")), -1.0f);
    }

    std::printf("\n=== 4. Elfedes-jelzes (nem hiba, csak jelezni kell) ===\n");
    {
        std::list<Param> program{mk("r", 1.0f)};
        std::list<Param> scene  {mk("r", 2.0f), mk("h", 5.0f)};
        std::list<Param> local  {mk("r", 3.0f), mk("egyedi", 9.0f)};
        std::vector<std::list<Param> const*> all{&local, &scene, &program};

        near("az alakzat 'r'-je elfedi a jelenetet (1. szint)",
             static_cast<float>(Scope::shadowed(all, "r", 0)), 1.0f);
        near("a jelenet 'r'-je elfedi a programot (2. szint)",
             static_cast<float>(Scope::shadowed(all, "r", 1)), 2.0f);
        near("az 'egyedi' nem fed el semmit",
             static_cast<float>(Scope::shadowed(all, "egyedi", 0)), -1.0f);
        near("a 'h' (jelenet) nem fed el program-szintut",
             static_cast<float>(Scope::shadowed(all, "h", 1)), -1.0f);
    }

    std::printf("\n=== 5. Az elfedes az ERTEKET is valtoztatja (nem csak jelzes) ===\n");
    {
        // Konkret pelda: ket jelenet ugyanazzal a program-szintu 'r'-rel, de az
        // egyikben van jelenet-szintu felulbiralat.
        std::list<Param> program{mk("r", 1.0f)};
        std::list<Param> scene_a{};                  // nincs sajat 'r'
        std::list<Param> scene_b{mk("r", 7.0f)};     // van

        float const* a = Scope::find({&scene_a, &program}, "r");
        float const* b = Scope::find({&scene_b, &program}, "r");
        ok("mindketto feloldodik", a && b);
        if (a && b) {
            near("A jelenet a program-szintut latja", *a, 1.0f);
            near("B jelenet a sajatjat latja",        *b, 7.0f);
            ok("tehat a ket jelenet MAST lat ugyanarra a nevre", *a != *b);
        }
    }

    std::printf("\n=== 6. A beepitett `t` (ido) ===\n");
    {
        using namespace Matek::Analizis;
        // Ugy oldjuk fel, ahogy a program: a `t` a SceneTime CIMERE mutat.
        auto res = [](std::string const& nm) -> std::shared_ptr<Kifejezes const> {
            if (nm == "t") return Kif(SceneTime::ptr()).get();
            return nullptr;
        };

        // Mozgo gomb: a kozeppontja x = t menten halad.
        Kif f = make_kif("(x - t)^2 + y^2 + z^2 - 1", res);

        SceneTime::value = 0.0f;
        near("t=0: a kozeppont az origoban", f.at({0, 0, 0}), -1.0f);
        near("t=0: (1,0,0) a feluleten",     f.at({1, 0, 0}),  0.0f);

        SceneTime::value = 5.0f;                 // CSAK az erteket irjuk at
        near("t=5: a kozeppont x=5-nel",     f.at({5, 0, 0}), -1.0f);
        near("t=5: (6,0,0) a feluleten",     f.at({6, 0, 0}),  0.0f);
        ok("tehat ujraparseolas nelkul kovet", true);

        // dF/dt: ezt a tagot hasznalja a szimulacio, hogy a reszecske EGYUTT
        // mozogjon a felulettel. F = (x-t)^2 + ... -> dF/dt = -2(x-t)
        Kif ft = f.derrive(SceneTime::ptr());
        SceneTime::value = 2.0f;
        near("dF/dt a (5,0,0) pontban = -2(5-2)", ft.at({5, 0, 0}), -6.0f);
        near("dF/dt a kozeppontban = 0",          ft.at({2, 0, 0}),  0.0f);

        // Numerikus ellenorzes: (F(t+h) - F(t-h)) / 2h
        float const h = 1e-3f;
        glm::vec3 q{4.0f, 0.5f, -0.25f};
        SceneTime::value = 2.0f + h; float fp = f.at(q);
        SceneTime::value = 2.0f - h; float fm = f.at(q);
        SceneTime::value = 2.0f;
        near("dF/dt szimbolikus == numerikus", ft.at(q), (fp - fm) / (2.0f * h), 1e-2f);

        // Ha a keplet NEM fugg t-tol, a derivalt azonosan 0 (tehat ingyen van).
        Kif still = make_kif("x^2 + y^2 + z^2 - 4", res);
        Kif still_t = still.derrive(SceneTime::ptr());
        SceneTime::value = 123.0f;
        near("t-fuggetlen alakzat: dF/dt = 0", still_t.at({1, 2, 3}), 0.0f);
        near("t-fuggetlen alakzat erteke sem valtozik", still.at({2, 0, 0}), 0.0f);

        SceneTime::value = 0.0f;   // ne szivarogjon at masik tesztre
    }

    std::printf("\n%s (%d hiba)\n", failures ? ">>> SIKERTELEN" : ">>> MINDEN TESZT OK", failures);
    return failures != 0;
}
