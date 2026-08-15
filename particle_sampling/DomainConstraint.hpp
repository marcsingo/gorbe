#ifndef GORBE_DOMAINCONSTRAINT_HPP
#define GORBE_DOMAINCONSTRAINT_HPP

#include <algorithm>
#include <cmath>
#include <glm.hpp>

// A tartomány-feltétel (dom > 0) matematikája, GL és részecske-típus nélkül, hogy
// önállóan tesztelhető legyen. Az ImplicitSurface ezeket hívja.
//
// Alapötlet: a részecskét EGYSZERRE két kényszer tartja.
//   1. F(p) = 0        — rajta a felületen (ezt a Witkin-lépés intézi)
//   2. dom(p) >= 0     — a megfelelő térrészben
//
// A második kényszert úgy érvényesítjük, hogy közben az elsőt nem sértjük meg: a
// feltétel gradiensének csak a FELÜLET MENTI (érintőirányú) részét használjuk.
namespace Domain {

    // ∇dom felület menti része:  g = ∇dom − (∇dom·∇F / |∇F|²)·∇F
    //
    // Erre igaz, hogy g ⊥ ∇F, tehát g irányba mozogva a részecske elsőrendben NEM
    // hagyja el a felületet. Ugyanakkor ez a felületen belüli leggyorsabb növekedés
    // iránya a feltételre nézve, mert ∇dom·g = |g|².
    inline glm::vec3 tangential_gradient(glm::vec3 dom_x, glm::vec3 F_x) {
        float fx2 = glm::dot(F_x, F_x);
        if (fx2 < 1e-12f) return dom_x;
        return dom_x - (glm::dot(dom_x, F_x) / fx2) * F_x;
    }

    // Előjeles GEOMETRIAI távolság a peremtől, a felület mentén (Taubin-közelítés,
    // ugyanaz az elv, mint az ImplicitSurface::surface_distance-nél). Erre azért van
    // szükség, mert a nyers dom-érték skálafüggő: "x > 2" és "100*x > 200" ugyanazt a
    // peremet jelenti, de százszoros értékkel. Így viszont a küszöbök hosszban
    // értelmezhetők, és minden feltételnél ugyanazt jelentik.
    //
    // |g| ~ 0 esetén a feltétel a felület mentén nem változik (a gradiense párhuzamos
    // a felület normálisával) — ilyenkor ±végtelent adunk vissza az előjel szerint.
    inline float distance(float dom, glm::vec3 dom_g) {
        float gl = std::sqrt(glm::dot(dom_g, dom_g));
        if (gl < 1e-6f) return dom >= 0.0f ? 1e30f : -1e30f;
        return dom / gl;
    }

    // A sebesség korrekciója. Két eset:
    //
    //  * dom_dist < 0 — a részecske a rossz térrészben van: +g irányú sebességet adunk
    //    hozzá, ami a FELÜLET MENTÉN csúsztatja a jó térrész felé. A sebességet
    //    korlátozzuk (max_slide · sigma lépésenként), különben a peremtől távoli
    //    részecskét az arányos visszahúzás egyetlen lépésben átlőné a tartományon.
    //
    //  * dom_dist >= 0 (bent van): PREDIKTÍV fal. Megnézzük, hova vinné a lépés, és
    //    CSAK akkor avatkozunk be, ha átlépné a peremet — akkor is csak annyit veszünk
    //    ki a kifelé mutató komponensből, hogy pontosan a peremen álljon meg. Fontos,
    //    hogy ne egy fix sávban fékezzünk mindenkit: a tartomány belsejében a
    //    részecskének szabadon kell mozognia, különben a taszítás nem tudja szétteríteni.
    //
    //    A számolás: d(dom)/dt = ∇dom·ṗ, és mivel ∇dom·g = |g|², a g-vel arányos
    //    korrekció pontosan a kívánt értékre állítja — a felület érintősíkjának
    //    elhagyása nélkül, mert g ⊥ ∇F.
    //
    // A visszahúzás a peremre (dom_dist = 0) konvergál, nem beljebb: egy megvágott
    // felületdarabon a részecskéknek EL KELL érniük a szélét. Ezért az "odakint van-e"
    // döntésekhez (rajzolás, fisszió) geometriai tűrés kell — lásd is_outside().
    inline glm::vec3 constrain(glm::vec3 p_dot,
                               glm::vec3 dom_x, glm::vec3 dom_g, float dom_dist,
                               float sigma, float dt,
                               float pull, float max_slide) {
        float g2 = glm::dot(dom_g, dom_g);
        if (g2 < 1e-12f) return p_dot;   // a felület mentén nem javítható
        float gl = std::sqrt(g2);

        if (dom_dist < 0.0f) {
            float max_speed = max_slide * sigma / std::max(dt, 1e-4f);
            float speed     = std::min(pull * (-dom_dist), max_speed);
            glm::vec3 v = p_dot + speed * (dom_g / gl);

            // A visszahúzás önmagában csak ARÁNYOS szabályzó, ezért egy tartós kifelé
            // ható erővel (taszítás, vonszolás) egyensúlyba kerülne, és a részecske a
            // peremen KÍVÜL állna meg. Ezért kívül is kivesszük a maradék kifelé mutató
            // komponenst: így a perem mindkét oldalról kemény fal, a részecske legfeljebb
            // rááll, de nem tolható tovább kifelé.
            float out = glm::dot(dom_x, v);
            if (out < 0.0f) v -= (out / g2) * dom_g;
            return v;
        }

        float d_dot     = glm::dot(dom_x, p_dot);   // d(dom)/dt
        float dist_rate = d_dot / gl;               // d(dom_dist)/dt
        if (dom_dist + dist_rate * dt >= 0.0f) return p_dot;   // nem lépné át: hagyjuk

        // Pont a peremen álljon meg: a cél d(dom_dist)/dt = −dom_dist/dt.
        float target_d_dot = (-dom_dist / std::max(dt, 1e-4f)) * gl;
        return p_dot - ((d_dot - target_d_dot) / g2) * dom_g;
    }

    // "Érdemben" a tartományon kívül van-e. A tűrés miatt a peremen ülő részecske nem
    // villog be-ki (a rajzolásban és a fisszió-tiltásban).
    inline bool is_outside(float dom_dist, float sigma) {
        return dom_dist < -0.25f * sigma;
    }

}

#endif //GORBE_DOMAINCONSTRAINT_HPP
