
#include <map>
#include <random>

#include "Particle.hpp"
#include "../model/Include.hpp"
#include "../matek/Kif.hpp"


using namespace Matek::Analizis;

struct Const {
    float delta_t = 0.03f;
    float phi = 15.0f;
    float rho = 15.0f;
    float alpha = 6.0f;
    float E = 0.8f*alpha;
    float beta = 10.0f;
    //szigmák értékei függvények
    float gamma = 4.0f;
    float nu = 0.2f;
    float delta = 0.7f;
};




class ImplicitSurface {
    Circle surface;
    Floaters floaters;
    ControlPoints controls;
public:
    explicit ImplicitSurface(Camera const &camera) :
        floaters{5, {0, 0, 1}, camera},
        controls{10, {1, 0, 0}, camera}
    {
        //véletlen pontok generálása
            int n = 50; // Hány darab pontot szeretnél?
        {
            float spread = 5.0f; // Milyen széles térrészben (pl. -5.0 és +5.0 között)

            // Véletlenszám-generátor inicializálása (Mersenne Twister)
            std::random_device rd;  // Hardveres entrópia a maghoz (seed)
            std::mt19937 gen(rd()); // A tényleges generátor

            // Egyenletes eloszlás a [-spread, spread] intervallumon
            std::uniform_real_distribution<float> dist(-spread, spread);

            for (int i = 0; i < n; ++i) {
                // Generálunk 3 véletlen koordinátát
                float x = dist(gen);
                float y = dist(gen);

                // Ha csak 2D-ben akarod szétszórni őket, a z-t állítsd 0.0f-re!
                float z = 0; //dist(gen);

                // Hozzáadjuk a floaters (vagy controls) tárolóhoz.
                // A dupla kapcsos zárójel azért kell, mert a Particle első adata a glm::vec3 pozíció.
                floaters.add_particle(Particle{{x, y, z}});
            }
        }

         Window::add_time_passed_event([this](auto p) {
            glm::vec3 q_dot = surface.q_dot;

             this->update_floaters(p.dt);
        });

    }

    void update_floaters(float dt) {
        auto& particles = floaters.ps();
        auto q_dot = surface.q_dot;

        for (auto& pp:particles) {
            pp.update_surface_data(surface);
        }
        calculate_repulsion(floaters.ps());
        float alpha = 6.0f;
        float E_hat = 0.8f * alpha;
        float rho = 15.0f;
        float beta = 10.0f;
        float sigma_hat = 0.5f;
        float sigma_max = 1.5f * sigma_hat;
        float gamma = 4.0f;
        float nu = 0.2f;
        float delta_death = 0.7f;

        std::vector<Particle> next_generation;
        next_generation.reserve(particles.size()*2);

        for (auto& pp : particles) {


            auto F_q = surface.get_F_q(pp.p);

            float dot_Fq_qdot = glm::dot(F_q, q_dot);
            float dot_Fx_P = glm::dot(pp.F_x, pp.P);
            float grad_sq = glm::dot(pp.F_x, pp.F_x);


            if (grad_sq > 0.0001f) {
                float lambda = (dot_Fx_P + dot_Fq_qdot + 15.0f * pp.F) / grad_sq;
                pp.p_dot = pp.P - lambda * pp.F_x;
            } else {
                pp.p_dot = glm::vec3{0};
            }

            pp.p += pp.p_dot * dt;

            float D_dot = -rho * (pp.D - E_hat);
            float sigma_dot = D_dot / (pp.D_sigma + beta);
            pp.sigma += sigma_dot * dt;
            pp.sigma = std::clamp(pp.sigma, 0.1f, 3.0f);

            bool survive = true;

            float equilibrium_speed = glm::length(pp.P);

            if (equilibrium_speed < gamma *pp.sigma) {
                if (pp.sigma < delta_death * sigma_hat) {
                    float R = (float)rand() / (float)RAND_MAX;
                    if (R > pp.sigma /(delta_death * sigma_hat)) {
                        survive = false;
                    }
                }

                if (survive &&
                    (pp.sigma > sigma_max ||( pp.D > nu * E_hat && pp.sigma > sigma_hat))) {
                    pp.sigma /= 1.41421f;

                    Particle child = pp;

                    glm::vec3 tangent = glm::vec3(-pp.F_x.y, pp.F_x.x, 0.0f);
                    if (glm::length(tangent) > 0.001f) tangent = glm::normalize(tangent);
                    else tangent = glm::vec3(1.0f, 0, 0);

                    float offset = ((float)rand() / RAND_MAX -0.5f) * pp.sigma;
                    child.p += tangent * offset;

                    next_generation.push_back(child);
                }
            }

            if (survive) {
                next_generation.push_back(pp);
            }
        }

        if (next_generation.empty()) {
            Particle seed;
            seed.p = {surface.q.x, surface.q.y + surface.q.z, 0.0f}; // Kör teteje
            seed.sigma = sigma_hat;
            next_generation.push_back(seed);
        }

        // particles = std::move(next_generation);
    }

    void calculate_repulsion(std::vector<Particle>& particles) {
       for (auto& pp : particles) {
            pp.P = glm::vec3(0.0f);
            pp.D = 0.0f;
            pp.D_sigma = 0.0f;
        }

        for (size_t i = 0; i < particles.size(); i++) {
            for (size_t j = i + 1; j < particles.size(); j++) {

                auto& p1 = particles[i];
                auto& p2 = particles[j];

                glm::vec3 r = p1.p - p2.p; // Vektor p2-ből p1-be
                float dist_sq = glm::dot(r, r);

                float sig1_sq = p1.sigma * p1.sigma;
                float sig2_sq = p2.sigma * p2.sigma;

                if (dist_sq > 9.0f * std::max(sig1_sq, sig2_sq)) continue;

                float E_ij = 0.0f;
                float E_ji = 0.0f;

                if (dist_sq < 9.0f * sig1_sq) {
                    E_ij = 6.0f * std::exp(-dist_sq / (2.0f * sig1_sq));
                    p1.D_sigma += (dist_sq / (p1.sigma * sig1_sq)) * E_ij;
                }

                if (dist_sq < 9.0f * sig2_sq) {
                    E_ji = 6.0f * std::exp(-dist_sq / (2.0f * sig2_sq));
                    p2.D_sigma += (dist_sq / (p2.sigma * sig2_sq)) * E_ji;
                }

               float total_energy = E_ij + E_ji;
                p1.D += total_energy;
                p2.D += total_energy;

                float shared_scalar = (E_ij / sig1_sq) + (E_ji / sig2_sq);

                p1.P += sig1_sq * r * shared_scalar;
                p2.P -= sig2_sq * r * shared_scalar; // Kivonjuk, mert p2-ből nézve a vektor -r
            }
        }
    }


    void draw(const Camera &camera) {
        floaters.draw(camera);
        controls.draw(camera);
    }


};



