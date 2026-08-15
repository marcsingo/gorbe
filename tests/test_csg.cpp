// Az uj halmazmuvelet-operatorok tesztje (BlobTree-cikk alapjan bevezetve).
//
// Ket egyseggomb:  A = origo kozeppontu,  B = (1,0,0) kozeppontu.
// Az F<0 = belul konvencioval: unio = min, metszet = max, kulonbseg = max(a,-b).
#include <cmath>
#include <cstdio>
#include <string>

#include "matek/Kif.hpp"

using namespace Matek::Analizis;

static int failures = 0;

static void check(std::string const& what, float got, float expected, float tol = 1e-3f) {
    bool ok = std::isfinite(got) && std::abs(got - expected) <= tol;
    if (!ok) ++failures;
    std::printf("  %-42s %-8s got=%11.5f exp=%11.5f\n",
                what.c_str(), ok ? "[OK]" : "[HIBA]", got, expected);
}

static void check_sign(std::string const& what, float got, bool expect_inside) {
    bool ok = std::isfinite(got) && ((got < 0.0f) == expect_inside);
    if (!ok) ++failures;
    std::printf("  %-42s %-8s F=%11.5f (%s, vart: %s)\n",
                what.c_str(), ok ? "[OK]" : "[HIBA]", got,
                got < 0 ? "belul" : "kivul", expect_inside ? "belul" : "kivul");
}

static void check_throws(std::string const& what, std::string const& formula) {
    bool threw = false;
    std::string msg;
    try { make_kif(formula); } catch (std::exception const& e) { threw = true; msg = e.what(); }
    if (!threw) ++failures;
    std::printf("  %-42s %-8s %s\n", what.c_str(), threw ? "[OK]" : "[HIBA]",
                threw ? "hibat dobott (helyes)" : "NEM dobott hibat");
}

// f1 / f2 nevfeloldassal
static Kif build(std::string const& formula) {
    static auto f1 = make_kif("x^2 + y^2 + z^2 - 1").get();
    static auto f2 = make_kif("(x-1)^2 + y^2 + z^2 - 1").get();
    return make_kif(formula, [](std::string const& nm) -> std::shared_ptr<Kifejezes const> {
        if (nm == "f1") return f1;
        if (nm == "f2") return f2;
        return nullptr;
    });
}

