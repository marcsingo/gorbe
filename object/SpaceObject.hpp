#ifndef GORBE_SPACE_OBJECT_HPP
#define GORBE_SPACE_OBJECT_HPP

#include "ObjectController.hpp"
#include "ObjectView.hpp"
#include "../particle_sampling/ParticleSystem.hpp"

// ===========================================================================
// Egy objektum, ami megjelenik a térben — egy alakzat a jelenetben.
//
// Három részből áll, MVC szerint:
//   model()      ParticleSystem    a felület és a részecskék + a szimuláció (GL nélkül)
//   view()       ObjectView        a kirajzolás (a meglévő Model ősosztályra épül)
//   controller() ObjectController  az óra (mikor lép) és a bevitel (kontrollpontok)
//
// A modell nem tud a másik kettőről; a nézet csak olvassa, a vezérlő lépteti.
// Induláskor üres és áll: a jelenet a képlet beállítása után indítja (start()).
// ===========================================================================
class SpaceObject {
    ParticleSystem   model_;
    ObjectView       view_;
    ObjectController controller_;

public:
    explicit SpaceObject(Camera const& camera) : controller_(model_, camera) {}

    SpaceObject(SpaceObject const&) = delete;
    SpaceObject& operator=(SpaceObject const&) = delete;

    ParticleSystem&   model()      { return model_; }
    ObjectView&       view()       { return view_; }
    ObjectController& controller() { return controller_; }

    // Új futás a már beállított felületen: friss részecskék, és az óra indul.
    void start() {
        model_.restart();
        controller_.set_running(true);
    }

    // Üres, álló állapot (a képlet eldobásakor).
    void stop() {
        model_.clear();
        controller_.set_running(false);
    }

    void draw(Camera const& camera) { view_.draw(model_, camera); }
};

#endif //GORBE_SPACE_OBJECT_HPP
