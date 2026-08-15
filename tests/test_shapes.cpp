// A main.cpp sablon-képleteinek ellenőrzése: parseolódnak-e, és a felületen
// (a várt ponton) tényleg 0-t adnak-e. Nem kell hozzá GL-kontextus.
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "matek/Kif.hpp"

using namespace Matek::Analizis;

static int failures = 0;

static void check(char const* what, float got, float expected, float tol = 1e-4f) {
    bool ok = std::isfinite(got) && std::abs(got - expected) <= tol;
    if (!ok) ++failures;
    std::printf("%-28s %-12s got=%12.6f expected=%12.6f\n",
                what, ok ? "[OK]" : "[HIBA]", got, expected);
}

// Egy képlet kiértékelése adott paraméterekkel és pontban.
static float eval(std::string const& formula,
                  std::map<std::string, float> const& params,
                  glm::vec3 at,
                  std::map<std::string, std::shared_ptr<Kifejezes const>> const& shapes = {}) {
    Kif f = make_kif(formula, [&](std::string const& nm) -> std::shared_ptr<Kifejezes const> {
        auto p = params.find(nm);
        if (p != params.end()) return Kif(&p->second).get();
        auto s = shapes.find(nm);
        if (s != shapes.end()) return s->second;
        return nullptr;
    });
    return f.at(at);
}

int main() {
    // A paraméterek CÍMÉT tárolja a fa, ezért a map-ek élettartama végig kell.
    std::map<std::string, float> gomb   {{"r", 1.0f}};
    std::map<std::string, float> ellip  {{"a", 3.0f}, {"b", 2.0f}, {"c", 1.0f}};
    std::map<std::string, float> torusz {{"R", 3.0f}, {"r", 1.0f}};
    std::map<std::string, float> ellsz  {{"a", 3.0f}, {"b", 1.5f}};
    std::map<std::string, float> heng   {{"r", 2.0f}};
    std::map<std::string, float> kup    {{"a", 1.0f}};
    std::map<std::string, float> hip    {{"a", 1.0f}, {"b", 1.0f}, {"c", 1.0f}};
    std::map<std::string, float> kocka  {{"a", 1.5f}};
    std::map<std::string, float> blend  {{"k", 0.5f}};

    // --- F == 0 a felületen ---
    check("gomb (1,0,0)",
          eval("x^2 + y^2 + z^2 - r^2", gomb, {1, 0, 0}), 0.0f);
    check("gomb kozeppont",
          eval("x^2 + y^2 + z^2 - r^2", gomb, {0, 0, 0}), -1.0f);
    check("ellipszoid (3,0,0)",
          eval("x^2/a^2 + y^2/b^2 + z^2/c^2 - 1", ellip, {3, 0, 0}), 0.0f);
    check("ellipszoid (0,0,1)",
          eval("x^2/a^2 + y^2/b^2 + z^2/c^2 - 1", ellip, {0, 0, 1}), 0.0f);
    check("torusz kulso (4,0,0)",
          eval("(x^2 + y^2 + z^2 + R^2 - r^2)^2 - 4*R^2*(x^2 + y^2)", torusz, {4, 0, 0}), 0.0f);
    check("torusz belso (2,0,0)",
          eval("(x^2 + y^2 + z^2 + R^2 - r^2)^2 - 4*R^2*(x^2 + y^2)", torusz, {2, 0, 0}), 0.0f);
    check("torusz teteje (3,0,1)",
          eval("(x^2 + y^2 + z^2 + R^2 - r^2)^2 - 4*R^2*(x^2 + y^2)", torusz, {3, 0, 1}), 0.0f);
    check("ellipszis (3,0,5)",
          eval("x^2/a^2 + y^2/b^2 - 1", ellsz, {3, 0, 5}), 0.0f);
    check("henger (2,0,7)",
          eval("x^2 + y^2 - r^2", heng, {2, 0, 7}), 0.0f);
    check("kup (1,0,1)",
          eval("x^2 + y^2 - a^2*z^2", kup, {1, 0, 1}), 0.0f);
    check("hiperboloid (1,0,0)",
          eval("x^2/a^2 + y^2/b^2 - z^2/c^2 - 1", hip, {1, 0, 0}), 0.0f);
    check("hiperboloid (0,1.414,1)",
          eval("x^2/a^2 + y^2/b^2 - z^2/c^2 - 1", hip, {0, std::sqrt(2.0f), 1}), 0.0f);
    check("kocka (1.5,0,0)",
          eval("x^4 + y^4 + z^4 - a^4", kocka, {1.5f, 0, 0}), 0.0f);

    // --- blend: két gomb sima unioja, ott ahol f1 == f2 -> f1 - k/2 ---
    {
        auto f1 = make_kif("x^2 + y^2 + z^2 - 1").get();
        auto f2 = make_kif("(x-1)^2 + y^2 + z^2 - 1").get();
        std::map<std::string, std::shared_ptr<Kifejezes const>> sh{{"f1", f1}, {"f2", f2}};
        check("blend (0.5,0,0)",
              eval("0.5*(f1 + f2 - sqrt((f1 - f2)^2 + k^2))", blend, {0.5f, 0, 0}, sh), -1.0f);
    }

    // --- gradiens: a szimulacio ezt hasznalja, ne legyen NaN/Inf a feluleten ---
    {
        Kif f = make_kif("(x^2 + y^2 + z^2 + R^2 - r^2)^2 - 4*R^2*(x^2 + y^2)",
                         [&](std::string const& nm) -> std::shared_ptr<Kifejezes const> {
                             auto p = torusz.find(nm);
                             return p != torusz.end() ? Kif(&p->second).get() : nullptr;
                         });
        Kif fx = f.derrive('x'), fy = f.derrive('y'), fz = f.derrive('z');
        // (4,0,0): a kulso egyenlitonel a normalis +x irany, y/z komponens 0
        glm::vec3 at{4, 0, 0};
        check("torusz dF/dx (4,0,0)", fx.at(at), 4.0f * 4.0f * (16.0f + 8.0f) - 8.0f * 9.0f * 4.0f);
        check("torusz dF/dy (4,0,0)", fy.at(at), 0.0f);
        check("torusz dF/dz (4,0,0)", fz.at(at), 0.0f);
        // masodrendu derivalt (Hesse) is legyen veges - a gorbulet ebbol jon
        check("torusz d2F/dx2 veges",  std::isfinite(fx.derrive('x').at(at)) ? 1.0f : 0.0f, 1.0f);
        check("torusz d2F/dydz veges", std::isfinite(fy.derrive('z').at(at)) ? 1.0f : 0.0f, 1.0f);
    }

    // --- precedencia-ellenorzes: a^2*z^2 == (a^2)*(z^2), nem a^(2*z^2) ---
    {
        std::map<std::string, float> p{{"a", 2.0f}};
        check("precedencia a^2*z^2", eval("a^2*z^2", p, {0, 0, 3}), 4.0f * 9.0f);
    }

    std::printf("\n%s (%d hiba)\n", failures ? "SIKERTELEN" : "MINDEN TESZT OK", failures);
    return failures != 0;
}
