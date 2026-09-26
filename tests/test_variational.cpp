// Variacios implicit feluletek (particle_sampling/Variational.hpp) — GL nelkul:
// a kepletes alakzat reszecskeibol epitett felulet visszaadja az eredeti alakot, a
// kifejezesfa megegyezik a kozvetlen kiertekelessel, es egy kenyszer mozgatasa utan
// eleg ujra megoldani (a fa a cimeken at koveti).
#include <cmath>
#include <cstdio>
#include <string>

#include "particle_sampling/ParticleSystem.hpp"
#include "particle_sampling/Variational.hpp"
#include "scene/Build.hpp"

static int failures = 0;
static void ok(std::string const& what, bool c, std::string const& info = "") {
    if (!c) ++failures;
    std::printf("  %-50s %-8s %s\n", what.c_str(), c ? "[OK]" : "[HIBA]", info.c_str());
}

// Egy kepletes alakzat mintavetelezese, majd atalakitasa.
static std::shared_ptr<Variational> convert(char const* formula, float d, glm::vec3 center) {
    ParticleSystem ps({}, 1234u);
    ps.d = d;
    ps.surface().set_tree(Matek::Analizis::make_kif(formula).get());
    ps.restart();
    for (int k = 0; k < 700; ++k) ps.step(0.03f);
    auto v = std::make_shared<Variational>(Variational::from_particles(ps.particles(), center, 0.05f));
    v->solve();
    return v;
}

// A variacios felulet legnagyobb geometriai elterese (|F|/|grad F|) az eredeti
// felulet mintapontjain, a kozepponthoz kepest eltolva.
template<class Pts>
static float max_dev(std::shared_ptr<Variational> const& v, Pts const& pts) {
    Surface s;
    s.set_tree(Variational::tree(v).get());
    float m = 0.0f;
    for (glm::vec3 p : pts) {
        float const g = glm::length(s.grad(p));
        m = std::max(m, std::abs(s.F.at(p)) / std::max(g, 1e-6f));
    }
    return m;
}

