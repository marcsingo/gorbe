
#include <iostream>
#include <vector>

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

    Camera3D camera(glm::vec4(0.0f, 0.0f, 800.0f, 800.0f), glm::vec3(0.0f, -8.0f, 8.0f), -90.0f, 45.0f);
    ImplicitSurface<4> is{camera};   // Sphere=<4>, Torus=<2>

    while (!Window::window_schould_close()) {
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        is.draw(camera);
        Window::event_handling();
    }

    Window::destroy_window();
    return 0;
}