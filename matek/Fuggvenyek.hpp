#ifndef MATEK_FUGGVENYEK_HPP
#define MATEK_FUGGVENYEK_HPP

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "fuggvenyek/Fuggveny.hpp"

// ===========================================================================
// A FÜGGVÉNYTÁBLA — minden beépített függvény egy helyen.
//
// Új függvény felvétele = EGY új sor az alábbi táblák egyikében. Innen dolgozik a
// parser (név, álnevek, argumentumszám), a kiértékelés és a lapos program (f1/f2),
// a deriválás (du/dv: a derivált a parser nyelvén, u és v a két argumentum), a
// foglalt nevek listája (scene/Names.hpp) és a súgó is. A tests/test_parser.cpp
// minden sort automatikusan ellenőriz: a szimbolikus deriváltat a numerikussal.
//
//   FUNCS  — "igazi" függvények: saját csomópontjuk van (Hivas), saját deriválttal.
//   MACROS — más függvényekből felírható rövidítések: a parser kifejti őket, ezért
//            a deriváltjuk magától adódik (pl. hypot(a, b) = sqrt(a^2 + b^2)).
// ===========================================================================
namespace Matek {
    namespace Analizis {

        inline FuncDef const FUNCS[] = {
            // --- trigonometria ---
            {.name = "sin", .f1 = [](float u) { return std::sin(u); }, .du = "cos(u)", .help = "szinusz"},
            {.name = "cos", .f1 = [](float u) { return std::cos(u); }, .du = "-sin(u)", .help = "koszinusz"},
            {.name = "tan", .aliases = "tg", .f1 = [](float u) { return std::tan(u); },
             .du = "1/cos(u)^2", .help = "tangens"},
            {.name = "ctg", .aliases = "cot", .f1 = [](float u) { return 1.0f / std::tan(u); },
             .du = "-1/sin(u)^2", .help = "kotangens"},
            {.name = "asin", .aliases = "arcsin", .f1 = [](float u) { return std::asin(u); },
             .du = "1/sqrt(1 - u^2)", .help = "arkusz szinusz"},
            {.name = "acos", .aliases = "arccos", .f1 = [](float u) { return std::acos(u); },
             .du = "-1/sqrt(1 - u^2)", .help = "arkusz koszinusz"},
            {.name = "atan", .aliases = "arctg arctan", .f1 = [](float u) { return std::atan(u); },
             .du = "1/(1 + u^2)", .help = "arkusz tangens"},
            {.name = "atan2", .f2 = [](float u, float v) { return std::atan2(u, v); },
             .du = "v/(u^2 + v^2)", .dv = "-u/(u^2 + v^2)", .help = "atan2(y, x): a pont szoge"},

            // --- hiperbolikus ---
            {.name = "sinh", .f1 = [](float u) { return std::sinh(u); }, .du = "cosh(u)", .help = "szinusz hiperbolikusz"},
            {.name = "cosh", .f1 = [](float u) { return std::cosh(u); }, .du = "sinh(u)", .help = "koszinusz hiperbolikusz"},
            {.name = "tanh", .f1 = [](float u) { return std::tanh(u); }, .du = "1 - tanh(u)^2", .help = "tangens hiperbolikusz"},

            // --- hatvány, gyök, logaritmus ---
            {.name = "exp", .f1 = [](float u) { return std::exp(u); }, .du = "exp(u)", .help = "e^u"},
            {.name = "ln", .f1 = [](float u) { return std::log(u); }, .du = "1/u", .help = "termeszetes logaritmus"},
            {.name = "log", .aliases = "lg", .f1 = [](float u) { return std::log10(u); },
             .du = "1/(u*ln(10))", .help = "10-es alapu logaritmus"},
            // A gyök deriváltja u = 0-ban végtelen (a függvény ott nem sima). Ahol ez a
            // felületre esne, adj hozzá egy kicsi tagot: sqrt(x^2 + y^2 + 0.01).
            {.name = "sqrt", .f1 = [](float u) { return std::sqrt(u); }, .du = "0.5/sqrt(u)", .help = "negyzetgyok"},
            {.name = "cbrt", .f1 = [](float u) { return std::cbrt(u); },
             .du = "1/(3*cbrt(u)^2)", .help = "kobgyok (negativra is)"},

            // --- előjel, kerekítés (a deriváltjuk majdnem mindenhol 0 vagy ±1) ---
            {.name = "abs", .f1 = [](float u) { return std::abs(u); }, .du = "sign(u)", .help = "abszolut ertek"},
            {.name = "sign", .aliases = "sgn",
             .f1 = [](float u) { return u > 0.0f ? 1.0f : (u < 0.0f ? -1.0f : 0.0f); }, .help = "elojel"},
            {.name = "floor", .f1 = [](float u) { return std::floor(u); }, .help = "lefele kerekites"},
            {.name = "ceil", .f1 = [](float u) { return std::ceil(u); }, .help = "felfele kerekites"},
            {.name = "round", .f1 = [](float u) { return std::round(u); }, .help = "kerekites"},

            // --- éles min/max: a CSG alapja (a varraton a gradiens ugrik) ---
            {.name = "min", .f2 = [](float u, float v) { return std::min(u, v); },
             .du = "(1 - sign(u - v))/2", .dv = "(1 + sign(u - v))/2", .help = "minimum"},
            {.name = "max", .f2 = [](float u, float v) { return std::max(u, v); },
             .du = "(1 + sign(u - v))/2", .dv = "(1 - sign(u - v))/2", .help = "maximum"},
        };

        struct MacroDef {
            char const* name;
            char const* aliases;
            char const* params;   // pl. "a, b, k = 0.5" — az alapértékes a végén, elhagyható
            char const* body;
            char const* help;
        };

