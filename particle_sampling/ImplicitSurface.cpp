
#include <map>
#include <random>
#include <cmath>

#include "Particle.hpp"
#include "../model/Include.hpp"
#include "../matek/Kif.hpp"


using namespace Matek::Analizis;


class SphereOccluder : public Model {
    Sphere const& sphere;
    glm::vec3 color;

    static constexpr int   RINGS   = 24;
    static constexpr int   SEGS    = 24;
    static constexpr float PI      = 3.14159265359f;
    static constexpr float TWO_PI  = 6.28318530718f;

    void render(const Camera&) override {
        vertices.clear();
        glm::vec3 c{sphere.q.x, sphere.q.y, sphere.q.z};
        float r = sphere.q.w;

        auto v = [&](float phi, float theta) -> glm::vec3 {
            return c + r * glm::vec3{
                std::sin(phi) * std::cos(theta),
                std::cos(phi),
                std::sin(phi) * std::sin(theta)
            };
        };

        for (int ri = 0; ri < RINGS; ++ri) {
            float p0 = PI * float(ri)     / float(RINGS);
            float p1 = PI * float(ri + 1) / float(RINGS);
            for (int si = 0; si < SEGS; ++si) {
                float t0 = TWO_PI * float(si)     / float(SEGS);
                float t1 = TWO_PI * float(si + 1) / float(SEGS);
                vertices.push_back(v(p0, t0));
                vertices.push_back(v(p1, t0));
                vertices.push_back(v(p1, t1));
                vertices.push_back(v(p0, t0));
                vertices.push_back(v(p1, t1));
                vertices.push_back(v(p0, t1));
            }
        }

        update_buffers();
        set_uniform("color", color);
        // A gömb mélységértékeit kicsit eltoljuk a kamerától, hogy a felszínen
        // lévő floaterek ne tűnjenek el a depth test miatt (Z-fighting elkerülése).
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 1.0f);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertices.size());
        glDisable(GL_POLYGON_OFFSET_FILL);
    }

public:
    SphereOccluder(Sphere const& s, glm::vec3 col, Camera const&)
        : sphere{s}, color{col}
    {
        update_buffers_on_draw = false;
        Builder::ShaderBuilder builder;
        set_shader(builder
            .add_vertex_shader  ("../particle_sampling/vertex.vert")
            .add_fragment_shader("../particle_sampling/fragment.glsl")
            .build());
    }
};



template<size_t L>
class ImplicitSurface {
    Sphere surface;
    Floaters<L> floaters;
    ControlPoints<L> controls;
    SphereOccluder sphere_mesh;

    std::mt19937 rng;
    std::uniform_real_distribution<float> dist_R;
public:
    explicit ImplicitSurface( Camera const &camera) :
        floaters{5, {0, 0, 1}, camera},
        controls{10, {1, 0, 0}, camera},
        sphere_mesh{surface, {0.85f, 0.85f, 0.85f}, camera},
    rng(std::random_device{}()),
    dist_R(0.0f, 1.0f)
    {
        // Egyetlen kezdő részecske — a globális fisszió (4.2 fejezet) ebből épít fel
        // egyenletes mintavételt anélkül, hogy előre el kellene helyezni a pontokat.
        Particle<L> p0;
        p0.p     = glm::vec3{surface.q.x, surface.q.y + surface.q.w, surface.q.z};
        p0.sigma = sigma_max;
        p0.F_x   = surface.grad(p0.p); // kezdeti normális a korong-rendereléshez
        floaters.add_particle(p0);

        controls.set_surface(&surface);

        Window::add_time_passed_event([this](auto p) {
            static float dt = 0;
            dt += p.dt;
            if (dt >= 0.03f) {
                this->simulation(p.t, dt);
                dt = 0.0f;
            }
        });

    }

    float const d = 2.0f * surface.q.w;

    float const alpha = 6.0f;
    float const sigma = 1.0f;
    float const PHI = 15.0f;
    float const E_v = 0.8f * alpha;
    float const rho = PHI;
    float const beta = 10.0f;
    float const gamma = 4.0f;
    float const sigma_v = d / 4.0f;
    float const sigma_max = std::max(d/2.0f, 1.5f * sigma_v);
    float const nu = 0.2f;
    float const delta = 0.7f;
    float const fraction = 0.001f;

    void calculate_particle(Particle<L>& p) {
        p.F = surface.F.at(p.p);
        p.F_x = surface.grad(p.p);
        p.P = glm::vec3{0};
        p.D = 0.0f;
        p.D_sigma = 0.0f;
    }

    void simulation(float t, float dt) {
        if (controls.ps().size() < 2) return;
        std::vector<Particle<L>> particles;
        for (auto& i : floaters.ps()) {
            calculate_particle(i);

            for (auto& j : floaters.ps()) {
                if (&i == &j) continue;
                auto r = i.p - j.p;
                auto E_ij = alpha*std::exp(-glm::dot(r, r) / (i.sigma*i.sigma*2));
                auto E_ji = alpha*std::exp(-glm::dot(r, r) / (j.sigma*j.sigma*2));
                i.P += r / (i.sigma*i.sigma) * E_ij + r / (j.sigma*j.sigma) * E_ji;
                i.D += E_ij;

                i.D_sigma += glm::dot(r, r)*E_ij;

            }
            i.P *= i.sigma*i.sigma;

            i.D_dot = -rho*(i.D - E_v);
            i.D_sigma *= (1/(i.sigma*i.sigma*i.sigma));

            i.sigma += (i.D_dot / (i.D_sigma +  beta)) * dt;
            i.sigma = std::max(i.sigma, 1e-3f);

            if (glm::length(i.F_x) > 1e-6f) {
                i.p_dot =
                    i.P -
                        ((glm::dot(i.F_x, i.P) + glm::dot(surface.q_dot,surface.get_F_q(i.p)) + PHI*i.F)
                            /
                        glm::dot(i.F_x, i.F_x)) * i.F_x;
            } else {
                i.p_dot = glm::vec3(0,0,0);
            }


            i.p += i.p_dot * dt;

            float R = dist_R(rng);

            if (glm::length(i.p_dot) < gamma*i.sigma &&
                (i.sigma > sigma_max || (i.D > nu * E_v && i.sigma > sigma_v)))
            {
                //fisszó
                i.sigma /= std::sqrt(2.0f);

                // Véletlen irányt a felszín érintősíkjába vetítjük
                glm::vec3 normal = (glm::length(i.F_x) > 1e-6f)
                    ? glm::normalize(i.F_x) : glm::vec3(0, 1, 0);
                glm::vec3 rand_vec = {
                    dist_R(rng) - 0.5f,
                    dist_R(rng) - 0.5f,
                    dist_R(rng) - 0.5f
                };
                glm::vec3 tangent = rand_vec - glm::dot(rand_vec, normal) * normal;
                if (glm::length(tangent) > 1e-6f) tangent = glm::normalize(tangent);

                glm::vec3 offset = tangent * (0.5f * i.sigma);
                i.p += offset;
                particles.push_back(i);

                Particle<L> child = i;
                child.p -= 2.0f * offset;
                particles.push_back(child);

            } else if (
                glm::length(i.p_dot) < gamma*i.sigma &&
                i.sigma < delta*sigma_v &&
                R > i.sigma/(delta*sigma_v))
            {
                //halál

            } else {
                //megmarad a pont
                particles.push_back(i);
            }

        }
        floaters.ps() = particles;
    }

    void draw(const Camera &camera) {
        sphere_mesh.draw(camera);
        floaters.draw(camera);
        controls.draw(camera);
    }


};



