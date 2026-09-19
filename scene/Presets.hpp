#ifndef GORBE_SCENE_PRESETS_HPP
#define GORBE_SCENE_PRESETS_HPP

#include <cstdio>
#include <string>
#include <vector>

#include "../particle_sampling/WarpPresets.hpp"
#include "../raytrace/Palette.hpp"
#include "Document.hpp"
#include "Names.hpp"

// ---------------------------------------------------------------------------
// Előre elkészített alakzatok (sablonok)
//
// Egy sablon = megjelenő név + az új alakzat nevének alapja + a képlet + a hozzá
// tartozó LOKÁLIS paraméterek kezdőértékkel. Hozzáadáskor ezekből egy teljesen
// közönséges alakzat születik, ami utána szabadon szerkeszthető.
//
// A képletek szándékosan POLINOMIÁLISAK (nincs bennük sqrt), ahol lehet: a
// sqrt(u) deriváltja u=0-ban végtelen — a felületen (F=0) ez pont a rossz hely
// lenne. A sima halmazműveletekben a gyök alatt mindig van egy +k² tag, ezért ott
// biztonságos.
// ---------------------------------------------------------------------------
struct PresetParam {
    char const* name;
    float       value;
};

struct Preset {
    char const* group;       // csoportfejléc a lenyílóban
    char const* label;       // ami a lenyíló listában látszik
    char const* base_name;   // az új alakzat nevének alapja (gomb1, gomb2, ...)
    char const* formula;
    std::vector<PresetParam> params;
    char const* domain = ""; // opcionális tartomány-feltétel (üres = korlátlan)
};

// A halmazműveletek a listában ELŐTTE álló két alakzatra hivatkoznak `f1`/`f2` néven —
// ezeket a saját alakzataid nevére kell átírni. Az F<0 = belül konvenció miatt az unió
// min és a metszet max (a BlobTree-cikk fordítva írja, mert ott a potenciál belül nagy).
inline std::vector<Preset> const PRESETS = {
    {"Alapalakzatok", "Gomb",                 "gomb",        "x^2 + y^2 + z^2 - r^2",
        {{"r", 1.0f}}},
    {"Alapalakzatok", "Ellipszoid",           "ellipszoid",  "x^2/a^2 + y^2/b^2 + z^2/c^2 - 1",
        {{"a", 3.0f}, {"b", 2.0f}, {"c", 1.0f}}},
    {"Alapalakzatok", "Torusz",               "torusz",      "(x^2 + y^2 + z^2 + R^2 - r^2)^2 - 4*R^2*(x^2 + y^2)",
        {{"R", 3.0f}, {"r", 1.0f}}},
    {"Alapalakzatok", "Ellipszis (ell. henger)", "ellipszis", "x^2/a^2 + y^2/b^2 - 1",
        {{"a", 3.0f}, {"b", 1.5f}}},
    {"Alapalakzatok", "Henger",               "henger",      "x^2 + y^2 - r^2",
        {{"r", 2.0f}}},
    {"Alapalakzatok", "Kup",                  "kup",         "x^2 + y^2 - a^2*z^2",
        {{"a", 1.0f}}},
    {"Alapalakzatok", "Hiperboloid (1 kopeny)", "hiperboloid", "x^2/a^2 + y^2/b^2 - z^2/c^2 - 1",
        {{"a", 1.0f}, {"b", 1.0f}, {"c", 1.0f}}},
    {"Alapalakzatok", "Lekerekitett kocka",   "kocka",       "x^4 + y^4 + z^4 - a^4",
        {{"a", 1.5f}}},

    // Onmagaban vegtelen feluletek, tartomany-feltetellel veges darabra szoritva.
    // A feltetel NEM epul be F-be: igy nyers szelu felulet-darabot kapunk, nem
    // egy zart test hatarat (ami a vagolapokat is tartalmazna).
    {"Vegtelen + tartomany", "Sik (negyzet darab)", "sik", "z", {{"m", 3.0f}},
        "x > -m and x < m and y > -m and y < m"},
    {"Vegtelen + tartomany", "Henger (veges hosszu)", "cso", "x^2 + y^2 - r^2",
        {{"r", 1.0f}, {"h", 3.0f}}, "z > -h and z < h"},

    // Éles (C0) halmazműveletek: a varraton törés van, a gradiens ugrik.
    {"Eles muveletek", "Unio  (f1 U f2)",       "unio",       "unio(f1, f2)",       {}},
    {"Eles muveletek", "Metszet  (f1 ^ f2)",    "metszet",    "metszet(f1, f2)",    {}},
    {"Eles muveletek", "Kulonbseg  (f1 - f2)",  "kulonbseg",  "kulonbseg(f1, f2)",  {}},

    // Sima (C^inf) halmazműveletek: k a lekerekítés mértéke. Ezek gradiense a
    // varraton is véges, ezért a részecske-szimulációhoz ezek a biztonságosak.
    {"Sima muveletek", "Sima unio (blend)",     "sunio",      "sunio(f1, f2, k)",      {{"k", 0.5f}}},
    {"Sima muveletek", "Sima metszet",          "smetszet",   "smetszet(f1, f2, k)",   {{"k", 0.5f}}},
    {"Sima muveletek", "Sima kulonbseg",        "skulonbseg", "skulonbseg(f1, f2, k)", {{"k", 0.5f}}},
};

