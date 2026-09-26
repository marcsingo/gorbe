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
//   controller() ObjectController  az óra (mikor lép)
//
// A modell nem tud a másik kettőről; a nézet csak olvassa, a vezérlő lépteti.
// Induláskor üres és áll: a jelenet a képlet beállítása után indítja (start()).
// ===========================================================================
class SpaceObject {
    ParticleSystem   model_;
    ObjectView       view_;
    ObjectController controller_;

public:
    SpaceObject() = default;

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

    // `dragged`: a húzott kontrollpont indexe (kiemelve rajzoljuk), vagy -1.
    void draw(Camera const& camera, int dragged = -1, std::vector<glm::vec3> const* pts = nullptr) {
        view_.draw(model_, camera, dragged, pts);
    }
};

#endif //GORBE_SPACE_OBJECT_HPP
