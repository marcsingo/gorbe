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
        q = {0, 0, 3};
        F = ((x- &q.x) ^ 2.0f)  + ((y - &q.y) ^2.0f) - ((&q.z) ^ 2.0_k);
        q_dot_function = [this](float t, float dt) {
            q = {
                std::cos(t)*5.0f,
                std::sin(t)*5.0f,
                3.0f + std::sin(t)
            };
            q_dot = {
                -std::sin(t)*5.0f,
                std::cos(t)*5.0f,
                std::cos(t)
            };
        };
        calculate();
    }
};