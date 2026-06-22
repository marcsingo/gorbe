#ifndef GORBE_APP_HPP
#define GORBE_APP_HPP

#include <functional>
#include <memory>

#include <glad/glad.h>

#include "utils/init.hpp"
#include "model/Window.hpp"
#include "model/Camera.hpp"
#include "particle_sampling/ImplicitSurface.hpp"

// A teljes boilerplate (GLFW/ablak init, kamera, render loop) egy helyen.
// Tipikus használat a main-ben:
//
//     App app;
//     app.show<Sphere>();   // vagy Torus, Ellipsoid, Ellipse...
//     app.run();
//
class App {
    // init_glfw + Window::init kötelezően a kamera (és minden eseményregisztráció)
    // ELŐTT kell lefusson. Bázis-alobjektumként ez a tagok előtt inicializálódik.
    struct GlContext {
        GlContext(int width, int height, char const *title) {
            Utils::init_glfw();
            Window::init(width, height, title);
            glEnable(GL_DEPTH_TEST);
        }
    } gl_context;

    Camera3D camera;

    std::shared_ptr<void> surface_keepalive;        // életben tartja a kiválasztott felületet
    std::function<void(Camera const &)> draw_fn;    // típus-független rajzolás

    glm::vec3 background{1.0f, 1.0f, 1.0f};

public:
    explicit App(int width = 800, int height = 800, char const *title = "Particle sampling")
        : gl_context(width, height, title),
          camera(glm::vec4(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
                 glm::vec3(0.0f, -8.0f, 8.0f), -90.0f, 45.0f) {}

    Camera3D &get_camera() { return camera; }
    void set_background(glm::vec3 color) { background = color; }

    // A felhasználó fő belépési pontja: kiválasztja a felület típusát, a hozzá tartozó
    // occluder automatikusan adódik. Visszaadja a felületet, ha menet közben kell rá hivatkozni.
    template<class SurfaceT>
    ImplicitSurface<SurfaceT> &show(SimParams params = {}) {
        auto surface = std::make_shared<ImplicitSurface<SurfaceT>>(camera, params);
        surface_keepalive = surface;
        draw_fn = [surface](Camera const &cam) { surface->draw(cam); };
        return *surface;
    }

    void run() {
        while (!Window::window_schould_close()) {
            glClearColor(background.r, background.g, background.b, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            if (draw_fn) draw_fn(camera);
            Window::event_handling();
        }
        Window::destroy_window();
    }
};

#endif //GORBE_APP_HPP
