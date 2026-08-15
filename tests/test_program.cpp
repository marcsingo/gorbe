// A lapos program (matek/Program.hpp) es a terbeli racs (SpatialGrid.hpp) tesztje.
//
// A legfontosabb allitas: a lefordított program PONTOSAN ugyanazt adja, mint a
// fabejaras — kepletenkent, kimenetenkent, sok veletlen pontban.
#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "matek/Kif.hpp"
#include "particle_sampling/SpatialGrid.hpp"

using namespace Matek::Analizis;

static int failures = 0;
static void ok(std::string const& what, bool c, std::string const& info = "") {
    if (!c) ++failures;
    std::printf("  %-44s %-8s %s\n", what.c_str(), c ? "[OK]" : "[HIBA]", info.c_str());
}

static std::size_t tsize(Kif const& k) { std::ostringstream os; os << k; return os.str().size(); }

int main() {
    auto s1 = make_kif("x^2 + y^2 + z^2 - 1").get();
    auto s2 = make_kif("(x-1)^2 + y^2 + z^2 - 1").get();
    auto res = [&](std::string const& n) -> std::shared_ptr<Kifejezes const> {
        if (n == "f1") return s1;
        if (n == "f2") return s2;
        return nullptr;
    };

    struct C { char const* name; char const* f; };
    C cases[] = {
        {"henger",              "x^2 + y^2 - 1"},
        {"gomb",                "x^2 + y^2 + z^2 - 1"},
        {"torusz",              "(x^2 + y^2 + z^2 + 9 - 1)^2 - 4*9*(x^2 + y^2)"},
        {"kocka",               "x^4 + y^4 + z^4 - 1"},
        {"eles unio",           "unio(f1, f2)"},
        {"sima unio",           "sunio(f1, f2, 0.5)"},
        {"vagott",              "metszet(unio(f1, f2), x^2 + y^2 + z^2 - 9)"},
        {"beagyazott 2 szintu", "sunio(metszet(unio(f1, f2), x^2+y^2+z^2-9), x^2+y^2-0.04, 0.3)"},
        {"trigonometrikus",     "sin(x)*cos(y) + tan(z/4) - 0.3"},
        {"vegyes",              "abs(x) + sign(y)*sqrt(z^2 + 1) + ln(x^2 + 2)"},
        {"feltetel",            "x > 2 and y < 3 or not (z > 0)"},
    };

    std::mt19937 rng(2024);
    std::uniform_real_distribution<float> d(-2.5f, 2.5f);
    std::vector<glm::vec3> pts(500);
    for (auto& p : pts) p = {d(rng), d(rng), d(rng)};

    std::printf("=== 1. Program == fabejaras (F + gradiens + Hesse, 10 kimenet) ===\n");
    std::printf("  %-22s %10s %10s %8s\n", "keplet", "fa (kar.)", "program", "utasitas/kar.");
    for (auto& c : cases) {
        Kif F = make_kif(c.f, res);
        Kif fx = F.derrive('x'), fy = F.derrive('y'), fz = F.derrive('z');
        Kif all[10] = {F, fx, fy, fz,
                       fx.derrive('x'), fx.derrive('y'), fx.derrive('z'),
                       fy.derrive('y'), fy.derrive('z'), fz.derrive('z')};

        Program prog;
        int out[10];
        for (int i = 0; i < 10; ++i) out[i] = all[i].get()->compile(prog);
        prog.finish();

        std::size_t tree_chars = 0;
        for (auto& k : all) tree_chars += tsize(k);

        double worst = 0.0;
        bool   all_finite_match = true;
        for (auto p : pts) {
            prog.run(p);
            for (int i = 0; i < 10; ++i) {
                float a = all[i].at(p);
                float b = prog.slot(out[i]);
                if (!std::isfinite(a) || !std::isfinite(b)) {
                    // NaN/Inf eseten a ketto "nem-vegessege" egyezzen
                    if (std::isfinite(a) != std::isfinite(b)) all_finite_match = false;
                    continue;
                }
                double denom = std::max(1.0, (double)std::abs(a));
                worst = std::max(worst, std::abs((double)a - (double)b) / denom);
            }
        }
        char info[160];
        std::snprintf(info, sizeof(info), "%-22s %10zu %10zu   max rel. elteres=%.2e",
                      c.name, tree_chars, prog.size(), worst);
        ok(std::string("azonos: ") + c.name, worst < 1e-6 && all_finite_match, info);
    }

    std::printf("\n=== 2. Sebesseg: fabejaras vs. program (10 kimenet/pont) ===\n");
    std::printf("  %-22s %12s %12s %8s\n", "keplet", "fa (us)", "program (us)", "gyorsulas");
    for (auto& c : cases) {
        Kif F = make_kif(c.f, res);
        Kif fx = F.derrive('x'), fy = F.derrive('y'), fz = F.derrive('z');
        Kif all[10] = {F, fx, fy, fz,
                       fx.derrive('x'), fx.derrive('y'), fx.derrive('z'),
                       fy.derrive('y'), fy.derrive('z'), fz.derrive('z')};
        Program prog; int out[10];
        for (int i = 0; i < 10; ++i) out[i] = all[i].get()->compile(prog);
        prog.finish();

        int const REP = 40;   // csak tajekoztato meres, ne lassitsa a ctest-et
        volatile float sink = 0.0f;
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int r = 0; r < REP; ++r) for (auto p : pts)
            for (int i = 0; i < 10; ++i) sink = sink + all[i].at(p);
        auto t1 = std::chrono::high_resolution_clock::now();
        for (int r = 0; r < REP; ++r) for (auto p : pts) {
            prog.run(p);
            for (int i = 0; i < 10; ++i) sink = sink + prog.slot(out[i]);
        }
        auto t2 = std::chrono::high_resolution_clock::now();

        double n = double(REP) * pts.size();
        double a = std::chrono::duration<double, std::micro>(t1 - t0).count() / n;
        double b = std::chrono::duration<double, std::micro>(t2 - t1).count() / n;
        std::printf("  %-22s %12.3f %12.3f %7.1fx\n", c.name, a, b, a / b);
    }

    std::printf("\n=== 3. SpatialGrid: azonos szomszedok, mint a nyers parbejaras ===\n");
    {
        std::mt19937 r2(99);
        std::uniform_real_distribution<float> dp(-20.0f, 20.0f);
        for (int n : {1, 2, 50, 500, 2000}) {
            std::vector<glm::vec3> ps(n);
            for (auto& p : ps) p = {dp(r2), dp(r2), dp(r2)};
            float cut = 3.0f;

            SpatialGrid g;
            g.build(ps.size(), [&](std::size_t k) { return ps[k]; }, cut);

            bool same = true;
            long long pairs_brute = 0, pairs_grid = 0;
            for (int i = 0; i < n && same; ++i) {
                std::vector<int> brute, viagrid;
                for (int j = 0; j < n; ++j) {
                    if (i == j) continue;
                    glm::vec3 r = ps[i] - ps[j];
                    if (glm::dot(r, r) <= cut * cut) brute.push_back(j);
                }
                g.for_each_near(ps[i], [&](int j) {
                    if (i == j) return;
                    glm::vec3 r = ps[i] - ps[j];
                    if (glm::dot(r, r) <= cut * cut) viagrid.push_back(j);
                });
                std::sort(brute.begin(), brute.end());
                std::sort(viagrid.begin(), viagrid.end());
                if (brute != viagrid) same = false;
                pairs_brute += n - 1;
                pairs_grid  += viagrid.size();
            }
            char info[128];
            std::snprintf(info, sizeof(info), "n=%d  parok: nyers=%lld racs=%lld",
                          n, pairs_brute, pairs_grid);
            ok("racs szomszedai == nyers szomszedai", same, info);
        }
        // Nagyon tavoli pont: ne akadjon ki a kulcs-pakolas
        std::vector<glm::vec3> far = {{0,0,0}, {1e6f, -1e6f, 5e5f}};
        SpatialGrid g;
        g.build(far.size(), [&](std::size_t k) { return far[k]; }, 1.0f);
        int found = 0;
        g.for_each_near(far[0], [&](int) { ++found; });
        ok("tavoli koordinata nem tor el semmit", found >= 1, "talalt=" + std::to_string(found));
    }

    std::printf("\n%s (%d hiba)\n", failures ? ">>> SIKERTELEN" : ">>> MINDEN TESZT OK", failures);
    return failures != 0;
}
