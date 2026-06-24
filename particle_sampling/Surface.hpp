#pragma once
#include <list>
#include "../matek/Kif.hpp"
#include "../matek/Analizis.hpp"
#include "Particle.hpp"
using namespace Matek::Analizis;

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
    Kif F;

    glm::vec<L, float> q;
    glm::vec<L, float> q_dot;

    // Ide írjuk folyamatosan az alakzat aktuális átmérőjét, ha valaki (pl. az
    // ImplicitSurface) megkérte rá a cím átadásával. Így a hívó d-je mindig a
    // felület valódi átmérőjét tükrözi, akkor is, ha a q paraméterek mozognak.
    float* d_ptr = nullptr;

    std::function<void(float, float)> q_dot_function = [](float t, float dt){};
    Surface() {
        q_dot = glm::vec<L, float>(0);
        Window::add_time_passed_event([this](auto p) {
            this->q_dot_function(p.t, p.dt);
            if (this->d_ptr) *this->d_ptr = this->diameter();
        });
    }

    virtual ~Surface() = default;

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

    void calculate() {
        for (int i = 0; i < L; i++) {
            F_dp_s[&q[i]] = F.derrive(&q[i]);
        }
        F_dx = F.derrive('x');
        F_dy = F.derrive('y');
        F_dz = F.derrive('z');
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

// Futásidőben, STRINGBŐL megadott implicit felület: F(x,y,z)=0.
// A q paraméterekre nincs szükség (a string csak x,y,z-t és konstansokat ismer),
// de a glm::vec<0> nem létezik, ezért egyetlen, nem használt dummy paramétert tartunk
// (Surface<1>). Az egyenlet menet közben átírható (set_equation), ami újraszámolja a
// deriváltakat is. Parse-hiba esetén std::runtime_error-t dob (a hívó kapja el).
struct StringSurface : Surface<1> {
    // Felhasználói paraméter: NÉV + ÉRTÉK. A value címe (&value) STABIL kell legyen,
    // mert a kifejezésfa Parameter-csomópontja erre mutató float const*-ot tárol —
    // ezért std::list-ben tartjuk (a node-ok címe beszúrásra/törlésre nem mozdul).
    struct Param { char name[32] = ""; float value = 0.0f; };
    std::list<Param> params;

    StringSurface() {
        q.x = 0.0f; // dummy, nem használt (glm::vec<1> nincs {…}-értékadás)
        set_equation("x^2 + y^2 + z^2 - 1"); // alap: egységgömb
    }

    // F(x,y,z) képlete stringből (pl. "x^2 + y^2 + z^2 - 1" vagy "x^2 + y^2 - r^2").
    // Az x/y/z változó; minden más azonosító a params-ban felvett paraméter neve kell
    // legyen, különben parse-hibát dob (std::runtime_error), amit a hívó kap el.
    void set_equation(std::string const& eq) {
        F = make_kif(eq, [this](std::string const& nm) -> float const* {
            for (auto& p : params)
                if (nm == p.name) return &p.value;
            return nullptr; // ismeretlen név -> a parser hibát dob
        });
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