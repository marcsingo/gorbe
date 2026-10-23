// GL-teszt: az alakzatkód a VALÓDI vezérlőn át (app/Controller.hpp), ahogy a gombok
// használják: mentés -> kódfájl -> Indítás (fordítás a Job lépéseiben) -> az update()
// a szimuláció órájával fut -> Töröl visszaállít -> mentés a kezdőértékekkel ->
// hibás kód -> "Build sikertelen" a hibasorokkal. A vezérlő mintavételezői GL-objektumok,
// ezért rejtett ablakot nyit (GORBE_BUILD_GL_TESTS).
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <glad/glad.h>
#include "utils/init.hpp"
#include "model/Window.hpp"
#include "app/Controller.hpp"
#include "scene/Presets.hpp"

namespace fs = std::filesystem;

static int failures = 0;
static void ok(std::string const& what, bool c, std::string const& info = "") {
    if (!c) ++failures;
    std::printf("  %-50s %-8s %s\n", what.c_str(), c ? "[OK]" : "[HIBA]", info.c_str());
}

// Az Indítás, ahogy a GUI kéri: a kérés, utána frame-enként apply(), amíg a munka tart.
static void start(Controller& ctl) {
    Ui::Requests rq;
    rq.build = true;
    ctl.apply(rq);
    auto const t0 = std::chrono::steady_clock::now();
    while (ctl.job() && std::chrono::steady_clock::now() - t0 < std::chrono::seconds(120))
        ctl.apply(Ui::Requests{});
}

// Néhány frame: a szimuláció órája (Window::event_handling) futtatja az update()-et.
static void frames(int n) {
    for (int i = 0; i < n; ++i) Window::event_handling();
}

static void write(fs::path const& p, char const* text) { std::ofstream(p, std::ios::binary) << text; }

