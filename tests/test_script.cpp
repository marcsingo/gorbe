// Az alakzatokhoz írt C++ kód (script/): a Host tábla és a futtató, GL és DLL
// nélkül — a minta-kódok ide vannak fordítva, a gyártófüggvényüket közvetlenül
// adjuk át. A start() szerkezeti változásai beépülnek, az update() értékírásai
// újraépítés nélkül hatnak, a GUI-n beállított érték a kezdőérték.
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "scene/Build.hpp"
#include "script/Compiler.hpp"
#include "script/Runtime.hpp"

using Matek::Analizis::Kif;

static int failures = 0;
static void ok(std::string const& what, bool c, std::string const& info = "") {
    if (!c) ++failures;
    std::printf("  %-55s %-8s %s\n", what.c_str(), c ? "[OK]" : "[HIBA]", info.c_str());
}
static void near(std::string const& what, float got, float exp, float tol = 1e-4f) {
    ok(what, std::isfinite(got) && std::abs(got - exp) <= tol,
       "got=" + std::to_string(got) + " exp=" + std::to_string(exp));
}

static Param& param(std::list<Param>& l, char const* n, float v) {
    auto& p = l.emplace_back();
    std::snprintf(p.name, sizeof(p.name), "%s", n);
    p.value = v;
    return p;
}
static Shape& shape(SceneDoc& sc, char const* n, char const* f, char const* script) {
    auto& s = sc.shapes.emplace_back();
    std::snprintf(s.name, sizeof(s.name), "%s", n);
    std::snprintf(s.formula, sizeof(s.formula), "%s", f);
    std::snprintf(s.script, sizeof(s.script), "%s", script);
    return s;
}
static float at(Shape const& s, float x, float y, float z) { return Kif(s.tree).at({x, y, z}); }
static bool has(Script::Runtime const& rt, char const* part) {
    for (auto const& e : rt.errors) if (e.find(part) != std::string::npos) return true;
    return false;
}

// --- minta-kódok (ugyanígy néznének ki a projektmappa kod/ fájljaiban) ---------

// Saját paraméter és warp a start()-ban; az update() a sugarat és a pozíciót írja.
struct Lukteto : gorbe::ShapeScript {
    float* r = nullptr;
    void start() override {
        r = &add_param("r", 2.0f);
        add_warp("x - 1", "y", "z");             // eltolás +1-gyel x mentén
    }
    void update(float t, float) override {
        *r = 2.0f + t;
        position().z = 10.0f * t;
    }
};
GORBE_SHAPE(Lukteto)

// A GUI-n felvett `k` paramétert írja, és rossz fázisban próbál szerkezetet állítani.
static bool rossz_warp_ok = true;
struct Rossz : gorbe::ShapeScript {
    void update(float, float) override {
        param("k") = 7.0f;
        param("nincs_ilyen") = 1.0f;             // hiba, de nem omlik össze
        rossz_warp_ok = add_warp("x", "y", "z");       // csak a start()-ban lehet
    }
};
GORBE_SHAPE(Rossz)

// A képletet cseréli le, és a második update()-ben kivételt dob.
struct Kivetel : gorbe::ShapeScript {
    int n = 0;
    void start() override { formula("x + y + z - c"); add_param("c", 1.0f); }
    void update(float, float) override {
        if (++n == 2) throw std::runtime_error("szandekos");
        param("c") += 1.0f;
    }
};
GORBE_SHAPE(Kivetel)

// A konzolra ír: minden ciklusban ugyanazt (összevonódik) és egy hibát.
struct Naplo : gorbe::ShapeScript {
    void start() override { print("indul, %d. probalkozas, r = %.2f", 1, 2.5); }
    void update(float, float) override {
        print("ciklus");
        error("valami baj");
    }
};
GORBE_SHAPE(Naplo)

#define FACTORY(C) Script::Factory{&gorbe_api_version_##C, &gorbe_create_##C, &gorbe_destroy_##C}

static int old_version() { return gorbe::API_VERSION + 1; }

