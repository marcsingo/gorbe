#include "App.hpp"

// A felület-típusok (Sphere, Torus, Ellipsoid, Ellipse, ...) itt vannak definiálva:
#include "particle_sampling/Surface.hpp"

// Saját ImGui felülethez:
// #include "imgui.h"

int main() {
    App app{800, 800, "Particle sampling"};

    // Csak a felületet kell kiválasztani. Próbáld ki: Torus, Ellipsoid, Ellipse.
    // Hangoláshoz: app.show<Sphere>({.alpha = 8.0f, .phi = 20.0f});
    app.show<Teszt>();

    // Saját ImGui panel (különben egy alap demo-panel jelenik meg):
    // app.set_gui([&] {
    //     ImGui::Begin("Vezerlopult");
    //     ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    //     ImGui::End();
    // });

    app.run();
    return 0;
}