        inline MacroDef const MACROS[] = {
            {"pow",        "",           "a, b",           "a^b",                      "hatvany"},
            {"hypot",      "",           "a, b",           "sqrt(a^2 + b^2)",          "atfogo"},
            {"length",     "",           "a, b, c = 0",    "sqrt(a^2 + b^2 + c^2)",    "vektor hossza"},
            {"clamp",      "",           "u, lo, hi",      "min(max(u, lo), hi)",      "szoritas [lo, hi]-be"},
            {"mod",        "",           "a, b",           "a - b*floor(a/b)",         "maradek (b > 0: [0, b))"},
            {"fract",      "",           "u",              "u - floor(u)",             "tortresz"},
            {"step",       "",           "e, u",           "(1 + sign(u - e))/2",      "lepcso: 0 ha u < e, 1 ha u > e"},
            {"smoothstep", "",           "e0, e1, u",
             "clamp((u - e0)/(e1 - e0), 0, 1)^2 * (3 - 2*clamp((u - e0)/(e1 - e0), 0, 1))",
             "sima atmenet 0 -> 1"},
            {"mix",        "lerp",       "a, b, s",        "a + (b - a)*s",            "linearis keveres"},

            // Éles halmazműveletek (F < 0 = belül konvencióval). A BlobTree-cikk max-ot ír
            // unióra, mert ott a potenciál BELÜL nagy; nálunk F belül negatív, ezért
            // unió = min és metszet = max.
            {"unio",       "union",      "a, b",           "min(a, b)",                "unio"},
            {"metszet",    "intersect",  "a, b",           "max(a, b)",                "metszet"},
            {"kulonbseg",  "subtract",   "a, b",           "max(a, -b)",               "kulonbseg"},

            // Sima (C^inf) halmazműveletek. A gyök alatti +k² miatt az argumentum sosem 0,
            // így a derivált a varraton is véges — ellentétben a cikk R-függvényével,
            // ami ott NaN-t adna. A k a lekerekítés mértéke: az F nagyságrendjéhez kell
            // igazítani, ezért érdemes megadni.
            {"smin",       "",           "a, b, k = 0.5",  "0.5*(a + b - sqrt((a - b)^2 + k^2))", "sima minimum"},
            {"smax",       "",           "a, b, k = 0.5",  "0.5*(a + b + sqrt((a - b)^2 + k^2))", "sima maximum"},
            {"sunio",      "sunion",     "a, b, k = 0.5",  "smin(a, b, k)",            "sima unio"},
            {"smetszet",   "sintersect", "a, b, k = 0.5",  "smax(a, b, k)",            "sima metszet"},
            {"skulonbseg", "ssubtract",  "a, b, k = 0.5",  "smax(a, -b, k)",           "sima kulonbseg"},
        };

        struct ConstDef { char const* name; float value; };
        inline ConstDef const CONSTANTS[] = {
            {"pi", 3.14159265358979f},
            {"e",  2.71828182845905f},
        };

        // A nyelv további foglalt szavai (változók és logikai kulcsszavak).
        inline char const* const KEYWORDS[] = {"x", "y", "z", "and", "or", "not"};

        // --- keresés ------------------------------------------------------------

        inline bool name_matches(char const* name, char const* aliases, std::string_view n) {
            if (n == name) return true;
            std::string_view al(aliases);
            while (!al.empty()) {
                auto sp = al.find(' ');
                if (al.substr(0, sp) == n) return true;
                if (sp == std::string_view::npos) break;
                al.remove_prefix(sp + 1);
            }
            return false;
        }

        inline FuncDef const* find_func(std::string_view n) {
            for (auto const& f : FUNCS) if (name_matches(f.name, f.aliases, n)) return &f;
            return nullptr;
        }
        inline MacroDef const* find_macro(std::string_view n) {
            for (auto const& m : MACROS) if (name_matches(m.name, m.aliases, n)) return &m;
            return nullptr;
        }
        inline ConstDef const* find_const(std::string_view n) {
            for (auto const& c : CONSTANTS) if (n == c.name) return &c;
            return nullptr;
        }

        // Minden név, amit a nyelv lefoglal (álnevekkel együtt). Ebből épül a foglalt
        // nevek listája és a hibaüzenetek "erre gondoltál?" javaslata.
        inline std::vector<std::string> builtin_names() {
            std::vector<std::string> v;
            auto add = [&](char const* name, char const* aliases) {
                v.emplace_back(name);
                std::string_view al(aliases);
                while (!al.empty()) {
                    auto sp = al.find(' ');
                    v.emplace_back(al.substr(0, sp));
                    if (sp == std::string_view::npos) break;
                    al.remove_prefix(sp + 1);
                }
            };
            for (auto const& f : FUNCS)     add(f.name, f.aliases);
            for (auto const& m : MACROS)    add(m.name, m.aliases);
            for (auto const& c : CONSTANTS) add(c.name, "");
            for (auto k : KEYWORDS)         add(k, "");
            return v;
        }

        inline bool is_builtin(std::string_view n) {
            for (auto const& s : builtin_names()) if (s == n) return true;
            return false;
        }

        // Egy táblabeli függvény hívása C++-ból (a DSL és a csomópontok deriváltjai).
        inline Tree call(char const* name, Tree a, Tree b = nullptr) {
            FuncDef const* f = find_func(name);
            if (!f || (f->arity() == 2) != static_cast<bool>(b))
                throw std::logic_error(std::string("nincs ilyen fuggveny/argumentumszam: ") + name);
            return std::make_shared<Hivas>(f, std::move(a), std::move(b));
        }

    }
}

#endif //MATEK_FUGGVENYEK_HPP