int main() {
    std::list<Param> program;

    std::printf("=== 1. start(): sajat parameter es warp, update(): eloben ===\n");
    {
        SceneDoc sc;
        Shape& s = shape(sc, "g", "x^2 + y^2 + z^2 - r^2", "Lukteto");
        Script::Runtime rt;
        ok("attach", rt.attach(s, sc, program, FACTORY(Lukteto)));
        rt.start_all();
        ok("a start() utan van script-parameter es -warp",
           s.script_params.size() == 1 && s.script_warps.size() == 1);
        std::string err = Build::build(sc, program);
        ok("a kiegeszitesekkel felepul", err.empty(), err);
        near("r = 2, eltolva: F(3,0,0) = 0", at(s, 3, 0, 0), 0.0f);
        ok("egysegtranszformacio mellett is beepul (warped)", s.warped);

        rt.update_all(1.0f, 0.016f);             // r = 3, z = 10 — újraépítés NÉLKÜL
        near("update utan: F(4,0,10) = 0", at(s, 4, 0, 10), 0.0f);
        ok("nincs hiba", rt.errors.empty(), rt.errors.empty() ? "" : rt.errors[0]);

        rt.swap_initial();
        near("swap_initial: a kezdo pozicio latszik", s.xform.pos[2], 0.0f);
        rt.swap_initial();
        near("... es vissza a futo ertekre", s.xform.pos[2], 10.0f);

        Build::drop_trees(sc);
        rt.clear();
        near("clear: a pozicio a kezdoertekre all", s.xform.pos[2], 0.0f);
        ok("clear: a kod kiegeszitesei eltunnek",
           s.script_params.empty() && s.script_warps.empty() && s.script_formula.empty());
    }

    std::printf("=== 2. A GUI-n beallitott ertek a kezdoertek ===\n");
    {
        SceneDoc sc;
        Shape& s = shape(sc, "g", "x^2 + y^2 + z^2 - r^2", "Lukteto");
        param(s.locals, "r", 5.0f);              // a GUI-n felvett r
        Script::Runtime rt;
        rt.attach(s, sc, program, FACTORY(Lukteto));
        rt.start_all();
        ok("add_param: a GUI-n felvett nyer, nincs uj", s.script_params.empty());
        Build::build(sc, program);
        near("r = 5 (a GUI-e): F(6,0,0) = 0", at(s, 6, 0, 0), 0.0f);
        rt.update_all(0.5f, 0.016f);
        near("az update irja: r = 2.5", s.locals.front().value, 2.5f);
        Build::drop_trees(sc);
        rt.clear();
        near("clear utan a GUI-s r = 5 vissza", s.locals.front().value, 5.0f);
    }

    std::printf("=== 3. Hibak: rossz fazis, ismeretlen nev, kivetel, verzio ===\n");
    {
        SceneDoc sc;
        Shape& a = shape(sc, "a", "x - k", "Rossz");
        Param& k = param(sc.params, "k", 1.0f);  // jelenet-szintű paraméter
        Shape& b = shape(sc, "b", "x", "Kivetel");
        Script::Runtime rt;
        rt.attach(a, sc, program, FACTORY(Rossz));
        rt.attach(b, sc, program, FACTORY(Kivetel));
        rt.start_all();
        std::string err = Build::build(sc, program);
        ok("felepul", err.empty(), err);
        near("formula(): b kepletet cserelt, F(1,0,0) = 0", at(b, 1, 0, 0), 0.0f);

        rt.update_all(0, 0);
        near("a jelenet-szintu k irhato", k.value, 7.0f);
        ok("add_warp az update-ben: hamis", !rossz_warp_ok);
        ok("... es hibat ir", has(rt, "csak a start()-ban"));
        ok("ismeretlen parameter: hiba", has(rt, "nincs 'nincs_ilyen'"));
        std::size_t const n = rt.errors.size();
        rt.update_all(0, 0);                     // b itt dob kivételt
        ok("kivetel: a kod leall, hibat ir", has(rt, "szandekos"));
        ok("ugyanaz a hiba nem ismetlodik", rt.errors.size() == n + 1);
        k.value = 0.0f;
        rt.update_all(0, 0);
        near("a masik kod tovabb fut", k.value, 7.0f);
        near("a leallt kod nem fut: c = 2", b.script_params.front().value, 2.0f);

        Build::drop_trees(sc);
        rt.clear();
        near("clear: a jelenet k-ja is visszaall", k.value, 1.0f);

        Shape& c = shape(sc, "c", "x", "Lukteto");
        ok("mas API-verzio: elutasitja",
           !rt.attach(c, sc, program, {&old_version, &gorbe_create_Lukteto, &gorbe_destroy_Lukteto}));
        ok("... erheto hibaval", has(rt, "verziojaval fordult"));
        ok("hianyzo gyartofuggveny: elutasitja", !rt.attach(c, sc, program, {}));
    }

    std::printf("=== 4. Konzol (utils/Log.hpp): print, error, osszevonas ===\n");
    {
        Log::clear();
        SceneDoc sc;
        Shape& s = shape(sc, "naplos", "x", "Naplo");
        Script::Runtime rt;
        rt.attach(s, sc, program, FACTORY(Naplo));
        SceneTime::value = 1.5f;
        rt.start_all();
        ok("print: a konzolra, formazva, az alakzat nevevel",
           Log::lines.size() == 1 && Log::lines[0].text == "indul, 1. probalkozas, r = 2.50" &&
           Log::lines[0].source == "naplos" && Log::lines[0].level == Log::Level::Info &&
           Log::lines[0].t == 1.5f, Log::lines.empty() ? "" : Log::lines[0].text);
        for (int i = 0; i < 5; ++i) rt.update_all(0, 0);
        // Sorrend: indul, ciklus, hiba, ciklus(x4) — a hiba egyszer, a ciklus a hiba
        // után már egyben gyűlik (egymás utáni egyforma sorok).
        std::size_t errs = 0;
        for (auto const& l : Log::lines) errs += l.level == Log::Level::Error;
        ok("error: a konzolra is, pirossal, egyszer", errs == 1);
        ok("az egymas utani egyforma sorok osszevonodnak",
           Log::lines.size() == 4 && Log::lines.back().text == "ciklus" && Log::lines.back().repeat == 4,
           std::to_string(Log::lines.size()) + " sor");

        Log::clear();
        for (std::size_t i = 0; i < Log::MAX_LINES + 10; ++i)
            Log::add(Log::Level::Info, "", std::to_string(i), 0.0f);
        ok("a legregebbi sorok kiesnek", Log::lines.size() == Log::MAX_LINES && Log::lines.front().text == "10");
        Log::clear();
    }

    std::printf("=== 5. Valodi forditas: kod/*.cpp -> DLL -> peldanyok ===\n");
    {
        std::string err;
        if (Script::Platform::find_compiler(err).empty()) {
            std::printf("  (kimarad: %s)\n", err.c_str());
        } else {
            namespace fs = std::filesystem;
            // Ékezetes mappanév: a valódi projektek útvonalában is lehet ("5. félév").
            fs::path const dir = fs::temp_directory_path() / fs::path(u8"gorbe_teszt_félév");
            std::error_code ec;
            fs::remove_all(dir, ec);
            fs::create_directories(dir / "kod");
            auto write = [&](char const* name, char const* text) {
                std::ofstream(dir / "kod" / name) << text;
            };
            write("Proba.cpp", R"(#include "../gorbe_api.hpp"
struct Proba : gorbe::ShapeScript {
    float* r = nullptr;
    void start() override { r = &add_param("r", 2.0f); }
    void update(float t, float) override { *r = 2.0f + t; position().x = 1.0f; }
};
GORBE_SHAPE(Proba)
)");
            write("Hibas.cpp", R"(#include "../gorbe_api.hpp"
struct Hibas : gorbe::ShapeScript {
    void update(float t, float) override { nincs_ilyen_valtozo = t; }
};
GORBE_SHAPE(Hibas)
)");
            auto compile = [&](SceneDoc const& sc, fs::path const& pd) {
                Script::Compilation c;
                if (Script::begin(c, sc, pd, GORBE_API_HEADER)) {
                    while (!Script::poll(c, 20)) {}
                    Script::finish(c);
                }
                return c;
            };

            // A "Kod letrehozasa" gomb: uj fajlnal a vaz, a meglevot nem irja felul.
            std::string cerr;
            fs::path const vaz = Script::create_source(dir, "Vaz", GORBE_API_HEADER, cerr);
            ok("create_source: uj fajl a vazzal", fs::is_regular_file(vaz) && vaz.filename() == "Vaz.cpp", cerr);
            auto const before = fs::file_size(dir / "kod" / "Proba.cpp");
            Script::create_source(dir, "Proba", GORBE_API_HEADER, cerr);
            ok("create_source: a meglevo fajl marad", fs::file_size(dir / "kod" / "Proba.cpp") == before);
            ok("create_source: rossz osztalynev hiba",
               Script::create_source(dir, "1rossz", GORBE_API_HEADER, cerr).empty() && !cerr.empty());

            SceneDoc sc;
            std::snprintf(sc.name, sizeof(sc.name), "Jelenet 1");
            Shape& s = shape(sc, "g", "x^2 + y^2 + z^2 - r^2", "Proba.cpp");
            shape(sc, "v", "x", "Vaz.cpp");
            Script::Compilation c = compile(sc, dir);
            ok("a jo kod es a vaz egyutt lefordul", c.ok && c.warnings.empty(),
               c.errors.empty() ? (c.warnings.empty() ? "" : c.warnings[0]) : c.errors[0]);
            ok("a DLL a build/<jelenet>/ mappaban", fs::is_regular_file(dir / "build" / "Jelenet_1" / "Jelenet_1.dll"));
            ok("a gorbe_api.hpp a projekt mappajaba kerult", fs::is_regular_file(dir / "gorbe_api.hpp"));
            {
                Script::Platform::Module m;      // a Runtime ELŐTT: utoljára szabaduljon fel
                Script::Runtime rt;
                ok("betoltes", Script::load(c, m, rt, sc, program));
                ok("peldanyok letrejottek, hiba nelkul", !rt.empty() && rt.errors.empty(),
                   rt.errors.empty() ? "" : rt.errors[0]);
                rt.start_all();
                std::string e = Build::build(sc, program);
                ok("felepul", e.empty(), e);
                rt.update_all(1.0f, 0.016f);     // r = 3, x-eltolás 1
                near("a DLL kodja mozgatja: F(4,0,0) = 0", at(s, 4, 0, 0), 0.0f);
                Build::drop_trees(sc);
            }

            SceneDoc bad;
            shape(bad, "h", "x", "Hibas.cpp");
            c = compile(bad, dir);
            ok("a hibas kod nem fordul", !c.ok);
            bool located = false;
            for (auto const& l : c.errors)
                if (l.find("kod\\Hibas.cpp(3)") == 0 && l.find("error C") != std::string::npos) located = true;
            ok("a hiba fajllal es sorral, rovid uttal", located, c.errors.empty() ? "" : c.errors[0]);

            SceneDoc missing;
            shape(missing, "m", "x", "Nincs.cpp");
            c = compile(missing, dir);
            ok("hianyzo kodfajl: hiba", !c.ok && !c.errors.empty() &&
               c.errors[0].find("nincs meg a kodfajl") != std::string::npos);
            c = compile(sc, fs::path());
            ok("mentetlen projekt: hiba", !c.ok && !c.errors.empty() &&
               c.errors[0].find("mentsd") != std::string::npos);
            fs::remove_all(dir, ec);
        }
    }

    std::printf(failures ? "\n%d HIBA\n" : "\nminden rendben\n", failures);
    return failures ? 1 : 0;
}
