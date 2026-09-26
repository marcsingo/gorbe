#ifndef GORBE_VARIATIONAL_HPP
#define GORBE_VARIATIONAL_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <stdexcept>
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
// A kifejezésfában EGY natív csomópont (RbfNode, lent): F-et, a gradienst és a
// Hesse-mátrixot zárt alakban, C++-ban számolja, nem szimbolikus deriválással. Így a
// fa kicsi marad (szimbolikusan a 162 kényszeres gömb programja 6334 utasítás volt,
// eltolva 11293), és egy transzformáció a láncszabállyal EGYSZER hat, nem tagonként.
//
// A csomópont magát a Variational objektumot olvassa: egy kényszer mozgatása, felvétele
// vagy törlése után elég a solve(), a fákat nem kell újraépíteni.
// ===========================================================================
struct Variational {
    std::vector<glm::vec3> centers;   // c_j
    std::vector<float>     values;    // h_j
    std::vector<float>     w;         // d_1..d_k, majd p0..p3 (a solve() tölti)

    // A solve() minden sikeres futása növeli: a szálankénti gyorsítótár (eval) ebből
    // tudja, hogy a függvény megváltozott.
    unsigned version = 0;

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
        w.assign(x.begin(), x.end());
        ++version;
        return true;
    }

    // Egy pontban MINDEN egyszerre, egyetlen ciklusban (a gyök tagonként egyszer):
    //   out[0] = F, out[1..3] = ∇F, out[4..9] = Hesse: xx xy xz yy yz zz
    //   |x−c|³ gradiense 3r·d, Hesse-mátrixa 3(r·I + d·dᵀ/r) — r → 0-ban 0 (folytonos).
    void eval_all(glm::vec3 p, float out[10]) const {
        std::size_t const k = centers.size();
        if (w.size() != k + 4) { std::fill(out, out + 10, 0.0f); return; }
        double F = w[k] + w[k + 1] * p.x + w[k + 2] * p.y + w[k + 3] * p.z;
        double g[3] = {w[k + 1], w[k + 2], w[k + 3]};
        double H[6] = {0, 0, 0, 0, 0, 0};
        for (std::size_t j = 0; j < k; ++j) {
            double const dx = p.x - centers[j].x, dy = p.y - centers[j].y, dz = p.z - centers[j].z;
            double const r = std::sqrt(dx * dx + dy * dy + dz * dz);
            double const wj = w[j], w3r = 3.0 * wj * r;
            F += wj * r * r * r;
            g[0] += w3r * dx; g[1] += w3r * dy; g[2] += w3r * dz;
            if (r > 0.0) {
                double const q = 3.0 * wj / r;
                H[0] += w3r + q * dx * dx; H[1] += q * dx * dy; H[2] += q * dx * dz;
                H[3] += w3r + q * dy * dy; H[4] += q * dy * dz; H[5] += w3r + q * dz * dz;
            }
        }
        out[0] = static_cast<float>(F);
        for (int i = 0; i < 3; ++i) out[1 + i] = static_cast<float>(g[i]);
        for (int i = 0; i < 6; ++i) out[4 + i] = static_cast<float>(H[i]);
    }

    // Egy kimenet (lásd eval_all). A program F-et, a gradienst és a Hesse-mátrixot külön
    // utasításokként kéri ugyanabban a pontban: a szálankénti gyorsítótár miatt a
    // ciklus pontonként csak EGYSZER fut le.
    float eval(int kind, glm::vec3 p) const {
        struct Cache { Variational const* v = nullptr; unsigned ver = 0; glm::vec3 p{0.0f}; float out[10]{}; };
        thread_local Cache c;
        if (c.v != this || c.ver != version || c.p != p) {
            eval_all(p, c.out);
            c.v = this; c.ver = version; c.p = p;
        }
        return c.out[kind];
    }

    // Közvetlen kiértékelés (a teszteknek és az összevetéshez).
    float at(glm::vec3 p) const {
        float out[10];
        eval_all(p, out);
        return out[0];
    }

    // A függvény kifejezésfaként: egyetlen RbfNode az x, y, z változókon. A `v`-t a fa
    // életben tartja.
    static Matek::Analizis::Kif tree(std::shared_ptr<Variational const> v);

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

// A variációs függvény (vagy egy deriváltja) csomópontként: v[kind](ax, ay, az). Az
// argumentumok a pont koordinátái — egységtranszformációnál x, y, z; elhelyezett
// alakzatnál a világ->lokális leképezés kifejezései. A deriválás a láncszabály:
//     d/dv F(a) = Σ_m F_m(a) · ∂a_m/∂v
// így a transzformáció EGYSZER szorzódik be, nem a ~300 tag mindegyikébe.
struct RbfNode : Matek::Analizis::Kifejezes {
    using Tree = Matek::Analizis::Tree;
    std::shared_ptr<Variational const> v;
    int kind;            // lásd Variational::eval_all
    Tree a[3];

