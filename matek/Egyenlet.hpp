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

    Egyenlet f_dx_dx, f_dx_dy, f_dx_dz;
    Egyenlet f_dy_dx, f_dy_dy, f_dy_dz;
    Egyenlet f_dz_dx, f_dz_dy, f_dz_dz;

    float delta;

    bool is_set;
    int dim;

    void ellenorzes() const {
        if (!is_set) {
            throw std::runtime_error("Nincs bealliva fuggveny");
        }
    }

public:
    explicit Implicit(int dimension = 2, float delta = 0.0001f) :
        delta(delta),
        is_set(false),
        dim(dimension)
    {
    }

    void operator=(Egyenlet && e) {
        f = std::move(e);
        f_dx = f->derrive('x')->simplify();
        f_dy = f->derrive('y')->simplify();
        f_dz = f->derrive('z')->simplify();

        f_dx_dx = f_dx->derrive('x')->simplify();
        f_dx_dy = f_dx->derrive('y')->simplify();
        f_dx_dz = f_dx->derrive('z')->simplify();

        f_dy_dx = f_dx_dy;
        f_dy_dy = f_dy->derrive('y')->simplify();
        f_dy_dz = f_dy->derrive('z')->simplify();

        f_dz_dx = f_dx_dz;
        f_dz_dy = f_dy_dz;
        f_dz_dz = f_dz->derrive('z')->simplify();

        is_set = true;
    }

    float K(glm::vec3 const & p) const {
        float A = -1.0f / std::pow(glm::length(grad(p)), 4);
        float fxx = f_dx_dx->at(p);
        float fxy = f_dx_dy->at(p);
        float fxz = f_dx_dz->at(p);
        float fyx = fxy;
        float fyy = f_dy_dy->at(p);
        float fyz = f_dy_dz->at(p);
        float fzx = fxz;
        float fzy = fyz;
        float fzz = f_dz_dz->at(p);

        auto g = grad(p);
        float B;
        if (dim == 3)
        B = glm::determinant(glm::mat4{
            fxx, fxy, fxz, g.x,
            fyx, fyy, fyz, g.y,
            fzx, fzy, fzz, g.z,
            g.x, g.y, g.z, 0
        });
        else if (dim == 2) {
            B = glm::determinant(glm::mat3{
            fxx, fxy, g.x,
            fyx, fyy, g.y,
            g.x, g.y,  0
        });
        }
        else
            B = 0;

        return A*B;
    }

    float operator()(glm::vec3 const & p) const {
        ellenorzes();
        return f->at(p);
    }
    glm::vec3 grad(glm::vec3 const & p) const {
        ellenorzes();
        return {
            f_dx->at(p),
            f_dy->at(p),
            f_dz->at(p)
        };
    }
    bool is_on(glm::vec3 const & p) const {
        ellenorzes();
        return std::abs(f->at(p)) < delta;
    }
    bool is_on(Point const & p) const {
        ellenorzes();
        return std::abs(p.f) < delta;
    }
    float distance_to(glm::vec3 const & p) const {
        ellenorzes();
        return std::abs(f->at(p) / glm::length(grad(p)));
    }
    float distance_to(Point const & p) const {
        ellenorzes();
        return std::abs(p.f / glm::length(p.grad));
    }
    float sgn(glm::vec3 const & p) const {
        ellenorzes();
        float t = f->at(p);
        if (t > 0.0f) return 1;
        if (t < 0.0f) return -1;
        return 0;
    }
    glm::vec3 F(Point const & p) const {
        ellenorzes();
        float s = 0;
        if (p.f < 0) s = -1.0f;
        else if (p.f > 0) s = 1.0f;
        return p.grad * s * (-1.0f);
    }
};

#endif //GORBE_EGYENLET_HPP