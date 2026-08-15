#pragma once
#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <memory>

#include "../matek/Kif.hpp"
#include "../matek/Analizis.hpp"
#include "Particle.hpp"

// Célzott using-deklarációk a régi globális `using namespace Matek::Analizis;` helyett:
// az utóbbi minden fordítási egységbe beszórta az `x`, `y`, `z` neveket is, ami ahogy
// nő a projekt, garantáltan ütközik. Az OPERÁTOROKAT (+ - * / ^) és a DSL-függvényeket
// (sin, cos, ...) nem kell felsorolni: azokat az argumentum típusa alapján az ADL
// megtalálja. A literál-operátort (2.0_k) viszont igen, arra nincs ADL.
using Matek::Analizis::Kif;
using Matek::Analizis::Kifejezes;
using Matek::Analizis::NameResolver;
using Matek::Analizis::Program;
using Matek::Analizis::make_kif;
using Matek::Analizis::kif_and;
using Matek::Analizis::x;
using Matek::Analizis::y;
using Matek::Analizis::z;
using Matek::Analizis::operator""_k;

template<size_t L>
struct Surface {
private:
    Surface(Surface const & s) = default;

public:
    // A felület paramétereinek (q) száma. Innen tudja az ImplicitSurface és az App
    // automatikusan levezetni az L sablonparamétert, így a main-ben elég a felület
    // típusát megadni (pl. Sphere, Torus), a számot nem kell ismerni.
    static constexpr size_t param_count = L;

    std::map<float const *, Kif> F_dp_s;
    Kif F_dx, F_dy, F_dz;
    // Második deriváltak (Hesse-mátrix) a görbület-számításhoz. Szimmetrikus, ezért
    // elég a felső háromszög: dxy = dyx, stb.
    Kif F_dxx, F_dxy, F_dxz, F_dyy, F_dyz, F_dzz;
    Kif F;

    glm::vec<L, float> q;
    glm::vec<L, float> q_dot;

    // Ide írjuk folyamatosan az alakzat aktuális átmérőjét, ha valaki (pl. az
    // ImplicitSurface) megkérte rá a cím átadásával. Így a hívó d-je mindig a
    // felület valódi átmérőjét tükrözi, akkor is, ha a q paraméterek mozognak.
    float* d_ptr = nullptr;

    std::function<void(float, float)> q_dot_function = [](float t, float dt){};

    // A `this`-t kapó eseménykezelő élettartama a példányhoz kötve: a Subscription
    // a destruktorban magától leiratkozik, így nem marad az ablakban megszűnt
    // objektumra mutató lambda.
    Window::Subscription tick_sub;

    Surface() {
        q_dot = glm::vec<L, float>(0);
        tick_sub = Window::Subscription(Window::add_time_passed_event([this](auto p) {
            this->q_dot_function(p.t, p.dt);
            if (this->d_ptr) *this->d_ptr = this->diameter();
        }));
    }

    virtual ~Surface() = default;

    // Az eseménykezelő `this`-re mutat, ezért a példány nem másolható/mozgatható.
    Surface(Surface&&) = delete;
    Surface& operator=(Surface const&) = delete;
    Surface& operator=(Surface&&) = delete;

    // Az alakzat aktuális átmérője a q paraméterekből. Felületenként más a képlet,
    // ezért virtuális; a folyamatos kiszámítást és kiírást viszont az ős intézi.
    virtual float diameter() const = 0;

    // A megadott cím alá folyamatosan az átmérőt írjuk; rögtön be is állítjuk,
    // hogy az első frame előtt is helyes legyen az érték.
    void bind_diameter(float* target) {
        d_ptr = target;
        if (d_ptr) *d_ptr = diameter();
    }

    glm::vec<L, float> get_F_q(glm::vec3 at) const {
        glm::vec<L, float> res{0};
        for (int i = 0; i < L; i++) {
            res[i] = F_dp_s.at(&q[i]).at(at);
        }
        return res;
    }

    glm::vec3 grad(glm::vec3 at) const {
        return glm::vec3{
            F_dx.at(at),
            F_dy.at(at),
            F_dz.at(at)
        };
    }

    // --- Tartomány-feltétel (opcionális) --------------------------------------
    // A részecske a "megfelelő térrészben" van, ahol Dom(x,y,z) > 0. Ez SZÁNDÉKOSAN
    // nem épül bele F-be:
    //   * F-be építve (metszet) egy zárt TEST határát kapnánk, tehát a vágólapok is
    //     megjelennének felületként — egy sík darabja helyett egy éket;
    //   * ráadásul F minden deriváltja felrobbanna (mérve: sík téglalapra vágva 4
    //     egymásba ágyazott metszettel 38x drágább, mint külön feltételként).
    // Így F érintetlen marad, és a feltételhez elég az ELSŐ derivált — az kell ahhoz,
    // hogy a részecskét a felület mentén tudjuk a jó térrész felé csúsztatni.
    Kif Dom;
    Kif Dom_dx, Dom_dy, Dom_dz;
    bool has_domain = false;

