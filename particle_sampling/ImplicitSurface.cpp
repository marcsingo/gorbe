
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



template<size_t L>
class ImplicitSurface {
    Circle surface;
    Floaters<L> floaters;
    ControlPoints<L> controls;

    std::mt19937 rng;
    std::uniform_real_distribution<float> dist_R;
public:
    explicit ImplicitSurface(Camera const &camera) :
        floaters{5, {0, 0, 1}, camera},
        controls{10, {1, 0, 0}, camera},
    rng(std::random_device{}()),
    dist_R(0.0f, 1.0f)
    {
        //véletlen pontok generálása
            int n = 20; // Hány darab pontot szeretnél?
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

                auto p = Particle<3>{{x, y, z}};
                p.sigma = sigma_max;

                // Hozzáadjuk a floaters (vagy controls) tárolóhoz.
                // A dupla kapcsos zárójel azért kell, mert a Particle első adata a glm::vec3 pozíció.
                controls.add_particle(p);
            }
        }

         Window::add_time_passed_event([this](auto p) {
            glm::vec3 q_dot = surface.q_dot;
                static float dt = 0;
             dt += p.dt;

             if (dt >= 0.003f) {
             this->simulation(p.t, p.dt);
                 dt = 0.0f;
             } else {

                 dt += p.dt;
             }
        });

    }

    float const d = 5.0f;

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
    float const fraction = 0.1f;

    void calculate_particle(Particle<L>& p) {
        p.F = surface.F.at(p.p);
        p.F_x = surface.grad(p.p);
        p.P = glm::vec3{0};
        p.D = 0.0f;
        p.D_sigma = 0.0f;
    }

    void simulation(float t, float dt) {
        std::vector<Particle<L>> particles;
        for (auto& i : controls.ps()) {
            calculate_particle(i);

            for (auto& j : controls.ps()) {
                if (&i == &j) continue;
                auto r = i.p - j.p;
                auto E_ij = alpha*std::exp(-glm::dot(r, r) / (i.sigma*i.sigma*2));
                auto E_ji = alpha*std::exp(-glm::dot(-r, -r) / (j.sigma*j.sigma*2));
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

                auto rand_dir = glm::normalize(glm::vec3{
                    dist_R(rng) - 0.5f,
                    dist_R(rng) - 0.5f,
                    dist_R(rng) - 0.5f
                });

                auto offset = rand_dir * fraction * i.sigma;
                i.p += offset;
                particles.push_back(i);


                Particle<L> child = i;
                child.p -= 2.0f* offset;
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
        controls.ps() = particles;
    }

    void draw(const Camera &camera) {
        floaters.draw(camera);
        controls.draw(camera);
    }


};



