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


    void calculate_point_datas(Point& p) {
        p.grad = f.grad(p.pos);
        p.f = f(p.pos);
        p.F = f.F(p);
        p.K = f.K(p.pos);
        p.m = 0.1f / std::abs(p.K) + 1.0f;
    }

    // <-- (p1)      (p2)
    glm::vec3 distance_force(Point const & p1, Point const & p2) {
        glm::vec3 d = p1.pos - p2.pos;
        float r = glm::length(d) + 1.0f;

        // Védelem az egybeeső pontoknál (nullával osztás elkerülése)
        // if (r < 0.001f) return glm::vec3{0.0f};

        // p1.m * p2.m a helyes szorzat!
        return  p1.m * p2.m * glm::normalize(d) / r / r;
    }

protected:
    void render(Camera const &camera) const override {
        glPointSize(5);
        this->set_uniform("color", glm::vec3{0, 0, 0});
        glDrawArrays(GL_POINTS, 0, vertices.size());
        normals.draw(camera);
        taszitoForce.draw(camera);
        tomeg.draw(camera);
        vonzoEro.draw(camera);
    }

    std::vector<Point> points;

    Vector normals;
    Vector taszitoForce;
    Vector tomeg;
    Vector vonzoEro;

    enum State {nulla, start, dist, korrigal};
    State state = nulla;

    float delta = 0.000001f;
public:
    Gorbe(float size) :
        Model(),
        normals{{1, 0, 0}},
        taszitoForce({0, 1, 0}),
        tomeg({0, 0, 1}),
        vonzoEro{{0.5f, 0.5f, 0}}
    {
        ///
        ///
        ///
        int s = 2;
        // f = ((((y / s) ^2_k) + ((x/s) ^ 2_k)-1) ^3_k) - ((x / s) ^2_k)*((y/s) ^3_k);
        //f = (x ^2_k) + (y ^2_k) + x*y -((x*y) ^2_k) /2 - 0.25f;
        //f = (y ^2_k) - (x ^3_k) + x;
        // f = (x ^ 2_k) + (y ^ 2_k) - 25;
        //f = x - y;
        f = (x ^ 2_k) / 9 + (y ^ 2_k) / 2 - 1;
        //f = (x ^3_k) - (y^3_k) - 3*x*y;
        // f = (((x ^2_k) + (y ^2_k)) ^ 2_k) - 2*((x ^2_k) - (y ^2_k));
        // f = sin((x^2_k)) - cos((x^2_k)) - 1;
        float step = 1.0f;

        int numPoints = static_cast<int>(std::pow((2.0f * size) / step, 2));

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
            taszitoForce.reset();
            tomeg.reset();
            vonzoEro.reset();

            for (auto& p : points) {
                calculate_point_datas(p);
            }

            for (auto& p : points) {

                point_moving(p, t, dt);

            }

            // delta *= 0.99f;
            update_buffers();
            normals.update();
            taszitoForce.update();
            tomeg.update();
            vonzoEro.update();
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
        vertices.push_back(p.pos);
        vonzoEro.add_vector(p.pos, p.vonzo_vel);
        taszitoForce.add_vector(p.pos, p.taszito_vel);
        tomeg.add_vector(p.pos, glm::normalize(p.grad)*(p.K));
        if (p.state == base) return;
        static float const gamma = 0.3f;

        float distance = f.distance_to(p);
        if (std::abs(p.f) > delta) {
            p.taszito_vel = {0, 0, 0};
            p.vonzo_vel += p.d * (p.F - gamma*p.vonzo_vel);
            auto seged = p.pos + p.vonzo_vel*dt;
            if (f(seged)*p.f < 0.0f) {
                p.d *= 0.5f;
                p.vonzo_vel = glm::vec3{0};
                p.taszito_vel = glm::vec3{0};
            }
            p.pos = p.pos + (p.vonzo_vel) * dt;
        } else {
            auto taszito_ero = glm::vec3{0};
            for (auto& p2 : points) {
                if (p.pos != p2.pos)
                    taszito_ero += distance_force(p, p2);
            }

            taszito_ero = taszito_ero - (glm::dot(taszito_ero, p.grad)/glm::dot(p.grad, p.grad)*p.grad);
            p.taszito_vel += (taszito_ero / p.m ) * dt;
            p.pos = p.pos + taszito_ero*dt;
            p.vonzo_vel = {0, 0, 0};
        }
        // if (glm::length(p.taszito_vel + p.vonzo_vel) >= delta)


    }

    // void point_moving(Point& p, float t, float dt) {
    //     if (p.state != base) {
    //         if (p.state == toDist) {
    //             if (f.distance_to(p.pos) > 0.0001f) {
    //                 p.state = fromDisttoCurve;
    //                 p.vonzo_vel = {0, 0, 0};
    //             } else {
    //
    //                 glm::vec3 sum{0};
    //                 for (auto& p2 : points) {
    //                     if (p.pos != p2.pos && p2.state == toDist) {
    //                         sum += distance_force(p, p2);
    //                     }
    //                 }
    //                 auto move_force = sum - (glm::dot(p.grad, sum) / glm::dot(p.grad, p.grad)*p.grad);
    //                 // distForce.add_vector(p.pos, sum);
    //                 // distDir.add_vector(p.pos, move_force);
    //                 p.pos += (move_force / p.m * dt);
    //                 p.state = fromDisttoCurve;
    //             }
    //         }
    //         float gamma = 0.8f;
    //         if (p.state == toCurve || p.state == fromDisttoCurve) {
    //
    //
    //
    //
    //             p.vonzo_vel = p.vonzo_vel + p.d * (p.F - gamma * p.vonzo_vel);
    //             auto seged = p.pos + dt*p.vonzo_vel;
    //
    //              // Vizualizáció
    //
    //             float next_h = f(seged);
    //             if (p.f * next_h < 0.0f) {
    //                 p.d *= 0.5f;
    //                 p.vonzo_vel = {0, 0, 0};
    //             }
    //
    //             if (f.distance_to(p.pos) >= 0.0001f) {
    //                 p.vonzo_vel = p.vonzo_vel + p.d * (p.F - gamma * p.vonzo_vel);
    //             }
    //
    //             if (p.state == fromDisttoCurve) {
    //                 p.state = toDist;
    //             }
    //         }
    //     normals.add_vector(p.pos, glm::normalize(p.grad) * 0.2f);
    //     }
    //     p.pos = p.pos + p.vonzo_vel*dt;
    //     vertices.push_back(p.pos);
    //     // ms.add_vector(p.pos, glm::normalize(glm::vec3{1, 1, 0})*p.m);
    // }


    std::vector<glm::vec3> const & get_vertices() { return this->vertices; }
    std::vector<glm::vec3> const & get_normals() { return this->vertices; }
};


#endif //GORBE_GORBE_HPP