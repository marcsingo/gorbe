//
// Created by madam on 2026. 03. 07..
//

#ifndef GORBE_WINDOW_HPP
#define GORBE_WINDOW_HPP

#include <cstdint>
#include <functional>
#include <vector>

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

// Egy regisztrált eseménykezelő azonosítója (0 = nincs).
using EventId = std::uint64_t;

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

    // Az eseménykezelők azonosítóval együtt tárolódnak, hogy le lehessen iratkozni
    // (lásd EventId / remove_event). Törléskor csak a függvényt ürítjük ki, a helyét
    // nem vesszük ki a vektorból: így egy esemény KÖZBEN történő leiratkozás sem
    // érvényteleníti a fölötte futó bejárást.
    template<class F>
    struct Slot {
        std::uint64_t id;
        F fn;
    };
    inline static std::uint64_t next_id = 1;

    inline static std::vector<Slot<KeyEvent>> key_events;
    static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);

    inline static std::vector<Slot<MousePosEvent>> mouse_pos_events;
    static void mouse_pos_callback(GLFWwindow* window, double x, double y);

    inline static std::vector<Slot<MouseButtonEvent>> mouse_button_events;
    static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);

    inline static std::vector<Slot<MouseScrollEvent>> mouse_scroll_events;
    static void mouse_scroll_callback(GLFWwindow* window, double x, double y);

    inline static std::vector<Slot<TimePassedEvent>> time_passed_events;

    template<class V>
    static bool erase_from(V& v, std::uint64_t id) {
        for (auto& s : v)
            if (s.id == id && s.fn) { s.fn = nullptr; return true; }
        return false;
    }

    // Ablak/framebuffer átméretezés: frissíti a width/height-ot és a glViewport-ot,
    // hogy resize-kor ne nyúljon szét a kép.
    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);

public:

    static void init(int width, int height, char const * text);
    static int get_width() {return width;}
    static int get_height() {return height;}
    static GLFWwindow* handle() {return window;} // pl. az ImGui backendnek
    static void destroy_window();

    // Az add_* függvények azonosítót adnak vissza; ezzel lehet később leiratkozni.
    // Aki `this`-t kapó lambdát regisztrál, a destruktorában KÖTELESEN iratkozzon le,
    // különben az ablak egy megszűnt objektumra mutató lambdát hívna. Erre való a
    // Window::Subscription (RAII), ami a destruktorában automatikusan leiratkozik.
    static EventId add_key_event(KeyEvent&& f);
    static EventId add_mouse_pos_event(MousePosEvent&& f);
    static EventId add_mouse_button_event(MouseButtonEvent&& f);
    static EventId add_mouse_scroll_event(MouseScrollEvent&& f);
    static EventId add_time_passed_event(TimePassedEvent&& f);

    static void remove_event(EventId id);

    // RAII-előfizetés: a hatókörét elhagyva magától leiratkozik.
    class Subscription {
        EventId id_ = 0;
    public:
        Subscription() = default;
        Subscription(EventId id) : id_(id) {}
        Subscription(Subscription const&) = delete;
        Subscription& operator=(Subscription const&) = delete;
        Subscription(Subscription&& o) noexcept : id_(o.id_) { o.id_ = 0; }
        Subscription& operator=(Subscription&& o) noexcept {
            if (this != &o) { reset(); id_ = o.id_; o.id_ = 0; }
            return *this;
        }
        ~Subscription() { reset(); }
        void reset() {
            if (id_) { Window::remove_event(id_); id_ = 0; }
        }
    };

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