#ifndef GORBE_TRANSFORM_HPP
#define GORBE_TRANSFORM_HPP

#include "../matek/Kif.hpp"

// Alakzatonkénti tér-transzformáció (eltolás / forgatás / skálázás).
//
// Az implicit alakzatot nem "mozgatjuk": a TERET transzformáljuk. Ha a lokális
// alakzat F_lok(p) = 0, és a lokálisból a világba a
//
//     p_világ = T + R · (S · p_lok)
//
// leképezés visz, akkor a világbeli alakzat egyenlete
//
//     F_világ(p) = F_lok( w(p) ),      w(p) = S⁻¹ · Rᵀ · (p − T)
//
// vagyis az INVERZ leképezést kell F-be behelyettesíteni (BlobTree-cikk, 3.4).
// A behelyettesítést a Kifejezes::substitute végzi, a deriváltakat pedig NEM kell
// külön kezelni: a szimbolikus deriválás a láncszabályt magától elvégzi. A cikknek
// ehhez explicit Jacobi-mátrixot kell számolnia, itt ez ingyen van.
//
// A paraméterek CÍM szerint épülnek be a kifejezésbe (Parameter csomópont), ezért a
// csúszkák élőben mozgatják az alakzatot — nem kell újra parseolni és deriválni.
namespace Matek::Analizis { }

struct TransformParams {
    float pos[3]   {0.0f, 0.0f, 0.0f};
    float rot[3]   {0.0f, 0.0f, 0.0f};   // RADIÁN, x/y/z tengely körül
    float scale[3] {1.0f, 1.0f, 1.0f};

    bool is_identity() const {
        for (int i = 0; i < 3; ++i)
            if (pos[i] != 0.0f || rot[i] != 0.0f || scale[i] != 1.0f) return false;
        return true;
    }

    void reset() {
        for (int i = 0; i < 3; ++i) { pos[i] = 0.0f; rot[i] = 0.0f; scale[i] = 1.0f; }
    }

    bool operator==(TransformParams const&) const = default;
};

// Ugyanez a leképezés számokkal, egy pontra (a kifejezés helyett): lokális -> világ,
// p_világ = T + Rz·Ry·Rx·(S·p). A részecskék ezzel mennek az alakzattal, ha a kód
// egyszerre nagyot mozdít rajta (script/Runtime.hpp).
inline glm::vec3 to_world(TransformParams const& t, glm::vec3 p) {
    p *= glm::vec3(t.scale[0], t.scale[1], t.scale[2]);
    float c = std::cos(t.rot[0]), s = std::sin(t.rot[0]);
    p = {p.x, p.y * c - p.z * s, p.y * s + p.z * c};                 // Rx
    c = std::cos(t.rot[1]); s = std::sin(t.rot[1]);
    p = {p.x * c + p.z * s, p.y, -p.x * s + p.z * c};                // Ry
    c = std::cos(t.rot[2]); s = std::sin(t.rot[2]);
    p = {p.x * c - p.y * s, p.x * s + p.y * c, p.z};                 // Rz
    return p + glm::vec3(t.pos[0], t.pos[1], t.pos[2]);
}

// ... és visszafelé: világ -> lokális, S⁻¹·Rᵀ·(p − T) — ugyanaz, mint a lenti
// make_inverse_warp, lépésről lépésre.
inline glm::vec3 to_local(TransformParams const& t, glm::vec3 p) {
    p -= glm::vec3(t.pos[0], t.pos[1], t.pos[2]);
    float c = std::cos(t.rot[2]), s = std::sin(t.rot[2]);
    p = {p.x * c + p.y * s, -p.x * s + p.y * c, p.z};                // Rz(−rz)
    c = std::cos(t.rot[1]); s = std::sin(t.rot[1]);
    p = {p.x * c - p.z * s, p.y, p.x * s + p.z * c};                 // Ry(−ry)
    c = std::cos(t.rot[0]); s = std::sin(t.rot[0]);
    p = {p.x, p.y * c + p.z * s, -p.y * s + p.z * c};                // Rx(−rx)
    return p / glm::vec3(t.scale[0], t.scale[1], t.scale[2]);
}

// A világ -> lokális leképezés három koordináta-kifejezése.
struct InverseWarp {
    Matek::Analizis::Kif lx, ly, lz;

