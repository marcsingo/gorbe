#ifndef GORBE_SCENE_CAMERADOC_HPP
#define GORBE_SCENE_CAMERADOC_HPP

#include <algorithm>
#include <cmath>

#include <glm.hpp>

// ---------------------------------------------------------------------------
// A jelenet KAMERÁJA (futás közben ebből látunk): tiszta adat, GL nélkül.
//
// A szerkesztő kamerája (model/Camera.hpp, Camera3D) a szerkesztéshez való szabad
// kamera; ez viszont a jelenet része, mint egy alakzat: mentődik, a Tulajdonságok
// panelen állítható, és a kód is mozgathatja (gorbe::CameraRef). A GUI-n beállított
// érték itt is KEZDŐÉRTÉK: amit a kód futás közben átír, a futás végén visszaáll.
//
// A szögek RADIÁNBAN, ugyanazzal a Z-up konvencióval, mint a szerkesztő kameráé
// (model/CameraBasis.hpp):
//   rot[0] = yaw   — azimut az xy síkban (0 = +x felé)
//   rot[1] = pitch — emelkedés az xy sík fölött (negatív = lefelé néz), ±89°-ra vágva
//   rot[2] = roll  — dőlés a nézőirány körül (pozitív = a kamera jobb oldala
//                    emelkedik, a kép az óramutató járásával megegyezően fordul)
// ---------------------------------------------------------------------------
struct CameraDoc {
    float pos[3] {0.0f, 0.0f, 0.0f};
    float rot[3] {0.0f, 0.0f, 0.0f};
    float fov = 45.0f;                     // függőleges látószög, FOKBAN

    static constexpr float PITCH_LIMIT = 1.5533430f;   // 89°: a pólusokon nincs "jobbra"
    static constexpr float FOV_MIN = 5.0f, FOV_MAX = 120.0f;

    struct Basis { glm::vec3 front, right, up; };

    // Alapból ugyanonnan néz, ahonnan a szerkesztő kamera indul (App::DEFAULT_EYE),
    // az origóra — így az első Indítás nem ugrik másik nézetbe.
    CameraDoc() { look_at({-14.0f, -14.0f, 16.0f}, {0.0f, 0.0f, 0.0f}); }

    glm::vec3 position() const { return {pos[0], pos[1], pos[2]}; }
    float     fov_deg() const  { return std::clamp(fov, FOV_MIN, FOV_MAX); }

    Basis basis() const {
        float const yaw = rot[0];
        float const pitch = std::clamp(rot[1], -PITCH_LIMIT, PITCH_LIMIT);
        Basis b;
        b.front = glm::normalize(glm::vec3{std::cos(yaw) * std::cos(pitch),
                                           std::sin(yaw) * std::cos(pitch),
                                           std::sin(pitch)});
        glm::vec3 const r = glm::normalize(glm::cross(b.front, glm::vec3{0.0f, 0.0f, 1.0f}));
        glm::vec3 const u = glm::cross(r, b.front);
        float const c = std::cos(rot[2]), s = std::sin(rot[2]);
        b.right = c * r + s * u;              // dőlés a nézőirány körül
        b.up    = -s * r + c * u;
        return b;
    }

    // Az `eye` pontba áll, és a `target` felé fordul (a dőlést megtartja).
    void look_at(glm::vec3 eye, glm::vec3 target) {
        pos[0] = eye.x; pos[1] = eye.y; pos[2] = eye.z;
        look_towards(target - eye);
    }

    // Csak elfordul az irány felé (a helye marad).
    void look_towards(glm::vec3 d) {
        if (glm::dot(d, d) < 1e-12f) return;
        d = glm::normalize(d);
        rot[0] = std::atan2(d.y, d.x);
        rot[1] = std::clamp(std::asin(std::clamp(d.z, -1.0f, 1.0f)), -PITCH_LIMIT, PITCH_LIMIT);
    }

    bool operator==(CameraDoc const&) const = default;
};

#endif //GORBE_SCENE_CAMERADOC_HPP
