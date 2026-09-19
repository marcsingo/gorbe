// A reszecske-szimulacio (ParticleSystem) — GL es ablak NELKUL. Eddig ez csak egy
// GL-kontextusos teszttel volt futtathato, mert a reszecskek egy GL-modellben eltek.
//
// Nem pontos szamokat ellenoriz (a szimulacio veletlen kezdoponttal indul), hanem
// azt, ami minden futasnal igaz kell legyen: a reszecskek a feluletre kerulnek,
// szetterulnek, a tartomanyon belul maradnak, es a plafon felett nem szaporodnak.
#include <cmath>
#include <cstdio>
#include <string>

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

    std::printf("\n=== 4. Kontrollpont: letesz, huz ===\n");
    {
        ParticleSystem ps;
        ps.surface().set_tree(Matek::Analizis::make_kif("x^2 + y^2 + z^2 - 4").get());
        ps.add_control({2, 0, 0});
        for (int k = 0; k < 200; ++k) ps.drag_control(0, {0, 2, 0}, 0.01f);
        glm::vec3 p = ps.controls()[0].p;
        ok("a huzott pont a celhoz ert", glm::length(p - glm::vec3{0, 2, 0}) < 0.05f);
        ok("a normalisa kovette (a gombon kifele)", ps.controls()[0].F_x.y > 3.0f);
    }

    std::printf("\n%s (%d hiba)\n", failures ? "SIKERTELEN" : "MINDEN RENDBEN", failures);
    return failures ? 1 : 0;
}
