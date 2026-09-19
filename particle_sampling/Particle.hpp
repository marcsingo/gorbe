#ifndef GORBE_PARTICLE_HPP
#define GORBE_PARTICLE_HPP

#include <glm.hpp>

// Egy részecske ÁLLAPOTA — tiszta adat, se GL, se ablak. A szimuláció
// (ParticleSystem) írja, a nézet (object/ObjectView.hpp) csak olvassa.

enum ParticleState {
    ramozog,      // még repül a felület felé (Figueiredo-Gomes ráhúzó ág)
    rajtamozog    // a felületen csúszik (Witkin-lépés)
};

struct Particle {
    glm::vec3 p {0};
    glm::vec3 p_dot{0};
    glm::vec3 P{0};
    glm::vec3 F_x{0};

    ParticleState state = ramozog;

    float F = 0.0f;
    float F_t = 0.0f; // dF/dt — a felület saját mozgása (ha a képlet függ a `t` időtől)
    float K = 0.0f;   // a felület közepes görbülete a részecske helyén (surface.curvature)

    // Tartomány-feltétel a részecske helyén: dom > 0 = megfelelő térrészben van.
    // Feltétel nélküli felületnél dom_dist végig +végtelen, tehát mindig "bent van".
    // Lásd particle_sampling/DomainConstraint.hpp.
    float     dom      = 1.0f;      // a feltétel nyers értéke
    glm::vec3 dom_x{0};             // ∇dom
    glm::vec3 dom_g{0};             // ∇dom felület menti (érintőirányú) része
    float     dom_dist = 1e30f;     // előjeles geometriai távolság a peremtől

    float sigma{10.0f};
    float D = 0.0f;
    float D_dot = 0.0f;
    float D_sigma = 0.0f;
    float delta = 0.01f;
    bool detah = false;
};

#endif //GORBE_PARTICLE_HPP