    Matek::Analizis::SubstMap as_map() const {
        return {{'x', lx.get()}, {'y', ly.get()}, {'z', lz.get()}};
    }
};

// Felépíti a világ -> lokális leképezést a MEGADOTT PÉLDÁNY címeire hivatkozva.
// A visszaadott kifejezés addig érvényes, amíg a `t` él (Parameter = float const*).
//
// A forgatás sorrendje: a lokálisból a világba R = Rz·Ry·Rx (előbb x, aztán y, aztán z),
// tehát visszafelé Rᵀ = Rx(−rx)·Ry(−ry)·Rz(−rz) — ebben a sorrendben alkalmazzuk.
inline InverseWarp make_inverse_warp(TransformParams const& t) {
    using namespace Matek::Analizis;

    // 1. eltolás visszavonása
    Kif dx = x - Kif(&t.pos[0]);
    Kif dy = y - Kif(&t.pos[1]);
    Kif dz = z - Kif(&t.pos[2]);

    Kif rx = Kif(&t.rot[0]), ry = Kif(&t.rot[1]), rz = Kif(&t.rot[2]);

    // 2. Rz(−rz):  ( x·cos + y·sin ,  −x·sin + y·cos ,  z )
    Kif c = cos(rz), s = sin(rz);
    Kif ax = dx * c + dy * s;
    Kif ay = Kif(0.0f) - dx * s + dy * c;
    Kif az = dz;

    // 3. Ry(−ry):  ( x·cos − z·sin ,  y ,  x·sin + z·cos )
    c = cos(ry); s = sin(ry);
    Kif bx = ax * c - az * s;
    Kif by = ay;
    Kif bz = ax * s + az * c;

    // 4. Rx(−rx):  ( x ,  y·cos + z·sin ,  −y·sin + z·cos )
    c = cos(rx); s = sin(rx);
    Kif ex = bx;
    Kif ey = by * c + bz * s;
    Kif ez = Kif(0.0f) - by * s + bz * c;

    // 5. skálázás visszavonása
    return InverseWarp{ ex / Kif(&t.scale[0]),
                        ey / Kif(&t.scale[1]),
                        ez / Kif(&t.scale[2]) };
}

// F (vagy egy tartomány-feltétel) transzformált alakja.
// `force`: egységtranszformációnál is beépül (a kontrollpontok megoldójának kell,
// hogy a pozíció paraméterként benne legyen a képletben).
inline Matek::Analizis::Kif apply_transform(Matek::Analizis::Kif const& f,
                                            TransformParams const& t, bool force = false) {
    if (t.is_identity() && !force) return f;
    return Matek::Analizis::substitute(f, make_inverse_warp(t).as_map());
}

// --- Általános warp: három tetszőleges kifejezés -----------------------------
//
// Egy warp három kifejezés (`wx`, `wy`, `wz`), amiket `x`, `y`, `z` helyére
// helyettesítünk. Semmi több: F_warpolt(p) = F( wx(p), wy(p), wz(p) ).
//
// FONTOS a jelentés: ezek a kifejezések a VISSZAFELÉ (world -> shape) leképezést
// írják le — "hol keressük ki az alakzatot ehhez a térbeli ponthoz". Ez nem
// szőrszálhasogatás: a tér warpolásához mindig az inverz leképezés kell (Barr
// 1984; BlobTree-cikk 3.4: "we wish to warp space, thus we use the inverse warp
// function"). A beépített sablonok már így vannak felírva; egyedi warpnál erre
// figyelni kell. Egy csavarás `+a` szöggel tehát a `-a`-val forgató kifejezés.
//
// Láncolásnál a lista ELSŐ eleme hat először az alakzatra (a behelyettesítések
// egymásba ágyazódnak, ami a visszafelé-leképezéseknél épp ezt a sorrendet adja).
inline Matek::Analizis::Kif apply_warp(Matek::Analizis::Kif const& f,
                                       Matek::Analizis::Kif const& wx,
                                       Matek::Analizis::Kif const& wy,
                                       Matek::Analizis::Kif const& wz) {
    return Matek::Analizis::substitute(f, {{'x', wx.get()},
                                           {'y', wy.get()},
                                           {'z', wz.get()}});
}

#endif //GORBE_TRANSFORM_HPP
