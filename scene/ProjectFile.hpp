#ifndef GORBE_SCENE_PROJECTFILE_HPP
#define GORBE_SCENE_PROJECTFILE_HPP

#include <cmath>
#include <cstdio>
#include <list>
#include <stdexcept>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

#include "../raytrace/Material.hpp"
#include "../raytrace/Palette.hpp"
#include "Document.hpp"
#include "Scope.hpp"

// ===========================================================================
// A projektfájl (.gorbe.json): a program-szintű paraméterek és a jelenetek,
// bennük az objektumokkal. GL nélkül fordul és tesztelhető (tests/test_project.cpp).
//
// Csak a felhasználó MUNKÁJA kerül bele. Ami újraszámolható, az nem: a részecskék,
// a felépített fák, a kijelölés, a hibaüzenet — betöltés után egy Indítás
// mindent visszaépít.
//
// A szín és az anyag NÉVVEL kerül bele, nem sorszámmal: különben egy új szín
// felvétele a palettára csendben átszínezné a régi projekteket.
//
// A betöltés hibatűrő: a hiányzó mező alapértéket kap, az ismeretlen szín/anyag
// az elsőt, a túl hosszú szöveg levágódik — ezekről figyelmeztetés készül, de a
// projekt betöltődik. Csak a nem-JSON és a más formátumú/újabb verziójú fájl hiba.
// ===========================================================================
namespace ProjectFile {

    using json = nlohmann::json;

    inline constexpr char const* FORMAT  = "gorbe-projekt";
    inline constexpr int         VERSION = 1;
    inline constexpr char const* EXTENSION = ".gorbe.json";

    struct Project {
        std::list<Param>    program_params;
        std::list<SceneDoc> scenes;
    };

    namespace detail {

        inline json vec3(glm::vec3 v) { return json::array({v.x, v.y, v.z}); }
        inline json vec3(float const v[3]) { return json::array({v[0], v[1], v[2]}); }

        inline json params(std::list<Param> const& ps) {
            json a = json::array();
            for (auto const& p : ps) {
                json pj = {{"name", p.name}, {"value", p.value}, {"min", p.min}, {"max", p.max}};
                if (p.derived()) pj["expr"] = p.expr;   // képletes paraméter
                a.push_back(pj);
            }
            return a;
        }

        inline json object(Shape const& s) {
            constexpr float DEG = 57.29577951308232f;
            json warps = json::array();
            for (auto const& w : s.warps)
                warps.push_back({{"name", w.name}, {"x", w.fx}, {"y", w.fy}, {"z", w.fz},
                                 {"enabled", w.enabled}});
            float rot_deg[3] = {s.xform.rot[0] * DEG, s.xform.rot[1] * DEG, s.xform.rot[2] * DEG};
            json controls = json::array();
            for (glm::vec3 c : s.controls) controls.push_back(vec3(c));
            json o = {
                {"name", s.name},
                {"formula", s.formula},
                {"domain", s.domain},
                {"visible", s.visible},
                {"color", Raytrace::palette_name(s.color_idx)},
                {"material", Raytrace::material_name(s.material_idx)},
                {"own_d", s.own_d},
                {"d", s.d},
                {"transform", {{"pos", vec3(s.xform.pos)}, {"rot_deg", vec3(rot_deg)},
                               {"scale", vec3(s.xform.scale)}}},
                {"warps", warps},
                {"params", params(s.locals)},
                {"controls", controls},
            };
            // Variációs alakzatnál a kényszerek (a súlyok betöltéskor újraszámolódnak).
            if (s.vari) {
                json cs = json::array();
                for (glm::vec3 c : s.vari->centers) cs.push_back(vec3(c));
                o["variational"] = {{"centers", cs}, {"values", s.vari->values}};
            }
            return o;
        }

        inline json scene(SceneDoc const& sc) {
            json funcs = json::array();
            for (auto const& f : sc.funcs)
                funcs.push_back({{"name", f.name}, {"params", f.params}, {"body", f.body}});
            json objects = json::array();
            for (auto const& s : sc.shapes) objects.push_back(object(s));
            return {
                {"name", sc.name},
                {"domain", sc.domain},
                {"d", sc.d_ui},
                {"curvature", sc.curv_ui},
                {"camera", {{"eye", vec3(sc.view.eye)}, {"target", vec3(sc.view.target)},
                            {"fov", sc.view.fov}}},
                {"params", params(sc.params)},
                {"functions", funcs},
                {"objects", objects},
            };
        }

        // --- olvasás --------------------------------------------------------------

