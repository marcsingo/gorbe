// A reszecske-szimulacio (ParticleSystem) — GL es ablak NELKUL. Eddig ez csak egy
// GL-kontextusos teszttel volt futtathato, mert a reszecskek egy GL-modellben eltek.
//
// Nem pontos szamokat ellenoriz (a szimulacio veletlen kezdoponttal indul), hanem
// azt, ami minden futasnal igaz kell legyen: a reszecskek a feluletre kerulnek,
// szetterulnek, a tartomanyon belul maradnak, es a plafon felett nem szaporodnak.
#include <cmath>
#include <cstdio>
#include <string>

#include <atomic>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#include "utils/Parallel.hpp"
#include "particle_sampling/ParticleSystem.hpp"

static int failures = 0;
static void ok(std::string const& what, bool c, std::string const& info = "") {
    if (!c) ++failures;
    std::printf("  %-50s %-8s %s\n", what.c_str(), c ? "[OK]" : "[HIBA]", info.c_str());
}

// Ennyi szimulacios idot futtatunk, a program lepeskozevel.
static void run(ParticleSystem& ps, float seconds) {
    for (float t = 0.0f; t < seconds; t += 0.03f) ps.step(0.03f);
}

// A felulettol mert geometriai tavolsag (|F| / |grad F|) a reszecskek hanyadanal kicsi.
static float on_surface_ratio(ParticleSystem const& ps, float tol) {
    int good = 0;
    for (auto const& p : ps.particles()) {
        glm::vec3 g = ps.surface().grad(p.p);
        float dist = std::abs(ps.surface().F.at(p.p)) / std::max(glm::length(g), 1e-6f);
        if (dist < tol) ++good;
    }
    return ps.particles().empty() ? 0.0f : float(good) / float(ps.particles().size());
}

