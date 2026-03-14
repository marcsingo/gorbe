//
// Created by madam on 2026. 03. 13..
//

#ifndef GORBE_EGYENLET_HPP
#define GORBE_EGYENLET_HPP

#include "Analizis.hpp"
#include <glad/glad.h>

using namespace Matek::Analizis;
using namespace Matek::Valtozok;

class Implicit {
    Egyenlet  f;
    Egyenlet  f_dx;
    Egyenlet  f_dy;
    Egyenlet  f_dz;

    float delta;

    bool is_set;

    void ellenorzes() const {
        if (!is_set) {
            throw std::runtime_error("Nincs bealliva fuggveny");
        }
    }

public:
    explicit Implicit(float delta = 0.0001f) :
        delta(delta),
        is_set(false)
    {
    }

    void operator=(Egyenlet && e) {
        f = std::move(e);
        f_dx = f->derrive('x')->simplify();
        f_dy = f->derrive('y')->simplify();
        f_dz = f->derrive('z')->simplify();
        is_set = true;
    }

    float operator()(glm::vec3 const & p) const {
        ellenorzes();
        return f->at(p);
    }
    glm::vec3 grad(glm::vec3 const & p) const {
        return {
            f_dx->at(p),
            f_dy->at(p),
            f_dz->at(p)
        };
    }
    bool is_on(glm::vec3 const & p) const {
        return std::abs(f->at(p)) < delta;
    }
    bool is_on(Point const & p) const {
        return std::abs(p.f) < delta;
    }
    float distance_to(glm::vec3 const & p) const {
        return std::abs(f->at(p) / glm::length(grad(p)));
    }
    float sgn(glm::vec3 const & p) const {
        float t = f->at(p);
        if (t > 0.0f) return 1;
        if (t < 0.0f) return -1;
        return 0;
    }
    glm::vec3 F(Point& p) const {
        float s = 0;
        if (p.f < 0) s = -1.0f;
        else if (p.f > 0) s = 1.0f;
        return p.grad * s * (-1.0f);
    }
};

#endif //GORBE_EGYENLET_HPP