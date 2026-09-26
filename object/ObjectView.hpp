#ifndef GORBE_OBJECT_VIEW_HPP
#define GORBE_OBJECT_VIEW_HPP

#include <cmath>
#include <cstddef>
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
// merőleges korong, 16 háromszögből — INSTANCINGGAL.
//
// A közös egységkorong egyszer kerül a GPU-ra (a Model csúcspufferébe); frame-enként
// részecskénként csak a középpont, a normális és a sugár megy fel (7 float), a
// korong síkját a vertex shader (disk.vert) számolja. Korábban a CPU építette fel
// részecskénként a 48 csúcsot és 48 normálist: 4000 részecskénél mérve 2.1 ms/frame
// és 4.6 MB/frame feltöltés; így ~0.1 MB.
class ParticleDisks : public Model {
    glm::vec3 color;
    // Kövesse-e a megjelenítési hézag-csúszkát (ParticleView::gap)? A KONTROLLPONTOKNÁL
    // nem: azok mérete szándékosan fix, mert az egérrel való elkapásuk sugara is fix —
    // ha a rajzolt korong elszakadna tőle, a felhasználó mellényúlna.
    bool follow_view_gap;
    std::vector<Particle> const* source = nullptr;

    static constexpr int DISK_SEGS = 16;
    static constexpr float TWO_PI  = 6.28318530718f;

    // Részecskénként (instance): középpont (3), normális (3), sugár (1).
    struct Instance { glm::vec3 center; glm::vec3 normal; float radius; };
    std::vector<Instance> instances;
    GLuint     inst_vbo = 0;
    GLsizeiptr inst_capacity = 0;   // a GPU-n lefoglalt méret (bájt)

protected:
    void render(Camera const&) override {
        if (!source || source->empty()) return;

        instances.clear();
        instances.reserve(source->size());
        for (auto const& p : *source) {
            // A tartomány-feltételen kívüli részecskék még "úton vannak" a jó térrész
            // felé (a felület mentén csúsznak) — azokat nem rajzoljuk ki.
            if (Domain::is_outside(p.dom_dist, p.sigma)) continue;

            // A korong sugara alapból σ/2 (ekkor a szomszédok éppen összeérnek); a
            // "Nezet es sugo" panel hézag-csúszkája ezt szűkíti vagy tágítja.
            float const R = follow_view_gap ? ParticleView::disk_radius(p.sigma)
                                            : p.sigma / 2.0f;
            if (R <= 0.0f) continue;
            instances.push_back({p.p, p.F_x, R});
        }
        if (instances.empty()) return;

        // Feltöltés: amíg belefér a lefoglalt pufferbe, csak felülírjuk.
        GLsizeiptr const bytes = static_cast<GLsizeiptr>(instances.size() * sizeof(Instance));
        glBindBuffer(GL_ARRAY_BUFFER, inst_vbo);
        if (bytes > inst_capacity) {
            glBufferData(GL_ARRAY_BUFFER, bytes, instances.data(), GL_DYNAMIC_DRAW);
            inst_capacity = bytes;
        } else {
            glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, instances.data());
        }
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        set_uniform("color", color);
        glDrawArraysInstanced(GL_TRIANGLES, 0, DISK_SEGS * 3, static_cast<GLsizei>(instances.size()));
    }

public:
    ParticleDisks(glm::vec3 color, bool follow_view_gap)
        : color(color), follow_view_gap(follow_view_gap) {
        set_shader(Builder::get_or_build(SHADER_DIR "/disk.vert", SHADER_DIR "/fragment.glsl"));

        // A közös egységkorong: 16 háromszög-legyező a z=0 síkban, sugár 1.
        for (int i = 0; i < DISK_SEGS; i++) {
            float a0 = TWO_PI * float(i)     / float(DISK_SEGS);
            float a1 = TWO_PI * float(i + 1) / float(DISK_SEGS);
            vertices.push_back({0.0f, 0.0f, 0.0f});
            vertices.push_back({std::cos(a0), std::sin(a0), 0.0f});
            vertices.push_back({std::cos(a1), std::sin(a1), 0.0f});
        }
        update_buffers();                 // egyszer; a VAO ezután kötve marad
        update_buffers_on_draw = false;

        // A részecskénkénti attribútumok ugyanabba a VAO-ba, osztóval (divisor 1):
        // ezek instance-onként lépnek, nem csúcsonként.
        glGenBuffers(1, &inst_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, inst_vbo);
        auto attr = [](GLuint loc, GLint n, std::size_t offset) {
            glVertexAttribPointer(loc, n, GL_FLOAT, GL_FALSE, sizeof(Instance),
                                  reinterpret_cast<void*>(offset));
            glEnableVertexAttribArray(loc);
            glVertexAttribDivisor(loc, 1);
        };
        attr(2, 3, offsetof(Instance, center));
        attr(3, 3, offsetof(Instance, normal));
        attr(4, 1, offsetof(Instance, radius));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    ~ParticleDisks() override { glDeleteBuffers(1, &inst_vbo); }

    void draw(std::vector<Particle> const& ps, Camera const& camera) {
        source = &ps;
        Model::draw(camera);
        source = nullptr;
    }
};