        struct Reader {
            std::vector<std::string>& warnings;

            // Mező a helyére, ha van és jó típusú; különben marad az alapérték.
            template<class T>
            void get(json const& j, char const* key, T& out, std::string const& where) {
                auto it = j.find(key);
                if (it == j.end()) return;
                try { out = it->get<T>(); }
                catch (json::exception const&) {
                    warnings.push_back(where + ": a(z) '" + key + "' mezo rossz tipusu, alapertek marad");
                }
            }

            template<std::size_t N>
            void str(json const& j, char const* key, char (&out)[N], std::string const& where) {
                std::string s;
                get(j, key, s, where);
                if (s.size() >= N)
                    warnings.push_back(where + ": a(z) '" + key + "' tul hosszu, levagva " +
                                       std::to_string(N - 1) + " karakterre");
                if (j.contains(key)) std::snprintf(out, N, "%s", s.c_str());
            }

            void vec3(json const& j, char const* key, float out[3], std::string const& where) {
                auto it = j.find(key);
                if (it == j.end()) return;
                bool ok = it->is_array() && it->size() == 3;
                for (std::size_t i = 0; ok && i < 3; ++i) ok = (*it)[i].is_number();
                if (!ok) {
                    warnings.push_back(where + ": a(z) '" + key + "' 3 elemu szamtomb kell legyen");
                    return;
                }
                for (std::size_t i = 0; i < 3; ++i) out[i] = (*it)[i].get<float>();
            }

            void params(json const& j, char const* key, std::list<Param>& out, std::string const& where) {
                auto it = j.find(key);
                if (it == j.end() || !it->is_array()) return;
                for (auto const& pj : *it) {
                    auto& p = out.emplace_back();
                    str(pj, "name", p.name, where + " parametere");
                    get(pj, "value", p.value, where);
                    get(pj, "min", p.min, where);
                    str(pj, "expr", p.expr, where + " parametere");
                    get(pj, "max", p.max, where);
                    p.fit_range_to_value();
                }
            }

            template<int COUNT>
            int index_by_name(json const& j, char const* key, char const* (*name_of)(int),
                              std::string const& where) {
                std::string n;
                get(j, key, n, where);
                if (n.empty()) return 0;
                for (int i = 0; i < COUNT; ++i)
                    if (n == name_of(i)) return i;
                warnings.push_back(where + ": ismeretlen " + key + " '" + n + "', az elso lesz helyette");
                return 0;
            }

            void object(json const& j, Shape& s, std::string const& where) {
                constexpr float RAD = 0.017453292519943295f;
                str(j, "name", s.name, where);
                std::string const w = where + " '" + s.name + "'";
                str(j, "formula", s.formula, w);
                str(j, "domain", s.domain, w);
                get(j, "visible", s.visible, w);
                s.color_idx = index_by_name<Raytrace::PALETTE_COUNT>(j, "color", Raytrace::palette_name, w);
                s.material_idx = index_by_name<Raytrace::MATERIAL_COUNT>(j, "material", Raytrace::material_name, w);
                get(j, "own_d", s.own_d, w);
                get(j, "d", s.d, w);
                if (auto t = j.find("transform"); t != j.end() && t->is_object()) {
                    vec3(*t, "pos", s.xform.pos, w);
                    float deg[3] = {0, 0, 0};
                    vec3(*t, "rot_deg", deg, w);
                    for (int i = 0; i < 3; ++i) s.xform.rot[i] = deg[i] * RAD;
                    vec3(*t, "scale", s.xform.scale, w);
                }
                if (auto ws = j.find("warps"); ws != j.end() && ws->is_array())
                    for (auto const& wj : *ws) {
                        Warp wp;
                        str(wj, "name", wp.name, w + " warpja");
                        str(wj, "x", wp.fx, w + " warpja");
                        str(wj, "y", wp.fy, w + " warpja");
                        str(wj, "z", wp.fz, w + " warpja");
                        get(wj, "enabled", wp.enabled, w);
                        s.warps.push_back(wp);
                    }
                params(j, "params", s.locals, w);
                if (auto cs = j.find("controls"); cs != j.end() && cs->is_array())
                    for (auto const& cj : *cs) {
                        float c[3] = {0.0f, 0.0f, 0.0f};
                        json const wrap = {{"c", cj}};
                        vec3(wrap, "c", c, w + " kontrollpontja");
                        s.controls.push_back({c[0], c[1], c[2]});
                    }
                if (auto vj = j.find("variational"); vj != j.end() && vj->is_object()) {
                    auto v = std::make_shared<Variational>();
                    std::vector<float> values;
                    get(*vj, "values", values, w);
                    if (auto cs = vj->find("centers"); cs != vj->end() && cs->is_array())
                        for (auto const& cj : *cs) {
                            float c[3] = {0.0f, 0.0f, 0.0f};
                            json const wrap = {{"c", cj}};
                            vec3(wrap, "c", c, w + " kenyszere");
                            v->centers.push_back({c[0], c[1], c[2]});
                        }
                    if (values.size() == v->centers.size()) {
                        v->values = std::move(values);
                        if (v->solve()) s.vari = std::move(v);
                    }
                    if (!s.vari)
                        warnings.push_back(w + ": a variacios kenyszerek hibasak, a keplet marad");
                }
            }