    void set_domain(std::shared_ptr<Kifejezes const> tree) {
        Dom = Kif(std::move(tree));
        Dom_dx = Dom.derrive('x');
        Dom_dy = Dom.derrive('y');
        Dom_dz = Dom.derrive('z');

        Kif const* all[4] = {&Dom, &Dom_dx, &Dom_dy, &Dom_dz};
        prog_dom = Program{};
        for (int i = 0; i < 4; ++i) out_dom[i] = all[i]->get()->compile(prog_dom);
        prog_dom.finish();

        has_domain = true;
    }

    void clear_domain() { has_domain = false; }

    glm::vec3 dom_grad(glm::vec3 at) const {
        return glm::vec3{
            Dom_dx.at(at),
            Dom_dy.at(at),
            Dom_dz.at(at)
        };
    }

    // Az implicit felület (F=0) KÖZEPES (mean) görbülete a pontban, a gradiensből és a
    // Hesse-mátrixból:
    //     H = ( ∇Fᵀ·Hess·∇F − |∇F|²·tr(Hess) ) / ( 2·|∇F|³ )
    // (Ez a div(∇F/|∇F|) képlete.) Előjeles: a normális irányától függ; az f<0 belül
    // konvencióval egy R sugarú gömbre H = −1/R, vagyis |H| = 1/R, a görbületi sugár 1/|H|.
    // Sík esetén (Hess=0) H=0. |∇F|→0 (kritikus pont) közelében 0-t adunk vissza.
    // A közepes görbület a gradiensből és a Hesse-elemekből (a fenti képlet).
    static float mean_curvature(glm::vec3 g,
                                float fxx, float fyy, float fzz,
                                float fxy, float fxz, float fyz) {
        float gn2 = glm::dot(g, g);
        if (gn2 < 1e-12f) return 0.0f;
        float ghg = g.x*g.x*fxx + g.y*g.y*fyy + g.z*g.z*fzz
                  + 2.0f*(g.x*g.y*fxy + g.x*g.z*fxz + g.y*g.z*fyz);
        float trace = fxx + fyy + fzz;
        return (ghg - gn2*trace) / (2.0f * gn2 * std::sqrt(gn2)); // 2·|∇F|³
    }

    float curvature(glm::vec3 at) const {
        return mean_curvature(grad(at),
                              F_dxx.at(at), F_dyy.at(at), F_dzz.at(at),
                              F_dxy.at(at), F_dxz.at(at), F_dyz.at(at));
    }

    // --- Lefordított, lapos programok (a szimuláció forró útja) ------------------
    // A fabejárás helyett a részecskénkénti kiértékelés ezeken megy: a szükséges
    // mennyiségek EGYETLEN lineáris menetben állnak elő, a közös részkifejezések
    // pedig csak egyszer futnak le (lásd matek/Program.hpp).
    //   prog_grad : F, ∂F/∂x, ∂F/∂y, ∂F/∂z
    //   prog_full : ugyanaz + a Hesse 6 eleme (csak ha kell a görbület)
    //   prog_dom  : a tartomány-feltétel és a gradiense
    Program prog_grad, prog_full, prog_dom;
    int out_grad[4]{};
    int out_full[10]{};
    int out_dom[4]{};

    // F és a gradiens egy menetben.
    void eval_grad(glm::vec3 at, float& F_out, glm::vec3& grad_out) const {
        prog_grad.run(at);
        F_out    = prog_grad.slot(out_grad[0]);
        grad_out = {prog_grad.slot(out_grad[1]),
                    prog_grad.slot(out_grad[2]),
                    prog_grad.slot(out_grad[3])};
    }

    // F, a gradiens és a közepes görbület egy menetben.
    void eval_full(glm::vec3 at, float& F_out, glm::vec3& grad_out, float& K_out) const {
        prog_full.run(at);
        F_out    = prog_full.slot(out_full[0]);
        grad_out = {prog_full.slot(out_full[1]),
                    prog_full.slot(out_full[2]),
                    prog_full.slot(out_full[3])};
        K_out = mean_curvature(grad_out,
                               prog_full.slot(out_full[4]),   // fxx
                               prog_full.slot(out_full[7]),   // fyy
                               prog_full.slot(out_full[9]),   // fzz
                               prog_full.slot(out_full[5]),   // fxy
                               prog_full.slot(out_full[6]),   // fxz
                               prog_full.slot(out_full[8]));  // fyz
    }

