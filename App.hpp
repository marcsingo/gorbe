#ifndef GORBE_APP_HPP
#define GORBE_APP_HPP

#include <functional>

#include <glad/glad.h>

#include "utils/init.hpp"
#include "model/Window.hpp"
#include "model/Camera.hpp"
#include "model/Gui.hpp"
#include "model/Axes.hpp"
#include "model/Shader.hpp"
#include "model/Framebuffer.hpp"
#include "model/Viewport.hpp"

// A boilerplate (GLFW/ablak init, ImGui, render loop, a nézet framebuffere) egy helyen.
//
// A JELENET nincs itt: a fülek saját kamerát és saját mintavételezőket tartanak, mert
// fülönként külön nézet és külön szimuláció kell. Az App csak annyit tud, hogy minden
// frame-ben meg kell hívnia a GUI- és a rajzoló visszahívást.
class App {
    // init_glfw + Window::init kötelezően minden eseményregisztráció ELŐTT kell
    // lefusson. Bázis-alobjektumként ez a tagok előtt inicializálódik.
    struct GlContext {
        GlContext(int width, int height, char const *title) {
            Utils::init_glfw();
            Window::init(width, height, title);
            glEnable(GL_DEPTH_TEST);
        }
    } gl_context;

    // Koordináta-tengelyek (piros=x, zöld=y, kék=z). A gl_context után jön létre,
    // így a Model (VAO/VBO) és a shader már érvényes GL-kontextusban épül fel.
    Axes axes;

    std::function<void()> draw_fn;   // a jelenet kirajzolása (a hívó tudja, melyiké)
    std::function<void()> gui_fn;    // a felhasználó ImGui-paneljei

    glm::vec3 background{1.0f, 1.0f, 1.0f};

    // A jelenet ide rajzolódik, és innen kerül ki egy ImGui-ablakba.
    Framebuffer scene_fbo;
    int want_w = 640, want_h = 480;   // a következő frame-re kért méret

public:
    // A jelenetben a z a "függőleges" (a sík-sablon z=0, a henger a z mentén áll),
    // ezért az alapnézet felülről néz az origóra.
    //
    // A szempont SZÁNDÉKOSAN nincs rajta az YZ síkon: onnan nézve a +Y és a +Z tengely
    // pontosan ugyanabba a képernyő-irányba vetülne, tehát fedné egymást.
    static constexpr glm::vec3 DEFAULT_EYE{-14.0f, -14.0f, 16.0f};

    explicit App(int width = 800, int height = 800, char const *title = "Particle sampling")
        : gl_context(width, height, title) {
        Gui::init(Window::handle());
    }

    Axes &get_axes() { return axes; }
    void set_background(glm::vec3 color) { background = color; }

    // Saját ImGui UI: minden frame-ben lefut (ImGui::Begin/End hívásokkal).
    void set_gui(std::function<void()> fn) { gui_fn = std::move(fn); }
    // A jelenet kirajzolása; a hívó dönti el, melyik fül tartalmát rajzolja.
    void set_draw(std::function<void()> fn) { draw_fn = std::move(fn); }

    // A 3D nézet textúrája. Az ImGui::Image ezt teszi ki a viewport-ablakba.
    unsigned int viewport_texture() const { return scene_fbo.texture(); }

    // A GUI ezzel kéri a nézet méretét; a változás a KÖVETKEZŐ frame elején lép
    // életbe. Szándékosan így: ha a textúra a frame közepén épülne újra, az ImGui
    // egy már rögzített (és ekkorra érvénytelen) azonosítóra hivatkozna.
    void request_viewport_size(int w, int h) { want_w = w; want_h = h; }

    void run() {
        while (!Window::window_schould_close()) {
            Gui::begin_frame();

            // 1. A nézet méretének beállítása az ELŐZŐ frame kérése alapján, MÉG a
            //    UI felépítése előtt — így a textúra azonosítója végig érvényes marad.
            scene_fbo.resize(want_w, want_h);

            // 2. UI: panelek + a viewport-ablak (ez rögzíti az ImGui::Image-et és
            //    kéri a következő frame méretét).
            if (gui_fn) gui_fn();

            // 3. A jelenet a saját framebufferébe.
            scene_fbo.bind();
            glClearColor(background.r, background.g, background.b, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            if (draw_fn) draw_fn();
            Framebuffer::unbind();

            // 4. A fő framebuffer törlése, majd a UI (benne a fenti képpel).
            glViewport(0, 0, Window::get_width(), Window::get_height());
            glClearColor(0.13f, 0.14f, 0.16f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            Gui::end_frame();
            Window::event_handling();    // swap + idő-események + poll
        }
        draw_fn = nullptr;
        gui_fn  = nullptr;
    }

    // A jelenetek (és bennük a Model-ek) felszabadítása UTÁN hívandó, még élő
    // GL-kontextussal: a shader-cache és az ablak elengedése.
    void shutdown() {
        Builder::clear_shader_cache();
        Gui::shutdown();
        Window::destroy_window();
    }
};

#endif //GORBE_APP_HPP
