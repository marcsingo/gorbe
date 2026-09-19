// A matek konyvtar nyelve: parser, kiiras, fuggvenytabla, egyszerusito.
//
//  1-3. oda-vissza: string -> fa -> string -> fa UGYANAZT a fuggvenyt adja, a
//       parameterek CIM szerint elnek tovabb, a kiiras fixpont
//  4.   a fuggvenytabla MINDEN sora: a szimbolikus derivalt == a numerikus
//       (egy uj sor a tablaban magatol bekerul ide)
//  5.   a roviditesek (2x, x**2, pi, ², arctg, ...) ugyanazt jelentik, mint a hosszu alak
//  6.   a hibauzenetek megjelolik a helyet, es javasolnak
//  7.   az egyszerusito
// A matek konyvtar GL es jelenet nelkul fordul.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

#include "matek/Parser.hpp"

using namespace Matek::Analizis;

static int failures = 0;
static void ok(std::string const& what, bool c, std::string const& info = "") {
    if (!c) ++failures;
    std::printf("  %-44s %-8s %s\n", what.c_str(), c ? "[OK]" : "[HIBA]", info.c_str());
}

// Ket fuggveny azonos-e a mintapontokon. NaN == NaN itt egyezesnek szamit (pl. ln
// negativ argumentumon mindket oldalon NaN).
static bool same(Kif const& f, Kif const& g) {
    glm::vec3 const P[] = {{0.3f, -0.7f, 1.1f}, {1.5f, 0.2f, -0.4f}, {-2.0f, 1.0f, 0.5f}, {0, 0, 0}};
    for (auto p : P) {
        float a = f.at(p), b = g.at(p);
        if (a == b || (std::isnan(a) && std::isnan(b))) continue;   // a == b: inf is
        if (!(std::abs(a - b) <= 1e-5f * (1.0f + std::abs(a)))) return false;
    }
    return true;
}

// UTF-8 jelek a kenyelmi irasmodhoz (a forrasfajl kodolasatol fuggetlenul).
#define PI_  "\xCF\x80"
#define SQ_  "\xC2\xB2"
#define CB_  "\xC2\xB3"
#define DOT_ "\xC2\xB7"

