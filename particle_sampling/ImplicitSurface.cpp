
#include <map>
#include <random>
#include <cmath>

#include "Particle.hpp"
#include "../model/Include.hpp"
#include "../matek/Kif.hpp"
#include "Occluders.hpp"


using namespace Matek::Analizis;



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
        // Particle<L> p0;
        // p0.p     = glm::vec3{surface.q.x, surface.q.y + surface.q.w, surface.q.z};
        // p0.sigma = sigma_max;
        // p0.F_x   = surface.grad(p0.p); // kezdeti normális a korong-rendereléshez
        // floaters.add_particle(p0);

        controls.set_surface(&surface);
        spawn_random_particles(1, 3);

        Window::add_time_passed_event([this](auto p) {
            static float dt = 0;
            dt += p.dt;
            if (dt >= 0.03f) {
                this->simulation(p.t, dt);
                dt = 0.0f;
            }
        });

    }

    float const d = 2.0f;// * surface.q.w;

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

    void spawn_random_particles(int n, float cube_size) {
        // A kocka közepe az origó, így a határok -méret/2 és +méret/2 között lesznek
        float half_size = cube_size / 2.0f;
        std::uniform_real_distribution<float> dist_cube(-half_size, half_size);

        for (int i = 0; i < n; ++i) {
            Particle<L> p;

            // Descartes-koordináták sorsolása a kockán belül
            p.p = glm::vec3{
                dist_cube(rng),
                dist_cube(rng),
                dist_cube(rng)
            };

            p.sigma = sigma_v;

            // A kezdeti gradiens kiszámítása kritikus a ráhúzó ág miatt
            p.F_x = surface.grad(p.p);

            floaters.add_particle(p);
        }
    }

    void calculate_particle(Particle<L>& p) {
        p.F = surface.F.at(p.p);
        p.F_x = surface.grad(p.p);
        p.P = glm::vec3{0};
        p.D = 0.0f;
        p.D_sigma = 0.0f;
        p.detah = false;
    }

    float sign(float x) {
        if (x == 0.0f) return 0.0f;
        return x > 0.0f ? 1.0f : -1.0f;
    }

    void witkin(Particle<L>& i, float dt) {
        for (auto& j : floaters.ps()) {
            if (&i == &j ||
                std::abs(j.F) > 5e-1f) continue;
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

        float sigma_update = (i.D_dot / (i.D_sigma + beta)) * dt;
        // Egy lépésben legfeljebb 30%-ot csökkenhet, hogy ne zuhanjon
        // halálküszöb alá azonnali D-spike miatt (pl. egyszerre érkező részecskék).
        sigma_update = std::max(sigma_update, -0.3f * i.sigma);
        i.sigma += sigma_update;
        i.sigma = std::max(i.sigma, 1e-3f);

        if (glm::length(i.F_x) > 1e-6f) {
            i.p_dot =
                i.P -
                    ((glm::dot(i.F_x, i.P) + glm::dot(surface.q_dot, surface.get_F_q(i.p)) + PHI*i.F)
                        /
                    glm::dot(i.F_x, i.F_x)) * i.F_x;
        } else {
            i.p_dot = glm::vec3(0,0,0);
        }

        i.p += i.p_dot * dt;
    }

    void masik(Particle<L>& i, float dt) {
        // Figueiredo-Gomes: a részecske nincs a felületen, rárepítjük
        i.p_dot += i.delta * (-sign(i.F)*i.F_x );
        auto uj_p = i.p + i.p_dot * dt;
        if (surface.F.at(uj_p) * i.F < 0.0f) {
            i.delta /= 2.0f;
            i.p_dot = glm::vec3{0};
        }
        i.p += i.p_dot * dt;
    }

    void simulation(float t, float dt) {
        // bool is_particle_on_surface =
        //     std::any_of(floaters.ps().begin(),
        //                 floaters.ps().end(),
        //                 [this](auto& p) {
        //                     calculate_particle(p);
        //                     return std::abs(p.F) < 1e-6f;
        //                 });
        // is_particle_on_surface = false;
        std::vector<Particle<L>> particles;
        for (auto& i : floaters.ps()) {
            calculate_particle(i);
            if (i.state == ramozog) {
                masik(i, dt);
                if (std::abs(i.F) < 1e-3f) {
                    i.state = rajtamozog;
                }
            }
            if (i.state == rajtamozog) {
                witkin(i, dt);
            }


            float R = dist_R(rng);

            if (i.detah) {
                // halál: van már felületi részecske, ez nem kell
            } else if (glm::length(i.p_dot) < gamma*i.sigma &&
                (i.sigma > sigma_max || (i.D > nu * E_v && i.sigma > sigma_v)))
            {
                // fisszió: csak felületi részecskéknél
                i.sigma /= std::sqrt(2.0f);
                i.delta = 0.01f;  // delta reset a gyerekeknek

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
                i.p_dot = glm::vec3{0};
                particles.push_back(i);

                Particle<L> child = i;
                child.p -= 2.0f * offset;
                particles.push_back(child);

            } else if (
                glm::length(i.p_dot) < gamma*i.sigma &&
                i.sigma < delta*sigma_v &&
                R > i.sigma/(delta*sigma_v))
            {
                // halál: sűrűség alapú eliminálás
            } else {
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



