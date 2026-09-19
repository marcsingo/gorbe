#ifndef GORBE_SCENE_NAMES_HPP
#define GORBE_SCENE_NAMES_HPP

#include <cctype>
#include <cstdio>
#include <cstring>

// A parser által lefoglalt nevek: a térbeli változók és a beépített függvények.
inline bool is_reserved(char const* n) {
    static char const* const R[] = {
        "x", "y", "z",
        "sin", "cos", "tan", "tg", "ctg", "cot", "ln", "log", "sqrt", "abs", "sign",
        "min", "max", "unio", "union", "metszet", "intersect", "kulonbseg", "subtract",
        "smin", "smax", "sunio", "sunion", "smetszet", "sintersect", "skulonbseg", "ssubtract",
        "and", "or", "not", "pi", "t"};
    for (auto r : R)
        if (std::strcmp(n, r) == 0) return true;
    return false;
}

// Azonosító-formátum: betűvel kezdődik, utána betű/szám/aláhúzás (ezt tudja a parser).
inline bool bad_ident(char const* n) {
    if (!std::isalpha(static_cast<unsigned char>(n[0]))) return true;
    for (char const* c = n; *c; ++c)
        if (!std::isalnum(static_cast<unsigned char>(*c)) && *c != '_') return true;
    return false;
}

// Szabad "prefix + sorszám" név keresése (g1, g2, ... / p1, p2, ... / f1, f2, ...).
// Bármilyen `name` mezős elemekből álló konténerre működik.
template<class Container>
void next_name(Container const& items, char const* prefix, char* out, size_t n) {
    for (int k = 1;; ++k) {
        char candidate[32];
        std::snprintf(candidate, sizeof(candidate), "%s%d", prefix, k);
        bool taken = false;
        for (auto const& it : items)
            if (std::strcmp(it.name, candidate) == 0) { taken = true; break; }
        if (!taken) { std::snprintf(out, n, "%s", candidate); return; }
    }
}

#endif //GORBE_SCENE_NAMES_HPP
