#ifndef GORBE_OBJECT_CONTROLLER_HPP
#define GORBE_OBJECT_CONTROLLER_HPP

#include "../model/Include.hpp"
#include "../particle_sampling/ParticleSystem.hpp"

// ===========================================================================
// Egy térbeli objektum VEZÉRLŐJE: az idő és a bevitel.
//
//  * Óra: gyűjti az eltelt időt, és megmondja, esedékes-e egy szimulációs lépés
//    (advance). Magát a lépést a jelenet vezérlője futtatja, az összes objektumét
//    PÁRHUZAMOSAN (app/Controller.hpp). Amíg nem fut (`running`), nincs lépés: a
//    háttérben lévő fülek objektumai állnak, a részecskék állapota megmarad, és
//    visszaváltáskor onnan folytatódik.
//  * Bevitel: Shift + bal kattintás új kontrollpontot tesz le, bal gombbal húzva
//    mozgatható. Csak az aktív jelenet objektumai reagálnak (`input_enabled`).
//
// Az eseménykezelők `this`-t kapnak; a Subscription a destruktorban leiratkoztat,
// ezért a példány nem másolható, de szabadon törölhető.
// ===========================================================================
class ObjectController {
    ParticleSystem& model;
    Camera const& camera;

    float sim_accum = 0.0f;
    int   selected  = -1;      // a húzott kontrollpont indexe
    bool  shift_on  = false;

    // Ilyen gyakran lép a szimuláció (másodperc). A lépés dt-je az azóta eltelt idő.
    static constexpr float SIM_PERIOD = 0.03f;

    Window::Subscription btn_sub, key_sub, drag_sub;

public:
    bool running = false;
    bool input_enabled = true;

    ObjectController(ParticleSystem& m, Camera const& cam) : model(m), camera(cam) {
        btn_sub = Window::Subscription(Window::add_mouse_button_event([this](auto p) {
            if (!input_enabled) return;
            if (p.button != GLFW_MOUSE_BUTTON_LEFT || (p.mods & GLFW_MOD_ALT)) return;
            if (p.action == GLFW_RELEASE) { selected = -1; return; }
            if (p.action != GLFW_PRESS) return;

            if (shift_on) {
                model.add_control(camera.get_mouse_pos_on_plane(glm::vec3(0.0f), camera.get_front()));
                return;
            }
            auto const& cs = model.controls();
            for (int i = 0; i < static_cast<int>(cs.size()); ++i) {
                auto mpos = camera.get_mouse_pos_on_plane(cs[i].p, camera.get_front());
                if (glm::length(mpos - cs[i].p) < ParticleSystem::CONTROL_RADIUS) {
                    selected = i;
                    break;
                }
            }
        }));

        key_sub = Window::Subscription(Window::add_key_event([this](auto p) {
            if (!input_enabled) { shift_on = false; return; }
            if (p.key == GLFW_KEY_LEFT_SHIFT || p.key == GLFW_KEY_RIGHT_SHIFT)
                shift_on = (p.action == GLFW_PRESS);
        }));

        drag_sub = Window::Subscription(Window::add_time_passed_event([this](auto ev) {
            if (!input_enabled || selected < 0) return;
            auto const& cs = model.controls();
            if (selected >= static_cast<int>(cs.size())) { selected = -1; return; }
            auto target = camera.get_mouse_pos_on_plane(cs[selected].p, camera.get_front());
            model.drag_control(selected, target, static_cast<float>(ev.dt));
        }));
    }

    ObjectController(ObjectController const&) = delete;
    ObjectController& operator=(ObjectController const&) = delete;

    // Az eltelt idő hozzáadása. Ha esedékes egy lépés, annak dt-jét adja vissza
    // (az utolsó lépés óta eltelt időt), különben 0-t. Leállítva mindig 0.
    float advance(float dt) {
        if (!running) return 0.0f;
        sim_accum += dt;
        if (sim_accum < SIM_PERIOD) return 0.0f;
        float const step_dt = sim_accum;
        sim_accum = 0.0f;
        return step_dt;
    }

    // Leállításkor a félig gyűjtött időt is eldobjuk, hogy az újraindítás ne
    // egy nagy lépéssel kezdjen.
    void set_running(bool v) {
        running = v;
        if (!v) sim_accum = 0.0f;
    }

    void set_input_enabled(bool v) {
        input_enabled = v;
        if (!v) selected = -1;
    }
};

#endif //GORBE_OBJECT_CONTROLLER_HPP
