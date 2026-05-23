//
// Created by madam on 2026. 04. 26..
//

#ifndef GORBE_PARTICLE_HPP
#define GORBE_PARTICLE_HPP
#include "../model/Include.hpp"
#include "Surface.hpp"
#include <algorithm>
#include <vector>

enum ParticleState {
    ramozog,
    rajtamozog
};

template<size_t L>
struct Particle {
    glm::vec3 p {0};
    glm::vec3 p_dot{0};

    glm::vec3 P;
    glm::vec3 F_x;
    glm::vec<L, float> F_q;

    ParticleState state = ramozog;

    float F;

    float sigma{10.0f};
    float D = 0.0f;
    float D_dot = 0.0f;
    float D_sigma = 0.0f;

    float delta = 0.01f;

    bool detah = false;

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
    glm::vec3 color;

    static constexpr int DISK_SEGS = 16;
    static constexpr float TWO_PI  = 6.28318530718f;

protected:
    std::vector<Particle<L>> particles;

    void render(const Camera &camera) override {
        if (particles.empty()) return;

        // CPU-oldalon korong-háromszögek generálása minden részecskéhez.
        // Minden korong 16 háromszögből áll (fan), összesen 48 csúcs/részecske.
        this->vertices.clear();
        this->vertices.reserve(particles.size() * DISK_SEGS * 3);

        for (auto& p : particles) {
            glm::vec3 N = (glm::length(p.F_x) > 1e-6f)
                ? glm::normalize(p.F_x)
                : glm::vec3(0.0f, 1.0f, 0.0f);

            glm::vec3 helper = (std::abs(N.x) < 0.9f)
                ? glm::vec3(1.0f, 0.0f, 0.0f)
                : glm::vec3(0.0f, 1.0f, 0.0f);
            glm::vec3 T = glm::normalize(glm::cross(N, helper));
            glm::vec3 B = glm::cross(N, T);

            for (int i = 0; i < DISK_SEGS; i++) {
                float a0 = TWO_PI * float(i)     / float(DISK_SEGS);
                float a1 = TWO_PI * float(i + 1) / float(DISK_SEGS);
                this->vertices.push_back(p.p);
                this->vertices.push_back(p.p + p.sigma / 2.0f * (std::cos(a0) * T + std::sin(a0) * B));
                this->vertices.push_back(p.p + p.sigma / 2.0f * (std::cos(a1) * T + std::sin(a1) * B));
            }
        }

        // Feltöltés a GPU-ra (update_buffers hagyja a VAO-t kötve)
        this->update_buffers();

        this->set_uniform("color", color);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)this->vertices.size());
    }

public:
    std::vector<Particle<L>>& ps() { return particles; }

    Particles(int /*size*/, glm::vec3 color, Camera const& camera) : color{color} {
        this->update_buffers_on_draw = false; // render() kezeli a feltöltést

        Builder::ShaderBuilder builder;
        set_shader(builder
            .add_vertex_shader  ("../particle_sampling/vertex.vert")
            .add_fragment_shader("../particle_sampling/fragment.glsl")
            .build());
    }

    ~Particles() = default;

    void add_particle(Particle<L> p) {
        particles.push_back(p);
    }

    Particle<L>& operator[](size_t i) { return particles[i]; }

    size_t size() { return particles.size(); }
};

template<size_t L>
struct ControlPoints final : public Particles<L> {
private:
    int selected_idx = -1;
    bool shift_is_on = false;
    Surface<L>* surface = nullptr;
    float phi = 15.0f;

    // Gauss-elimináció részleges főelem-kereséssel: M*x = b
    static std::vector<float> solve_linear(std::vector<std::vector<float>> M,
                                           std::vector<float> b) {
        int n = (int)b.size();
        for (int col = 0; col < n; ++col) {
            int pivot = col;
            for (int row = col + 1; row < n; ++row)
                if (std::abs(M[row][col]) > std::abs(M[pivot][col]))
                    pivot = row;
            std::swap(M[col], M[pivot]);
            std::swap(b[col], b[pivot]);
            if (std::abs(M[col][col]) < 1e-10f) continue;
            for (int row = col + 1; row < n; ++row) {
                float f = M[row][col] / M[col][col];
                for (int k = col; k < n; ++k)
                    M[row][k] -= f * M[col][k];
                b[row] -= f * b[col];
            }
        }
        std::vector<float> x(n, 0.0f);
        for (int i = n - 1; i >= 0; --i) {
            if (std::abs(M[i][i]) < 1e-10f) continue;
            float sum = b[i];
            for (int j = i + 1; j < n; ++j)
                sum -= M[i][j] * x[j];
            x[i] = sum / M[i][i];
        }
        return x;
    }

public:
    void set_surface(Surface<L>* s) { surface = s; }

