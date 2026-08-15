// A tartomany-kenyszer tesztje. Ugyanazt a kodot hivja, amit az ImplicitSurface
// (particle_sampling/DomainConstraint.hpp), es valodi idolepteteses szimulaciot fut.
//
// A ket kritikus allitas:
//   1. a rossz terreszbol a reszecske BECSUSZIK a joba, es KOZBEN A FELULETEN MARAD
//   2. a peremen NEM lep at, meg akkor sem, ha a taszitas kifele nyomja
#include <cmath>
#include <cstdio>
#include <string>

#include "matek/Kif.hpp"
#include "particle_sampling/DomainConstraint.hpp"

using namespace Matek::Analizis;

static int failures = 0;
static float norm(glm::vec3 v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

static void ok(std::string const& what, bool cond, std::string const& info = "") {
    if (!cond) ++failures;
    std::printf("  %-46s %-8s %s\n", what.c_str(), cond ? "[OK]" : "[HIBA]", info.c_str());
}
static void near(std::string const& what, float got, float exp, float tol) {
    bool c = std::isfinite(got) && std::abs(got - exp) <= tol;
    if (!c) ++failures;
    std::printf("  %-46s %-8s got=%.5f exp=%.5f\n", what.c_str(), c ? "[OK]" : "[HIBA]", got, exp);
}

struct Field {
    Kif f, fx, fy, fz;
    explicit Field(std::string const& s) : f(make_kif(s)) {
        fx = f.derrive('x'); fy = f.derrive('y'); fz = f.derrive('z');
    }
    float     val(glm::vec3 p) const { return f.at(p); }
    glm::vec3 grad(glm::vec3 p) const { return {fx.at(p), fy.at(p), fz.at(p)}; }
};

// Egy reszecske idoleptetese: felulet-visszacsatolas (PHI*F) + tartomany-kenyszer.
// Ez az ImplicitSurface::witkin() vaza, taszitas nelkul (P = kulso sebesseg).
struct Sim {
    Field F, D;
    float sigma = 0.5f, dt = 0.03f, PHI = 15.0f;
    Sim(std::string const& f, std::string const& d) : F(f), D(d) {}

    glm::vec3 step(glm::vec3 p, glm::vec3 P = glm::vec3{0}) {
        glm::vec3 Fx = F.grad(p);
        float fx2 = glm::dot(Fx, Fx);
        glm::vec3 p_dot = (fx2 > 1e-12f)
            ? P - ((glm::dot(Fx, P) + PHI * F.val(p)) / fx2) * Fx
            : glm::vec3{0};

        float     dom   = D.val(p);
        glm::vec3 dom_x = D.grad(p);
        glm::vec3 g     = Domain::tangential_gradient(dom_x, Fx);
        float     dist  = Domain::distance(dom, g);
        p_dot = Domain::constrain(p_dot, dom_x, g, dist, sigma, dt, 6.0f, 0.25f);
        return p + p_dot * dt;
    }
};

int main() {
    std::printf("=== 1. Parser: feltetelek lekepezese ===\n");
    near("x > 2  @ x=5   (-> x-2)",      make_kif("x > 2").at({5, 0, 0}),  3.0f, 1e-5f);
    near("x > 2  @ x=0",                make_kif("x > 2").at({0, 0, 0}), -2.0f, 1e-5f);
    near("x < 2  @ x=0   (-> 2-x)",     make_kif("x < 2").at({0, 0, 0}),  2.0f, 1e-5f);
    near("x>2 and x<6 @ x=4 (-> min)",  make_kif("x > 2 and x < 6").at({4, 0, 0}), 2.0f, 1e-5f);
    near("x>2 and x<6 @ x=7",           make_kif("x > 2 and x < 6").at({7, 0, 0}), -1.0f, 1e-5f);
    near("x>2 or x<-2  @ x=0 (-> max)", make_kif("x > 2 or x < 0 - 2").at({0, 0, 0}), -2.0f, 1e-5f);
    near("not (x > 2)  @ x=5",          make_kif("not (x > 2)").at({5, 0, 0}), -3.0f, 1e-5f);
    near("&& alias",                    make_kif("x > 2 && x < 6").at({4, 0, 0}), 2.0f, 1e-5f);
    near("precedencia: x+1 > 2 @ x=3",  make_kif("x + 1 > 2").at({3, 0, 0}), 2.0f, 1e-5f);
    // "or"-ral kezdodo azonosito ne toresse el a kulcsszo-felismerest
    {
        auto sub = make_kif("x").get();
        Kif k = make_kif("orso + 1", [&](std::string const& n) {
            return n == "orso" ? sub : nullptr; });
        near("'orso' nem 'or'+'so'", k.at({4, 0, 0}), 5.0f, 1e-5f);
    }

    std::printf("\n=== 2. Sik z=0, tartomany x>2: becsuszas a felulet menten ===\n");
    {
        Sim sim("z", "x > 2");
        glm::vec3 p{-3.0f, 0.0f, 0.0f};      // a feluleten, de rossz terreszben
        float max_z = 0.0f;
        int   steps = 0;
        for (; steps < 2000; ++steps) {
            p = sim.step(p);
            max_z = std::max(max_z, std::abs(p.z));
            if (p.x > 2.0f) break;
        }
        ok("becsuszott a jo terreszbe", p.x > 2.0f - 1e-3f,
           "x=" + std::to_string(p.x) + " " + std::to_string(steps) + " lepesben");
        ok("KOZBEN vegig a feluleten maradt", max_z < 1e-4f,
           "max |z| = " + std::to_string(max_z));
    }

    std::printf("\n=== 3. A peremen nem lep at, ha kifele nyomjuk ===\n");
    {
        Sim sim("z", "x > 2");
        glm::vec3 p{4.0f, 0.0f, 0.0f};
        // eros, allando kifele (-x) mutato "taszitas"
        float min_x = 1e30f;
        for (int k = 0; k < 3000; ++k) {
            p = sim.step(p, glm::vec3{-3.0f, 0.0f, 0.0f});
            min_x = std::min(min_x, p.x);
        }
        ok("nem ment at a rossz terreszbe", min_x > 2.0f - 1e-3f,
           "min x = " + std::to_string(min_x));
        ok("a peremen allt meg", std::abs(p.x - 2.0f) < 0.05f,
           "x = " + std::to_string(p.x));
        ok("a feluleten maradt", std::abs(p.z) < 1e-4f);
    }

    std::printf("\n=== 4. Gorbult felulet: gomb, tartomany z>0 ===\n");
    {
        // egyseggomb; a reszecske a deli poluson indul, a jo terresz az eszaki felgomb
        Sim sim("x^2 + y^2 + z^2 - 1", "z > 0");
        glm::vec3 p{0.2f, 0.0f, -0.9797959f};   // kozel a deli polushoz, a gombon
        float max_off = 0.0f;
        int steps = 0;
        for (; steps < 4000; ++steps) {
            p = sim.step(p);
            max_off = std::max(max_off, std::abs(norm(p) - 1.0f));  // eltéres a gombtol
            if (p.z > 0.0f) break;
        }
        ok("felcsuszott az eszaki felgombre", p.z > -1e-3f,
           "z=" + std::to_string(p.z) + " " + std::to_string(steps) + " lepesben");
        // A csuszas ERINTO iranyu, gorbult feluleten tehat masodrendben kivisz:
        // (v*dt)^2 / 2R nagysagrendu tranziens eltéres, amit a felulet-visszacsatolas
        // kesleltetve hoz vissza. 0.25*sigma lepesnel R=1-en ez ~0.8%/lepes, a
        // felhalmozodassal ~2%. Ez NEM latszik: amig a reszecske kint van, nem rajzoljuk.
        ok("a tranziens eltéres korlatos", max_off < 2.5e-2f,
           "max ||p|-1| = " + std::to_string(max_off));
        // A lenyeg: megerkezes utan (mar nem csuszik) visszaall a feluletre.
        for (int k = 0; k < 200; ++k) p = sim.step(p);
        ok("megerkezes utan PONTOSAN a gombon van", std::abs(norm(p) - 1.0f) < 1e-4f,
           "||p|-1| = " + std::to_string(std::abs(norm(p) - 1.0f)));
        ok("es a jo terreszben maradt", p.z > -1e-3f, "z = " + std::to_string(p.z));
    }

    std::printf("\n=== 5. Teglalap-tartomany: mind a negy oldal tart ===\n");
    {
        Sim sim("z", "x > 0 - 2 and x < 2 and y > 0 - 2 and y < 2");
        glm::vec3 dirs[] = {{3,0,0}, {-3,0,0}, {0,3,0}, {0,-3,0}};
        for (auto d : dirs) {
            glm::vec3 p{0.0f, 0.0f, 0.0f};
            for (int k = 0; k < 2000; ++k) p = sim.step(p, d);
            bool inside = p.x > -2.05f && p.x < 2.05f && p.y > -2.05f && p.y < 2.05f;
            ok("bent maradt (" + std::to_string((int)d.x) + "," + std::to_string((int)d.y) + ") iranyu nyomasnal",
               inside, "p=(" + std::to_string(p.x) + "," + std::to_string(p.y) + ")");
        }
    }

    std::printf("\n=== 6. Skalafuggetlenseg: 100*x > 200 ugyanaz, mint x > 2 ===\n");
    {
        Sim a("z", "x > 2"), b("z", "100*x > 200");
        glm::vec3 pa{-3, 0, 0}, pb{-3, 0, 0};
        for (int k = 0; k < 300; ++k) { pa = a.step(pa); pb = b.step(pb); }
        near("azonos palya (a geometriai tavolsag miatt)", pb.x, pa.x, 1e-3f);
    }

    std::printf("\n=== 7. is_outside turese (perem-villogas ellen) ===\n");
    ok("pont a peremen NEM szamit kivulinek", !Domain::is_outside(0.0f, 0.5f));
    ok("kicsit kint (turesen belul) sem",     !Domain::is_outside(-0.1f, 0.5f));
    ok("erdemben kint igen",                   Domain::is_outside(-0.5f, 0.5f));

    std::printf("\n=== 8. Globalis + lokalis tartomany OSSZEKAPCSOLASA ===\n");
    {
        // Ezt a format allitja elo a main.cpp:  "(globalis) and (lokalis)"
        char const* GLOB = "x > 0 - 8 and x < 8 and y > 0 - 8 and y < 8 and z > 0 - 8 and z < 8";
        char const* LOC  = "z > 0";
        std::string combined = std::string("(") + GLOB + ") and (" + LOC + ")";
        Kif c = make_kif(combined);

        // igaz = pozitiv;  a ket feltetel ES-kapcsolata
        ok("mindketto teljesul -> bent",      c.at({1.0f, 1.0f, 1.0f}) > 0.0f,  "p=(1,1,1)");
        ok("csak a globalis (z<0) -> kint",   c.at({1.0f, 1.0f, -1.0f}) < 0.0f, "p=(1,1,-1)");
        ok("csak a lokalis (x>8) -> kint",    c.at({20.0f, 0.0f, 1.0f}) < 0.0f, "p=(20,0,1)");
        ok("egyik sem -> kint",               c.at({20.0f, 0.0f, -1.0f}) < 0.0f, "p=(20,0,-1)");
        // az ES a szigorubb feltetel erteket viszi tovabb (min)
        near("min-szemantika: a szukebb dont", c.at({7.0f, 0.0f, 0.5f}), 0.5f, 1e-5f);

        // gomb alaku munkater (a masik gyorsgomb)
        Kif g = make_kif("x^2 + y^2 + z^2 < 64");
        ok("gomb-munkater: origo bent",  g.at({0, 0, 0}) > 0.0f);
        ok("gomb-munkater: r=10 kint",   g.at({10, 0, 0}) < 0.0f);
        near("gomb-munkater erteke r=6-nal", g.at({6, 0, 0}), 28.0f, 1e-4f);
    }

    std::printf("\n=== 9. Vegtelen sik globalis munkaterben ===\n");
    {
        // F = z (vegtelen sik), tartomany = csak a globalis doboz |x|,|y| < 8
        Sim sim("z", "(x > 0 - 8 and x < 8 and y > 0 - 8 and y < 8) and (z > 0 - 8 and z < 8)");
        glm::vec3 p{-20.0f, 0.0f, 0.0f};      // a feluleten, de a munkateren kivul
        float max_z = 0.0f;
        for (int k = 0; k < 3000; ++k) { p = sim.step(p); max_z = std::max(max_z, std::abs(p.z)); }
        ok("becsuszott a munkaterbe", p.x > -8.05f && p.x < 8.05f,
           "x = " + std::to_string(p.x));
        ok("kozben a feluleten maradt", max_z < 1e-4f, "max |z| = " + std::to_string(max_z));

        // kifele nyomva sem lep ki a dobozbol
        for (int k = 0; k < 2000; ++k) p = sim.step(p, glm::vec3{-4.0f, 0.0f, 0.0f});
        ok("a doboz fala tart", p.x > -8.05f, "x = " + std::to_string(p.x));
    }

    std::printf("\n%s (%d hiba)\n", failures ? ">>> SIKERTELEN" : ">>> MINDEN TESZT OK", failures);
    return failures != 0;
}
