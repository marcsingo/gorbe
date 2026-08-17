#ifndef GORBE_RAYTRACE_PALETTE_HPP
#define GORBE_RAYTRACE_PALETTE_HPP

#include <glm.hpp>

// Előre megadott színlista, amiből az alakzatok színt kapnak.
//
// Az értékek szándékosan nem teljesen telítettek: a műanyag-árnyaláson (színes
// diffúz + FEHÉR csúcsfény) a telített alapszín kiég, és elveszik a forma.
namespace Raytrace {

    struct NamedColor {
        char const* name;
        glm::vec3   rgb;
    };

    inline NamedColor const PALETTE[] = {
        {"Piros",        {0.82f, 0.24f, 0.22f}},
        {"Narancs",      {0.90f, 0.49f, 0.16f}},
        {"Sarga",        {0.92f, 0.76f, 0.20f}},
        {"Zold",         {0.31f, 0.65f, 0.32f}},
        {"Turkiz",       {0.20f, 0.65f, 0.62f}},
        {"Kek",          {0.24f, 0.47f, 0.82f}},
        {"Lila",         {0.51f, 0.34f, 0.72f}},
        {"Rozsaszin",    {0.85f, 0.42f, 0.60f}},
        {"Vilagosszurke",{0.78f, 0.78f, 0.80f}},
        {"Sotetszurke",  {0.32f, 0.34f, 0.38f}},
    };

    inline constexpr int PALETTE_COUNT = static_cast<int>(sizeof(PALETTE) / sizeof(PALETTE[0]));

    inline glm::vec3 palette_color(int idx) {
        if (idx < 0 || idx >= PALETTE_COUNT) idx = 0;
        return PALETTE[idx].rgb;
    }

    inline char const* palette_name(int idx) {
        if (idx < 0 || idx >= PALETTE_COUNT) idx = 0;
        return PALETTE[idx].name;
    }

}

#endif //GORBE_RAYTRACE_PALETTE_HPP
