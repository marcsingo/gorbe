
#include <iostream>
#include <vector>

// Feltételezem, hogy ezek a headerek megvannak a korábbi lépésekből
#include "utils/init.hpp"
#include "model/Camera.hpp"
#include "model/Model.hpp"
#include "model/Window.hpp"
#include "objects/Gorbe.hpp"
#include "objects/Vector.hpp"
#include "particle_sampling/ImplicitSurface.cpp"

int main() {
    Utils::init_glfw();
    Window::init(800, 800, "OpenGl Hello World");

    // 1. DEPTH TEST bekapcsolása a 3D-hez!
    glEnable(GL_DEPTH_TEST);

    // 2. 3D-s kamera inicializálása
    Camera3D camera(glm::vec4(0.0f, 0.0f, 800.0f, 800.0f), glm::vec3(0.0f, 0.0f, 5.0f));

    Gorbe gorbe{5.0f};

    while (!Window::window_schould_close()) {
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

        // 3. Ne felejtsd el törölni a GL_DEPTH_BUFFER_BIT-et is a cikluson belül!
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        gorbe.draw(camera);
        Window::event_handling();
    }

    Window::destroy_window();
    return 0;
}


