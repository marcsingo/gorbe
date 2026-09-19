#ifndef GORBE_APP_SCENE_HPP
#define GORBE_APP_SCENE_HPP

#include <memory>
#include <string>
#include <vector>

#include "../App.hpp"
#include "../particle_sampling/Surface.hpp"
#include "../particle_sampling/ImplicitSurface.hpp"
#include "../scene/Build.hpp"
#include "../scene/Document.hpp"

// ---------------------------------------------------------------------------
// Egy JELENET = egy fül. Önálló: saját alakzatok, saját paraméterek, saját munkatér,
// saját kamera és saját mintavételezők.
//
// Az adatok a SceneDoc-ból jönnek (scene/Document.hpp); itt csak a FUTÁSIDEJŰ
// állapot van: a kijelölés, az utolsó hibaüzenet, a kamera és a mintavételezők.
//
// A háttérben lévő fülek szimulációja ÁLL (a részecskék állapota megmarad, tehát
// visszaváltáskor onnan folytatódik), és a bevitelt sem kapják meg — enélkül minden
// fül kamerája együtt mozogna.
// ---------------------------------------------------------------------------
struct Scene : SceneDoc {
    using EqSurface = ImplicitSurface<StringSurface>;

    Shape* selected = nullptr;

    std::string error;      // parse-hiba az utolsó Indításból

    // Fülönként saját kamera: a nézet megmarad fülváltáskor.
    Camera3D camera{glm::vec4(0.0f, 0.0f, 1200.0f, 800.0f), App::DEFAULT_EYE, -90.0f, 45.0f};

    // Alakzatonként egy önálló mintavételező. A lista i-edik alakzata a pool i-edik
    // felületére kerül; a méret együtt mozog (az ImplicitSurface a destruktorában
    // leiratkozik az ablak eseményeiről, ezért szabadon megszüntethető).
    std::vector<std::unique_ptr<EqSurface>> pool;

    Scene() { camera.look_at(App::DEFAULT_EYE, glm::vec3(0.0f)); }

    // Aktív fül: fut a szimuláció és megkapja a bevitelt.
    void set_active(bool a) {
        camera.input_enabled = a;
        for (auto& p : pool) {
            p->set_running(a);
            p->set_input_enabled(a);
        }
    }

    void draw() {
        for (auto& p : pool) p->draw(camera);
    }

    // A samplerek számát az alakzatokéhoz igazítja (felvétel/törlés után), és
    // átadja nekik a GUI-ból jövő, élőben ható beállításokat.
    void sync_pool() {
        while (pool.size() > shapes.size()) pool.pop_back();
        while (pool.size() < shapes.size()) {
            // A jelenet SAJÁT kamerája: a mintavételezők kontrollpontjai ehhez
            // vetítenek vissza, tehát fülenként külön kell.
            auto p = std::make_unique<EqSurface>(camera);
            p->set_manual_diameter(true);  // a d-t a GUI állítja
            p->clear();                    // induláskor üres, álló
            pool.push_back(std::move(p));
        }
        std::size_t i = 0;
        for (auto& s : shapes) {
            auto& p = *pool[i++];
            p.set_visible(s.visible);
            p.d = d_ui;
            p.curvature_repulsion = curv_ui;
        }
    }

    // Minden képlet eldobása + a szimuláció leállítása. KÖTELEZŐ, mielőtt egy
    // paraméter vagy alakzat törlődik: a már beparseolt fák a törölt float
    // CÍMÉT tárolják, onnantól felszabadított memóriára mutatnának.
    void drop() {
        for (auto& p : pool) {
            p->clear();
            p->get_surface().set_tree(Kif(0.0f).get());
            p->get_surface().clear_domain();
        }
        Build::drop_trees(*this);
        error.clear();
    }
};

#endif //GORBE_APP_SCENE_HPP
