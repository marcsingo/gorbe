#ifndef GORBE_APP_SCENE_HPP
#define GORBE_APP_SCENE_HPP

#include <memory>
#include <string>
#include <vector>

#include "../App.hpp"
#include "../object/SpaceObject.hpp"
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
    Shape* selected = nullptr;

    std::string error;      // parse-hiba az utolsó Indításból

    // A húzott kontrollpont: melyik objektum (a pool indexe) hányadik pontja; -1 = nincs.
    int drag_obj = -1, drag_ctrl = -1;

    // Fülönként saját kamera: a nézet megmarad fülváltáskor.
    Camera3D camera{glm::vec4(0.0f, 0.0f, 1200.0f, 800.0f), App::DEFAULT_EYE, -90.0f, 45.0f};

    // Alakzatonként egy térbeli objektum (object/SpaceObject.hpp). A lista i-edik
    // alakzata a pool i-edik objektuma; a méret együtt mozog (a vezérlő a
    // destruktorában leiratkozik az ablak eseményeiről, ezért szabadon megszüntethető).
    std::vector<std::unique_ptr<SpaceObject>> pool;

    Scene() { camera.look_at(App::DEFAULT_EYE, glm::vec3(0.0f)); }

    // Aktív fül: fut a szimuláció és megkapja a bevitelt.
    void set_active(bool a) {
        camera.input_enabled = a;
        for (auto& p : pool) p->controller().set_running(a);
    }

    // Variációs szerkesztő fül: egy alakzat az origó körül, a kényszerei húzhatók
    // (app/Controller.hpp). Nem mentődik, a forrás-alakzat viszont igen.
    bool editor = false;

    // A következő frame-ben a fülsáv erre a fülre váltson (egy már nyitott
    // szerkesztő újbóli megnyitásakor).
    bool focus_tab = false;

    void draw() {
        auto it = shapes.begin();
        for (std::size_t i = 0; i < pool.size(); ++i, ++it) {
            int const dragged = static_cast<int>(i) == drag_obj ? drag_ctrl : -1;
            if (!editor || !it->vari) { pool[i]->draw(camera, dragged); continue; }
            // A szerkesztőben a határkényszerek a kontrollpontok (a normálkényszerek
            // a felület mellett ülnek, azokat nem rajzoljuk). A kiemelés indexe a
            // rajzolt listában értendő.
            Variational const& v = *it->vari;
            std::vector<glm::vec3> pts;
            int shown = -1;
            for (std::size_t k = 0; k < v.centers.size(); ++k)
                if (v.values[k] == 0.0f) {
                    if (static_cast<int>(k) == dragged) shown = static_cast<int>(pts.size());
                    pts.push_back(v.centers[k]);
                }
            pool[i]->draw(camera, shown, &pts);
        }
    }

    // A samplerek számát az alakzatokéhoz igazítja (felvétel/törlés után), és
    // átadja nekik a GUI-ból jövő, élőben ható beállításokat.
    void sync_pool() {
        while (pool.size() > shapes.size()) pool.pop_back();
        while (pool.size() < shapes.size()) {
            // Induláskor üres és áll.
            pool.push_back(std::make_unique<SpaceObject>());
        }
        std::size_t i = 0;
        for (auto& s : shapes) {
            auto& p = *pool[i++];
            p.view().visible = s.visible;
            p.model().d = effective_d(s, *this);
            p.model().curvature_repulsion = curv_ui;
        }
    }

    // Minden képlet eldobása + a szimuláció leállítása. KÖTELEZŐ, mielőtt egy
    // paraméter vagy alakzat törlődik: a már beparseolt fák a törölt float
    // CÍMÉT tárolják, onnantól felszabadított memóriára mutatnának.
    void drop() {
        for (auto& p : pool) {
            p->stop();
            p->model().surface().set_tree(Kif(0.0f).get());
            p->model().surface().clear_domain();
        }
        Build::drop_trees(*this);
        error.clear();
        drag_obj = drag_ctrl = -1;
    }
};

#endif //GORBE_APP_SCENE_HPP
