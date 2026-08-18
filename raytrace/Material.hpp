#ifndef GORBE_RAYTRACE_MATERIAL_HPP
#define GORBE_RAYTRACE_MATERIAL_HPP

#include <cmath>
#include <cstdint>

#include <glm.hpp>

// Előre megadott ANYAGOK a sugárkövetett fényképhez (üveg, fém, fa, ...).
//
// Az anyag a SZÍN MELLÉ jön, nem helyette: a paletta adja az alapszínt, az anyag
// pedig azt mondja meg, hogyan viselkedik a fény rajta. Ugyanaz a piros lehet
// matt gumi, csillogó műanyag vagy áttetsző üveg.
//
// Az anyagot négy dolog írja le:
//   1. helyi árnyalás (diffúz/csúcsfény) — minden anyagnál van;
//   2. TÜKRÖZÉS: mennyit lát a környezetéből (fém, króm);
//   3. ÁTLÁTSZÓSÁG + törésmutató: átenged-e fényt, és mennyire töri meg (üveg);
//   4. MINTÁZAT: a felület pontról pontra változó alapszíne (fa erezet, márvány).
//
// A `metallic` az, ami a fémet a műanyagtól elválasztja: fémnél a csúcsfény ÉS a
// tükörkép is felveszi az anyag színét, műanyagnál a csúcsfény FEHÉR marad.
namespace Raytrace {

    // Eljárásos (számított) felületi mintázat. Nincs textúrafájl: a mintát a
    // TÉRBELI pontból számoljuk, így minden alakzatra ráfeszül, torzulás nélkül.
    enum class Pattern { None, Wood, Marble };

    struct Material {
        char const* name;

        float diffuse;        // szórt visszaverődés erőssége
        float specular;       // csúcsfény erőssége
        float shininess;      // csúcsfény élessége (nagy = kicsi, éles folt)

        float reflectivity;   // 0..1 tükrözés merőleges nézetnél
        bool  metallic;       // a csúcsfény és a tükörkép felveszi az anyag színét

        float transparency;   // 0..1 átlátszóság
        float ior;            // törésmutató (üveg ~1.5, víz ~1.33, levegő 1)
        float absorb;         // Beer-féle elnyelés: mélyebb üvegben telítettebb szín

        Pattern pattern;
        float   pattern_scale;
    };

    // A felkínált anyagok. Az első az alapértelmezett (műanyag), hogy a korábban
    // készült képek ugyanúgy nézzenek ki, mint eddig.
    inline Material const MATERIALS[] = {
        //  név            diff  spec  shin   refl  metal  transp  ior   absorb  minta            skála
        {"Muanyag",        0.80f, 0.38f,  48.0f, 0.04f, false, 0.00f, 1.50f, 0.00f, Pattern::None,   1.0f},
        {"Gumi (matt)",    0.95f, 0.04f,   6.0f, 0.00f, false, 0.00f, 1.50f, 0.00f, Pattern::None,   1.0f},
        {"Keramia",        0.85f, 0.55f, 110.0f, 0.06f, false, 0.00f, 1.50f, 0.00f, Pattern::None,   1.0f},
        {"Fem",            0.18f, 0.85f, 120.0f, 0.55f, true,  0.00f, 1.50f, 0.00f, Pattern::None,   1.0f},
        {"Krom (tukor)",   0.05f, 1.00f, 400.0f, 0.90f, true,  0.00f, 1.50f, 0.00f, Pattern::None,   1.0f},
        {"Uveg",           0.06f, 0.90f, 250.0f, 0.08f, false, 0.94f, 1.50f, 0.35f, Pattern::None,   1.0f},
        {"Fa",             0.92f, 0.10f,  14.0f, 0.00f, false, 0.00f, 1.50f, 0.00f, Pattern::Wood,   1.6f},
        {"Marvany",        0.88f, 0.45f,  90.0f, 0.05f, false, 0.00f, 1.50f, 0.00f, Pattern::Marble, 0.9f},
    };

    inline constexpr int MATERIAL_COUNT = static_cast<int>(sizeof(MATERIALS) / sizeof(MATERIALS[0]));

    inline Material const& material_of(int idx) {
        if (idx < 0 || idx >= MATERIAL_COUNT) idx = 0;
        return MATERIALS[idx];
    }

    inline char const* material_name(int idx) { return material_of(idx).name; }

    // -----------------------------------------------------------------------
    // Zaj a mintázatokhoz.
    //
    // Rácspontokhoz rendelt véletlen értékek háromlineáris simítással (value noise).
    // A "véletlen" itt egy HASH: ugyanaz a pont mindig ugyanazt adja, tehát a minta
    // nem sercen két render között, és nem kell táblázatot tárolni.
    // -----------------------------------------------------------------------
    namespace detail {

