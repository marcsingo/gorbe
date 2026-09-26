#ifndef GORBE_VARIATIONAL_HPP
#define GORBE_VARIATIONAL_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include <glm.hpp>

#include "../matek/Kif.hpp"
#include "Particle.hpp"

// ===========================================================================
// Variációs implicit felület (Turk–O'Brien, "Variational Implicit Surfaces").
//
//     F(x) = Σ_j d_j·|x − c_j|³ + p0 + p1·x + p2·y + p3·z
//
// A c_j kényszerpontokban F az előírt h_j értéket veszi fel: 0 a felületen
// (határkényszer), nem nulla a normálkényszeren. A d_j és p súlyok a cikk (8)
// egyenletrendszeréből jönnek. A projekt konvenciója szerint F < 0 BELÜL, tehát a
// normálkényszer a felületen kívül, a normális irányában ül, pozitív értékkel.
//
// A tree() kifejezésfája a c_j-t és a súlyokat CÍM szerint tartja (Parameter): egy
// kényszer mozgatása után elég a solve(), a fát nem kell újraépíteni és újrafordítani.
// Ezért kényszert felvenni/törölni csak a tree() ELŐTT szabad (a vektorok
// átfoglalása a címeket érvénytelenítené).
// ===========================================================================
struct Variational {
    std::vector<glm::vec3> centers;   // c_j
    std::vector<float>     values;    // h_j
    std::vector<float>     w;         // d_1..d_k, majd p0..p3 (a solve() tölti)

    void add(glm::vec3 c, float h) { centers.push_back(c); values.push_back(h); }

    // Határkényszer a felületi pontra, és normálkényszer `eps`-nyire kifelé, `eps`
    // értékkel — így a felület közelében |∇F| ≈ 1, mint egy távolságfüggvénynél, és a
    // szimuláció küszöbei (PHI, surface_distance) a szokásos tartományban maradnak.
    void add_oriented(glm::vec3 p, glm::vec3 normal, float eps) {
        add(p, 0.0f);
        add(p + eps * normal, eps);
    }

    // A határkényszer normálkényszer-párja, vagy -1. Az add_oriented közvetlenül a
    // határkényszer UTÁN teszi (nem nulla értékkel); a később lerakott határkényszerek
    // pár nélküliek. Húzáskor a pár együtt mozog a ponttal (a normális megmarad).
    int partner(std::size_t i) const {
        return (values[i] == 0.0f && i + 1 < values.size() && values[i + 1] != 0.0f)
               ? static_cast<int>(i + 1) : -1;
    }

    // Egy határkényszer törlése a párjával együtt.
    void remove(std::size_t i) {
        if (int const p = partner(i); p >= 0) {
            centers.erase(centers.begin() + p);
            values.erase(values.begin() + p);
        }
        centers.erase(centers.begin() + static_cast<std::ptrdiff_t>(i));
        values.erase(values.begin() + static_cast<std::ptrdiff_t>(i));
    }

    // A felületi részecskék súlypontja: a szerkesztő ide teszi az origót.
    static glm::vec3 centroid(std::vector<Particle> const& ps) {
        glm::vec3 c{0.0f};
        int n = 0;
        for (auto const& p : ps)
            if (p.state == rajtamozog) { c += p.p; ++n; }
        return n ? c / static_cast<float>(n) : c;
    }

    static double phi(glm::vec3 a, glm::vec3 b) {
        double const r = glm::length(a - b);
        return r * r * r;
    }

