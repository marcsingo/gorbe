
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

    glEnable(GL_DEPTH_TEST);

    Camera3D camera(glm::vec4(0.0f, 0.0f, 800.0f, 800.0f), glm::vec3(0.0f, 0.0f, 10.0f));
    ImplicitSurface<4> is{camera};

    while (!Window::window_schould_close()) {
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

        // 3. Ne felejtsd el törölni a GL_DEPTH_BUFFER_BIT-et is a cikluson belül!
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        is.draw(camera);
        // gorbe.draw(camera);
        Window::event_handling();
    }

    Window::destroy_window();
    return 0;
}


