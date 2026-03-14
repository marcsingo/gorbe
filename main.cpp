
#include <iostream>
#include <vector>

// Feltételezem, hogy ezek a headerek megvannak a korábbi lépésekből
#include "utils/init.hpp"
#include "model/Camera.hpp"
#include "model/Model.hpp"
#include "model/Window.hpp"
#include "objects/Gorbe.hpp"
#include "objects/Vector.hpp"

int main() {
    // 1. Ablak létrehozása (a korábban írt Init namespace-ből)
    Utils::init_glfw();
    Window::init(800, 800, "OpenGl Hello World");

    float cws = 5.0f;

    Camera2D camera(glm::vec4(0.0f, 0.0f, 800.0f, 800.0f));
    camera.walls = glm::vec4(-cws, cws, -cws, cws); // Bal, Jobb, Alsó, Felső

    Gorbe gorbe{cws};
    // Normals ns;
    // --- FŐ RENDERELŐ CIKLUS ---
    while (!Window::window_schould_close()) {
        glClearColor(1, 1, 1, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        gorbe.draw(camera);
        // ns.set_points(gorbe.get_vertices(), gorbe.get_normals());
        // ns.draw(camera);
        Window::event_handling();
    }

    Window::destroy_window();
    return 0;
}