            void scene(json const& j, SceneDoc& sc, std::string const& where) {
                str(j, "name", sc.name, where);
                std::string const w = "jelenet '" + std::string(sc.name) + "'";
                str(j, "domain", sc.domain, w);
                get(j, "d", sc.d_ui, w);
                get(j, "curvature", sc.curv_ui, w);
                if (auto c = j.find("camera"); c != j.end() && c->is_object()) {
                    float e[3] = {sc.view.eye.x, sc.view.eye.y, sc.view.eye.z};
                    float t[3] = {sc.view.target.x, sc.view.target.y, sc.view.target.z};
                    vec3(*c, "eye", e, w);
                    vec3(*c, "target", t, w);
                    sc.view.eye = {e[0], e[1], e[2]};
                    sc.view.target = {t[0], t[1], t[2]};
                    get(*c, "fov", sc.view.fov, w);
                }
                params(j, "params", sc.params, w);
                if (auto fs = j.find("functions"); fs != j.end() && fs->is_array())
                    for (auto const& fj : *fs) {
                        auto& f = sc.funcs.emplace_back();
                        str(fj, "name", f.name, w + " fuggvenye");
                        str(fj, "params", f.params, w + " fuggvenye");
                        str(fj, "body", f.body, w + " fuggvenye");
                    }
                if (auto os = j.find("objects"); os != j.end() && os->is_array())
                    for (auto const& oj : *os) object(oj, sc.shapes.emplace_back(), w + " objektuma");
            }
        };
    }

    // A projekt szövegként (szépen tördelt JSON). A `Scenes` bármilyen tároló, aminek
    // az elemei SceneDoc-ok (vagy abból származnak, mint az app/Scene).
    template<class Scenes>
    std::string to_text(std::list<Param> const& program_params, Scenes const& scenes) {
        json scs = json::array();
        for (SceneDoc const& sc : scenes) scs.push_back(detail::scene(sc));
        json root = {
            {"format", FORMAT},
            {"version", VERSION},
            {"program_params", detail::params(program_params)},
            {"scenes", scs},
        };
        return root.dump(2) + "\n";
    }

    // Szövegből projekt. Hibánál (nem JSON, más formátum, újabb verzió) kivételt dob;
    // a javítható eltérésekről a `warnings`-ba ír.
    inline Project from_text(std::string const& text, std::vector<std::string>& warnings) {
        json root;
        try {
            root = json::parse(text);
        } catch (json::parse_error const& e) {
            throw std::runtime_error(std::string("nem ervenyes JSON: ") + e.what());
        }
        if (!root.is_object() || root.value("format", "") != FORMAT)
            throw std::runtime_error("ez nem gorbe-projekt fajl (hianyzik a \"format\": \"" +
                                     std::string(FORMAT) + "\")");
        int const version = root.value("version", 0);
        if (version > VERSION)
            throw std::runtime_error("a fajl egy ujabb programverzioval keszult (" +
                                     std::to_string(version) + ". verzio), ez csak a " +
                                     std::to_string(VERSION) + ".-et ismeri");

        Project pr;
        detail::Reader rd{warnings};
        rd.params(root, "program_params", pr.program_params, "program-szint");
        if (auto s = root.find("scenes"); s != root.end() && s->is_array()) {
            int k = 1;
            for (auto const& sj : *s)
                rd.scene(sj, pr.scenes.emplace_back(), std::to_string(k++) + ". jelenet");
        }
        if (pr.scenes.empty()) {
            warnings.push_back("a projektben nem volt jelenet, egy uresel indul");
            std::snprintf(pr.scenes.emplace_back().name, 32, "Jelenet 1");
        }
        return pr;
    }

}

#endif //GORBE_SCENE_PROJECTFILE_HPP