// Új, üres alakzat a lista végén, szabad `base1, base2, ...` néven és a palettán
// körbeforgó színnel. A lista végére szúrás biztonságos: std::list, a meglévő
// alakzatok (és a lokálisaik címe) nem mozdulnak.
inline Shape& new_shape(std::list<Shape>& shapes, char const* base_name) {
    auto& s = shapes.emplace_back();
    next_name(shapes, base_name, s.name, sizeof(s.name));
    s.color_idx = (static_cast<int>(shapes.size()) - 1) % Raytrace::PALETTE_COUNT;
    return s;
}

// Új alakzat felvétele sablonból: a képlet és a lokális paraméterek is bemásolódnak.
// Innentől teljesen közönséges alakzat, szabadon szerkeszthető.
inline Shape& add_preset(std::list<Shape>& shapes, Preset const& pr) {
    auto& s = new_shape(shapes, pr.base_name);
    std::snprintf(s.formula, sizeof(s.formula), "%s", pr.formula);
    std::snprintf(s.domain,  sizeof(s.domain),  "%s", pr.domain);
    for (auto const& pp : pr.params) {
        auto& p = s.locals.emplace_back();
        std::snprintf(p.name, sizeof(p.name), "%s", pp.name);
        p.value = pp.value;
        p.fit_range_to_value();
    }
    return s;
}

// Warp hozzáadása sablonból. A sablon paraméterei az alakzat lokálisai közé kerülnek,
// ütközésmentes néven; a $1/$2 helyére ezek a nevek kerülnek a kifejezésekbe.
inline void add_warp(Shape& s, WarpPresets::Preset const& wp) {
    Warp w;
    std::snprintf(w.name, sizeof(w.name), "%s", wp.label);
    std::vector<std::string> names;
    for (auto const& pp : wp.params) {
        auto& p = s.locals.emplace_back();
        next_name(s.locals, pp.name, p.name, sizeof(p.name));
        p.value = pp.value;
        p.min   = pp.min;
        p.max   = pp.max;
        names.emplace_back(p.name);
    }
    WarpPresets::fill_template(w.fx, sizeof(w.fx), wp.fx, names);
    WarpPresets::fill_template(w.fy, sizeof(w.fy), wp.fy, names);
    WarpPresets::fill_template(w.fz, sizeof(w.fz), wp.fz, names);
    s.warps.push_back(w);
}

#endif //GORBE_SCENE_PRESETS_HPP
