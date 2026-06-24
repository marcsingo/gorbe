#ifndef GORBE_IMPLICITSURFACE_HPP
#define GORBE_IMPLICITSURFACE_HPP

#include <map>
#include <random>
#include <cmath>

#include "Particle.hpp"
#include "../model/Include.hpp"
#include "../matek/Kif.hpp"
#include "Occluders.hpp"


using namespace Matek::Analizis;


// A részecske-szimuláció hangolható paraméterei. A main-ből opcionálisan átadható
// az ImplicitSurface / App.show() hívásnak; ha nem adsz semmit, az alapértékek
// (a cikk eredeti beállításai) lépnek életbe.
struct SimParams {
    float d        = 4.0f;   // jellemző méretskála
    float alpha    = 6.0f;   // tűrési energia erőssége
    float sigma    = 1.0f;
    float phi      = 15.0f;  // felületre-húzó visszacsatolás
    float beta     = 10.0f;  // sigma-csillapítás
    float gamma    = 4.0f;   // egyensúly-küszöb a fisszióhoz/halálhoz
    float nu       = 0.2f;   // fisszió sűrűség-küszöbe
    float delta    = 0.7f;   // halál sűrűség-küszöbe
    float fraction = 0.001f;
};


// SurfaceT: a megjelenítendő implicit felület típusa (Sphere, Torus, Ellipsoid, ...).
// Az L paraméterszámot és a hozzá tartozó occludert automatikusan levezetjük, így
// a main-ben elég a felület típusát megadni.
template<class SurfaceT>
class ImplicitSurface {
    static constexpr size_t L = SurfaceT::param_count;
    using Occluder = typename OccluderFor<SurfaceT>::type;

    SurfaceT surface;
    Floaters<L> floaters;
    ControlPoints<L> controls;
    Occluder sphere_mesh;

    std::mt19937 rng;
    std::uniform_real_distribution<float> dist_R;

    // A szimuláció fut-e (alapból igen, hogy a show<T>() változatlanul működjön),
    // és a fix-lépésű integrátor időakkumulátora.
    bool  running   = true;
    float sim_accum = 0.0f;
public:
    explicit ImplicitSurface( Camera const &camera, SimParams params = {}) :
        floaters{5, {0, 0, 1}, camera},
        controls{10, {1, 0, 0}, camera},
        sphere_mesh{surface, {0.85f, 0.85f, 0.85f}, camera},
        rng(std::random_device{}()),
        dist_R(0.0f, 1.0f),
        d(params.d), alpha(params.alpha), sigma(params.sigma), PHI(params.phi),
        E_v(0.8f * params.alpha), rho(params.phi), beta(params.beta), gamma(params.gamma),
        nu(params.nu), delta(params.delta), fraction(params.fraction)
    {
        // Egyetlen kezdő részecske — a globális fisszió (4.2 fejezet) ebből épít fel
        // egyenletes mintavételt anélkül, hogy előre el kellene helyezni a pontokat.
        // Particle<L> p0;
        // p0.p     = glm::vec3{surface.q.x, surface.q.y + surface.q.w, surface.q.z};
        // p0.sigma = sigma_max;
        // p0.F_x   = surface.grad(p0.p); // kezdeti normális a korong-rendereléshez
        // floaters.add_particle(p0);

        controls.set_surface(&surface);
        // A felület mostantól folyamatosan a saját átmérőjét írja a d-be.
        surface.bind_diameter(&d);
        spawn_random_particles(2, 3);

        Window::add_time_passed_event([this](auto p) {
            if (!running) return;            // leállított állapotban nem szimulálunk
            sim_accum += p.dt;
            if (sim_accum >= 0.03f) {
                this->simulation(p.t, sim_accum);
                sim_accum = 0.0f;
            }
        });

    }

    // --- Futásidejű vezérlés (egyetlen, állandó életű példányhoz) ----------------
    // A felület eseménykezelői (Window::add_*_event) a konstruktorban, egyszer
    // regisztrálódnak és erre a példányra mutatnak. Ezért a szimulációt NEM a példány
    // megsemmisítésével/újraépítésével indítjuk-állítjuk (az dangling lambdákat hagyna),
    // hanem ezekkel a kapcsolókkal.

    SurfaceT&       get_surface()       { return surface; }
    SurfaceT const& get_surface() const { return surface; }

    bool is_running() const { return running; }
    void stop()  { running = false; }

    // Új futás: friss részecskékkel, futó állapotban. set_equation() UTÁN hívandó,
    // mert a kezdő gradiensekhez már az új F kell.
    void restart() {
        floaters.ps().clear();
        controls.ps().clear();
        sim_accum = 0.0f;
        spawn_random_particles(2, 3);
        running = true;
    }

    // Minden részecske törlése és leállítás (üres, álló jelenet).
    void clear() {
        running = false;
        floaters.ps().clear();
        controls.ps().clear();
        sim_accum = 0.0f;
    }

    // Az alakzat aktuális átmérője. NEM const: a felület (Surface) folyamatosan
    // ide írja a valódi átmérőt (lásd surface.bind_diameter(&d) a konstruktorban),
    // így a belőle számolt skálák (sigma_v, sigma_max) követik a felület változását.
    float d;

    // d-ből származó, ezért menet közben is helyes méretskálák.
    float sigma_v()   const { return d / 4.0f; }
    float sigma_max() const { return std::max(d / 2.0f, 1.5f * sigma_v()); }

    // Az átmérőt (d) alapból a felület folyamatosan felülírja a valódi átmérőjével
    // (lásd surface.bind_diameter(&d) a konstruktorban). Ezzel kézi vezérlésre lehet
    // váltani: ekkor a d szabadon állítható (pl. ImGui-csúszkáról), a felület már
    // nem írja felül. Visszakapcsolva újra a felület átmérőjét követi.
    void set_manual_diameter(bool on) { surface.bind_diameter(on ? nullptr : &d); }

    // Értéküket a konstruktor init-listája adja a SimParams-ból (lásd fentebb).
    float const alpha;
    float const sigma;
    float const PHI;
    float const E_v;
    float const rho;
    float const beta;
    float const gamma;
    float const nu;
    float const delta;
    float const fraction;

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

            p.sigma = sigma_v();

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

    // A felülettől mért közelítő GEOMETRIAI távolság: |F| / |∇F| (elsőrendű, Taubin).
    // A nyers |F| skálafüggő (a tórusz kvartikus F-je a felülettől 0.05-re már ~5),
    // ezért a felület-közelség küszöböket erre normáljuk, hogy minden alakzatnál
    // ugyanazt jelentsék.
    static float surface_distance(Particle<L> const& p) {
        float g = glm::length(p.F_x);
        return g > 1e-6f ? std::abs(p.F) / g : std::abs(p.F);
    }

    void witkin(Particle<L>& i, float dt) {
        for (auto& j : floaters.ps()) {
            if (&i == &j ||
                surface_distance(j) > 5e-1f) continue;
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
                // Geometriai közelség (nem nyers F): minden alakzatnál ugyanazt jelenti.
                if (surface_distance(i) < 1e-2f) {
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
                (i.sigma > sigma_max() || (i.D > nu * E_v && i.sigma > sigma_v())))
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
                i.sigma < delta*sigma_v() &&
                R > i.sigma/(delta*sigma_v()))
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

#endif //GORBE_IMPLICITSURFACE_HPP

