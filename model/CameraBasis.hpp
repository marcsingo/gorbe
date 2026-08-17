#ifndef GORBE_CAMERABASIS_HPP
#define GORBE_CAMERABASIS_HPP

#include <cmath>
#include <glm.hpp>

// A kamera irányvektorai és az egérkezelés előjel-konvenciója, GL és ablak nélkül —
// hogy tesztelhető legyen. A Camera3D ezt használja.
namespace CameraBasis {

    // Z-UP konvenció: a jelenetben a z a függőleges (a sík-sablon z=0, a henger a z
    // mentén áll), ezért a kamera is a z-t tartja a képernyőn függőlegesen. Így a
    // talajrács vízszintesnek látszik, és forgatáskor sem billen el a horizont.
    //   yaw   = azimut az xy síkban (0 = +x felé)
    //   pitch = emelkedés az xy sík fölött (negatív = lefelé nézünk)
    inline constexpr glm::vec3 WORLD_UP{0.0f, 0.0f, 1.0f};

    struct Basis {
        glm::vec3 front;
        glm::vec3 right;
        glm::vec3 up;
    };

    inline Basis from_angles(float yaw_deg, float pitch_deg) {
        float const yaw   = yaw_deg   * 3.14159265358979f / 180.0f;
        float const pitch = pitch_deg * 3.14159265358979f / 180.0f;

        Basis b;
        b.front = glm::normalize(glm::vec3{
            std::cos(yaw) * std::cos(pitch),
            std::sin(yaw) * std::cos(pitch),
            std::sin(pitch)
        });
        // A normalizálás fontos: felfelé/lefelé nézve a keresztszorzat rövidül.
        b.right = glm::normalize(glm::cross(b.front, WORLD_UP));
        b.up    = glm::normalize(glm::cross(b.right, b.front));
        return b;
    }

    // Az egér elmozdulásából szögváltozás. A `dx`, `dy` KÉPERNYŐ-koordinátában van,
    // tehát dy LEFELÉ pozitív.
    //
    // Mindkét előjel negatív, és mindkettőnek jó oka van:
    //
    //  * `pitch`: a képernyő y lefelé nő, tehát a felfelé húzáshoz negatív dy tartozik —
    //    hogy felfelé húzva felfelé nézzünk, kivonni kell.
    //
    //  * `yaw`: a Z-up bázisban `right = (sin yaw, −cos yaw, 0)`, viszont
    //    `d front/d yaw = (−sin yaw, cos yaw, 0) = −right`. Vagyis a yaw NÖVELÉSE
    //    balra (a right-tal ELLENTÉTES irányba) fordítja a nézetet. Hogy jobbra
    //    húzva jobbra nézzünk, itt is kivonni kell. (Y-up bázisnál ez fordítva volt —
    //    a Z-up-ra átálláskor ez a jel maradt tévesen pluszban, ezért volt a
    //    vízszintes forgatás inverz, miközben a függőleges jónak tűnt.)
    inline void apply_mouse_delta(float dx, float dy, float sensitivity,
                                  float& yaw_deg, float& pitch_deg) {
        yaw_deg   -= dx * sensitivity;
        pitch_deg -= dy * sensitivity;

        // A pólusokat kihagyjuk: ott a right vektor kiszámíthatatlan (front ∥ world_up).
        if (pitch_deg >  89.0f) pitch_deg =  89.0f;
        if (pitch_deg < -89.0f) pitch_deg = -89.0f;
    }

}

#endif //GORBE_CAMERABASIS_HPP
