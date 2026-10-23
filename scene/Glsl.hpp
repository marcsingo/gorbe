#ifndef GORBE_SCENE_GLSL_HPP
#define GORBE_SCENE_GLSL_HPP

#include <algorithm>
#include <cstdio>
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

    // Egy variációs felület 3D textúraként (generate, variational_texture). A felület
    // függvénye a kényszerek összege (több száz tag): a sugárkövetés lépésenként ezt
    // nem bírja, ezért a GPU egyszer kiértékeli egy VOLUME_N^3-as rácson (a `sum`
    // függvénnyel, a felület saját koordinátáiban, a [lo, hi] dobozban), és a shader a
    // textúrából olvas — egy kiértékelés így egyetlen (hardveresen interpolált) olvasás.
    struct Volume {
        Variational const* v = nullptr;
        unsigned version = 0;                // a felület melyik változatából sütöttük
        std::string sum;                     // a teljes összeg GLSL-függvénye (gp_cube-ot hív)
        std::string name;                    // a függvény neve (gp_rbf_...)
        std::string sampler;                 // a uniform sampler3D neve
        glm::vec3 lo{0.0f}, hi{0.0f};
    };
    inline constexpr int VOLUME_N = 128;

    struct Code {
        std::string source;                  // uniform + előtag + segédfüggvények + gp_F/gp_D
        std::vector<float const*> params;    // u_p[i] = *params[i] (képkockánként feltöltendő)
        int shapes = 0;                      // hány gp_F<i> van
        std::vector<Shape const*> shape_of;  // a gp_F<i> melyik alakzaté
        std::vector<Volume> volumes;         // a textúrás variációs felületek (kisütendők)
    };

    namespace detail {
        // GLSL lebegőpontos literál (mindig ponttal vagy kitevővel).
        inline std::string flt(float f) {
            char b[32];
            std::snprintf(b, sizeof(b), "%.9e", static_cast<double>(f));
            return b;
        }
        inline std::string vec3(glm::vec3 v) {
            return "vec3(" + flt(v.x) + ", " + flt(v.y) + ", " + flt(v.z) + ")";
        }

        // A variációs függvény textúrás változata: a doboz belsejében a textúra (a
        // rácspontok a texelközéppontok), kívül folytatás a doboztól mért távolsággal —
        // a felület közelében |∇F| ~ 1 (Variational::add_oriented), tehát ez illeszkedik,
        // és a menetelés a dobozon kívül is jól lép. A doboz bőven a felület körül van,
        // a peremén F > 0, ezért a folytatás ott folytonos.
        inline std::string texture_function(Volume const& vol) {
            std::string const n = std::to_string(VOLUME_N);
            return "uniform sampler3D " + vol.sampler + ";\n"
                   "float " + vol.name + "(vec3 q) {\n"
                   "    const vec3 lo = " + vec3(vol.lo) + ", hi = " + vec3(vol.hi) + ";\n"
                   "    vec3 c = clamp(q, lo, hi);\n"
                   "    vec3 uv = (c - lo) / (hi - lo) * (float(" + n + " - 1) / float(" + n + ")) + 0.5 / float(" + n + ");\n"
                   "    float v = texture(" + vol.sampler + ", uv).r;\n"
                   "    float d = length(q - c);\n"
                   "    return d > 0.0 ? max(v, 0.0) + d : v;\n"
                   "}\n";
        }
    }

    // `variational_texture`: a variációs felületek 3D textúrából (Code::volumes; a
    // textúrákat a hívó süti ki és köti be — model/RayView.hpp). Nélküle a teljes
    // összeg kerül a kódba (pontos, de lépésenként több száz tag).
    inline Code generate(SceneDoc const& sc, bool variational_texture = false) {
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
        // A variációs felületek összegét textúra-olvasásra cseréljük (az összeg a
        // Volume-ba kerül, azzal sütjük ki a textúrát).
        if (variational_texture) {
            for (auto const& s : sc.shapes) {
                Variational const* v = s.vari.get();
                if (!v || std::any_of(c.volumes.begin(), c.volumes.end(),
                                      [v](Volume const& x) { return x.v == v; })) continue;
                std::string const head = "float " + v->glsl_name() + "(";
                auto it = std::find_if(functions.begin(), functions.end(),
                                       [&](std::string const& f) { return f.rfind(head, 0) == 0; });
                if (it == functions.end() || v->centers.empty()) continue;   // nincs a kódban
                Volume vol;
                vol.v = v;
                vol.version = v->version;
                vol.sum = *it;
                vol.name = v->glsl_name();
                vol.sampler = "u_vol" + std::to_string(c.volumes.size());
                vol.lo = vol.hi = v->centers.front();
                for (glm::vec3 p : v->centers) { vol.lo = glm::min(vol.lo, p); vol.hi = glm::max(vol.hi, p); }
                float const extent = std::max({vol.hi.x - vol.lo.x, vol.hi.y - vol.lo.y, vol.hi.z - vol.lo.z});
                glm::vec3 const pad(0.25f * extent + 0.1f);
                vol.lo -= pad;
                vol.hi += pad;
                *it = detail::texture_function(vol);
                c.volumes.push_back(std::move(vol));
            }
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
