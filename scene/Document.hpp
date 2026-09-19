#ifndef GORBE_SCENE_DOCUMENT_HPP
#define GORBE_SCENE_DOCUMENT_HPP

#include <list>
#include <memory>
#include <vector>

#include "../matek/Kif.hpp"
#include "../particle_sampling/Transform.hpp"
#include "Scope.hpp"

// ---------------------------------------------------------------------------
// Jelenet-modell (az MVC MODEL rétege: nincs benne se GL, se ImGui)
//
// Egy ALAKZAT = név + implicit képlet (F(x,y,z)=0) + saját, LOKÁLIS paraméterek.
// Emellett vannak jelenet- és program-szintű paraméterek (lásd Scope.hpp).
//
// FONTOS az élettartam: a Parameter csomópont a `value` CÍMÉT tárolja
// (float const*), nem az értékét. Ezért kell std::list (a node-ok nem mozdulnak
// beszúráskor/törléskor), és ezért kell minden beparseolt képletet eldobni, ha
// egy paraméter vagy egy alakzat (a lokálisaival együtt) törlődik. A törlést
// ezért csak a vezérlő (app/Controller.hpp) végzi, a drop-pal együtt.
// ---------------------------------------------------------------------------

// Egy tér-warp: három kifejezés, amiket x, y, z helyére helyettesítünk.
// A jelentésük a VISSZAFELÉ (tér -> alakzat) leképezés — lásd Transform.hpp.
struct Warp {
    char name[32] = "";
    char fx[192]  = "x";
    char fy[192]  = "y";
    char fz[192]  = "z";
    bool enabled  = true;
};

struct Shape {
    char name[32]     = "";
    char formula[256] = "";
    // Opcionális tartomány-feltétel: a részecskék csak ott élnek, ahol ez teljesül
    // (pl. "x > 2 and x < 6"). Így lehet egy önmagában végtelen felületet — síkot,
    // hengert — véges darabon megjeleníteni, a CSG-vágás fedőlapjai nélkül.
    char domain[256]  = "";
    bool visible      = true;
    std::list<Param> locals;

    // Tér-transzformáció: az alakzatot nem mozgatjuk, hanem az inverz leképezést
    // helyettesítjük F-be (lásd particle_sampling/Transform.hpp). A mezők CÍME épül
    // be a kifejezésbe, ezért a csúszkák élőben mozgatják az alakzatot.
    TransformParams xform;

    // Warp-lánc. Az ELSŐ elem hat először az alakzatra; a lánc után jön a fenti
    // affin transzformáció, tehát a warpok az alakzat SAJÁT terében dolgoznak,
    // és a kész, deformált alakzatot helyezi el a pozíció/forgatás/méret.
    std::vector<Warp> warps;

    // Szín a paletta-listából (Raytrace::PALETTE), anyag az anyaglistából
    // (Raytrace::MATERIALS). Mindkettő CSAK a fényképre hat: a valós idejű
    // nézetben a részecskék a szokásos árnyalásukat kapják.
    int color_idx = 0;
    int material_idx = 0;

    // Saját méretskála (d): ha `own_d` igaz, az alakzat ezt használja, és a jelenet
    // globális d-je nem hat rá; ha hamis, a globálisat követi. Így egy finomabb
    // részlet sűrűbben mintavételezhető, a többi alakzat maradhat durvább.
    bool  own_d = false;
    float d     = 2.0f;

    // --- az utolsó Indításkor felépített állapot (scene/Build.hpp) ----------
    // A `dom_tree` a globális ÉS a saját (transzformált) feltétel ÉS-kapcsolata —
    // a fénykép ezt használja, hogy pontosan azt lássa, amit a szimuláció.
    std::shared_ptr<Matek::Analizis::Kifejezes const> tree;
    std::shared_ptr<Matek::Analizis::Kifejezes const> dom_tree;
    // Beépült-e a warp az utolsó Indításkor? Egységtranszformációnál nem épül be
    // (hogy az egyszerű alakzatok olcsók maradjanak), ezért az első hozzányúláskor
    // újra kell építeni — különben a csúszka némán nem csinálna semmit.
    bool warped = false;
};

// A jelenet SAJÁT függvénye: név(paraméterek) = törzs, pl. g(u, k = 1) = u^2 + k.
// A képletekben úgy hívható, mint egy beépített függvény. A törzs a saját
// paraméterein kívül x, y, z-t, a jelenet- és program-szintű paramétereket, a `t`-t
// és a listában NÁLA KORÁBBI függvényeket látja (így rekurzió nem lehet).
//
// A képletbe a KIFEJTETT törzs épül be, a UserFunc memóriájára semmi nem mutat —
// ezért törölni is szabadon lehet (legfeljebb a következő Indítás hibát ad).
struct UserFunc {
    char name[32]   = "";
    char params[64] = "u";
    char body[256]  = "";
};

// Egy jelenet (fül) adatai. A futásidejű része (kamera, mintavételezők) az
// app/Scene.hpp-ben van, ami ebből származik.
struct SceneDoc {
    char name[32] = "";

    // JELENET-szintű paraméterek. A hatókör kívülről befelé: program -> jelenet ->
    // alakzat; a belső ELFEDI a külsőt (mint C++-ban).
    std::list<Param> params;

    // A jelenet munkatere: az a térrész, amiben egyáltalán értelmezzük az alakzatokat.
    // Minden alakzatra érvényes, a saját tartomány-feltételével ÉS-kapcsolatban.
    char domain[256] = "";

    std::list<Shape> shapes;

    // A jelenet saját függvényei (lásd UserFunc).
    std::list<UserFunc> funcs;

    float d_ui    = 2.0f;   // globális méretskála (a saját d nélküli alakzatoknak)
    float curv_ui = 1.0f;   // görbület-adaptív taszítás (0 = egyenletes)
};

// Az alakzat ténylegesen használt méretskálája.
inline float effective_d(Shape const& s, SceneDoc const& sc) {
    return s.own_d ? s.d : sc.d_ui;
}

#endif //GORBE_SCENE_DOCUMENT_HPP