        inline float lattice(int x, int y, int z) {
            // Előjeles -> előjel nélküli konverzió jól definiált (moduláris), a
            // szorzás túlcsordulása pedig unsigned-on nem UB.
            std::uint32_t h = static_cast<std::uint32_t>(x) * 374761393u
                            + static_cast<std::uint32_t>(y) * 668265263u
                            + static_cast<std::uint32_t>(z) * 1274126177u;
            h = (h ^ (h >> 13)) * 1274126177u;
            h ^= h >> 16;
            return static_cast<float>(h & 0xFFFFFFu) / static_cast<float>(0xFFFFFF);
        }

        inline float vnoise(glm::vec3 p) {
            float const fx = std::floor(p.x), fy = std::floor(p.y), fz = std::floor(p.z);
            int const X = static_cast<int>(fx), Y = static_cast<int>(fy), Z = static_cast<int>(fz);
            glm::vec3 f{p.x - fx, p.y - fy, p.z - fz};
            glm::vec3 const w = f * f * (glm::vec3(3.0f) - 2.0f * f);   // simítás

            auto L = [&](int a, int b, int c) { return lattice(X + a, Y + b, Z + c); };
            float const x00 = glm::mix(L(0, 0, 0), L(1, 0, 0), w.x);
            float const x10 = glm::mix(L(0, 1, 0), L(1, 1, 0), w.x);
            float const x01 = glm::mix(L(0, 0, 1), L(1, 0, 1), w.x);
            float const x11 = glm::mix(L(0, 1, 1), L(1, 1, 1), w.x);
            return glm::mix(glm::mix(x00, x10, w.y), glm::mix(x01, x11, w.y), w.z);
        }

        // Több, egyre finomabb zajréteg összege — ettől lesz a fa erezete
        // szabálytalan, nem gépiesen körkörös.
        inline float turbulence(glm::vec3 p, int octaves) {
            float sum = 0.0f, amp = 0.5f;
            for (int i = 0; i < octaves; ++i) {
                sum += amp * vnoise(p);
                p   *= 2.03f;      // nem pontosan 2: így nem esnek egybe a rácsok
                amp *= 0.5f;
            }
            return sum;
        }

    } // namespace detail

    // Az alapszín módosítása a felületi pont alapján. Mintázat nélküli anyagnál
    // változatlanul visszaadja a színt (tehát ingyen van).
    //
    // A minta a VILÁGKOORDINÁTÁS pontból számol, és a fa évgyűrűi a z tengely
    // körül futnak (a jelenetben a z a függőleges) — egy álló henger így úgy néz
    // ki, mint egy fatörzs.
    inline glm::vec3 pattern_albedo(glm::vec3 base, Material const& m, glm::vec3 p) {
        if (m.pattern == Pattern::None) return base;

        glm::vec3 const q = p * m.pattern_scale;

        if (m.pattern == Pattern::Wood) {
            // Évgyűrűk: a z tengelytől mért távolság, hullámos peremmel.
            float const r    = std::sqrt(q.x * q.x + q.y * q.y)
                             + 0.35f * detail::turbulence(q * 0.7f, 3);
            // |sin| és nem törtrész: a törtrésznek UGRÁSA van minden egésznél, ami
            // csúnyán tördelt (aliasos) élt adna. Így a gyűrű pereme sima.
            float const ring = std::abs(std::sin(3.14159265358979f * r));
            // A kitevő <1: a sötét gyűrű keskeny, a világos test széles.
            float const band = std::pow(ring, 0.35f);
            // Finom rostozat a szál (z) mentén: hosszan elnyújtott zaj.
            float const grain = 0.85f + 0.15f * detail::vnoise(
                                    glm::vec3(q.x * 6.0f, q.y * 6.0f, q.z * 0.6f));
            return base * (glm::mix(0.55f, 1.15f, band) * grain);
        }

        // Márvány: a zajjal eltorzított szinusz nullátmenetei adják az ereket.
        float const v    = std::sin((q.x + 2.6f * detail::turbulence(q * 0.6f, 4))
                                    * 3.14159265358979f);
        float const vein = std::pow(std::abs(v), 0.35f);
        glm::vec3 const light = glm::mix(base, glm::vec3(0.93f), 0.5f);  // világos kő
        glm::vec3 const dark  = base * 0.45f;                            // sötét ér
        return glm::mix(dark, light, vein);
    }

}

#endif //GORBE_RAYTRACE_MATERIAL_HPP