int main() {
    std::printf("=== 1. Gomb: a reszecskek a feluletre kerulnek es szetterulnek ===\n");
    {
        ParticleSystem ps;
        ps.d = 2.0f;
        ps.surface().set_tree(Matek::Analizis::make_kif("x^2 + y^2 + z^2 - 4").get());
        ps.restart();
        ok("indulaskor 8 reszecske", ps.particles().size() == 8);
        run(ps, 20.0f);
        std::size_t n = ps.particles().size();
        ok("a fisszio felepitette a mintavetelt", n > 30, std::to_string(n) + " db");
        float r = on_surface_ratio(ps, 0.05f);
        ok("tobbseguk a feluleten (tav < 0.05)", r > 0.9f, std::to_string(r));

        // Szetterules: a tomegkozeppont kozel az origohoz (nem egy csomoban vannak).
        glm::vec3 c{0};
        for (auto const& p : ps.particles()) c += p.p;
        c /= float(n);
        ok("egyenletesen korbeveszik (tomegkozeppont ~ 0)", glm::length(c) < 0.5f,
           std::to_string(glm::length(c)));

        ps.clear();
        ok("clear() utan ures", ps.particles().empty());
    }

    std::printf("\n=== 2. Tartomany: sik, csak x > 0 fele ===\n");
    {
        ParticleSystem ps;
        ps.d = 1.5f;
        ps.surface().set_tree(Matek::Analizis::make_kif("z").get());
        ps.surface().set_domain(Matek::Analizis::make_kif("x > 0 and x < 3 and y > -3 and y < 3").get());
        ps.restart();
        run(ps, 15.0f);
        int outside = 0, shown = 0;
        for (auto const& p : ps.particles()) {
            if (Domain::is_outside(p.dom_dist, p.sigma)) continue;   // a nezet sem rajzolja
            ++shown;
            if (p.p.x < -0.3f) ++outside;
        }
        ok("van latszo reszecske", shown > 10, std::to_string(shown) + " db");
        ok("a latszok a tartomanyban (x > -0.3)", outside == 0, std::to_string(outside) + " kint");
    }

    std::printf("\n=== 3. Reszecske-plafon: vegtelen sik sem fagyaszt le ===\n");
    {
        ParticleSystem ps;
        ps.d = 0.5f;
        ps.max_particles = 300;
        ps.surface().set_tree(Matek::Analizis::make_kif("z").get());
        ps.restart();
        run(ps, 20.0f);
        ok("legfeljebb a plafon", static_cast<int>(ps.particles().size()) <= ps.max_particles,
           std::to_string(ps.particles().size()) + " db");
    }

    std::printf("\n=== 4. Kontrollpontok: a felulet koveti a huzott pontot (a cikk megoldoja) ===\n");
    {
        // Gomb a (cx, cy, cz) kozepponttal es r sugarral; a megoldo parameterei (q)
        // cim szerint: ezeket irja, ahogy a GUI csuszkai is.
        float r = 1.0f, cx = 0.0f, cy = 0.0f, cz = 0.0f;
        auto res = [&](std::string const& n) -> Matek::Analizis::Tree {
            if (n == "r")  return Matek::Analizis::Kif(&r).get();
            if (n == "cx") return Matek::Analizis::Kif(&cx).get();
            if (n == "cy") return Matek::Analizis::Kif(&cy).get();
            if (n == "cz") return Matek::Analizis::Kif(&cz).get();
            return nullptr;
        };
        auto F = [&](glm::vec3 p) { return (p.x-cx)*(p.x-cx) + (p.y-cy)*(p.y-cy) + (p.z-cz)*(p.z-cz) - r*r; };

        ParticleSystem ps;
        ps.d = 0.6f;
        ps.surface().set_tree(Matek::Analizis::make_kif("(x-cx)^2 + (y-cy)^2 + (z-cz)^2 - r^2", res).get());
        std::vector<glm::vec3> pts = {{1, 0, 0}, {-1, 0, 0}};   // mindketto a gombon
        ps.bind_controls(&pts, {&r, &cx, &cy, &cz});
        ps.restart();
        for (int k = 0; k < 300; ++k) ps.step(0.03f);            // mintavetel a gombon

        // Az elso pontot kifele huzzuk (1.5, 0, 0)-ig; a masik helyben marad.
        for (int k = 0; k < 400; ++k) {
            ps.solve_controls(0, {1.5f, 0.0f, 0.0f}, 0.01f);
            if (k % 3 == 0) ps.step(0.03f);                      // a reszecskek kozben kovetik
        }
        ps.end_drag();
        ok("a huzott pont a celhoz ert", glm::length(pts[0] - glm::vec3{1.5f, 0, 0}) < 0.02f);
        ok("a felulet atmegy a huzott ponton", std::abs(F(pts[0])) < 0.02f, std::to_string(F(pts[0])));
        ok("es a helyben maradt ponton is", std::abs(F(pts[1])) < 0.02f, std::to_string(F(pts[1])));
        ok("a gomb nott es eltolodott (r ~ 1.25, cx ~ 0.25)",
           std::abs(r - 1.25f) < 0.03f && std::abs(cx - 0.25f) < 0.03f,
           "r=" + std::to_string(r) + " cx=" + std::to_string(cx));
        ok("oldalra nem mozdult (cy, cz ~ 0)", std::abs(cy) < 1e-3f && std::abs(cz) < 1e-3f);
        for (int k = 0; k < 100; ++k) ps.step(0.03f);
        ok("a reszecskek az uj feluleten vannak", on_surface_ratio(ps, 0.05f) > 0.9f,
           std::to_string(on_surface_ratio(ps, 0.05f)));

        ps.unbind_controls();
        ok("elengedve nincs kontrollpont", ps.controls() == nullptr);
    }

    std::printf("\n=== 4b. Kontrollpontok: tobb pont, mint parameter (tulhatarozott) ===\n");
    {
        // 6 pont a gombon, de csak 4 parameter (r + kozeppont): M = J*J^T szingularis.
        // A huzas nem teljesitheto pontosan — de a parameterek nem ugralhatnak el.
        float r = 1.0f, cx = 0.0f, cy = 0.0f, cz = 0.0f;
        auto res = [&](std::string const& n) -> Matek::Analizis::Tree {
            if (n == "r")  return Matek::Analizis::Kif(&r).get();
            if (n == "cx") return Matek::Analizis::Kif(&cx).get();
            if (n == "cy") return Matek::Analizis::Kif(&cy).get();
            if (n == "cz") return Matek::Analizis::Kif(&cz).get();
            return nullptr;
        };
        ParticleSystem ps;
        ps.surface().set_tree(Matek::Analizis::make_kif("(x-cx)^2 + (y-cy)^2 + (z-cz)^2 - r^2", res).get());
        std::vector<glm::vec3> pts = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        ps.bind_controls(&pts, {&r, &cx, &cy, &cz});

        float max_jump = 0.0f;
        bool finite = true;
        for (int k = 0; k < 400; ++k) {
            float const r0 = r, x0 = cx, y0 = cy, z0 = cz;
            ps.solve_controls(0, {1.5f, 0.0f, 0.0f}, 0.01f);
            finite = finite && std::isfinite(r) && std::isfinite(cx) && std::isfinite(cy) && std::isfinite(cz);
            max_jump = std::max({max_jump, std::abs(r - r0), std::abs(cx - x0),
                                 std::abs(cy - y0), std::abs(cz - z0)});
        }
        ok("a parameterek vegesek", finite);
        ok("nincs ugras (lepesenkent < 0.05)", max_jump < 0.05f, std::to_string(max_jump));
        ok("a gomb korlatos marad (0.5 < r < 2, |c| < 1)",
           r > 0.5f && r < 2.0f && glm::length(glm::vec3{cx, cy, cz}) < 1.0f,
           "r=" + std::to_string(r) + " c=(" + std::to_string(cx) + "," + std::to_string(cy) +
           "," + std::to_string(cz) + ")");
    }

    std::printf("\n=== 5. Parhuzamos leptetes (utils/Parallel.hpp) ===\n");
    {
        // Minden index pontosan egyszer fut, akkor is, ha tobb feladat van, mint mag.
        std::size_t const n = 4 * std::max(1u, std::thread::hardware_concurrency()) + 3;
        std::vector<std::atomic<int>> hits(n);
        Parallel::for_each(n, [&](std::size_t i) { hits[i]++; });
        bool once = true;
        for (auto& h : hits) once = once && h == 1;
        ok("minden index pontosan egyszer", once, std::to_string(n) + " feladat");

        bool caught = false;
        try { Parallel::for_each(8, [](std::size_t i) { if (i == 5) throw std::runtime_error("x"); }); }
        catch (std::runtime_error const&) { caught = true; }
        ok("a szalbeli kivetel a hivohoz jut", caught);

        // Tobb objektum egyszerre leptetve: mindegyik ugyanugy felepiti a mintavetelt,
        // mint egyedul (nincs kozos, irt allapot koztuk).
        char const* const F[] = {"x^2 + y^2 + z^2 - 4",
                                 "(x^2 + y^2 + z^2 + 9 - 1)^2 - 4*9*(x^2 + y^2)",
                                 "x^2 + y^2 - 1",
                                 "sunio(x^2+y^2+z^2-4, (x-2.5)^2+y^2+z^2-4, 0.5)"};
        std::vector<std::unique_ptr<ParticleSystem>> objs;
        for (char const* f : F) {
            auto& o = objs.emplace_back(std::make_unique<ParticleSystem>());
            o->d = 1.0f;
            o->surface().set_tree(Matek::Analizis::make_kif(f).get());
            if (std::string(f) == "x^2 + y^2 - 1")   // vegtelen henger: veges darab
                o->surface().set_domain(Matek::Analizis::make_kif("z > -2 and z < 2").get());
            o->restart();
        }
        for (float t = 0.0f; t < 20.0f; t += 0.03f)
            Parallel::for_each(objs.size(), [&](std::size_t i) { objs[i]->step(0.03f); });
        for (std::size_t i = 0; i < objs.size(); ++i) {
            float r = on_surface_ratio(*objs[i], 0.05f);
            ok(std::string("parhuzamosan: ") + F[i], objs[i]->particles().size() > 30 && r > 0.9f,
               std::to_string(objs[i]->particles().size()) + " db, feluleten " + std::to_string(r));
        }
    }

    std::printf("\n=== 6. Egy objektumon belul: parhuzamos == soros ===\n");
    {
        // Beagyazott es egyideju hivas: nincs holtpont, minden lefut.
        std::atomic<int> total{0};
        Parallel::for_each(8, [&](std::size_t) {
            Parallel::for_each(8, [&](std::size_t) { total++; });
        });
        std::thread other([&] { Parallel::for_each(50, [&](std::size_t) { total++; }); });
        Parallel::for_each(50, [&](std::size_t) { total++; });
        other.join();
        ok("beagyazott es egyideju hivas is lefut", total == 64 + 100, std::to_string(total.load()));

        // Ugyanabbol az allapotbol (azonos seed, sorosan felepitve) egy-egy lepes sorosan
        // es parhuzamosan: az eredmeny csak az osszegzesi sorrend kerekiteseben terhet el.
        auto make = [] {
            auto ps = std::make_unique<ParticleSystem>(SimParams{}, 12345u);
            ps->d = 1.0f;
            ps->max_threads = 1;
            ps->surface().set_tree(Matek::Analizis::make_kif("x^2 + y^2 + z^2 - 16").get());
            ps->restart();
            for (int k = 0; k < 300; ++k) ps->step(0.03f);
            return ps;
        };
        auto serial = make(), parallel = make();
        parallel->max_threads = 0;
        std::size_t const n0 = serial->particles().size();
        ok("azonos seed -> azonos kiindulas", n0 == parallel->particles().size() && n0 >= 1024,
           std::to_string(n0) + " db (tobb darabra oszlik)");
        for (int k = 0; k < 5; ++k) { serial->step(0.03f); parallel->step(0.03f); }
        bool same_n = serial->particles().size() == parallel->particles().size();
        float worst = 0.0f;
        if (same_n)
            for (std::size_t i = 0; i < serial->particles().size(); ++i)
                worst = std::max(worst, glm::length(serial->particles()[i].p - parallel->particles()[i].p));
        ok("5 lepes utan ugyanannyi reszecske", same_n);
        ok("es ugyanott (elteres < 1e-4)", same_n && worst < 1e-4f, "max elteres " + std::to_string(worst));
    }

    std::printf("\n%s (%d hiba)\n", failures ? "SIKERTELEN" : "MINDEN RENDBEN", failures);
    return failures ? 1 : 0;
}