    // A (8) egyenletrendszer megoldása. Hamis, ha szinguláris (pl. minden pont egy
    // síkban van, így a lineáris tag nem határozható meg) — ilyenkor w változatlan.
    bool solve() {
        std::size_t const k = centers.size(), n = k + 4;
        if (k == 0) return false;
        std::vector<double> A(n * n, 0.0), b(n, 0.0);
        auto at = [&](std::size_t i, std::size_t j) -> double& { return A[i * n + j]; };
        for (std::size_t i = 0; i < k; ++i) {
            for (std::size_t j = 0; j < k; ++j) at(i, j) = phi(centers[i], centers[j]);
            double const P[4] = {1.0, centers[i].x, centers[i].y, centers[i].z};
            for (std::size_t m = 0; m < 4; ++m) at(i, k + m) = at(k + m, i) = P[m];
            b[i] = values[i];
        }

        // Gauss-elimináció részleges főelem-kereséssel, double-ben: a mátrix nem
        // pozitív definit (nyeregpont-alakú), és a kondíciószáma a pontszámmal nő.
        for (std::size_t col = 0; col < n; ++col) {
            std::size_t piv = col;
            for (std::size_t r = col + 1; r < n; ++r)
                if (std::abs(at(r, col)) > std::abs(at(piv, col))) piv = r;
            if (std::abs(at(piv, col)) < 1e-12) return false;
            if (piv != col) {
                for (std::size_t c = 0; c < n; ++c) std::swap(at(col, c), at(piv, c));
                std::swap(b[col], b[piv]);
            }
            for (std::size_t r = col + 1; r < n; ++r) {
                double const f = at(r, col) / at(col, col);
                if (f == 0.0) continue;
                for (std::size_t c = col; c < n; ++c) at(r, c) -= f * at(col, c);
                b[r] -= f * b[col];
            }
        }
        std::vector<double> x(n);
        for (std::size_t i = n; i-- > 0;) {
            double s = b[i];
            for (std::size_t j = i + 1; j < n; ++j) s -= at(i, j) * x[j];
            x[i] = s / at(i, i);
        }
        w.assign(x.begin(), x.end());   // a címek a tree() óta nem mozdulhatnak: w mérete fix
        return true;
    }

    // Közvetlen kiértékelés (a teszteknek és az összevetéshez).
    float at(glm::vec3 p) const {
        std::size_t const k = centers.size();
        double s = w[k] + w[k + 1] * p.x + w[k + 2] * p.y + w[k + 3] * p.z;
        for (std::size_t j = 0; j < k; ++j) s += w[j] * phi(p, centers[j]);
        return static_cast<float>(s);
    }

    // A függvény kifejezésfaként, a súlyokra és a középpontokra CÍM szerint hivatkozva.
    // A solve() után hívandó (w-nek már a végleges méretén kell lennie).
    //
    // |x−c|³ = r²·sqrt(r² + ε): a tiszta r²·sqrt(r²) szimbolikus deriváltja 0/0 lenne
    // pont a kényszerekben — ahol a részecskék ülnek. Az ε = 1e-12 a függvényt nem
    // változtatja érzékelhetően.
    Matek::Analizis::Kif tree() const {
        using namespace Matek::Analizis;
        std::size_t const k = centers.size();
        Kif f = Kif(&w[k]) + Kif(&w[k + 1]) * x + Kif(&w[k + 2]) * y + Kif(&w[k + 3]) * z;
        for (std::size_t j = 0; j < k; ++j) {
            Kif const dx = x - Kif(&centers[j].x), dy = y - Kif(&centers[j].y),
                      dz = z - Kif(&centers[j].z);
            Kif const r2 = dx * dx + dy * dy + dz * dz;
            f = f + Kif(&w[j]) * (r2 * sqrt(r2 + Kif(1e-12f)));
        }
        return f;
    }

    // Egy (képletes) alakzat átalakítása a részecskéiből (a cikk 4.3 fejezete, poligonháló
    // helyett részecskékkel): a felületi részecskék helye a határkényszer, a gradiensük a
    // normális. `center` az új origó (a szerkesztő jelenet az alakzat közepére áll).
    //
    // ponytail: legfeljebb `max_points` részecske, egyenletes lépésközzel (a részecskék
    // cellák szerint rendezettek, így térben is szétszórtak). A megoldás O(k³), a
    // kiértékelés részecskénként O(k) — néhány száz kényszer felett ritkítás/gyors
    // kiértékelés (Beatson) kellene.
    static Variational from_particles(std::vector<Particle> const& ps, glm::vec3 center,
                                      float eps, std::size_t max_points = 150) {
        std::vector<Particle const*> on;
        for (auto const& p : ps)
            if (p.state == rajtamozog && glm::length(p.F_x) > 1e-6f) on.push_back(&p);
        Variational v;
        std::size_t const step = std::max<std::size_t>(1, (on.size() + max_points - 1) / max_points);
        for (std::size_t i = 0; i < on.size(); i += step)
            v.add_oriented(on[i]->p - center, glm::normalize(on[i]->F_x), eps);
        return v;
    }
};

#endif //GORBE_VARIATIONAL_HPP
