//
// Created by madam on 2026. 03. 07..
//

#ifndef GORBE_WINDOW_HPP
#define GORBE_WINDOW_HPP

#include <list>
#include <functional>

#include <glad/glad.h>

#include "vec2.hpp"
#include "GLFW/glfw3.h"

struct KeyEventInformation {
    int key, scancode, action, mods;
};

struct MousePosEventInformation {
    double x, y;
    glm::vec2 operator()() const { return {x, y};}
};

struct MouseButtonEventInformation {
    int button, action, mods;
};

struct MouseScrollEventInformation {
    double offsetX, offsetY;
};

struct TimePassedEventInformation {
    double t, dt;
};

typedef std::function<void(KeyEventInformation)> KeyEvent;
typedef std::function<void(MousePosEventInformation)> MousePosEvent;
typedef std::function<void(MouseButtonEventInformation)> MouseButtonEvent;
typedef std::function<void(MouseScrollEventInformation)> MouseScrollEvent;
typedef std::function<void(TimePassedEventInformation)> TimePassedEvent;

class Window {

    inline static GLFWwindow* window;

    inline static int width;
    inline static int height;
    inline static char const * text = nullptr;

    inline static std::vector<KeyEvent> key_events;
    static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);

    inline static std::vector<MousePosEvent> mouse_pos_events;
    static void mouse_pos_callback(GLFWwindow* window, double x, double y);

    inline static std::vector<MouseButtonEvent> mouse_button_events;
    static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);

    inline static std::vector<MouseScrollEvent> mouse_scroll_events;
    static void mouse_scroll_callback(GLFWwindow* window, double x, double y);

    inline static std::vector<TimePassedEvent> time_passed_events;

public:

    static void init(int width, int height, char const * text);
    static int get_width() {return width;}
    static int get_height() {return height;}
    static GLFWwindow* handle() {return window;} // pl. az ImGui backendnek
    static void destroy_window();

    static void add_key_event(KeyEvent&& f);
    static void add_mouse_pos_event(MousePosEvent&& f);
    static void add_mouse_button_event(MouseButtonEvent&& f);
    static void add_mouse_scroll_event(MouseScrollEvent&& f);
    static void add_time_passed_event(TimePassedEvent&& f);

    static MousePosEventInformation get_mouse_info();

    static void disable_cursor() {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }

    static bool window_schould_close() {
        return glfwWindowShouldClose(window);
    }

    static void event_handling();
};


#endif //GORBE_WINDOW_HPP