// A kamera Z-up bazisa es az egerkezeles ELOJEL-konvencioja.
//
// Ez a teszt azert van, mert a Y-up -> Z-up atallaskor a vizszintes forgatas
// eszrevetlenul INVERZ lett: a fuggoleges jonak tunt, a vizszintes viszont
// ellenkezo iranyba forgatott. A "huzd jobbra -> nezz jobbra" fajta allitasokat
// szemmel nehez ellenorizni, ezert itt szamszerusitve vannak.
#include <cmath>
#include <cstdio>
#include <string>

#include "model/CameraBasis.hpp"

static int failures = 0;
static float norm(glm::vec3 v) { return std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z); }

static void ok(std::string const& what, bool c, std::string const& info = "") {
    if (!c) ++failures;
    std::printf("  %-52s %-8s %s\n", what.c_str(), c ? "[OK]" : "[HIBA]", info.c_str());
}
static void near(std::string const& what, float got, float exp, float tol = 1e-4f) {
    bool c = std::isfinite(got) && std::abs(got - exp) <= tol;
    if (!c) ++failures;
    std::printf("  %-52s %-8s got=%9.5f exp=%9.5f\n", what.c_str(), c ? "[OK]" : "[HIBA]", got, exp);
}

int main() {
    using namespace CameraBasis;

    std::printf("=== 1. Z-up bazis alapallasban (yaw=0, pitch=0) ===\n");
    {
        Basis b = from_angles(0.0f, 0.0f);
        near("front x", b.front.x, 1.0f);  near("front y", b.front.y, 0.0f);  near("front z", b.front.z, 0.0f);
        // +x fele nezve, +z felfele: a jobb kez fele -y
        near("right x", b.right.x, 0.0f);  near("right y", b.right.y, -1.0f); near("right z", b.right.z, 0.0f);
        near("up z (a Z a fuggoleges)", b.up.z, 1.0f);
    }

    std::printf("\n=== 2. A bazis ortonormalt es jobbsodrasu (tobb szognel) ===\n");
    {
        float worst_ortho = 0.0f, worst_len = 0.0f, worst_hand = 0.0f;
        for (float yaw = -170.0f; yaw <= 170.0f; yaw += 37.0f)
            for (float pitch = -85.0f; pitch <= 85.0f; pitch += 23.0f) {
                Basis b = from_angles(yaw, pitch);
                worst_ortho = std::max({worst_ortho,
                    std::abs(glm::dot(b.front, b.right)),
                    std::abs(glm::dot(b.front, b.up)),
                    std::abs(glm::dot(b.right, b.up))});
                worst_len = std::max({worst_len,
                    std::abs(norm(b.front) - 1.0f),
                    std::abs(norm(b.right) - 1.0f),
                    std::abs(norm(b.up) - 1.0f)});
                // A bazist up = right x front szerint epitjuk, amibol
                //     right x up = right x (right x front) = -front
                // kovetkezik. Ez a szokasos nezet-bazis: szemkoordinataban a kamera a
                // -z fele nez, tehat (right, up, -front) a jobbsodrasu harmas.
                worst_hand = std::max(worst_hand, norm(glm::cross(b.right, b.up) + b.front));
                worst_hand = std::max(worst_hand, norm(glm::cross(b.right, b.front) - b.up));
            }
        ok("merolegesek", worst_ortho < 1e-5f, "max |dot| = " + std::to_string(worst_ortho));
        ok("egysegnyi hosszuak", worst_len < 1e-5f, "max hiba = " + std::to_string(worst_len));
        ok("konzisztens nezet-bazis (right x up = -front)", worst_hand < 1e-5f, "max hiba = " + std::to_string(worst_hand));
        // a fuggoleges mindig "felfele" mutat, sosem billen at
        for (float yaw = -180.0f; yaw <= 180.0f; yaw += 45.0f)
            for (float pitch = -85.0f; pitch <= 85.0f; pitch += 17.0f)
                if (from_angles(yaw, pitch).up.z <= 0.0f) {
                    ok("a horizont sosem billen at", false,
                       "yaw=" + std::to_string(yaw) + " pitch=" + std::to_string(pitch));
                    goto done;
                }
        ok("a horizont sosem billen at", true);
        done:;
    }

    std::printf("\n=== 3. EGERKEZELES: az iranyoknak egyezniuk kell ===\n");
    {
        // Tobb kiindulo szognel is: a huzas iranya es a nezet elfordulasa egyezzen.
        float worst_h = 1e30f, worst_v = 1e30f;
        for (float yaw0 = -150.0f; yaw0 <= 150.0f; yaw0 += 50.0f)
            for (float pitch0 = -40.0f; pitch0 <= 40.0f; pitch0 += 20.0f) {
                Basis b0 = from_angles(yaw0, pitch0);

                // JOBBRA huzas (dx > 0) -> a nezet a `right` fele fordul
                {
                    float yaw = yaw0, pitch = pitch0;
                    apply_mouse_delta(10.0f, 0.0f, 0.1f, yaw, pitch);
                    glm::vec3 f1 = from_angles(yaw, pitch).front;
                    worst_h = std::min(worst_h, glm::dot(f1 - b0.front, b0.right));
                }
                // FELFELE huzas (dy < 0, mert a kepernyo y lefele no) -> felfele nezunk
                {
                    float yaw = yaw0, pitch = pitch0;
                    apply_mouse_delta(0.0f, -10.0f, 0.1f, yaw, pitch);
                    glm::vec3 f1 = from_angles(yaw, pitch).front;
                    worst_v = std::min(worst_v, glm::dot(f1 - b0.front, b0.up));
                }
            }
        ok("jobbra huzas -> jobbra fordul a nezet", worst_h > 1e-4f,
           "min vetulet = " + std::to_string(worst_h));
        ok("felfele huzas -> felfele fordul a nezet", worst_v > 1e-4f,
           "min vetulet = " + std::to_string(worst_v));
    }

    std::printf("\n=== 4. A ket irany UGYANUGY viselkedik (ez volt a hiba) ===\n");
    {
        // A hibas allapotban a vizszintes vetulet NEGATIV volt, a fuggoleges pozitiv.
        // Itt megkoveteljuk, hogy a ket irany egyforma elojelu es nagysagrendu legyen.
        Basis b0 = from_angles(30.0f, -20.0f);

        float yaw = 30.0f, pitch = -20.0f;
        apply_mouse_delta(10.0f, 0.0f, 0.1f, yaw, pitch);
        float h = glm::dot(from_angles(yaw, pitch).front - b0.front, b0.right);

        yaw = 30.0f; pitch = -20.0f;
        apply_mouse_delta(0.0f, -10.0f, 0.1f, yaw, pitch);
        float v = glm::dot(from_angles(yaw, pitch).front - b0.front, b0.up);

        ok("mindket irany POZITIV vetuletu", h > 0.0f && v > 0.0f,
           "vizszintes=" + std::to_string(h) + " fuggoleges=" + std::to_string(v));
        ok("azonos nagysagrend (nincs elojelfordulas)",
           std::abs(h - v) < 0.25f * std::max(std::abs(h), std::abs(v)) + 1e-3f,
           "kulonbseg=" + std::to_string(std::abs(h - v)));

        // BALRA / LEFELE huzasnal pontosan az ellenkezoje
        yaw = 30.0f; pitch = -20.0f;
        apply_mouse_delta(-10.0f, 0.0f, 0.1f, yaw, pitch);
        float h2 = glm::dot(from_angles(yaw, pitch).front - b0.front, b0.right);
        yaw = 30.0f; pitch = -20.0f;
        apply_mouse_delta(0.0f, 10.0f, 0.1f, yaw, pitch);
        float v2 = glm::dot(from_angles(yaw, pitch).front - b0.front, b0.up);
        ok("balra huzas -> balra fordul",   h2 < 0.0f, "vetulet=" + std::to_string(h2));
        ok("lefele huzas -> lefele fordul", v2 < 0.0f, "vetulet=" + std::to_string(v2));
    }

    std::printf("\n=== 5. A pitch levagasa (a polusokon a right kiszamithatatlan) ===\n");
    {
        float yaw = 0.0f, pitch = 0.0f;
        for (int i = 0; i < 200; ++i) apply_mouse_delta(0.0f, -50.0f, 0.1f, yaw, pitch);
        near("felfele nem megy 89 fok fole", pitch, 89.0f);
        for (int i = 0; i < 400; ++i) apply_mouse_delta(0.0f, 50.0f, 0.1f, yaw, pitch);
        near("lefele nem megy -89 fok ala", pitch, -89.0f);
    }

    std::printf("\n%s (%d hiba)\n", failures ? ">>> SIKERTELEN" : ">>> MINDEN TESZT OK", failures);
    return failures != 0;
}
