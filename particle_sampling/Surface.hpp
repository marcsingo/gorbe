#include "../matek/Kif.hpp"
#include "../matek/Analizis.hpp"
#include "Particle.hpp"
using namespace Matek::Analizis;

template<size_t L>
struct Surface {
private:
    Surface(Surface const & s) = default;

public:
    std::map<float const *, Kif> F_dp_s;
    Kif F_dx, F_dy, F_dz;
    Kif F;

    glm::vec<L, float> q;
    glm::vec<L, float> q_dot;

    std::function<void(float, float)> q_dot_function = [](float t, float dt){};
    Surface() {
        q_dot = glm::vec<L, float>(0);
        Window::add_time_passed_event([this](auto p) {
            this->q_dot_function(p.t, p.dt);
        });
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
};