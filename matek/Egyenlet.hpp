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
    Egyenlet const f;
    Egyenlet const f_dx;
    Egyenlet const f_dy;
    Egyenlet const f_dz;

    float delta;

    bool is_set;

public:
    explicit Implicit(Egyenlet && e, float delta = 0.0001f) :
        f(std::move(e)),
        f_dx(f->derrive('x')),
        f_dy(f->derrive('y')),
        f_dz(f->derrive('z')),
        delta(delta),
        is_set(true)
    {

    }

    float operator()(glm::vec3 const & p) const {
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
    float distance_to(glm::vec3 const & p) const {
        return std::abs(f->at(p) / glm::length(grad(p)));
    }
    float sgn(glm::vec3 const & p) const {
        float t = f->at(p);
        if (t > 0.0f) return 1;
        if (t < 0.0f) return -1;
        return 0;
    }
    glm::vec3 F(glm::vec3 const & p) const {
        return grad(p) * sgn(p) * (-1.0f);
    }
};

#endif //GORBE_EGYENLET_HPP