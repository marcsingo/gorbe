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
        ctl.shutdown();
    }
    fs::remove_all(dir, ec);
    Window::destroy_window();

    std::printf(failures ? "\n%d HIBA\n" : "\nminden rendben\n", failures);
    return failures ? 1 : 0;
}
