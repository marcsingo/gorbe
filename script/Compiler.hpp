#ifndef GORBE_SCRIPT_COMPILER_HPP
#define GORBE_SCRIPT_COMPILER_HPP

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "../scene/Document.hpp"
#include "Platform.hpp"
#include "Runtime.hpp"

// ---------------------------------------------------------------------------
// Egy jelenet alakzatkódjának fordítása és betöltése (az Indítás első lépései).
// Platformfüggetlen: a fordító hívása és a DLL kezelése a Platform.hpp-ben van.
//
// A projekt mappájában:
//     gorbe_api.hpp                  minden fordításkor felülíródik
//     kod/<Osztaly>.cpp              a felhasználó kódja (fájlnév = osztálynév)
//     build/<jelenet>/               a fordítás kimenete: .obj, .dll, forditas.log
//
// Egy jelenet minden kódja EGY DLL-be fordul (a cl /MP párhuzamosan fordítja a
// fájlokat). Jelenetenként külön DLL, hogy egy jelenet újraindítása ne állítsa le a
// többi jelenet futó kódját.
//
// Menete (a vezérlőben a Job lépései):
//     Compilation c;
//     if (begin(c, ...)) { while (!poll(c, 20)) ...; finish(c); }
//     if (c.ok) load(c, module, runtime, scene, program);
//
// ponytail: minden Indítás újrafordít (a vcvars ~1-2 s); ha zavaró, a fájlidőkből
// eldönthető, hogy kell-e.
// ---------------------------------------------------------------------------
namespace Script {

    namespace fs = std::filesystem;

    struct Compilation {
        bool ok = false;
        std::vector<std::string> errors;     // a fordító hibasorai (vagy az előkészítésé)
        int warnings = 0;
        fs::path dll, log;
        std::vector<fs::path> sources;
        std::unique_ptr<Platform::Process> proc;
    };

    // A fájl = osztály neve: C++-azonosító kell, mert a gyártófüggvény nevébe kerül.
    inline bool valid_class_name(std::string const& s) {
        if (s.empty() || std::isdigit(static_cast<unsigned char>(s[0]))) return false;
        return std::all_of(s.begin(), s.end(), [](char c) {
            return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
        });
    }

    inline std::string class_name(Shape const& s) { return fs::path(s.script).stem().string(); }

    inline bool has_scripts(SceneDoc const& sc) {
        return std::any_of(sc.shapes.begin(), sc.shapes.end(), [](Shape const& s) { return s.script[0]; });
    }

    // A jelenet neve a build almappájához: csak betű, szám és aláhúzás.
    inline std::string unit_name(SceneDoc const& sc) {
        std::string n;
        for (char c : std::string(sc.name))
            n += std::isalnum(static_cast<unsigned char>(c)) ? c : '_';
        return n.empty() ? "jelenet" : n;
    }

    // Előkészítés és a fordító indítása. Hamis: már itt hiba van (c.errors), a
    // fordító el sem indult. `api_header`: a program saját gorbe_api.hpp-je, ez kerül
    // a projekt mappájába.
    inline bool begin(Compilation& c, SceneDoc const& sc, fs::path const& project_dir,
                      fs::path const& api_header) {
        c = {};
        if (project_dir.empty()) {
            c.errors.push_back("a kodhoz elobb mentsd a projektet (a kod a projekt mappajaban van)");
            return false;
        }
        for (auto const& s : sc.shapes) {
            if (!s.script[0]) continue;
            std::string const cls = class_name(s);
            if (!valid_class_name(cls)) {
                c.errors.push_back(std::string(s.name) + ": a kodfajl neve (" + s.script +
                                   ") nem lehet osztalynev (betu, szam, _; nem kezdodhet szammal)");
                continue;
            }
            fs::path const src = project_dir / "kod" / (cls + ".cpp");
            std::error_code ec;
            if (!fs::is_regular_file(src, ec)) {
                c.errors.push_back(std::string(s.name) + ": nincs meg a kodfajl: " + src.string());
                continue;
            }
            if (std::find(c.sources.begin(), c.sources.end(), src) == c.sources.end())
                c.sources.push_back(src);
        }
        if (!c.errors.empty()) return false;

        std::string err;
        fs::path const env = Platform::find_compiler(err);
        if (env.empty()) { c.errors.push_back(err); return false; }

        fs::path const out = project_dir / "build" / unit_name(sc);
        std::error_code ec;
        fs::create_directories(out, ec);
        fs::copy_file(api_header, project_dir / "gorbe_api.hpp", fs::copy_options::overwrite_existing, ec);
        if (ec) {
            c.errors.push_back("nem irhato a gorbe_api.hpp a projekt mappajaba: " + ec.message());
            return false;
        }
        c.dll = out / (unit_name(sc) + ".dll");
        c.log = out / "forditas.log";
        fs::remove(c.dll, ec);          // ha bent maradna egy régi, ne azt töltsük be
        c.proc = std::make_unique<Platform::Process>();
        if (!c.proc->start(Platform::compile_command(env, c.sources, project_dir, c.dll, out, c.log), out, err)) {
            c.errors.push_back(err);
            return false;
        }
        return true;
    }

    // Kész-e a fordító (legfeljebb `ms`-ig vár).
    inline bool poll(Compilation& c, int ms) { return !c.proc || c.proc->wait(ms); }

