//
// Created by madam on 2026. 03. 08..
//

#ifndef GORBE_GORBE_HPP
#define GORBE_GORBE_HPP
#include <complex>
#include <filesystem>

#include "Vector.hpp"
#include "../model/Model.hpp"

#include "../matek/Analizis.hpp"
#include "../model/Shader.hpp"
#include "../model/Window.hpp"
#include "Point.hpp"
#include "../matek/Egyenlet.hpp"
#include <random>
#include <cmath>

using namespace Matek::Analizis;
using namespace Matek::Valtozok;
using namespace Builder;

inline std::ostream& operator<<(std::ostream& os, glm::vec3& v) {
    os << "(" << v.x << "," << v.y << "," << v.z << ")" << std::endl;
    return os;
}


class Gorbe : public Model {

    Implicit f;

    // glm::vec3 grad(glm::vec3 p) {
    //     return glm::vec3{
    //         fdx->at(p),
    //         fdy->at(p),
    //         fdz->at(p)
    //     };
    // }
    //
    // float sgn(float val) {
    //     if (val > 0) return 1;
    //     if (val < 0) return -1;
    //     return 0;
    // }
    //
    // glm::vec3 F(float f_p, glm::vec3 p) {
    //     return  -sgn(f_p) * grad(p);
    // }
    //
    // float distance_to_surface(glm::vec3& p) {
    //     return f->at(p) / glm::length(grad(p));
    // }
    //
    // bool is_nulla(float value) {
    //     if (std::abs(value) < 0.001f) {
    //         return true;
    //     }
    //     return false;
    // }

    void calculate_point_datas(Point& p) {
        p.grad = f.grad(p.pos);
        p.f = f(p.pos);
    }

    // <-- (p1)      (p2)
    glm::vec3 distance_force(glm::vec3& p1, glm::vec3& p2) {
        glm::vec3 d = p1 - p2;
        float r = glm::length(d);
        return glm::normalize(d) / r / r;
    }

protected:
    void render(Camera const &camera) const override {
        glPointSize(5);
        this->set_uniform("color", glm::vec3{0, 0, 0});
        glDrawArrays(GL_POINTS, 0, vertices.size());
        normals.draw(camera);
        distForce.draw(camera);
        distDir.draw(camera);
    }

    std::vector<Point> points;

    Vector normals;
    Vector distForce;
    Vector distDir;

    enum State {nulla, start, dist, korrigal};
    State state = nulla;
public:
    Gorbe(float size) :
        Model(),
        normals{{1, 0, 0}},
        distForce({0, 1, 0}),
        distDir({0, 0, 1})
    {
        ///
        ///
        ///
        int s = 2;
        f = ((((y / s) ^2_k) + ((x/s) ^ 2_k)-1) ^3_k) - ((x / s) ^2_k)*((y/s) ^3_k);
        //f = (x ^2_k) + (y ^2_k) + x*y -((x*y) ^2_k) /2 - 0.25f;
        //f = (y ^2_k) - (x ^3_k) + x;
        //f = (x ^ 2_k) + (y ^ 2_k) - 25;
        //f = x - y;
        // fdx = f->derrive('x')->simplify();
        // fdy = f->derrive('y')->simplify();
        // fdz = f->derrive('z')->simplify();
        //
        // f->print(std::cout); std::cout << std::endl;
        // fdx->print(std::cout); std::cout << std::endl;
        // fdy->print(std::cout); std::cout << std::endl;
        // fdz->print(std::cout); std::cout << std::endl;

        float step = 1.0f;

        int numPoints = static_cast<int>(std::pow((3.5f * size) / step, 2));

        std::random_device rd;
        std::mt19937 gen(rd()); // Mersenne Twister motor
        std::uniform_real_distribution<float> dist(-size, size); // Egyenletes eloszlás

        // 3. Generálás egy egyszerű ciklussal
        for (int i = 0; i < numPoints; ++i) {
            float x = dist(gen);
            float y = dist(gen);

            points.push_back(Point{
                .pos = glm::vec3{x, y, 0.0f}
            });
        }

        std::cout << vertices.size() << std::endl;

        update_buffers();
        Builder::ShaderBuilder builder;
        set_shader(builder
            .add_vertex_shader("../objects/vertex.vert")
            .add_fragment_shader("../objects/fragment.glsl")
            .build());

        Window::add_time_passed_event([this](double t, double dt) {

            this->vertices.clear();
            normals.reset();
            distForce.reset();
            distDir.reset();

            for (auto& p : points) {

                point_moving(p, t, dt);

            }

            update_buffers();
            normals.update();
            distForce.update();
            distDir.update();
        });

        Window::add_key_event([this](int key, int scancode, int action, int mode) {
            if (key == GLFW_KEY_I && action == GLFW_PRESS) {
                for (auto& p : points) {
                    p.state = toCurve;
                }
            } else if (key == GLFW_KEY_V && action == GLFW_PRESS) {
                for (auto& p : points) {
                    p.state = toDist;
                }
            } else if (key == GLFW_KEY_B && action == GLFW_PRESS) {
                this->state = nulla;
                for (auto& p : points) {
                    p.state = base;
                }
                std::cout << points.size() << std::endl;
            } else if (key == GLFW_KEY_T && action == GLFW_PRESS) {
                std::vector<Point> newPoints;
                for (auto& p : points) {
                    if (std::abs(f.distance_to(p.pos)) < 0.01f) newPoints.push_back(p);
                }
                points = newPoints;
            } else if (key == GLFW_KEY_O && action == GLFW_PRESS) {
                for (auto& p : points) {
                    p.state = fromDisttoCurve;
                }
            }
        });


    }

    void point_moving(Point& p, float t, float dt) {
        if (p.state != base) {
            if (p.state == toDist) {

                glm::vec3 sum{0};
                for (auto& p2 : points) {
                    if (p.pos != p2.pos && p2.state == toDist) {

                        sum += distance_force(p.pos, p2.pos);
                    }
                }
                //p.v = p.v + sum * 0.5f * dt;
                auto t_v = glm::normalize(sum);
                auto g_v = glm::normalize(p.grad);
                auto move = t_v - (glm::dot(g_v,t_v)*g_v);
                distForce.add_vector(p.pos, glm::normalize(sum)*0.2f);
                distDir.add_vector(p.pos, glm::normalize(
                    move
                )*0.2f);
                p.pos = p.pos + move*dt;
                p.state = fromDisttoCurve;
            }
            float gamma = 0.8f;
            if (p.state == toCurve || p.state == fromDisttoCurve) {

                calculate_point_datas(p);


                glm::vec3 F_unc = f.F(p);

                p.vel = p.vel + p.d * (F_unc - gamma * p.vel);
                auto seged = p.pos + dt*p.vel;

                 // Vizualizáció

                float next_h = f(seged);
                if (p.f * next_h < 0.0f) {
                    p.d *= 0.5f;
                    p.vel = {0, 0, 0};
                }

                if (!f.is_on(p)) {
                    p.pos = seged;
                }

                if (p.state == fromDisttoCurve) {
                    p.state = toDist;
                }
            }
        normals.add_vector(p.pos, glm::normalize(p.grad) * 0.2f);
        }
        vertices.push_back(p.pos);

    }


    std::vector<glm::vec3> const & get_vertices() { return this->vertices; }
    std::vector<glm::vec3> const & get_normals() { return this->vertices; }
};


#endif //GORBE_GORBE_HPP