// A kontrollpontok: fix méretű, árnyalt kockák. A húzott kocka más színű.
//
// Kevés pont van, ezért a csúcsokat egyszerűen a CPU rakja össze (kockánként 36);
// a húzott kocka csúcsai kerülnek a végére, így két rajzolással más színt kaphat.
class ControlCubes : public Model {
    std::vector<glm::vec3> const* source = nullptr;
    int highlighted = -1;

    // A kocka fél élhossza. Kisebb, mint az elkapási sugár (CONTROL_RADIUS): a kocka ne
    // takarja az alakzatot, de a megfogásához ne kelljen pontosan rákattintani.
    static constexpr float CUBE_HALF = 0.05f;

    static inline glm::vec3 const COLOR{0.90f, 0.27f, 0.25f};
    static inline glm::vec3 const DRAGGED_COLOR{1.00f, 0.80f, 0.20f};

    // Egy kocka hat lapja: a normális és a lap két élvektora (a sarkok ±1 egységben).
    static void add_cube(std::vector<glm::vec3>& v, std::vector<glm::vec3>& n, glm::vec3 c, float h) {
        struct Face { glm::vec3 N, U, V; };
        static Face const F[6] = {
            {{ 1, 0, 0}, {0, 1, 0}, {0, 0, 1}}, {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}},
            {{ 0, 1, 0}, {0, 0, 1}, {1, 0, 0}}, {{ 0,-1, 0}, {1, 0, 0}, {0, 0, 1}},
            {{ 0, 0, 1}, {1, 0, 0}, {0, 1, 0}}, {{ 0, 0,-1}, {0, 1, 0}, {1, 0, 0}},
        };
        for (auto const& f : F) {
            glm::vec3 const o = c + h * f.N;
            glm::vec3 const a = o + h * (-f.U - f.V), b = o + h * (f.U - f.V);
            glm::vec3 const d = o + h * (f.U + f.V), e = o + h * (-f.U + f.V);
            for (glm::vec3 q : {a, b, d, a, d, e}) { v.push_back(q); n.push_back(f.N); }
        }
    }

protected:
    void render(Camera const&) override {
        if (!source || source->empty()) return;
        vertices.clear();
        normals.clear();
        float const h = CUBE_HALF;
        for (int i = 0; i < static_cast<int>(source->size()); ++i)
            if (i != highlighted) add_cube(vertices, normals, (*source)[static_cast<std::size_t>(i)], h);
        GLsizei const normal_count = static_cast<GLsizei>(vertices.size());
        if (highlighted >= 0 && highlighted < static_cast<int>(source->size()))
            add_cube(vertices, normals, (*source)[static_cast<std::size_t>(highlighted)], h);

        update_buffers();
        set_uniform("color", COLOR);
        glDrawArrays(GL_TRIANGLES, 0, normal_count);
        if (static_cast<GLsizei>(vertices.size()) > normal_count) {
            set_uniform("color", DRAGGED_COLOR);
            glDrawArrays(GL_TRIANGLES, normal_count, static_cast<GLsizei>(vertices.size()) - normal_count);
        }
    }

public:
    ControlCubes() {
        update_buffers_on_draw = false;   // a render() tölti fel a csúcsokat
        set_shader(Builder::get_or_build(SHADER_DIR "/vertex.vert", SHADER_DIR "/fragment.glsl"));
    }

    void draw(std::vector<glm::vec3> const* pts, int dragged, Camera const& camera) {
        source = pts;
        highlighted = dragged;
        Model::draw(camera);
        source = nullptr;
    }
};

class ObjectView {
    // Nem a tiszta (0,0,1): a telített alapszínen az árnyalás alig olvasható (a kék
    // csatorna egyedül nem ad elég kontrasztot). Egy kissé világosabb, kevertebb szín
    // viszont szépen mutatja a formát.
    ParticleDisks floaters{{0.20f, 0.45f, 0.90f}, true};
    ControlCubes  controls;

public:
    // Megjelenjen-e (a szimuláció attól még futhat a háttérben).
    bool visible = true;

    // `dragged`: a húzott kontrollpont indexe (kiemelve), vagy -1. `pts`: ha nem
    // nullptr, ezeket rajzolja kontrollpontként (a variációs szerkesztő kényszerei).
    void draw(ParticleSystem const& model, Camera const& camera, int dragged = -1,
              std::vector<glm::vec3> const* pts = nullptr) {
        if (!visible) return;
        floaters.draw(model.particles(), camera);
        controls.draw(pts ? pts : model.controls(), dragged, camera);
    }
};

#endif //GORBE_OBJECT_VIEW_HPP
