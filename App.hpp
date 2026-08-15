#ifndef GORBE_APP_HPP
#define GORBE_APP_HPP

#include <functional>
#include <memory>
#include <vector>

#include <glad/glad.h>

#include "utils/init.hpp"
#include "model/Window.hpp"
#include "model/Camera.hpp"
#include "model/Gui.hpp"
#include "model/Axes.hpp"
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

    // Koordináta-tengelyek (piros=x, zöld=y, kék=z). A gl_context után jön létre,
    // így a Model (VAO/VBO) és a shader már érvényes GL-kontextusban épül fel.
    Axes axes;

    std::shared_ptr<void> surface_keepalive;        // életben tartja a kiválasztott felületet
    std::function<void(Camera const &)> draw_fn;    // típus-független rajzolás
    std::function<void()> gui_fn;                   // a felhasználó ImGui-panelei

    // A stringből megadott alakzatok samplerei (lásd resize_equation_surfaces).
    std::vector<std::unique_ptr<ImplicitSurface<StringSurface>>> eq_pool;

    glm::vec3 background{1.0f, 1.0f, 1.0f};

public:
    // A jelenetben a z a "függőleges" (a sík-sablon z=0, a henger a z mentén áll),
    // ezért az alapnézet felülről néz az origóra: így a z=0 talajrács és a rajta álló
    // alakzat is jól látszik.
    //
    // A szempont SZÁNDÉKOSAN nincs rajta az YZ síkon: onnan nézve a +Y és a +Z tengely
    // pontosan ugyanabba a képernyő-irányba (felfelé) vetülne, tehát fedné egymást.
    // A (-,-,+) oktánsból viszont mindhárom tengely külön irányba mutat.
    static constexpr glm::vec3 DEFAULT_EYE{-14.0f, -14.0f, 16.0f};

    explicit App(int width = 800, int height = 800, char const *title = "Particle sampling")
        : gl_context(width, height, title),
          camera(glm::vec4(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
                 DEFAULT_EYE, -90.0f, 45.0f) {
        camera.look_at(DEFAULT_EYE, glm::vec3(0.0f));
        Gui::init(Window::handle());
    }

    Camera3D &get_camera() { return camera; }
    Axes     &get_axes()   { return axes; }
    void set_background(glm::vec3 color) { background = color; }

    // Saját ImGui UI: a megadott függvény minden frame-ben lefut (ImGui::Begin/End hívásokkal).
    void set_gui(std::function<void()> fn) { gui_fn = std::move(fn); }

    // A felhasználó fő belépési pontja: kiválasztja a felület típusát, a hozzá tartozó
    // occluder automatikusan adódik. Visszaadja a felületet, ha menet közben kell rá hivatkozni.
    template<class SurfaceT>
    ImplicitSurface<SurfaceT> &show(SimParams params = {}) {
        auto surface = std::make_shared<ImplicitSurface<SurfaceT>>(camera, params);
        surface_keepalive = surface;
        draw_fn = [surface](Camera const &cam) { surface->draw(cam); };
        return *surface;
    }

    // Egyetlen, stringből megadott egyenlethez (egy felület). Több, külön mintavételezett
    // alakzathoz lásd make_equation_pool-t. ÁLLANDÓ életű felületet hoz létre (a Window
    // eseménykezelői erre mutatnak), és rögtön ÁLLÓ állapotba teszi (clear): a szimuláció
    // a GUI-ból, az F beállítása (get_surface().set_tree(...)) + restart() után indul.
    ImplicitSurface<StringSurface> &show_equation() {
        auto surface = std::make_shared<ImplicitSurface<StringSurface>>(camera);
        surface_keepalive = surface;
        draw_fn = [surface](Camera const &cam) { surface->draw(cam); };
        surface->clear(); // induláskor üres, álló jelenet — az "Indít"-ra vár
        return *surface;
    }

    // Stringből megadott alakzatok samplerei. Bármikor hívható: a lista tetszőlegesen
    // nőhet és csökkenhet, mert az ImplicitSurface a destruktorában leiratkozik az
    // ablak eseményeiről (Window::Subscription), tehát nem marad utána lógó lambda.
    using EqSurface = ImplicitSurface<StringSurface>;

    // Gondoskodik róla, hogy pontosan `n` sampler legyen; a visszaadott mutatók a
    // következő hívásig érvényesek.
    std::vector<EqSurface*> resize_equation_surfaces(std::size_t n) {
        while (eq_pool.size() > n) eq_pool.pop_back();
        while (eq_pool.size() < n) {
            auto s = std::make_unique<EqSurface>(camera);
            s->set_manual_diameter(true); // a d-t a GUI állítja, ne a felület írja felül
            s->clear();                   // induláskor üres, álló
            eq_pool.push_back(std::move(s));
        }
        std::vector<EqSurface*> out;
        out.reserve(eq_pool.size());
        for (auto& s : eq_pool) out.push_back(s.get());
        draw_fn = [this](Camera const &cam) { for (auto &s : eq_pool) s->draw(cam); };
        return out;
    }

    void run() {
        while (!Window::window_schould_close()) {
            Gui::begin_frame();
            if (gui_fn) gui_fn(); //else Gui::demo_panel();

            glClearColor(background.r, background.g, background.b, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            axes.draw(camera);
            if (draw_fn) draw_fn(camera);

            Gui::end_frame();            // a UI a jelenet fölé kerül, a swap előtt
            Window::event_handling();    // swap + idő-események + poll
        }
        // A GL erőforrásokat még élő kontextus mellett kell elengedni: előbb a
        // jelenet (Model-ek: VAO/VBO), aztán a shader-cache, végül az ablak.
        eq_pool.clear();
        surface_keepalive.reset();
        draw_fn = nullptr;
        Builder::clear_shader_cache();
        Gui::shutdown();
        Window::destroy_window();
    }
};

#endif //GORBE_APP_HPP
