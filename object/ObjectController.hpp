#ifndef GORBE_OBJECT_CONTROLLER_HPP
#define GORBE_OBJECT_CONTROLLER_HPP

#include "../particle_sampling/ParticleSystem.hpp"

// ===========================================================================
// Egy térbeli objektum VEZÉRLŐJE: az óra.
//
// Gyűjti az eltelt időt, és megmondja, esedékes-e egy szimulációs lépés
// (advance). Magát a lépést a jelenet vezérlője futtatja, az összes objektumét
// PÁRHUZAMOSAN (app/Controller.hpp). Amíg nem fut (`running`), nincs lépés: a
// háttérben lévő fülek objektumai állnak, a részecskék állapota megmarad, és
// visszaváltáskor onnan folytatódik.
//
// A kontrollpontok egér-kezelése a JELENET vezérlőjében van: egy kattintás így
// pontosan egy objektumot érint (azt, amelyikre kattintottunk).
// ===========================================================================
class ObjectController {
    float sim_accum = 0.0f;

    // Ilyen gyakran lép a szimuláció (másodperc). A lépés dt-je az azóta eltelt idő.
    static constexpr float SIM_PERIOD = 0.03f;

public:
    bool running = false;

    // Az eltelt idő hozzáadása. Ha esedékes egy lépés, annak dt-jét adja vissza
    // (az utolsó lépés óta eltelt időt), különben 0-t. Leállítva mindig 0.
    float advance(float dt) {
        if (!running) return 0.0f;
        sim_accum += dt;
        if (sim_accum < SIM_PERIOD) return 0.0f;
        float const step_dt = sim_accum;
        sim_accum = 0.0f;
        return step_dt;
    }

    // Leállításkor a félig gyűjtött időt is eldobjuk, hogy az újraindítás ne
    // egy nagy lépéssel kezdjen.
    void set_running(bool v) {
        running = v;
        if (!v) sim_accum = 0.0f;
    }
};

#endif //GORBE_OBJECT_CONTROLLER_HPP