int main() {
    // A parameterek: a resolver nev -> cim, a namer cim -> nev (egymas inverzei).
    float a = 1.5f, b = 0.25f;
    NameResolver res = [&](std::string const& n) -> Tree {
        if (n == "a") return Kif(&a).get();
        if (n == "b") return Kif(&b).get();
        return nullptr;
    };
    ParamNamer nam = [&](float const* p) -> std::string {
        return p == &a ? "a" : p == &b ? "b" : "";
    };

    std::printf("=== 1. Oda-vissza: minden nyelvi elem ===\n");
    char const* const CASES[] = {
        "x^2 + y^2 + z^2 - a^2",
        "sin(x)*cos(y) + tan(z/4) + ctg(x + 2)",
        "ln(x^2 + 1) + log(y^2 + 1)",
        "sqrt(x^2 + b)",
        "abs(x) - sign(y)",
        "min(x, y) + max(y, z)",
        "unio(x - a, y) + metszet(x, y) + kulonbseg(x, z)",
        "sunio(x, y, 0.3) + smetszet(x, y) + skulonbseg(x, y, b)",
        "x > 1 and y < 2 or not (z >= a)",
        "-x^2 + 2^-x",
        "(0 - 3)^2 + x",
        "1e-3*x + 2.5E+4 - 0.1",
        "pi*x/180",
        "x - (y - z)",
        "x / (y / 2)",
        "2^3^x",
        "asin(x/4) + acos(y/4) + atan(z) + atan2(y, x)",
        "sinh(x/2) + cosh(y/2) + tanh(z) + exp(x/3) + cbrt(y) - floor(z) + ceil(x) + round(y)",
        "hypot(x, y) + length(x, y, z) + clamp(x, -1, a) + mod(y, 1.5) + fract(z)",
        "step(0.2, x) + smoothstep(0, 1, y) + mix(x, y, 0.25) + pow(x^2 + 1, b)",
        "2x + 3(y + 1) + x(y - 2) + (x + 1)(z - 1) + 2 pi + x**2 + 2e",
        PI_ " * x" SQ_ " " DOT_ " a",
        "sin(x)^2 + -cos(y)",
    };
    for (char const* src : CASES) {
        Kif f = make_kif(src, res);
        std::string txt = kif_text(f, nam);
        Kif g;
        try { g = make_kif(txt, res); }
        catch (std::exception const& e) { ok(src, false, std::string("nem olvashato vissza: ") + e.what()); continue; }
        ok(src, same(f, g) && kif_text(g, nam) == txt, txt);
    }

    std::printf("\n=== 2. Parameterek: nev marad, cim szerint el ===\n");
    {
        Kif g = make_kif(kif_text(make_kif("a*x + b", res), nam), res);
        a = 3.0f;
        ok("visszaolvasva is koveti a-t", std::abs(g.at({1, 0, 0}) - (3.0f + b)) < 1e-6f);
        a = 1.5f;
        std::string no_name = kif_text(make_kif("a*x", res));
        ok("namer nelkul: a pillanatnyi ertek", no_name == "(1.5 * x)", no_name);
    }

    std::printf("\n=== 3. Konstansok: pontos, olvashato ===\n");
    {
        std::string t = kif_text(make_kif("0.1"));
        ok("0.1 roviden, bitre pontosan", t == "0.1" && make_kif(t).at({0, 0, 0}) == 0.1f, t);
        std::string n = kif_text(Kif(-3.0f) ^ Kif(2.0f));
        ok("negativ alap zarojelben: (-3)^2 = 9", make_kif(n).at({0, 0, 0}) == 9.0f, n);
        ok("ln neve olvashato", kif_text(make_kif("ln(x)")) == "ln(x)");
        ok("elojelvaltas roviden: (-x)", kif_text(make_kif("-x")) == "(-x)", kif_text(make_kif("-x")));
    }

    std::printf("\n=== 4. A fuggvenytabla MINDEN sora: szimbolikus == numerikus derivalt ===\n");
    {
        // Az argumentumok x-tol fuggnek, es a biztonsagos tartomanyban maradnak
        // (asin/acos: |u| < 1, ln/sqrt: u > 0, round: nem 0.5 kozeleben).
        char const* const ARGS[] = {"(0.4 + 0.1*x)", "(0.7 - 0.2*x)", "(0.9 + 0.05*x)"};
        auto check_d = [&](std::string const& src) {
            Kif f = make_kif(src);
            Kif d = f.derive('x');
            bool good = true;
            std::string info;
            for (float x0 : {0.5f, 1.7f}) {
                float h = 1e-2f;
                float num = (f.at({x0 + h, 0, 0}) - f.at({x0 - h, 0, 0})) / (2 * h);
                float sym = d.at({x0, 0, 0});
                if (!(std::abs(num - sym) <= 2e-3f * (1.0f + std::abs(sym)))) {
                    good = false;
                    info = "num=" + std::to_string(num) + " szimb=" + std::to_string(sym);
                }
            }
            ok("d/dx " + src, good, info);
        };
        for (auto const& fd : FUNCS) {
            std::string src = std::string(fd.name) + "(" + ARGS[0];
            if (fd.arity() == 2) src += std::string(", ") + ARGS[1];
            check_d(src + ")");
        }
        for (auto const& m : MACROS) {
            int n = 1;   // a kotelezo parameterek szama
            for (char const* c = m.params; *c; ++c) if (*c == ',') ++n;
            if (std::strchr(m.params, '=')) --n;
            std::string src = std::string(m.name) + "(";
            for (int k = 0; k < n; ++k) src += (k ? ", " : "") + std::string(ARGS[k]);
            check_d(src + ")");
        }
    }

    std::printf("\n=== 5. Rovidites: ugyanazt jelenti, mint a hosszu alak ===\n");
    {
        std::pair<char const*, char const*> const SAME[] = {
            {"2x", "2*x"}, {"3(x + 1)", "3*(x + 1)"}, {"x(y + 1)", "x*(y + 1)"},
            {"(x + 1)(x - 1)", "(x + 1)*(x - 1)"}, {"2 pi x", "2*pi*x"},
            {"x**2**0.5", "x^(2^0.5)"}, {"2e", "2*e"}, {"2e-1", "0.2"}, {"-x^2", "-(x^2)"},
            {"a(x + 1)", "a*(x + 1)"}, {PI_, "pi"}, {"x" SQ_ " + y" CB_, "x^2 + y^3"},
            {"x " DOT_ " y", "x*y"}, {"x > -1 and y < 2", "min(x + 1, 2 - y)"},
            {"arctg(x) + lg(y + 2)", "atan(x) + log(y + 2)"}, {"e", "exp(1)"},
        };
        for (auto [a_src, b_src] : SAME)
            ok(std::string(a_src) + "  ==  " + b_src, same(make_kif(a_src, res), make_kif(b_src, res)));
    }

    std::printf("\n=== 6. Hibauzenetek: hol a hiba, es mire gondolt ===\n");
    {
        auto err_of = [&](char const* src) -> std::string {
            try { make_kif(src, res); } catch (std::exception const& e) { return e.what(); }
            return "";
        };
        auto first_line = [](std::string const& e) { return e.substr(0, e.find('\n')); };
        std::string e1 = err_of("x^2 + sni(y)");
        ok("elgepelt fuggveny -> javaslat: sin", e1.find("erre gondoltal: sin?") != std::string::npos,
           first_line(e1));
        ok("a hiba helye megjelolve", e1.find("\n  x^2 + sni(y)\n        ^") != std::string::npos);
        std::string e2 = err_of("xy + 1");
        ok("xy -> javaslat: x*y", e2.find("x*y") != std::string::npos, first_line(e2));
        std::string e3 = err_of("atan2(x)");
        ok("rossz argumentumszam", e3.find("2 argumentumot var, kapott 1") != std::string::npos,
           first_line(e3));
        std::string e4 = err_of("sin + 1");
        ok("fuggveny zarojel nelkul", e4.find("zarojelben kell") != std::string::npos, first_line(e4));
        std::string e5 = err_of("(x + 1");
        ok("hianyzo zarojel", e5.find("hianyzo ')'") != std::string::npos, first_line(e5));
    }

    std::printf("\n=== 7. Egyszerusites ===\n");
    {
        auto simp = [&](char const* src) { return kif_text(make_kif(src, res).simplify(), nam); };
        ok("2 + 3 -> 5", simp("2 + 3") == "5", simp("2 + 3"));
        ok("2*x*3 -> 6x", simp("2*x*3") == "(6 * x)", simp("2*x*3"));
        ok("x - x -> 0", simp("x - x") == "0");
        ok("(x+a)/(x+a) -> 1", simp("(x + a)/(x + a)") == "1");
        ok("sin(0) + x -> x", simp("sin(0) + x") == "x");
        ok("a*0 -> 0 (parameterrel is)", simp("a*0") == "0");
        ok("a + 0 -> a (es el tovabb)", simp("a + 0") == "a");
        Kif d = make_kif("x^3").derive('x');
        ok("d/dx x^3 -> 3*x^2", kif_text(d) == "(3 * (x ^ 2))", kif_text(d));
    }

    std::printf("\n=== 8. Ismeretlen valtozo nem ad csendben 0-t ===\n");
    {
        bool threw = false;
        try { (void)Kif('q').at({1, 2, 3}); } catch (std::logic_error const&) { threw = true; }
        ok("Valtozo('q').at() kivetelt dob", threw);
    }

    std::printf("\n%s (%d hiba)\n", failures ? "SIKERTELEN" : "MINDEN RENDBEN", failures);
    return failures ? 1 : 0;
}
