#ifndef GORBE_SCENE_GLSL_HPP
#define GORBE_SCENE_GLSL_HPP

#include <algorithm>
#include <string>
#include <vector>

#include "../matek/Kif.hpp"
#include "Document.hpp"

// ---------------------------------------------------------------------------
// A felépített jelenet (Build::build után) GLSL-kódként: a GPU-s sugárkövetéshez.
//
// Alakzatonként (a lista sorrendjében, csak a felépítettek) két függvény:
//     float gp_F<i>(float x, float y, float z)   az implicit függvény (F < 0 belül)
//     float gp_D<i>(float x, float y, float z)   a tartomány-feltétel (> 0: látszik)
// A kifejezések a kiíróból jönnek (Matek::Analizis::glsl_text), tehát ugyanazt
// számolják, mint a CPU: a transzformáció, a warpok, a CSG és a saját függvények
// már be vannak helyettesítve a fába.
//
// A PARAMÉTEREK (alakzat-, jelenet- és program-szintűek, a transzformáció mezői, a
// kód saját paraméterei és a `t` idő) mind a `uniform float u_p[N]` elemei: a fában a
// CÍMÜK van, és a namer minden címhez egy indexet ad. A CPU minden képkockán
// feltölti őket (Code::params) — így a csúszkák és a kód írásai újrafordítás nélkül,
// élőben hatnak, mint a CPU-n. Újrafordítás csak a SZERKEZET változásakor kell
// (Build::scene_sig).
//
// A forrás #version és main nélküli: a felhasználó (a sugárkövető shader) teszi elé,
// illetve köré.
// ---------------------------------------------------------------------------
namespace Glsl {

    // A kiíró által hívott segédfüggvények (a GLSL-ben nincs megfelelőjük, vagy más).
    inline constexpr char const* PRELUDE = R"(
float gp_sq(float a) { return a * a; }
float gp_cube(float a) { return a * a * a; }
// A std::pow szerint: egész kitevőnél negatív alapra is (a GLSL pow-ja ott definiálatlan).
float gp_pow(float a, float b) {
    if (b == floor(b)) {
        if (b == 0.0) return 1.0;
        float r = pow(abs(a), b);
        return (a < 0.0 && mod(b, 2.0) != 0.0) ? -r : r;
    }
    return pow(a, b);
}
float gp_ctg(float a) { return 1.0 / tan(a); }
float gp_log10(float a) { return log(a) * 0.4342944819032518; }
float gp_cbrt(float a) { return sign(a) * pow(abs(a), 1.0 / 3.0); }
)";

    struct Code {
        std::string source;                  // uniform + előtag + segédfüggvények + gp_F/gp_D
        std::vector<float const*> params;    // u_p[i] = *params[i] (képkockánként feltöltendő)
        int shapes = 0;                      // hány gp_F<i> van
        std::vector<Shape const*> shape_of;  // a gp_F<i> melyik alakzaté
    };

    inline Code generate(SceneDoc const& sc) {
        using Matek::Analizis::Kif;
        Code c;
        std::vector<std::string> functions;
        auto namer = [&c](float const* p) -> std::string {
            for (std::size_t i = 0; i < c.params.size(); ++i)
                if (c.params[i] == p) return "u_p[" + std::to_string(i) + "]";
            c.params.push_back(p);
            return "u_p[" + std::to_string(c.params.size() - 1) + "]";
        };
        std::string body;
        for (auto const& s : sc.shapes) {
            if (!s.tree) continue;
            std::string const i = std::to_string(c.shapes++);
            c.shape_of.push_back(&s);
            body += "float gp_F" + i + "(float x, float y, float z) {\n    return " +
                    Matek::Analizis::glsl_text(Kif(s.tree), namer, &functions) + ";\n}\n";
            body += "float gp_D" + i + "(float x, float y, float z) {\n    return " +
                    (s.dom_tree ? Matek::Analizis::glsl_text(Kif(s.dom_tree), namer, &functions)
                                : std::string("1.0")) + ";\n}\n";
        }
        // Az uniform tömb mérete csak a kiírás után ismert (legalább 1: üres tömb nem lehet).
        c.source = "uniform float u_p[" + std::to_string(std::max<std::size_t>(c.params.size(), 1)) + "];\n";
        c.source += PRELUDE;
        for (auto const& f : functions) c.source += f;
        c.source += body;
        return c;
    }

    // A paraméterek pillanatnyi értéke (a glUniform1fv-hez).
    inline std::vector<float> values(Code const& c) {
        std::vector<float> v;
        v.reserve(c.params.size());
        for (float const* p : c.params) v.push_back(*p);
        if (v.empty()) v.push_back(0.0f);
        return v;
    }

}

#endif //GORBE_SCENE_GLSL_HPP
