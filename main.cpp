#include "App.hpp"

// A felület-típusok (Sphere, Torus, Ellipsoid, Ellipse, ...) itt vannak definiálva:
#include "particle_sampling/Surface.hpp"

int main() {
    App app{800, 800, "Particle sampling"};

    // Csak a felületet kell kiválasztani. Próbáld ki: Torus, Ellipsoid, Ellipse.
    // Hangoláshoz: app.show<Sphere>({.alpha = 8.0f, .phi = 20.0f});
    app.show<Torus>();



    app.run();
    return 0;
}