int main() {
    std::printf("=== 1. Gomb (r = 2, kozeppont (1, 0, 0)) reszecskeibol ===\n");
    {
        auto vp = convert("(x-1)^2 + y^2 + z^2 - 4", 1.5f, {1, 0, 0});
        Variational& v = *vp;
        std::size_t const k = v.centers.size();
        ok("van eleg kenyszer", k >= 40, std::to_string(k / 2) + " pont + normalis");

        float worst = 0.0f;
        for (std::size_t i = 0; i < k; ++i) worst = std::max(worst, std::abs(v.at(v.centers[i]) - v.values[i]));
        ok("a kenyszereket pontosan teljesiti", worst < 1e-3f, std::to_string(worst));

        auto t = Variational::tree(vp);
        float diff = 0.0f;
        for (glm::vec3 p : {glm::vec3{0.3f, -1, 2}, glm::vec3{3, 1, 0}, glm::vec3{-0.5f, 0.2f, 0.1f}})
            diff = std::max(diff, std::abs(t.at(p) - v.at(p)) / (1.0f + std::abs(v.at(p))));
        ok("a fa = a kozvetlen kiertekeles", diff < 1e-4f, std::to_string(diff));

        ok("belul negativ, kivul pozitiv", v.at({0, 0, 0}) < 0.0f && v.at({4, 0, 0}) > 0.0f);

        // Az eredeti gomb pontjai (a kozepponthoz kepest, tehat az origo koruli r=2 gomb).
        std::vector<glm::vec3> sphere;
        for (int i = 0; i < 12; ++i)
            for (int j = 1; j < 6; ++j) {
                float const th = i * 0.5236f, ph = j * 0.5236f;
                sphere.push_back(2.0f * glm::vec3{std::sin(ph) * std::cos(th), std::sin(ph) * std::sin(th), std::cos(ph)});
            }
        float const dev = max_dev(vp, sphere);
        ok("visszaadja a gombot (elteres < 0.05)", dev < 0.05f, std::to_string(dev));

        // A kenyszerpontokban (ahol a reszecskek ulnek) a gradiens es a Hesse veges.
        Surface s;
        s.set_tree(t.get());
        Surface::Workspace ws;
        float F, K, Ft;
        glm::vec3 g;
        s.eval_full(v.centers[0], F, g, K, Ft, ws);
        ok("a kenyszerben veges a gradiens es a gorbulet",
           std::isfinite(g.x) && std::isfinite(g.y) && std::isfinite(g.z) && std::isfinite(K));
        ok("|grad F| ~ 1 a feluleten (tavolsagszeru)", std::abs(glm::length(g) - 1.0f) < 0.3f,
           std::to_string(glm::length(g)));

        // Egy pont kihuzasa: csak ujra kell oldani, a fa (a Variational-t olvassa) koveti.
        glm::vec3 const pulled = v.centers[0] * 1.3f;
        v.centers[0] = pulled;
        v.centers[1] = pulled + 0.05f * glm::normalize(pulled);
        v.solve();
        ok("mozgatas utan a fa atmegy az uj ponton", std::abs(t.at(pulled)) < 1e-3f,
           std::to_string(t.at(pulled)));
    }

    std::printf("\n=== 2. Torusz (R = 2, r = 0.7): a lyuk megmarad ===\n");
    {
        auto v = convert("(x^2 + y^2 + z^2 + 4 - 0.49)^2 - 16*(x^2 + y^2)", 0.8f, {0, 0, 0});
        ok("a lyuk kozepe kivul van", v->at({0, 0, 0}) > 0.0f, std::to_string(v->at({0, 0, 0})));
        ok("a cso belseje belul van", v->at({2, 0, 0}) < 0.0f, std::to_string(v->at({2, 0, 0})));
        std::vector<glm::vec3> torus;
        for (int i = 0; i < 16; ++i)
            for (int j = 0; j < 8; ++j) {
                float const u = i * 0.3927f, a = j * 0.7854f;
                float const rr = 2.0f + 0.7f * std::cos(a);
                torus.push_back({rr * std::cos(u), rr * std::sin(u), 0.7f * std::sin(a)});
            }
        float const dev = max_dev(v, torus);
        ok("visszaadja a toruszt (elteres < 0.1)", dev < 0.1f, std::to_string(dev));
    }

    std::printf("\n=== 3. A reszecske-szimulacio a variacios feluleten ===\n");
    {
        auto v = convert("x^2 + y^2 + z^2 - 4", 1.5f, {0, 0, 0});
        ParticleSystem ps({}, 99u);
        ps.d = 1.5f;
        ps.surface().set_tree(Variational::tree(v).get());
        ps.restart();
        for (int k = 0; k < 500; ++k) ps.step(0.03f);
        int good = 0;
        for (auto const& p : ps.particles())
            if (std::abs(glm::length(p.p) - 2.0f) < 0.05f) ++good;
        float const ratio = ps.particles().empty() ? 0.0f : float(good) / float(ps.particles().size());
        ok("a reszecskek a gombon vannak", ps.particles().size() > 30 && ratio > 0.9f,
           std::to_string(ps.particles().size()) + " db, " + std::to_string(ratio));
    }

    std::printf("\n=== 4. Jelenetben: a kepletet felvaltja, a pozicio eltolja ===\n");
    {
        SceneDoc sc;
        auto& s = sc.shapes.emplace_back();
        std::snprintf(s.name, sizeof(s.name), "v");
        std::snprintf(s.formula, sizeof(s.formula), "ez nem is keplet (");   // nem parseolodik
        s.vari = std::make_shared<Variational>();
        for (glm::vec3 c : {glm::vec3{1, 1, 1}, glm::vec3{1, -1, -1}, glm::vec3{-1, 1, -1}, glm::vec3{-1, -1, 1}})
            s.vari->add(c, 0.0f);
        s.vari->add({0, 0, 0}, -1.0f);
        s.vari->solve();
        s.xform.pos[0] = 5.0f;
        std::string const err = Build::build(sc, {});
        ok("felepul (a kepletet nem nezi)", err.empty(), err);
        if (err.empty()) {
            Matek::Analizis::Kif t(s.tree);
            ok("a kenyszer az eltolt helyen van", std::abs(t.at({6, 1, 1})) < 1e-4f, std::to_string(t.at({6, 1, 1})));
            ok("a kozepe az eltolt helyen belul", t.at({5, 0, 0}) < 0.0f);
            // Huzas: a fa a cimeken at kovet, ujraepites nelkul.
            s.vari->centers[0] = {1.5f, 1.5f, 1.5f};
            s.vari->solve();
            ok("huzas utan ujraepites nelkul is koveti", std::abs(t.at({6.5f, 1.5f, 1.5f})) < 1e-4f);
            // Uj kenyszer a feluleten: a felulet nem valtozik (a cikk "csomopont-beszurasa").
            glm::vec3 const q = {0.9f, 0.0f, 0.0f};
            float lo = 0.0f, hi = 3.0f;                           // a felulet metszese az x tengelyen
            for (int i = 0; i < 60; ++i) ((s.vari->at({(lo + hi) / 2, 0, 0}) < 0.0f) ? lo : hi) = (lo + hi) / 2;
            glm::vec3 const on{lo, 0.0f, 0.0f};
            float const before = s.vari->at(q);
            s.vari->add(on, 0.0f);
            s.vari->solve();
            ok("uj pont a feluleten: a felulet nem valtozik", std::abs(s.vari->at(q) - before) < 1e-4f,
               std::to_string(before) + " -> " + std::to_string(s.vari->at(q)));
            s.vari->remove(s.vari->centers.size() - 1);
            ok("torles utan ujra megoldhato", s.vari->solve() && std::abs(s.vari->at(q) - before) < 1e-4f);
        }
    }

    std::printf("\n=== 5. Nativ csomopont: derivaltak a lancszabalyon at, kis program ===\n");
    {
        auto v = convert("x^2 + y^2/2 + z^2 - 3", 1.5f, {0, 0, 0});
        SceneDoc sc;
        auto& s = sc.shapes.emplace_back();
        s.vari = v;
        s.xform.pos[0] = 1.0f; s.xform.pos[1] = -2.0f; s.xform.pos[2] = 0.5f;
        s.xform.rot[2] = 0.5f; s.xform.rot[0] = 0.3f;
        s.xform.scale[0] = 1.2f; s.xform.scale[1] = 0.8f;
        std::string const err = Build::build(sc, {});
        ok("elhelyezve felepul", err.empty(), err);

        Surface surf;
        surf.set_tree(s.tree);
        ok("a program kicsi (nem tagonkent derival)", surf.prog_full.size() < 400,
           std::to_string(surf.prog_full.size()) + " utasitas (szimbolikusan ~24000 volt)");

        // Gradiens es Hesse vs. kozepponti differencia, harom pontban (egy a feluleten).
        float const h = 1e-3f;
        float worst_g = 0.0f, worst_h = 0.0f;
        for (glm::vec3 p : {glm::vec3{1.3f, -1.1f, 0.9f}, glm::vec3{2.5f, -2.0f, 0.0f}, glm::vec3{0.2f, -3.0f, 1.5f}}) {
            glm::vec3 const g = surf.grad(p);
            Kif const* D[3] = {&surf.F_dx, &surf.F_dy, &surf.F_dz};
            float const H[3][3] = {{surf.F_dxx.at(p), surf.F_dxy.at(p), surf.F_dxz.at(p)},
                                   {surf.F_dxy.at(p), surf.F_dyy.at(p), surf.F_dyz.at(p)},
                                   {surf.F_dxz.at(p), surf.F_dyz.at(p), surf.F_dzz.at(p)}};
            for (int i = 0; i < 3; ++i) {
                glm::vec3 e{0.0f}; e[i] = h;
                float const fd = (surf.F.at(p + e) - surf.F.at(p - e)) / (2 * h);
                worst_g = std::max(worst_g, std::abs(fd - g[i]) / (1.0f + std::abs(g[i])));
                for (int m = 0; m < 3; ++m) {
                    float const fdh = (D[m]->at(p + e) - D[m]->at(p - e)) / (2 * h);
                    worst_h = std::max(worst_h, std::abs(fdh - H[m][i]) / (1.0f + std::abs(H[m][i])));
                }
            }
        }
        ok("gradiens = numerikus", worst_g < 2e-2f, std::to_string(worst_g));
        ok("Hesse = numerikus", worst_h < 2e-2f, std::to_string(worst_h));

        // A lefordított program ugyanazt adja, mint a fabejárás.
        Surface::Workspace ws;
        float F, K, Ft;
        glm::vec3 g;
        glm::vec3 const q{1.3f, -1.1f, 0.9f};
        surf.eval_full(q, F, g, K, Ft, ws);
        ok("program = fabejaras", std::abs(F - surf.F.at(q)) < 1e-5f && glm::length(g - surf.grad(q)) < 1e-4f);

        // Uj kenyszer ujraepites nelkul: a fa a Variational-t olvassa, tehat rogton koveti.
        glm::vec3 const local{0.0f, 0.0f, 2.2f};
        v->add(local, 0.0f);
        v->solve();
        float const Fw = v->at(local);
        ok("uj kenyszer utan a fa ujraepites nelkul is koveti", std::abs(Fw) < 1e-4f && std::abs(v->at(local)) < 1e-4f);
        Surface::Workspace ws2;
        float F2, K2, Ft2;
        glm::vec3 g2;
        surf.eval_full(q, F2, g2, K2, Ft2, ws2);
        ok("a program is az uj fuggvenyt szamolja", std::abs(F2 - surf.F.at(q)) < 1e-5f);
    }

    std::printf("\n%s\n", failures ? "VAN HIBA" : "minden rendben");
    return failures ? 1 : 0;
}
