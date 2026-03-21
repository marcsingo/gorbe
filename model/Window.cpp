#include "Window.hpp"

#include "../utils/init.hpp"

void Window::init(int width, int height, char const * text) {

        Window::width = width;
        Window::height = height;
        Window::text = text;
        window = Utils::create_window(width, height, text);


    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, mouse_pos_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, mouse_scroll_callback);
}


void Window::destroy_window() {
    glfwDestroyWindow(window);
    glfwTerminate();
}


void Window::key_callback(GLFWwindow *window, int key, int scancode, int action, int mode) {
    for (auto &f : key_events) {
        f(key, scancode, action, mode);
    }
}

void Window::add_key_event(KeyEvent&& f) {
    key_events.push_back(std::move(f));
}

void Window::mouse_pos_callback(GLFWwindow *window, double x, double y) {
    for (auto &f : mouse_pos_events) {
        f(x, y);
    }
}

void Window::add_mouse_pos_event(MousePosEvent&& f) {
    mouse_pos_events.push_back(std::move(f));
}

void Window::mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {
    for (auto& f: mouse_button_events) {
        f(button, action, mods);
    }
}

void Window::add_mouse_button_event(MouseButtonEvent&& f) {
    mouse_button_events.push_back(std::move(f));
}

void Window::mouse_scroll_callback(GLFWwindow *window, double x, double y) {
    for (auto& f : mouse_scroll_events) {
        f(x, y);
    }
}

void Window::add_mouse_scroll_event(MouseScrollEvent&& f) {
    mouse_scroll_events.push_back(std::move(f));
}

void Window::event_handling() {
    static double t = glfwGetTime();
    static double dt = 0;
    glfwSwapBuffers(window);
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (dt != 0.0f)
    for (auto &f : time_passed_events) {
        f(t, dt);
    }
    glfwPollEvents();

    double now = glfwGetTime();
    dt = now - t;
    t = now;
}

void Window::add_time_passed_event(TimePassedEvent &&f) {
    time_passed_events.push_back(std::move(f));
}