    RbfNode(std::shared_ptr<Variational const> v, int kind, Tree ax, Tree ay, Tree az)
        : v(std::move(v)), kind(kind), a{std::move(ax), std::move(ay), std::move(az)} {}

    float at(glm::vec3 const p) const override {
        return v->eval(kind, {a[0]->at(p), a[1]->at(p), a[2]->at(p)});
    }

    // A kind-hoz tartozó derivált kind az m-edik koordináta szerint.
    static int deriv_kind(int kind, int m) {
        static int const SECOND[3][3] = {{4, 5, 6}, {5, 7, 8}, {6, 8, 9}};
        if (kind == 0) return 1 + m;
        if (kind <= 3) return SECOND[kind - 1][m];
        // Harmadik derivált sehol nem kell (a görbülethez a Hesse elég).
        throw std::logic_error("variacios felulet: harmadrendu derivalt nincs");
    }

    Tree derive(Matek::Analizis::Var const& var) const override {
        using Matek::Analizis::Kif;
        Kif sum(0.0f);
        for (int m = 0; m < 3; ++m)
            sum = sum + Kif(Tree(std::make_shared<RbfNode>(v, deriv_kind(kind, m), a[0], a[1], a[2]))) *
                        Kif(a[m]->derive(var));
        return sum.get();
    }

    Tree simplify() const override {
        using namespace Matek::Analizis;
        Tree s[3] = {a[0]->simplify(), a[1]->simplify(), a[2]->simplify()};
        auto c0 = const_of(s[0]), c1 = const_of(s[1]), c2 = const_of(s[2]);
        if (c0 && c1 && c2)
            return std::make_shared<Konstans>(v->eval(kind, {c0->get_value(), c1->get_value(), c2->get_value()}));
        return std::make_shared<RbfNode>(v, kind, s[0], s[1], s[2]);
    }

    bool same(Matek::Analizis::Kifejezes const& o) const override {
        auto r = dynamic_cast<RbfNode const*>(&o);
        return r && r->v == v && r->kind == kind &&
               a[0]->same(*r->a[0]) && a[1]->same(*r->a[1]) && a[2]->same(*r->a[2]);
    }

    // F maga a parser nyelvén, kifejtve (visszaolvasható, de a kényszerek pillanatnyi
    // súlyaival — onnantól nem követi a szerkesztést); a deriváltak csak jelölve.
    void print(std::ostream& os, Matek::Analizis::ParamNamer const& namer) const override {
        using Matek::Analizis::Konstans;
        if (kind != 0) {
            os << "rbf_d" << kind << '(';
            for (int m = 0; m < 3; ++m) { if (m) os << ", "; a[m]->print(os, namer); }
            os << ')';
            return;
        }
        std::size_t const k = v->centers.size();
        auto num = [&](float f) { Konstans(f).print(os, namer); };
        num(v->w[k]);
        for (int m = 0; m < 3; ++m) { os << " + "; num(v->w[k + 1 + m]); os << '*'; a[m]->print(os, namer); }
        for (std::size_t j = 0; j < k; ++j) {
            os << " + "; num(v->w[j]); os << "*length(";
            for (int m = 0; m < 3; ++m) {
                if (m) os << ", ";
                a[m]->print(os, namer); os << " - "; num(v->centers[j][m]);
            }
            os << ")^3";
        }
    }

    Tree substitute(Matek::Analizis::SubstMap const& m) const override {
        return std::make_shared<RbfNode>(v, kind, a[0]->substitute(m), a[1]->substitute(m), a[2]->substitute(m));
    }

    static float native(void const* obj, int kind, float x, float y, float z) {
        return static_cast<Variational const*>(obj)->eval(kind, {x, y, z});
    }

    int compile(Matek::Analizis::Program& prog) const override {
        int const s0 = a[0]->compile(prog), s1 = a[1]->compile(prog), s2 = a[2]->compile(prog);
        return prog.emit_native({&native, v.get(), kind, s2, v}, s0, s1);
    }
};

inline Matek::Analizis::Kif Variational::tree(std::shared_ptr<Variational const> v) {
    using namespace Matek::Analizis;
    return Kif(Tree(std::make_shared<RbfNode>(std::move(v), 0, x.get(), y.get(), z.get())));
}

#endif //GORBE_VARIATIONAL_HPP