// A projekt Kifejezes.hpp-ja csak a vec3.hpp-t huzza be, glm::length nincs.
static float norm(glm::vec3 v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

static glm::vec3 grad(Kif const& f, glm::vec3 at) {
    return {f.derrive('x').at(at), f.derrive('y').at(at), f.derrive('z').at(at)};
}

// Numerikus (centralis differencia) gradiens az ellenorzeshez.
static glm::vec3 num_grad(Kif const& f, glm::vec3 at, float h = 1e-3f) {
    return {
        (f.at(at + glm::vec3{h, 0, 0}) - f.at(at - glm::vec3{h, 0, 0})) / (2 * h),
        (f.at(at + glm::vec3{0, h, 0}) - f.at(at - glm::vec3{0, h, 0})) / (2 * h),
        (f.at(at + glm::vec3{0, 0, h}) - f.at(at - glm::vec3{0, 0, h})) / (2 * h),
    };
}

int main() {
    glm::vec3 const csakA{-0.5f, 0, 0};   // csak A-ban
    glm::vec3 const csakB{ 1.5f, 0, 0};   // csak B-ben
    glm::vec3 const mindketto{0.5f, 0, 0};
    glm::vec3 const sehol{3.0f, 0, 0};

    std::printf("=== 1. Eles halmazmuveletek: ertek es bennlevoseg ===\n");
    check_sign("unio      @ csak A",      build("unio(f1, f2)").at(csakA),      true);
    check_sign("unio      @ csak B",      build("unio(f1, f2)").at(csakB),      true);
    check_sign("unio      @ mindketto",   build("unio(f1, f2)").at(mindketto),  true);
    check_sign("unio      @ sehol",       build("unio(f1, f2)").at(sehol),      false);
    check_sign("metszet   @ csak A",      build("metszet(f1, f2)").at(csakA),   false);
    check_sign("metszet   @ csak B",      build("metszet(f1, f2)").at(csakB),   false);
    check_sign("metszet   @ mindketto",   build("metszet(f1, f2)").at(mindketto), true);
    check_sign("kulonbseg @ csak A",      build("kulonbseg(f1, f2)").at(csakA), true);
    check_sign("kulonbseg @ mindketto",   build("kulonbseg(f1, f2)").at(mindketto), false);
    check_sign("kulonbseg @ csak B",      build("kulonbseg(f1, f2)").at(csakB), false);

    std::printf("\n  pontos ertekek:\n");
    check("unio(f1,f2) @ csak A",      build("unio(f1, f2)").at(csakA),      -0.75f);
    check("metszet(f1,f2) @ csak A",   build("metszet(f1, f2)").at(csakA),    1.25f);
    check("kulonbseg(f1,f2) @ mindketto", build("kulonbseg(f1, f2)").at(mindketto), 0.75f);
    check("unio(f1,f2) @ sehol",       build("unio(f1, f2)").at(sehol),       3.00f);

    std::printf("\n=== 2. Szimbolikus derivalt: a NYERTES ag gradiense ===\n");
    // csakA-ban f1 = -0.75 < f2 = 1.25, tehat min -> f1, max -> f2
    {
        auto g = grad(build("unio(f1, f2)"), csakA);
        check("d unio/dx @ csak A  (= d f1/dx = 2x)", g.x, -1.0f);
        check("d unio/dy @ csak A", g.y, 0.0f);
        auto gm = grad(build("metszet(f1, f2)"), csakA);
        check("d metszet/dx @ csak A (= d f2/dx = 2(x-1))", gm.x, -3.0f);
        auto gk = grad(build("kulonbseg(f1, f2)"), csakA);
        check("d kulonbseg/dx @ csak A (= d f1/dx)", gk.x, -1.0f);
    }

    std::printf("\n=== 3. Szimbolikus vs. numerikus gradiens ===\n");
    {
        char const* forms[] = {
            "unio(f1, f2)", "metszet(f1, f2)", "kulonbseg(f1, f2)",
            "sunio(f1, f2, 0.5)", "smetszet(f1, f2, 0.5)", "skulonbseg(f1, f2, 0.5)",
            "abs(f1)", "min(f1, f2)", "max(f1, f2)",
        };
        glm::vec3 pts[] = {csakA, csakB, sehol};
        for (auto* fs : forms) {
            Kif f = build(fs);
            for (auto p : pts) {
                auto gs = grad(f, p);
                auto gn = num_grad(f, p);
                float err = norm(gs - gn);
                bool ok = std::isfinite(err) && err < 5e-2f;
                if (!ok) ++failures;
                std::printf("  %-24s @ (%4.1f,%3.1f,%3.1f) %-8s |szimb-num|=%.2e\n",
                            fs, p.x, p.y, p.z, ok ? "[OK]" : "[HIBA]", err);
            }
        }
    }

    std::printf("\n=== 4. Masodrendu derivalt (a gorbulethez kell) ===\n");
    {
        Kif f = build("unio(f1, f2)");
        // csakA-ban f1 nyer, d2 f1/dx2 = 2
        check("d2 unio/dx2 @ csak A", f.derrive('x').derrive('x').at(csakA), 2.0f);
        float s = build("sunio(f1, f2, 0.5)").derrive('x').derrive('x').at(csakA);
        check("d2 sunio/dx2 @ csak A veges", std::isfinite(s) ? 1.0f : 0.0f, 1.0f);
    }

    std::printf("\n=== 5. Sima operatorok a varraton (f1 = f2 = 0) ===\n");
    {
        glm::vec3 seam{0.5f, std::sqrt(0.75f), 0.0f};
        check("f1 @ varrat", build("f1").at(seam), 0.0f);
        check("f2 @ varrat", build("f2").at(seam), 0.0f);
        // smin(a,a,k) = a - k/2
        check("sunio(f1,f2,0.5) @ varrat = -k/2", build("sunio(f1, f2, 0.5)").at(seam), -0.25f);
        check("smetszet(f1,f2,0.5) @ varrat = +k/2", build("smetszet(f1, f2, 0.5)").at(seam), 0.25f);
        auto g = grad(build("sunio(f1, f2, 0.5)"), seam);
        bool fin = std::isfinite(g.x) && std::isfinite(g.y) && std::isfinite(g.z);
        if (!fin) ++failures;
        std::printf("  %-42s %-8s grad=(%.4f,%.4f,%.4f)\n",
                    "sunio gradiens a varraton veges", fin ? "[OK]" : "[HIBA]", g.x, g.y, g.z);
    }

    std::printf("\n=== 6. Alapertelmezett k es aliasok ===\n");
    check("sunio(f1,f2) == sunio(f1,f2,0.5)",
          build("sunio(f1, f2)").at(csakA) - build("sunio(f1, f2, 0.5)").at(csakA), 0.0f);
    check("union == unio",
          build("union(f1, f2)").at(csakA) - build("unio(f1, f2)").at(csakA), 0.0f);
    check("intersect == metszet",
          build("intersect(f1, f2)").at(csakA) - build("metszet(f1, f2)").at(csakA), 0.0f);
    check("subtract == kulonbseg",
          build("subtract(f1, f2)").at(csakA) - build("kulonbseg(f1, f2)").at(csakA), 0.0f);
    check("ssubtract == skulonbseg",
          build("ssubtract(f1, f2)").at(csakA) - build("skulonbseg(f1, f2)").at(csakA), 0.0f);

    std::printf("\n=== 7. abs / sign ===\n");
    check("abs(-3)",       make_kif("abs(0 - 3)").at({0, 0, 0}), 3.0f);
    check("sign(-3)",      make_kif("sign(0 - 3)").at({0, 0, 0}), -1.0f);
    check("sign(0)",       make_kif("sign(0)").at({0, 0, 0}), 0.0f);
    check("d abs(x)/dx @ x=2",  make_kif("abs(x)").derrive('x').at({2, 0, 0}), 1.0f);
    check("d abs(x)/dx @ x=-2", make_kif("abs(x)").derrive('x').at({-2, 0, 0}), -1.0f);

    std::printf("\n=== 8. Hibas hivasok ===\n");
    check_throws("min(x) - keves argumentum",        "min(x)");
    check_throws("sin(x,y) - sok argumentum",        "sin(x, y)");
    check_throws("sunio(x) - keves argumentum",      "sunio(x)");
    check_throws("unio(x,y,z) - sok argumentum",     "unio(x, y, z)");
    check_throws("nemletezo(x) - ismeretlen fv",     "nemletezo(x)");
    check_throws("min(x, - hianyzo zarojel",         "min(x, y");

    std::printf("\n=== 9. Beagyazott CSG (3 szint) ===\n");
    {
        // (A U B) minusz egy z-tengelyu henger (sugara sqrt(0.04) = 0.2): kifurjuk.
        Kif f = build("kulonbseg(unio(f1, f2), x^2 + y^2 - 0.04)");
        // a furat BELSEJEBEN (tengelytol 0.1-re) mar KIVUL vagyunk az alakzaton,
        // a furaton kivul (tengelytol 0.5-re) viszont BELUL, mert ott A U B belseje van
        check_sign("(A U B) - henger @ furatban (r=0.1)", f.at({0.1f, 0.0f, 0.0f}), false);
        check_sign("(A U B) - henger @ furaton kivul",    f.at({0.5f, 0.5f, 0.0f}), true);
        auto gs = grad(f, {0.5f, 0.5f, 0.0f});
        auto gn = num_grad(f, {0.5f, 0.5f, 0.0f});
        float err = norm(gs - gn);
        bool ok = std::isfinite(err) && err < 5e-2f;
        if (!ok) ++failures;
        std::printf("  %-42s %-8s |szimb-num|=%.2e\n",
                    "beagyazott CSG gradiens", ok ? "[OK]" : "[HIBA]", err);
    }

    std::printf("\n%s (%d hiba)\n", failures ? ">>> SIKERTELEN" : ">>> MINDEN TESZT OK", failures);
    return failures != 0;
}
