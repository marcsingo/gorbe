// A projektfajl (scene/ProjectFile.hpp): mentes -> betoltes ugyanazt a projektet adja,
// es a hibas / regi / hianyos fajl ertheto hibat vagy figyelmeztetest ad.
#include <cmath>
#include <cstdio>
#include <string>

#include "scene/Build.hpp"
#include "scene/ProjectFile.hpp"

using Matek::Analizis::Kif;

static int failures = 0;
static void ok(std::string const& what, bool c, std::string const& info = "") {
    if (!c) ++failures;
    std::printf("  %-50s %-8s %s\n", what.c_str(), c ? "[OK]" : "[HIBA]", info.c_str());
}

template<std::size_t N>
static void set(char (&dst)[N], char const* s) { std::snprintf(dst, N, "%s", s); }

static Param& param(std::list<Param>& l, char const* n, float v, float lo, float hi) {
    auto& p = l.emplace_back();
    set(p.name, n); p.value = v; p.min = lo; p.max = hi;
    return p;
}

int main() {
    // --- egy minden mezot kitolto projekt -------------------------------------
    std::list<Param> program;
    param(program, "g1", 0.5f, 0.0f, 2.0f);

    std::list<SceneDoc> scenes;
    auto& sc = scenes.emplace_back();
    set(sc.name, "Elso");
    set(sc.domain, "x^2 + y^2 + z^2 < 64");
    sc.d_ui = 1.5f; sc.curv_ui = 0.25f;
    sc.view.eye = {1, 2, 3}; sc.view.target = {0, 0, 1}; sc.view.fov = 30.0f;
    param(sc.params, "s1", 2.0f, -5.0f, 5.0f);
    auto& f = sc.funcs.emplace_back();
    set(f.name, "g"); set(f.params, "u, k = 1"); set(f.body, "u^2 + k");

    auto& a = sc.shapes.emplace_back();
    set(a.name, "gomb1"); set(a.formula, "g(x) + g(y) + g(z) - r^2");
    set(a.domain, "z > -1");
    a.visible = false; a.color_idx = 3; a.material_idx = 2;
    a.own_d = true; a.d = 0.75f;
    a.xform.pos[0] = 1.0f; a.xform.rot[2] = 0.5f; a.xform.scale[1] = 2.0f;
    Warp w; set(w.name, "Csavaras"); set(w.fx, "x*cos(tw*z)"); w.enabled = false;
    a.warps.push_back(w);
    param(a.locals, "r", 1.25f, 0.0f, 3.0f);

    auto& b = sc.shapes.emplace_back();
    set(b.name, "unio1"); set(b.formula, "unio(gomb1, z)");
    auto& sc2 = scenes.emplace_back();
    set(sc2.name, "Masodik");

    std::string const text = ProjectFile::to_text(program, scenes);

    std::printf("=== 1. Mentes -> betoltes: minden mezo visszajon ===\n");
    std::vector<std::string> warn;
    auto pr = ProjectFile::from_text(text, warn);
    ok("nincs figyelmeztetes", warn.empty(), warn.empty() ? "" : warn.front());
    ok("program-parameter", pr.program_params.size() == 1 &&
       std::string(pr.program_params.front().name) == "g1" && pr.program_params.front().max == 2.0f);
    ok("ket jelenet, sorrendben", pr.scenes.size() == 2 &&
       std::string(pr.scenes.back().name) == "Masodik");

    SceneDoc const& s = pr.scenes.front();
    ok("jelenet: nev, munkater, d, gorbulet",
       std::string(s.name) == "Elso" && std::string(s.domain) == sc.domain &&
       s.d_ui == 1.5f && s.curv_ui == 0.25f);
    ok("jelenet: kamera", s.view.eye == glm::vec3(1, 2, 3) && s.view.target == glm::vec3(0, 0, 1) &&
       s.view.fov == 30.0f);
    ok("jelenet: parameter tartomannyal", s.params.front().value == 2.0f && s.params.front().min == -5.0f);
    ok("jelenet: sajat fuggveny", s.funcs.size() == 1 && std::string(s.funcs.front().params) == "u, k = 1");
    ok("ket objektum, sorrendben", s.shapes.size() == 2 && std::string(s.shapes.back().name) == "unio1");

    Shape const& o = s.shapes.front();
    ok("objektum: nev, keplet, tartomany",
       std::string(o.name) == "gomb1" && std::string(o.formula) == a.formula &&
       std::string(o.domain) == "z > -1");
    ok("objektum: lathatosag, szin, anyag", !o.visible && o.color_idx == 3 && o.material_idx == 2);
    ok("objektum: sajat d", o.own_d && o.d == 0.75f);
    ok("objektum: transzformacio (fok <-> radian)",
       o.xform.pos[0] == 1.0f && std::abs(o.xform.rot[2] - 0.5f) < 1e-5f && o.xform.scale[1] == 2.0f);
    ok("objektum: warp", o.warps.size() == 1 && !o.warps[0].enabled &&
       std::string(o.warps[0].fx) == "x*cos(tw*z)");
    ok("objektum: lokalis parameter", o.locals.front().value == 1.25f && o.locals.front().max == 3.0f);

    ok("masodszor mentve betu szerint ugyanaz", ProjectFile::to_text(pr.program_params, pr.scenes) == text);

    std::printf("\n=== 2. A betoltott projekt fel is epul, ugyanazzal az eredmennyel ===\n");
    {
        std::string e1 = Build::build(sc, program);
        std::string e2 = Build::build(pr.scenes.front(), pr.program_params);
        ok("mindketto felepul", e1.empty() && e2.empty(), e1 + e2);
        glm::vec3 p{0.3f, -0.4f, 0.2f};
        float v1 = Kif(sc.shapes.back().tree).at(p), v2 = Kif(pr.scenes.front().shapes.back().tree).at(p);
        ok("ugyanaz a fuggveny", std::abs(v1 - v2) < 1e-5f, std::to_string(v1) + " / " + std::to_string(v2));
    }

    std::printf("\n=== 3. Hibak es figyelmeztetesek ===\n");
    auto error_of = [](std::string const& t) -> std::string {
        std::vector<std::string> w;
        try { ProjectFile::from_text(t, w); } catch (std::exception const& e) { return e.what(); }
        return "";
    };
    ok("nem JSON -> hiba", error_of("{ ez nem json").find("nem ervenyes JSON") == 0);
    ok("mas formatum -> hiba", error_of(R"({"format": "valami"})").find("nem gorbe-projekt") != std::string::npos);
    ok("ujabb verzio -> hiba",
       error_of(R"({"format": "gorbe-projekt", "version": 99})").find("ujabb programverzioval") != std::string::npos);

    {
        std::vector<std::string> w;
        auto p = ProjectFile::from_text(R"({"format": "gorbe-projekt", "version": 1,
            "scenes": [{"name": "J", "objects": [
                {"name": "a", "formula": "x", "color": "Lila-ultra", "d": "sok"}]}]})", w);
        Shape const& sh = p.scenes.front().shapes.front();
        ok("ismeretlen szin -> figyelmeztetes, az elso szin", sh.color_idx == 0 && !w.empty(),
           w.empty() ? "" : w.front());
        ok("rossz tipusu mezo -> alapertek + figyelmeztetes", sh.d == 2.0f && w.size() == 2);
        ok("hianyzo mezok -> alapertekek", sh.visible && !sh.own_d && sh.xform.scale[0] == 1.0f);
    }
    {
        std::vector<std::string> w;
        auto p = ProjectFile::from_text(R"({"format": "gorbe-projekt", "version": 1})", w);
        ok("jelenet nelkul -> egy ures jelenettel indul", p.scenes.size() == 1 && !w.empty());
    }
    {
        std::vector<std::string> w;
        std::string hosszu(300, 'x');
        auto p = ProjectFile::from_text(R"({"format": "gorbe-projekt", "version": 1,
            "scenes": [{"name": ")" + hosszu + R"("}]})", w);
        ok("tul hosszu szoveg -> levagva + figyelmeztetes",
           std::string(p.scenes.front().name).size() == 31 && !w.empty());
    }
    {
        std::vector<std::string> w;
        auto p = ProjectFile::from_text(R"({"format": "gorbe-projekt", "version": 1,
            "scenes": [{"name": "Árvíztűrő \"tukor\""}]})", w);
        ok("Unicode es idezojel a szovegben", std::string(p.scenes.front().name) ==
           "\xC3\x81rv\xC3\xADzt\xC5\xB1r\xC5\x91 \"tukor\"");
    }

    std::printf("\n%s (%d hiba)\n", failures ? "SIKERTELEN" : "MINDEN RENDBEN", failures);
    return failures ? 1 : 0;
}