    // A tartomány-feltétel és a gradiense egy menetben.
    void eval_domain(glm::vec3 at, float& dom_out, glm::vec3& grad_out) const {
        prog_dom.run(at);
        dom_out  = prog_dom.slot(out_dom[0]);
        grad_out = {prog_dom.slot(out_dom[1]),
                    prog_dom.slot(out_dom[2]),
                    prog_dom.slot(out_dom[3])};
    }

    void calculate() {
        for (int i = 0; i < L; i++) {
            F_dp_s[&q[i]] = F.derrive(&q[i]);
        }
        F_dx = F.derrive('x');
        F_dy = F.derrive('y');
        F_dz = F.derrive('z');

        // Hesse-mátrix (szimmetrikus): a már kész elsőrendű deriváltakat deriváljuk tovább.
        F_dxx = F_dx.derrive('x');
        F_dxy = F_dx.derrive('y');
        F_dxz = F_dx.derrive('z');
        F_dyy = F_dy.derrive('y');
        F_dyz = F_dy.derrive('z');
        F_dzz = F_dz.derrive('z');

        compile_programs();
    }

    void compile_programs() {
        Kif const* all[10] = {&F, &F_dx, &F_dy, &F_dz,
                              &F_dxx, &F_dxy, &F_dxz, &F_dyy, &F_dyz, &F_dzz};

        prog_grad = Program{};
        for (int i = 0; i < 4; ++i) out_grad[i] = all[i]->get()->compile(prog_grad);
        prog_grad.finish();

        prog_full = Program{};
        for (int i = 0; i < 10; ++i) out_full[i] = all[i]->get()->compile(prog_full);
        prog_full.finish();
    }
};



struct Circle : Surface<3> {
    Circle() {
        q = {0, 0, 1};
        F = ((x- &q.x) ^ 2.0f)  + ((y - &q.y) ^2.0f) - ((&q.z) ^ 2.0_k);
        q_dot_function = [this](float t, float dt) {
            // q = {
            //     std::cos(t)*5.0f,
            //     std::sin(t)*5.0f,
            //     3.0f + std::sin(t)
            // };
            // q_dot = {
            //     -std::sin(t)*5.0f,
            //     std::cos(t)*5.0f,
            //     std::cos(t)
            // };
        };
        calculate();
    }
    // q = {cx, cy, r}  ->  átmérő = 2r
    float diameter() const override { return 2.0f * q.z; }
};

// q = {cx, cy, cz, r}
struct Sphere : Surface<4> {
    Sphere() {
        q = {0, 0, 0, 1};
        F = ((x - &q.x)^2.0f) + ((y - &q.y)^2.0f) + ((z - &q.z)^2.0f) - ((&q.w)^2.0_k);
        q_dot_function = [this](float t, float dt) {
            // q = {
            //     std::cos(t)*5.0f,
            //     std::sin(t)*5.0f,
            //     3.0f + std::sin(t)
            // };
            // q_dot = {
            //     -std::sin(t)*5.0f,
            //     std::cos(t)*5.0f,
            //     std::cos(t)
            // };
        };
        calculate();
    }
    // q = {cx, cy, cz, r}  ->  átmérő = 2r
    float diameter() const override { return 2.0f * q.w; }
};

// q = {cx, cy, r}  —  henger a z-tengely mentén
struct Cylinder : Surface<3> {
    Cylinder() {
        q = {0, 0, 2};
        F = ((x - &q.x)^2.0f) + ((y - &q.y)^2.0f) - ((&q.z)^2.0_k);
        q_dot_function = [this](float t, float dt) {
            // q = {
            //     std::cos(t)*5.0f,
            //     std::sin(t)*5.0f,
            //     3.0f + std::sin(t)
            // };
            // q_dot = {
            //     -std::sin(t)*5.0f,
            //     std::cos(t)*5.0f,
            //     std::cos(t)
            // };
        };
        calculate();
    }
    // q = {cx, cy, r}  ->  átmérő = 2r
    float diameter() const override { return 2.0f * q.z; }
};

