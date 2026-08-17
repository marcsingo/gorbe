#ifndef GORBE_WARPPRESETS_HPP
#define GORBE_WARPPRESETS_HPP

#include <cstdio>
#include <string>
#include <vector>

// Előre elkészített tér-warpok.
//
// Egy warp három kifejezés (x', y', z'), amiket x, y, z helyére helyettesítünk.
// A jelentésük a VISSZAFELÉ (tér -> alakzat) leképezés, ezért az itt szereplő
// képletek az adott deformáció INVERZEI — lásd particle_sampling/Transform.hpp.
// Például a `+a` szöggel forgató warphoz a `-a`-val forgató kifejezés tartozik.
//
// A `$1`, `$2`, ... helyére a hozzáadáskor generált (ütközésmentes) paraméternevek
// kerülnek, és a paraméterek az alakzat lokálisai közé kerülnek — így csúszkával
// állíthatók, és élőben hatnak.
//
// Azért van külön fejlécben (nem a main.cpp-ben), hogy a teszt PONTOSAN ezt az
// adatot ellenőrizze, ne egy kézzel lemásolt párját.
namespace WarpPresets {

    struct ParamDesc {
        char const* name;    // csak ALAP: az ütközésmentes név ebből képződik
        float       value;
    };

    struct Preset {
        char const* label;
        char const* fx;
        char const* fy;
        char const* fz;
        std::vector<ParamDesc> params;
        char const* hint = "";
    };

    inline std::vector<Preset> const ALL = {
        // --- Affin lépések. Ugyanaz, mint a Tulajdonságok panel "Transzformacio"
        // szekciója, DE a láncba illeszthető, tehát tetszőleges sorrendben
        // keverhető a deformációkkal (pl. csavarás -> eltolás -> újabb csavarás).
        {"Eltolas",
         "x - $1", "y - $2", "z - $3",
         {{"tx", 0.0f}, {"ty", 0.0f}, {"tz", 0.0f}},
         "eltolas x / y / z irányban"},

        {"Forgatas z korul",
         "x*cos($1*pi/180) + y*sin($1*pi/180)",
         "0 - x*sin($1*pi/180) + y*cos($1*pi/180)",
         "z",
         {{"rz", 0.0f}},
         "FOKBAN (a pi/180 valtja radianra)"},

        {"Forgatas x korul",
         "x",
         "y*cos($1*pi/180) + z*sin($1*pi/180)",
         "0 - y*sin($1*pi/180) + z*cos($1*pi/180)",
         {{"rx", 0.0f}},
         "FOKBAN"},

        {"Forgatas y korul",
         "x*cos($1*pi/180) - z*sin($1*pi/180)",
         "y",
         "x*sin($1*pi/180) + z*cos($1*pi/180)",
         {{"ry", 0.0f}},
         "FOKBAN"},

        {"Skalazas",
         "x/$1", "y/$2", "z/$3",
         {{"sx", 1.0f}, {"sy", 1.0f}, {"sz", 1.0f}},
         "tengelyenkent; 1 = valtozatlan, 0 NEM lehet"},

        // --- Deformációk ---
        {"Csavaras (twist) z korul",
         "x*cos($1*z) + y*sin($1*z)",
         "0 - x*sin($1*z) + y*cos($1*z)",
         "z",
         {{"tw", 0.30f}},
         "a z tengely menten csavarja; $1 = radian/egyseg"},

        {"Kuposítás (taper) z menten",
         "x/(1 + $1*z)", "y/(1 + $1*z)", "z",
         {{"tp", 0.15f}},
         "FIGYELEM: 1 + k*z = 0 helyen szingularis"},

        {"Nyiras (shear) x-ben, z szerint",
         "x - $1*z", "y", "z",
         {{"sh", 0.30f}},
         "a magassaggal aranyosan tolja x-ben"},

        {"Hullam (wave) z-ben, x szerint",
         "x", "y", "z - $1*sin($2*x)",
         {{"wa", 0.30f}, {"wf", 1.00f}},
         "$1 = amplitudo, $2 = frekvencia"},

        {"Egyedi (ures)", "x", "y", "z", {},
         "irj sajatot: a ter -> alakzat lekepezest"},
    };

    // A "$1", "$2", ... helyettesítése a tényleges paraméternevekkel.
    inline void fill_template(char* out, std::size_t n, char const* tpl,
                              std::vector<std::string> const& names) {
        std::string r;
        for (char const* c = tpl; *c; ++c) {
            if (*c == '$' && c[1] >= '1' && c[1] <= '9') {
                std::size_t idx = static_cast<std::size_t>(c[1] - '1');
                if (idx < names.size()) r += names[idx];
                ++c;
            } else {
                r += *c;
            }
        }
        std::snprintf(out, n, "%s", r.c_str());
    }

}

#endif //GORBE_WARPPRESETS_HPP
