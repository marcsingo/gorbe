#ifndef GORBE_APP_SCENE_HPP
#define GORBE_APP_SCENE_HPP

#include <memory>
#include <string>
#include <vector>

#include "../App.hpp"
#include "../object/SpaceObject.hpp"
#include "../scene/Build.hpp"
#include "../scene/Document.hpp"
#include "../script/Platform.hpp"
#include "../script/Runtime.hpp"

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

    // Fülönként saját SZERKESZTŐ kamera: a nézet megmarad fülváltáskor.
    Camera3D camera{glm::vec4(0.0f, 0.0f, 1200.0f, 800.0f), App::DEFAULT_EYE, -90.0f, 45.0f};

    // --- futás és a jelenet kamerája --------------------------------------------
    // FUT: az Indítás (sikeres felépítés) után, a Töröl-ig (drop). Futás közben a nézet
    // a jelenet SAJÁT kamerájából lát (scene_camera, a dokumentumban), a billentyűzet
    // és az egér a kódé (gorbe::ShapeScript::key_down, ...), a szerkesztő kamera áll.
    // A nézet tetején át lehet váltani a szerkesztő kamerára (show_scene_camera):
    // akkor a billentyűk megint a szerkesztő kamerát mozgatják.
    bool playing = false;
    bool show_scene_camera = true;
    DocCamera scene_view{scene_camera};

    // A kódé-e a bevitel, és a jelenet kamerájából látunk-e (a szerkesztő fülön soha).
    bool game_view() const { return playing && show_scene_camera && !editor; }
    Camera& view_camera() { return game_view() ? static_cast<Camera&>(scene_view) : camera; }
    Camera const& view_camera() const {
        return game_view() ? static_cast<Camera const&>(scene_view) : camera;
    }

    // A Tulajdonságok panel a jelenet kameráját mutatja (az Alakzatok lista "Kamera" sora).
    bool camera_selected = false;

    // --- automatikus előnézet (szerkesztés közben, lásd Controller::auto_preview) ---
    // Melyik szerkezetet (Build::scene_sig) mutatja az előnézet, és mióta más a jelenet
    // (a gépelés végét kivárjuk, nem építünk minden leütésre). 0 = újra kell építeni.
    std::size_t preview_sig = 0;
    double      dirty_since = -1.0;

    // Alakzatonként egy térbeli objektum (object/SpaceObject.hpp). A lista i-edik
    // alakzata a pool i-edik objektuma; a méret együtt mozog (a vezérlő a
    // destruktorában leiratkozik az ablak eseményeiről, ezért szabadon megszüntethető).
    std::vector<std::unique_ptr<SpaceObject>> pool;

    // Az alakzatok C++ kódja (script/): a jelenet DLL-je és a belőle jött példányok.
    // A SORREND számít: a runtime szűnik meg előbb (a példányokat a DLL kódja törli),
    // csak utána a module.
    Script::Platform::Module module;
    Script::Runtime runtime;

    // Az utolsó Indítás kódfordításának eredménye a panelre (üres: nem volt kód).
    std::string build_status;
    bool build_ok = false;
    std::vector<std::string> build_errors;   // a fordító hibasorai
    std::string build_log;                   // a teljes napló helye

    Scene() {
        camera.look_at(App::DEFAULT_EYE, glm::vec3(0.0f));
        // A kód (gorbe::ShapeRef) ezen át éri el egy alakzat futó felületét és
        // részecskéit: a lista i-edik alakzata a pool i-edik objektuma.
        runtime.system_of = [this](Shape const* s) -> ParticleSystem* {
            std::size_t i = 0;
            for (auto const& sh : shapes) {
                if (&sh == s) return i < pool.size() ? &pool[i]->model() : nullptr;
                ++i;
            }
            return nullptr;
        };
        // A kód másolata (gorbe::ShapeRef::clone): saját objektum a lista végén, és indul.
        runtime.spawn = [this](Shape& s) {
            sync_pool();
            std::size_t i = 0;
            for (auto const& sh : shapes) { if (&sh == &s) break; ++i; }
            bind_surface(s, *pool[i]);
            pool[i]->start();
            s.sampled_sig = Build::shape_sig(s);
        };
    }

    // A felépített fa (és a kontrollpontok) átadása az alakzat mintavételezőjének.
    static void bind_surface(Shape& s, SpaceObject& p) {
        auto& surf = p.model().surface();
        surf.set_tree(s.tree);
        if (s.dom_tree) surf.set_domain(s.dom_tree);
        else            surf.clear_domain();
        p.model().bind_controls(&s.controls, Build::control_params(s));
        // Saját kezdő részecskék (az alakzat helye körül).
        p.model().spawn_center = {s.xform.pos[0], s.xform.pos[1], s.xform.pos[2]};
    }

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
            if (!editor || !it->vari) { pool[i]->draw(view_camera(), dragged); continue; }
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
            pool[i]->draw(view_camera(), shown, &pts);
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
    //
    // A kód is leáll (a példányok a törlendő alakzatra mutathatnak), és amihez
    // hozzányúlt, a GUI-n beállított kezdőértékére áll vissza — a fák eldobása UTÁN,
    // mert a kód saját paramétereire (script_params) is mutathatnak.
    void drop() {
        for (auto& p : pool) {
            p->stop();
            p->model().surface().set_tree(Kif(0.0f).get());
            p->model().surface().clear_domain();
        }
        Build::drop_trees(*this);
        if (selected && selected->script_clone) selected = nullptr;
        runtime.clear();           // ... a kód másolatai is törlődnek
        sync_pool();
        playing = false;           // a futás véget ért: újra a szerkesztő kamera
        preview_sig = 0;           // ... és szerkesztésben az előnézet magától visszajön
        error.clear();
        drag_obj = drag_ctrl = -1;
    }
};

#endif //GORBE_APP_SCENE_HPP
