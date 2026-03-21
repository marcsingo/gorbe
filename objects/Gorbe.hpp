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
#include <ranges>

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
        p.g = f.grad(p.p);
        p.f = f(p.p);
        p.F = f.F(p);
        p.K = f.K(p.p);
        p.m = 1.0f / std::abs(p.K) + 1;
    }

    // <-- (p1)      (p2)
    glm::vec3 distance_force(Point const & p1, Point const & p2) {
        glm::vec3 d = p1.p - p2.p;
        float r = glm::length(d) + 1.0f;

        // Védelem az egybeeső pontoknál (nullával osztás elkerülése)
        // if (r < 0.001f) return glm::vec3{0.0f};

        // p1.m * p2.m a helyes szorzat!
        return  p1.m * p2.m * glm::normalize(d) / r / r;
    }

protected:
    void render(Camera const &camera) const override {
        glPointSize(2);
        this->set_uniform("color", glm::vec3{0, 0, 0});
        glDrawArrays(GL_POINTS, 0, vertices.size());
        gradiens.draw(camera);
        forces.draw(camera);
        velocities.draw(camera);
        vonzoEro.draw(camera);
    }

    std::vector<Point> points;

    Vector gradiens;
    Vector forces;
    Vector velocities;
    Vector vonzoEro;

    enum State {nulla, start, dist, korrigal};
    State state = nulla;

    float delta = 0.000001f;
public:
    Gorbe(float size) :
        Model(),
        f{3},
        gradiens{{1, 0, 0}},
        forces({0, 1, 0}),
        velocities({0, 0, 1}),
        vonzoEro{{0.5f, 0.5f, 0}}
    {
        ///
        ///
        ///
        int s = 2;
        //f = ((((y / s) ^2_k) + ((x/s) ^ 2_k)-1) ^3_k) - ((x / s) ^2_k)*((y/s) ^3_k);
        //f = (x ^2_k) + (y ^2_k) + x*y -((x*y) ^2_k) /2 - 0.25f;
        //f = (y ^2_k) - (x ^3_k) + x;
        // f = (x ^ 2_k) + (y ^ 2_k) - 25;
        //f = x - y;
         // f = (x ^ 2_k) / 9 + (y ^ 2_k) / 2 - 1;
        //f = (x ^3_k) - (y^3_k) - 3*x*y;
         // f = (((x ^2_k) + (y ^2_k)) ^ 2_k) - 2*((x ^2_k) - (y ^2_k));
        // f = sin((x^2_k)) - cos((x^2_k)) - 1;
        // f = (x^2_k) + (y^2_k) + (z^2_k) -1;
        f = (((((((x^2_k) + (y^2_k)) ^(0.5_k)) - 2) ^2_k) + (z^2_k)) - 1);

        float step = 1.0f;

        int numPoints = static_cast<int>(std::pow((50.0f * size) / step, 2));

        std::random_device rd;
        std::mt19937 gen(rd()); // Mersenne Twister motor
        std::uniform_real_distribution<float> dist(-size, size); // Egyenletes eloszlás

        // 3. Generálás egy egyszerű ciklussal
        for (int i = 0; i < numPoints; ++i) {
            float x = dist(gen);
            float y = dist(gen);
            float z = dist(gen);
            auto temp = Point{
                .p = glm::vec3{x, y, z}
            };
            points.push_back(temp);
            vertices.push_back(glm::vec3{x, y, z});
        }

        // std::cout << vertices.size() << std::endl;

        update_buffers();
        Builder::ShaderBuilder builder;
        set_shader(builder
            .add_vertex_shader("../objects/vertex.vert")
            .add_fragment_shader("../objects/fragment.glsl")
            .build());

        Window::add_time_passed_event([this](double t, double dt) {

            gradiens.reset();
            forces.reset();
            velocities.reset();
            vonzoEro.reset();

            for (auto& p : points) {
                calculate_point_datas(p);
                // gradiens.add_vector(p.p, glm::normalize(p.g)*p.m);

            }

            for (int i = 0; i < points.size(); ++i) {
                auto& p = points[i];
                vertices[i] = p.p;
                if (p.state == base) continue;

                point_moving(p, t, dt);

                glm::vec3 fs = {0, 0, 0};
                for (auto &val: p.forces | std::views::values) {
                    fs += val;
                }
                // forces.add_vector(p.p, fs);
                p.v += -p.v * p.nu;
                p.v = p.v + fs / p.m * static_cast<float>(dt);
                // velocities.add_vector(p.p, p.v);

                auto temp = p.p + p.v * static_cast<float>(dt);
                if (f(temp)*f(p.p) < 0.0f) p.delta *= 0.5f;
                p.p = temp;
                p.forces.erase(p.forces.begin(), p.forces.end());
            }
            // epsilon *= 0.998f;
            // std::cout << epsilon << std::endl;
            // delta *= 0.99f;
            update_buffers();
            gradiens.update();
            forces.update();
            velocities.update();
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
                    if (std::abs(f.distance_to(p.p)) < 0.01f) newPoints.push_back(p);
                }
                points = newPoints;
            } else if (key == GLFW_KEY_O && action == GLFW_PRESS) {
                for (auto& p : points) {
                    p.state = fromDisttoCurve;
                }
            }
        });


    }

    float const M = 1000.0f;
    float epsilon = 0.00001f;
    void point_moving(Point& p, double t, double dt) {
        if (std::abs(p.f) >= epsilon) {
            p.forces["felulethez"] = p.delta * (p.F*1.0f - p.v);

        }
        else {
            p.forces["eloszlas"] = {0, 0, 0};
            for (auto& p2 : points) {
                if (p.p != p2.p && p2.f < epsilon) {
                    p.forces["eloszlas"] += distance_force(p, p2)*10.0f;
                }
            }
        }
    }

    std::vector<glm::vec3> const & get_vertices() { return this->vertices; }
    std::vector<glm::vec3> const & get_normals() { return this->vertices; }
};


#endif //GORBE_GORBE_HPP