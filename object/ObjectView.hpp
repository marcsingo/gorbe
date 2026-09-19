#ifndef GORBE_OBJECT_VIEW_HPP
#define GORBE_OBJECT_VIEW_HPP

#include <cmath>
#include <vector>

#include "../model/Include.hpp"
#include "../particle_sampling/DomainConstraint.hpp"
#include "../particle_sampling/Particle.hpp"
#include "../particle_sampling/ParticleSystem.hpp"
#include "../particle_sampling/ParticleView.hpp"

// ===========================================================================
// Egy térbeli objektum NÉZETE: a modell (ParticleSystem) részecskéit rajzolja ki.
// Csak olvassa a modellt; a GL-erőforrásokat a meglévő Model ősosztály kezeli.
// ===========================================================================

// Részecskék korongokként: minden részecske egy lapos, a felület normálisára
// merőleges korong, 16 háromszögből.
class ParticleDisks : public Model {
    glm::vec3 color;
    // Kövesse-e a megjelenítési hézag-csúszkát (ParticleView::gap)? A KONTROLLPONTOKNÁL
    // nem: azok mérete szándékosan fix, mert az egérrel való elkapásuk sugara is fix —
    // ha a rajzolt korong elszakadna tőle, a felhasználó mellényúlna.
    bool follow_view_gap;
    std::vector<Particle> const* source = nullptr;

    static constexpr int DISK_SEGS = 16;
    static constexpr float TWO_PI  = 6.28318530718f;

protected:
    void render(Camera const&) override {
        if (!source || source->empty()) return;

        // CPU-oldalon korong-háromszögek generálása minden részecskéhez.
        // Minden korong 16 háromszögből áll (fan), összesen 48 csúcs/részecske.
        vertices.clear();
        normals.clear();
        vertices.reserve(source->size() * DISK_SEGS * 3);
        normals.reserve(source->size() * DISK_SEGS * 3);

        for (auto const& p : *source) {
            // A tartomány-feltételen kívüli részecskék még "úton vannak" a jó térrész
            // felé (a felület mentén csúsznak) — azokat nem rajzoljuk ki.
            if (Domain::is_outside(p.dom_dist, p.sigma)) continue;

            glm::vec3 N = (glm::length(p.F_x) > 1e-6f)
                ? glm::normalize(p.F_x)
                : glm::vec3(0.0f, 1.0f, 0.0f);
            glm::vec3 helper = (std::abs(N.x) < 0.9f)
                ? glm::vec3(1.0f, 0.0f, 0.0f)
                : glm::vec3(0.0f, 1.0f, 0.0f);
            glm::vec3 T = glm::normalize(glm::cross(N, helper));
            glm::vec3 B = glm::cross(N, T);

            // A korong sugara alapból σ/2 (ekkor a szomszédok éppen összeérnek); a
            // "Nezet es sugo" panel hézag-csúszkája ezt szűkíti vagy tágítja.
            float const R = follow_view_gap ? ParticleView::disk_radius(p.sigma)
                                            : p.sigma / 2.0f;
            if (R <= 0.0f) continue;

            for (int i = 0; i < DISK_SEGS; i++) {
                float a0 = TWO_PI * float(i)     / float(DISK_SEGS);
                float a1 = TWO_PI * float(i + 1) / float(DISK_SEGS);
                vertices.push_back(p.p);
                vertices.push_back(p.p + R * (std::cos(a0) * T + std::sin(a0) * B));
                vertices.push_back(p.p + R * (std::cos(a1) * T + std::sin(a1) * B));
                // A korong lapos, tehát mindhárom csúcsához a felület normálisa
                // (a már kiszámolt N) tartozik — ettől olvasható térben az alak.
                normals.push_back(N);
                normals.push_back(N);
                normals.push_back(N);
            }
        }

        // Feltöltés a GPU-ra (update_buffers hagyja a VAO-t kötve)
        update_buffers();
        set_uniform("color", color);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertices.size());
    }

public:
    ParticleDisks(glm::vec3 color, bool follow_view_gap)
        : color(color), follow_view_gap(follow_view_gap) {
        update_buffers_on_draw = false;   // a render() tölti fel a csúcsokat
        set_shader(Builder::get_or_build(SHADER_DIR "/vertex.vert",
                                         SHADER_DIR "/fragment.glsl"));
    }

    void draw(std::vector<Particle> const& ps, Camera const& camera) {
        source = &ps;
        Model::draw(camera);
        source = nullptr;
    }
};

class ObjectView {
    // Nem a tiszta (0,0,1) / (1,0,0): a telített alapszínen az árnyalás alig
    // olvasható (a kék csatorna egyedül nem ad elég kontrasztot). Egy kissé
    // világosabb, kevertebb szín viszont szépen mutatja a formát.
    ParticleDisks floaters{{0.20f, 0.45f, 0.90f}, true};
    ParticleDisks controls{{0.90f, 0.27f, 0.25f}, false};

public:
    // Megjelenjen-e (a szimuláció attól még futhat a háttérben).
    bool visible = true;

    void draw(ParticleSystem const& model, Camera const& camera) {
        if (!visible) return;
        floaters.draw(model.particles(), camera);
        controls.draw(model.controls(), camera);
    }
};

#endif //GORBE_OBJECT_VIEW_HPP
