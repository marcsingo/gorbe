#ifndef GORBE_PARTICLE_VIEW_HPP
#define GORBE_PARTICLE_VIEW_HPP

#include <algorithm>

// A részecske-korongok MEGJELENÍTÉSI mérete.
//
// Minden részecskét egy lapos korong jelöl, aminek a sugara alapból σ/2. Ennek oka,
// hogy a Witkin-féle taszítás a szomszédokat nagyjából σ távolságra állítja be, tehát
// σ/2 sugárnál a korongok ÉPPEN ÖSSZEÉRNEK — így a felület folytonos hártyának látszik.
//
// Ez viszont el is fedi a mintavételezést: nem látszik, hol vannak valójában a pontok,
// és mennyire egyenletes az eloszlásuk. A `gap` ebből vesz el (vagy ad hozzá): a
// szomszédos korongok szélei közt látszó rés kb. σ·gap lesz.
//
// FONTOS: ez CSAK a rajzolás — a szimulációt (σ-t, a taszítást, a részecskék helyét)
// nem érinti, tehát a csúszka mozgatása nem indítja újra és nem billenti ki a
// mintavételezést. A `d` csúszka az, ami a TÉNYLEGES részecsketávolságot állítja.
//
// Program-szintű, nem fülönkénti: megjelenítési ízlés, nem a jelenet tulajdonsága.
namespace ParticleView {

    inline constexpr float GAP_MIN = -0.60f;   // átfedő korongok (tömörebb felület)
    inline constexpr float GAP_MAX =  0.90f;   // apró pontok, tág réssel

    inline float gap = 0.0f;                   // 0 = a korongok éppen összeérnek

    // A kirajzolandó korong sugara az adott σ-hoz.
    inline float disk_radius(float sigma) {
        float const g = std::clamp(gap, GAP_MIN, GAP_MAX);
        float const r = 0.5f * sigma * (1.0f - g);
        // A `>` NaN-ra is hamis, tehát a hibás σ nem fordítja ki a korongot.
        return r > 0.0f ? r : 0.0f;
    }

}

#endif //GORBE_PARTICLE_VIEW_HPP