// q = {R, r}  —  tórusz, algebrai forma (sqrt nélkül)
// F = (x²+y²+z²+R²-r²)² - 4R²(x²+y²)
struct Torus : Surface<2> {
    Torus() {
        q = {3, 1};
        Kif R = &q.x;
        Kif r = &q.y;
        Kif inner = (x^2.0f) + (y^2.0f) + (z^2.0f) + (R^2.0f) - (r^2.0f);
        F = (inner^2.0f) - 4.0f*(R^2.0f)*((x^2.0f) + (y^2.0f));
        q_dot_function = [this](float t, float dt) {
            // q = {
            //     std::cos(t)*5.0f,
            //     std::sin(t)*5.0f,
            //     3.0f + std::sin(t)
            // };
            // q_dot = {
            //     -std::sin(t)*5.0f,
            //     std::cos(t)*5.0f,
            //     std::cos(t)
            // };
        };
        calculate();
    }
    // q = {R, r}. A mintavételi skálát a CSŐ átmérője (a legkisebb jellemző méret)
    // határozza meg, nem a külső 2(R+r). Különben σ̂ = d/4 nagyobb lenne a cső
    // sugaránál, és a repulzió átérne a csövön/lyukon -> instabilitás (lásd cikk).
    float diameter() const override { return 2.0f * q.y; }
};

// q = {cx, cy, a, b}  —  ellipszis (z=0 síkban)
// F = (x-cx)²/a² + (y-cy)²/b² - 1
struct Ellipse : Surface<4> {
    Ellipse() {
        q = {0, 0, 3, 1.5f};
        F = ((x - &q.x)^2.0f)/((&q.z)^2.0_k) + ((y - &q.y)^2.0f)/((&q.w)^2.0_k) - 1.0_k;
        q_dot_function = [this](float t, float dt) {
        };
        calculate();
    }
    // q = {cx, cy, a, b}. A legvékonyabb féltengely adja a jellemző skálát.
    float diameter() const override { return 2.0f * std::min(q.z, q.w); }
};

// q = {a, b, c}  —  origó középpontú ellipszoid
// F = x²/a² + y²/b² + z²/c² - 1
struct Ellipsoid : Surface<3> {
    Ellipsoid() {
        q = {3.0f, 2.0f, 1.0f};
        F = (x^2.0f)/((&q.x)^2.0_k)
          + (y^2.0f)/((&q.y)^2.0_k)
          + (z^2.0f)/((&q.z)^2.0_k) - 1.0_k;
        calculate();
    }
    // q = {a, b, c}. A legvékonyabb tengely adja a jellemző skálát (2·min), különben
    // egy lapos ellipszoid a vékony irányban ugyanúgy instabillá válik, mint a tórusz.
    float diameter() const override { return 2.0f * std::min({q.x, q.y, q.z}); }
};

// Futásidőben megadott implicit felület: F(x,y,z)=0, ahol az F-et KÉSZ (már beparseolt)
// kifejezésfaként kapja (set_tree). Egy-egy ilyen felület EGY alakzatot mintavételez; a
// több alakzatot a hívó (main) külön-külön StringSurface-ekhez rendeli (sampler-pool),
// így mindegyiknek saját ImplicitSurface-lefutása és saját kezdő részecskéi vannak.
// A q paraméterekre nincs szükség (a képlet csak x,y,z + paraméterek), de a glm::vec<0>
// nem létezik, ezért egyetlen, nem használt dummy paramétert tartunk (Surface<1>).
struct StringSurface : Surface<1> {
    StringSurface() {
        q.x = 0.0f;       // dummy, nem használt (glm::vec<1> nincs {…}-értékadás)
        F = Kif(0.0f);    // üres placeholder, amíg nem kap képletet (a pool addig áll)
        calculate();
    }
    // Az F-et egy KÉSZ kifejezésfára állítja, és újraszámolja a deriváltakat.
    void set_tree(std::shared_ptr<Kifejezes const> tree) {
        F = Kif(std::move(tree));
        calculate();
    }
    float diameter() const override { return 2.0f; }
};

struct Teszt : Surface<4> {
    Teszt() {
        q = {0.0f, 0.0f, 0.0f, 0.0f};
        float R = 2.5f, r = 1.0f, eps = 1e-3f;
        auto sxy = (((x^2.0f) + (y^2.0f) + eps)^0.5f);                 // sqrt(x²+y²)
        auto f1  = (((((sxy - R)^2.0f) + (z^2.0f) + eps)^0.5f)) - r;   // sqrt((sxy-R)²+z²)-r


        float cx = 4.8f, a = 1.3f, b = 1.0f, c = 1.0f;
        auto f2 = (((((x - cx)^2.0f)/(a*a)) + ((y^2.0f)/(b*b)) + ((z^2.0f)/(c*c)) + eps)^0.5f) - 1.0_k;

        float k = 0.5f;
        F = 0.5_k * ( f1 + f2 - ((((f1 - f2)^2.0f) + (k*k))^0.5f) );
        F = "x^4 + y^4 + z^4 - 1";
        calculate();
    }
    float diameter() const override { return 2.0f; }
};