int main() {
    Utils::init_glfw();
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    Window::init(320, 240, "test_gl_script");

    std::string err;
    if (Script::Platform::find_compiler(err).empty()) {
        std::printf("  (kimarad: %s)\n", err.c_str());
        Window::destroy_window();
        return 0;
    }

    fs::path const dir = fs::temp_directory_path() / fs::path(u8"gorbe_gl_teszt_félév");
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);
    fs::path const project = dir / "proba.gorbe.json";
    {
        Controller ctl;
        Scene& sc = ctl.cur();
        Shape& s = new_shape(sc.shapes, "gomb");
        std::snprintf(s.formula, sizeof(s.formula), "x^2 + y^2 + z^2 - r^2");
        auto& r = s.locals.emplace_back();
        std::snprintf(r.name, sizeof(r.name), "r");
        r.value = 2.0f;

        std::printf("=== 1. Mentes, kodfajl, Inditas ===\n");
        start(ctl);
        ok("kod nelkul nincs build-uzenet", sc.build_status.empty() && sc.error.empty(), sc.error);
        ok("mentes", ctl.save_project(project.string()));
        fs::path const src = Script::create_source(dir, "Mozgo", fs::path(BINARY_DIR) / "gorbe_api.hpp", err);
        ok("a vaz elkeszult", !src.empty(), err);
        write(src, R"(#include "../gorbe_api.hpp"
struct Mozgo : gorbe::ShapeScript {
    void start() override { add_param("k", 4.0f); }
    void update(float, float) override { position().x = 3.0f; param("r") = 1.5f; }
};
GORBE_SHAPE(Mozgo)
)");
        std::snprintf(s.script, sizeof(s.script), "Mozgo.cpp");
        start(ctl);
        ok("Build sikeres", sc.build_ok && sc.build_status == "Build sikeres", sc.build_status +
           (sc.build_errors.empty() ? "" : ": " + sc.build_errors[0]));
        ok("a kod fut, a start() parametere megvan", !sc.runtime.empty() && s.script_params.size() == 1);
        ok("a konzolon: Build sikeres, a jelenet nevevel", !Log::lines.empty() &&
           Log::lines.back().text == "Build sikeres" && Log::lines.back().source == sc.name);

        frames(3);
        ok("az update() a szimulacio orajaval fut", s.xform.pos[0] == 3.0f && r.value == 1.5f,
           std::to_string(s.xform.pos[0]) + " / " + std::to_string(r.value));
        ok("futas kozben nincs hiba", sc.runtime.errors.empty() && sc.error.empty(),
           sc.runtime.errors.empty() ? sc.error : sc.runtime.errors[0]);

        std::printf("=== 2. Mentes futas kozben: a kezdoertekek ===\n");
        ok("mentes", ctl.save_project(project.string()));
        ok("a futo ertek megmaradt", s.xform.pos[0] == 3.0f);
        std::stringstream text;
        text << std::ifstream(project).rdbuf();
        std::vector<std::string> warn;
        auto pr = ProjectFile::from_text(text.str(), warn);
        Shape const& saved = pr.scenes.front().shapes.front();
        ok("a fajlban a kezdoertek es a kodfajl", saved.xform.pos[0] == 0.0f &&
           saved.locals.front().value == 2.0f && std::string(saved.script) == "Mozgo.cpp" &&
           saved.locals.size() == 1);

        std::printf("=== 3. Torol: a kezdoertekek vissza ===\n");
        Ui::Requests rq;
        rq.drop = true;
        ctl.apply(rq);
        ok("pozicio es r vissza, a kod leallt", s.xform.pos[0] == 0.0f && r.value == 2.0f &&
           sc.runtime.empty() && s.script_params.empty());

        std::printf("=== 4. Hibas kod: Build sikertelen, hibasorokkal ===\n");
        write(src, R"(#include "../gorbe_api.hpp"
struct Mozgo : gorbe::ShapeScript {
    void update(float t, float) override { nincs = t; }
};
GORBE_SHAPE(Mozgo)
)");
        start(ctl);
        ok("Build sikertelen", !sc.build_ok && sc.build_status == "Build sikertelen");
        ok("a hiba fajllal es sorral", !sc.build_errors.empty() &&
           sc.build_errors[0].find("kod\\Mozgo.cpp(3)") == 0, sc.build_errors.empty() ? "" : sc.build_errors[0]);
        ok("a jelenet nem indult, a kod nem fut", sc.runtime.empty());
        ok("a konzolon a hibasor, pirossal", !Log::lines.empty() &&
           Log::lines.back().level == Log::Level::Error &&
           Log::lines.back().text.find("kod\\Mozgo.cpp(3)") == 0);

        std::printf("=== 5. Javitas utan ujra sikeres (a DLL felulirhato) ===\n");
        write(src, R"(#include "../gorbe_api.hpp"
struct Mozgo : gorbe::ShapeScript { void update(float, float) override { position().y = 1.0f; } };
GORBE_SHAPE(Mozgo)
)");
        start(ctl);
        ok("Build sikeres", sc.build_ok, sc.build_errors.empty() ? "" : sc.build_errors[0]);
        start(ctl);
        ok("meg egyszer (a betoltott DLL helyere)", sc.build_ok, sc.build_errors.empty() ? "" : sc.build_errors[0]);
        frames(3);
        ok("az uj kod fut", s.xform.pos[1] == 1.0f);

        std::printf("=== 6. Utkozes a programban: a gomb kitolja magat a falbol ===\n");
        // A "gomb" (r = 2) az origóban, a "fal" (r = 1) x = 2.5-ben: 0.5 mélyen átfednek.
        // A kód minden ciklusban kitolja a gömböt a normális mentén.
        Shape& fal = new_shape(sc.shapes, "fal");
        std::snprintf(fal.name, sizeof(fal.name), "fal");
        std::snprintf(fal.formula, sizeof(fal.formula), "x^2 + y^2 + z^2 - 1");
        fal.xform.pos[0] = 2.5f;
        sc.d_ui = 0.8f;
        write(src, R"(#include "../gorbe_api.hpp"
struct Mozgo : gorbe::ShapeScript {
    gorbe::ShapeRef fal;
    void start() override { fal = shape("fal"); }
    void update(float, float) override {
        if (gorbe::Hit h = collide(fal)) {
            position() += h.normal * h.depth;
            print("utkozes, melyseg %.3f", h.depth);
        }
    }
};
GORBE_SHAPE(Mozgo)
)");
        start(ctl);
        ok("Build sikeres", sc.build_ok, sc.build_errors.empty() ? "" : sc.build_errors[0]);
        // A szimuláció VALÓS időben halad (a rejtett ablak frame-jei gyorsak), és a
        // fisszió néhány másodperc alatt teríti be a gömböket: addig futunk, amíg a
        // gömb kiszabadul (legfeljebb 40 s).
        double const t0 = glfwGetTime();
        while (glfwGetTime() - t0 < 40.0 && s.xform.pos[0] > -0.45f) frames(10);
        frames(30);                       // kiszabadult: utána már nem mozdul tovább
        bool logged = false;
        for (auto const& l : Log::lines) logged |= l.text.rfind("utkozes, melyseg", 0) == 0;
        ok("utkozott, es a konzolra irt", logged);
        ok("kitolta magat -x iranyba, epp az atfedesnyit (0.5)", s.xform.pos[0] < -0.45f && s.xform.pos[0] > -0.7f,
           std::to_string(s.xform.pos[0]));
        ok("futas kozben nincs hiba", sc.runtime.errors.empty(),
           sc.runtime.errors.empty() ? "" : sc.runtime.errors[0]);

        std::printf("=== 7. Futas: a jelenet kameraja, WASD a kode ===\n");
        // A billentyűt a GLFW-be nem lehet beinjektálni: a lenyomás oda megy, ahová az
        // ablak eseménye is menne (Runtime::key_event) — a többi a valódi úton.
        write(src, R"(#include "../gorbe_api.hpp"
struct Mozgo : gorbe::ShapeScript {
    void update(float, float dt) override {
        gorbe::vec3 v;
        if (key_down('W')) v += camera().forward();
        if (key_down('S')) v -= camera().forward();
        if (key_down('D')) v += camera().right();
        if (key_down('A')) v -= camera().right();
        v.z = 0;
        position() += gorbe::normalize(v) * (4.0f * dt);
        camera().position() = position() + gorbe::vec3{-6, 0, 4};   // mögötte, fölötte
        camera().look_at(position());
    }
};
GORBE_SHAPE(Mozgo)
)");
        CameraDoc const initial_cam = sc.scene_camera;
        start(ctl);
        ok("Build sikeres", sc.build_ok, sc.build_errors.empty() ? "" : sc.build_errors[0]);
        ok("az Inditas utan fut, a jelenet kamerajabol latunk",
           sc.playing && sc.game_view() && &sc.view_camera() == &sc.scene_view);
        ctl.apply(Ui::Requests{});
        ok("a szerkeszto kamera nem kap bemenetet", !sc.camera.input_enabled);
        frames(5);
        float const x0 = s.xform.pos[0];
        ok("a kamera kovet (mogotte all)", std::abs(sc.scene_camera.pos[0] - (x0 - 6.0f)) < 1e-4f,
           std::to_string(sc.scene_camera.pos[0]));

        sc.runtime.key_event('W', true);
        double const tw = glfwGetTime();
        while (glfwGetTime() - tw < 0.5) frames(1);
        sc.runtime.key_event('W', false);
        frames(3);
        float const moved = s.xform.pos[0] - x0;
        // A kamera +x felé néz (mögötte áll): W = +x, ~4 egység/s, 0.5 s alatt ~2.
        ok("W-re a kamera nezoiranyaba megy (+x, kb. 2 egyseg)", moved > 1.0f && moved < 3.5f,
           std::to_string(moved));
        ok("... vizszintesen (z marad)", std::abs(s.xform.pos[2]) < 1e-4f);

        sc.show_scene_camera = false;                 // a nézet tetején a váltó
        ctl.apply(Ui::Requests{});
        sc.runtime.key_event('W', true);
        frames(3);                                     // a prepare_input kinullázza
        float const x1 = s.xform.pos[0];
        frames(10);
        ok("szerkeszto kamerabol nezve a kod nem kap bemenetet",
           s.xform.pos[0] == x1 && !sc.runtime.input.active);
        ok("... es a szerkeszto kamera igen", sc.camera.input_enabled && &sc.view_camera() == &sc.camera);
        sc.show_scene_camera = true;

        Ui::Requests stop;
        stop.drop = true;
        ctl.apply(stop);
        ok("Torol: leall, a szerkeszto kamera jon vissza", !sc.playing && !sc.game_view());
        ok("... a jelenet kameraja a kezdoertekere all", sc.scene_camera == initial_cam);

        std::printf("=== 8. Szerkesztes: automatikus elonezet ===\n");
        // A GUI frame-jei: apply (benne az előnézet) + a szimuláció órája.
        auto idle = [&](double sec) {
            double const t0 = glfwGetTime();
            while (glfwGetTime() - t0 < sec) { ctl.apply(Ui::Requests{}); frames(1); }
        };
        idle(3.0);
        ok("Leallit utan magatol mintavetelez", !sc.playing && sc.pool[0]->model().particles().size() > 20 &&
           sc.pool[1]->model().particles().size() > 8,
           std::to_string(sc.pool[0]->model().particles().size()) + " / " +
           std::to_string(sc.pool[1]->model().particles().size()));
        ok("... a kod nelkul (nem fut, a kamera all)", sc.runtime.empty() && sc.scene_camera == initial_cam);

        // A fal képletét módosítjuk (r: 1 -> 1.5): csak a fal indul újra.
        std::size_t const n_gomb = sc.pool[0]->model().particles().size();
        std::snprintf(fal.formula, sizeof(fal.formula), "x^2 + y^2 + z^2 - 2.25");
        idle(0.7);
        float const on_new = sc.pool[1]->model().surface().F.at({4.0f, 0.0f, 0.0f});   // 2.5 + 1.5
        ok("a modositas magatol beepult (gepeles utani szunetben)", std::abs(on_new) < 1e-4f,
           std::to_string(on_new));
        ok("... a valtozatlan alakzat nem indult ujra", sc.pool[0]->model().particles().size() >= n_gomb / 2 &&
           sc.pool[0]->model().particles().size() > 20, std::to_string(sc.pool[0]->model().particles().size()));
        ok("... es nem indult el a futas", !sc.playing);

        // Hibás képlet: a régi alak marad, mellette a hiba; javítás után eltűnik.
        std::snprintf(fal.formula, sizeof(fal.formula), "x^2 + (");
        idle(0.7);
        ok("hibas keplet: hiba latszik, a regi alak marad", !sc.error.empty() &&
           std::abs(sc.pool[1]->model().surface().F.at({4.0f, 0.0f, 0.0f})) < 1e-4f &&
           !sc.pool[1]->model().particles().empty(), sc.error);
        std::snprintf(fal.formula, sizeof(fal.formula), "x^2 + y^2 + z^2 - 1");
        idle(0.7);
        ok("javitva: a hiba eltunik, az uj alak epul", sc.error.empty() &&
           std::abs(sc.pool[1]->model().surface().F.at({3.5f, 0.0f, 0.0f})) < 1e-4f, sc.error);
        ctl.shutdown();
    }
    fs::remove_all(dir, ec);
    Window::destroy_window();

    std::printf(failures ? "\n%d HIBA\n" : "\nminden rendben\n", failures);
    return failures ? 1 : 0;
}