    ControlPoints(int size, const glm::vec3 &color, Camera const &camera)
        : Particles<L>(size, color, camera) {

        Window::add_mouse_button_event([this, &camera](auto p) {
            if (p.button == GLFW_MOUSE_BUTTON_LEFT && !(p.mods & GLFW_MOD_ALT)) {
                if (p.action == GLFW_PRESS) {
                    if (shift_is_on) {
                        auto pos = camera.get_mouse_pos_on_plane(glm::vec3(0.0f), camera.get_front());
                        Particle<L> cp{pos};
                        cp.sigma = 0.5f; // fix vizuális sugár kontrollpontokhoz
                        if (surface) cp.F_x = surface->grad(pos);
                        this->particles.push_back(cp);
                        return;
                    }
                    for (int i = 0; i < (int)this->particles.size(); ++i) {
                        auto mpos = camera.get_mouse_pos_on_plane(this->particles[i].p, camera.get_front());
                        if (glm::length(mpos - this->particles[i].p) < 0.5f) {
                            selected_idx = i;
                            break;
                        }
                    }
                } else if (p.action == GLFW_RELEASE) {
                    selected_idx = -1;
                }
            }
        });

        Window::add_key_event([this](auto p) {
            if (p.key == GLFW_KEY_LEFT_SHIFT)
                shift_is_on = (p.action == GLFW_PRESS);
        });

        Window::add_time_passed_event([this, &camera](auto ev) {
            // 1. Mozgatott pont sebességének és pozíciójának frissítése
            if (selected_idx >= 0 && selected_idx < (int)this->particles.size()) {
                auto& sel = this->particles[selected_idx];
                auto mpos = camera.get_mouse_pos_on_plane(sel.p, camera.get_front());
                sel.p_dot = 10.0f * (mpos - sel.p);
                sel.p += sel.p_dot * static_cast<float>(ev.dt);
                // Normális frissítése vonszoláskor (korong orientációja kövesse a felszínt)
                if (surface) sel.F_x = surface->grad(sel.p);
            }

            if (surface == nullptr || this->particles.empty()) return;

            int n = (int)this->particles.size();

            // F_q^i gyűjtése minden control ponthoz
            std::vector<glm::vec<L, float>> F_q_all(n);
            std::vector<float> b_vec(n);

            for (int i = 0; i < n; ++i) {
                auto& pi = this->particles[i];
                // P^i: csak a kijelölt pontnál nem nulla (7. egyenlet jobb oldala)
                glm::vec3 P_i = (i == selected_idx) ? pi.p_dot : glm::vec3{0};

                F_q_all[i] = surface->get_F_q(pi.p);
                float F_i   = surface->F.at(pi.p);
                glm::vec3 F_x_i = surface->grad(pi.p);

                // b[i] = F_x^i · P^i + phi * F^i
                b_vec[i] = glm::dot(F_x_i, P_i) + phi * F_i;
            }

            // M[i][j] = F_q^i · F_q^j  (7. egyenlet mátrixa)
            std::vector<std::vector<float>> M(n, std::vector<float>(n, 0.0f));
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                    M[i][j] = glm::dot(F_q_all[i], F_q_all[j]);

            // Mλ = b megoldása
            auto lambda = solve_linear(M, b_vec);

            // q_dot = Q - Σ lambda_j * F_q^j,  Q = 0 (8. egyenlet)
            glm::vec<L, float> q_dot{0};
            for (int j = 0; j < n; ++j)
                q_dot -= lambda[j] * F_q_all[j];

            surface->q_dot = q_dot;

            // q Euler-integrálása (8. egyenlet utáni lépés a cikkben)
            surface->q += q_dot * static_cast<float>(ev.dt);
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