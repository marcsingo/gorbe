//
// Created by madam on 2026. 04. 26..
//

#ifndef GORBE_PARTICLE_HPP
#define GORBE_PARTICLE_HPP
#include "../model/Include.hpp"
#include "Surface.hpp"
#include <algorithm>

template<size_t L>
struct Particle {
    glm::vec3 p {0};
    glm::vec3 p_dot{0};

    glm::vec3 P;
    glm::vec3 F_x;
    glm::vec<L, float> F_q;

    float F;

    float sigma{10.0f};
    float D = 0.0f;
    float D_dot = 0.0f;
    float D_sigma = 0.0f;

    void update_surface_data(const Surface<L>& surface) {
        this->F = surface.F.at(this->p);
        this->F_x = surface.grad(this->p);

        if (glm::dot(this->F_x, this->F_x) < 0.00001f) {
            this->F_x = glm::vec3{0};
        }
    }
};

template<size_t L>
struct Particles : Model {
private:

    int p_size;
    glm::vec3 color;



protected:
    std::vector<Particle<L>> particles;
    void render(const Camera &camera) override {
        this->vertices.resize(particles.size());
        for (int i = 0; i < particles.size(); i++) {
            this->vertices[i] = particles[i].p;
        }
        glPointSize(p_size);
        this->set_uniform("color", color);
        glDrawArrays(GL_POINTS, 0, vertices.size());
    }
public:
    std::vector<Particle<L>>& ps(){ return particles; };
    Particles(int size, glm::vec3 color, Camera const & camera) : p_size{size}, color{color} {
        this->update_buffers_on_draw = true;
        Builder::ShaderBuilder builder;
        set_shader(builder
            .add_vertex_shader("../particle_sampling/vertex.vert")
            .add_fragment_shader("../particle_sampling/fragment.glsl")
            .build());



    }

    void add_particle(Particle<L> p) {
        particles.push_back(p);
    }

    Particle<L>& operator[](size_t i) { return particles[i]; };

    size_t size() {return particles.size();}


};

template<size_t L>
struct ControlPoints final : public Particles<L> {
private:
    Particle<L>* selected = nullptr;
    bool shift_is_on = false;
public:
    ControlPoints(int size, const glm::vec3 &color, Camera const &camera)
        : Particles<L>(size, color, camera) {
        Window::add_mouse_button_event([this, &camera](auto p) {
                if (p.action == GLFW_PRESS && p.button == GLFW_MOUSE_BUTTON_LEFT) {
                    if (shift_is_on ) {
                        this->particles.push_back(Particle<L>{camera.get_mouse_pos_in_world()});
                        return;
                    }
                    if (selected != nullptr) {
                        selected = nullptr;
                        return;
                    }
                    auto mpos = camera.get_mouse_pos_in_world();
                    for (auto& part : this->particles) {
                        if (glm::length(mpos - part.p) < 0.5f) {
                            this->selected = &part;

                        }
                    }
                }
            });

        Window::add_key_event([this, &camera](auto p) {
           if (p.key == GLFW_KEY_LEFT_SHIFT) {
               if (p.action == GLFW_PRESS) {
                   shift_is_on = true;
               } else {
                   shift_is_on = false;
               }
           }
        });

        Window::add_time_passed_event([this, &camera](auto p) {
            if (this->selected != nullptr) {

                std::cout << selected->p_dot.x << " " << selected->p_dot.y << std::endl;

                auto mpos = camera.get_mouse_pos_in_world();
                this->selected->p_dot = 10.0f * (mpos - selected->p);
                this->selected->p += selected->p_dot*static_cast<float>(p.dt);
            }

        });
    }
};

template<size_t L>
struct Floaters final : Particles<L> {
    Floaters(int size, const glm::vec3 &color, Camera const &camera)
        : Particles<L>(size, color, camera) {
    }
};


#endif //GORBE_PARTICLE_HPP