    // A fordító naplójának feldolgozása. A hibasorok elejéről a projekt mappáját
    // levágjuk, hogy "kod\Gomb1.cpp(12): error C2065: ..." maradjon. Nem a mappa
    // szövegét keressük: a cl más kódlapon írja a naplót, így egy ékezetes mappanév
    // ("5. félév") betűre nem egyezne — a sor "\kod\" részénél vágunk.
    inline void finish(Compilation& c) {
        int const code = c.proc ? c.proc->exit_code() : 1;
        c.proc.reset();
        std::vector<std::string> lines;
        std::ifstream f(c.log);
        for (std::string ln; std::getline(f, ln);) {
            if (!ln.empty() && ln.back() == '\r') ln.pop_back();
            if (std::size_t const paren = ln.find('('); paren != std::string::npos)
                if (std::size_t const k = ln.rfind("\\kod\\", paren); k != std::string::npos)
                    ln.erase(0, k + 1);
            lines.push_back(ln);
        }
        auto has = [](std::string const& ln, char const* what) { return ln.find(what) != std::string::npos; };
        for (auto const& ln : lines) {
            if (has(ln, ": error ") || has(ln, ": fatal error ") || has(ln, " error D")) c.errors.push_back(ln);
            else if (has(ln, ": warning ")) ++c.warnings;
        }
        std::error_code ec;
        c.ok = code == 0 && c.errors.empty() && fs::is_regular_file(c.dll, ec);
        // Hiba, de nincs "error" sor (pl. a vcvars maga hasalt el): a napló vége.
        if (!c.ok && c.errors.empty()) {
            c.errors.push_back("a fordito hibaval allt le (kod " + std::to_string(code) + ")");
            for (std::size_t i = lines.size() > 5 ? lines.size() - 5 : 0; i < lines.size(); ++i)
                c.errors.push_back(lines[i]);
        }
    }

    // Egy új kódfájl váza: a két függvény üresen, egy-egy kikommentezett példával.
    inline std::string template_source(std::string const& cls) {
        return "#include \"../gorbe_api.hpp\"\n"
               "\n"
               "// Az alakzat kodja. Az Inditas gomb forditja le es inditja el.\n"
               "// A feluleten beallitott parameterek es a pozicio a KEZDOERTEKEK.\n"
               "struct " + cls + " : gorbe::ShapeScript {\n"
               "    // Egyszer, az Inditaskor (a kepletek felepitese elott). Itt lehet\n"
               "    // szerkezetet allitani: add_param, add_warp, formula.\n"
               "    void start() override {\n"
               "        // add_warp(\"x + 0.3*sin(3*y)\", \"y\", \"z\");\n"
               "    }\n"
               "\n"
               "    // Minden ciklusban. t: az ido (mp), dt: az elozo ciklus ota eltelt ido.\n"
               "    // Itt csak erteket allits: param(\"r\"), position(), rotation(), scale().\n"
               "    void update(float t, float dt) override {\n"
               "        // rotation().z += 0.5f * dt;\n"
               "        (void)t; (void)dt;\n"
               "    }\n"
               "};\n"
               "GORBE_SHAPE(" + cls + ")\n";
    }

    // A kódfájl az alakzathoz: ha még nincs ilyen fájl, a váz kerül bele (ha van, az
    // marad — így több alakzat is használhatja ugyanazt). Mellé a gorbe_api.hpp, hogy
    // a szerkesztő kódkiegészítése a fordítás előtt is működjön. A fájl útvonala jön
    // vissza, vagy üres (és `err`).
    inline fs::path create_source(fs::path const& project_dir, std::string const& cls,
                                  fs::path const& api_header, std::string& err) {
        if (project_dir.empty()) { err = "a kodhoz elobb mentsd a projektet"; return {}; }
        if (!valid_class_name(cls)) {
            err = "'" + cls + "' nem lehet osztalynev (betu, szam, _; nem kezdodhet szammal)";
            return {};
        }
        std::error_code ec;
        fs::create_directories(project_dir / "kod", ec);
        fs::copy_file(api_header, project_dir / "gorbe_api.hpp", fs::copy_options::overwrite_existing, ec);
        fs::path const src = project_dir / "kod" / (cls + ".cpp");
        if (!fs::exists(src, ec)) {
            std::ofstream f(src, std::ios::binary);
            f << template_source(cls);
            if (!f) { err = "nem irhato: " + src.string(); return {}; }
        }
        return src;
    }

    // A kész DLL betöltése és alakzatonként egy példány. A hiányzó GORBE_SHAPE vagy
    // a más API-verzió a runtime.errors-ba kerül; a többi alakzat kódja ettől fut.
    inline bool load(Compilation const& c, Platform::Module& m, Runtime& rt,
                     SceneDoc& sc, std::list<Param>& program) {
        std::string err;
        if (!m.load(c.dll, err)) { rt.errors.push_back(err); return false; }
        for (auto& s : sc.shapes) {
            if (!s.script[0]) continue;
            std::string const cls = class_name(s);
            Factory f;
            f.version = reinterpret_cast<int (*)()>(m.symbol("gorbe_api_version_" + cls));
            f.create  = reinterpret_cast<gorbe::ShapeScript* (*)()>(m.symbol("gorbe_create_" + cls));
            f.destroy = reinterpret_cast<void (*)(gorbe::ShapeScript*)>(m.symbol("gorbe_destroy_" + cls));
            rt.attach(s, sc, program, f);
        }
        return true;
    }

}

#endif //GORBE_SCRIPT_COMPILER_HPP
