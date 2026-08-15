#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

#include "Include.hpp"

// Térbeli tájékozódási segédlet:  piros = x,  zöld = y,  kék = z.
//
// Részei:
//   * talajrács a z = 0 síkban (a jelenet "földje": a sík-sablon z=0, a henger a
//     z tengely mentén áll, tehát a z a függőleges irány),
//   * három hosszú tengely, ami a távolban a háttérbe fakul — így ad irányérzetet
//     anélkül, hogy a közeli geometriát elnyomná,
//   * nyílhegy a pozitív végeken, egész értékeknél osztások,
//   * X / Y / Z betűk a nyílhegyeknél, a kamerára billboardozva.
//
// A vastag vonalakat NEM glLineWidth-tel rajzoljuk: a core profil csak az 1.0
// vonalvastagságot garantálja, a többit a driver csendben elnyelheti. Helyette a
// tengelyek kamerára fordított vékony szalagok (két háromszög), így minden gépen
// egyformán néznek ki, és a nyílhegyekkel is konzisztensek.
class Axes : public Model {
    float grid_extent;    // a rács ± kiterjedése (egész egységekben)
    float axis_length;    // a tengelyek fél-hossza (jóval túlnyúlik a rácson)
    float label_dist;     // hol áll a nyílhegy és a betű
    float label_size_px;  // a betűk fél-magassága KÉPPONTBAN

    // Egy rajzolási adag: mód + tartomány a csúcspuffere(ben) + szín.
    struct Batch {
        GLenum    mode;
        int       start;
        int       count;
        glm::vec3 color;
    };
    std::vector<Batch> batches;

    static glm::vec3 fade(glm::vec3 c, float t) {          // fehér háttér felé halványítás
        return c + (glm::vec3(1.0f) - c) * t;
    }

    // Egy világ-pontban mekkora egy képpont világegységben. Ezzel lesznek a
    // segédelemek ÁLLANDÓ KÉPERNYŐ-MÉRETŰEK: közel se hízik el a tengely, távol
    // se tűnik el, és a zoom sem változtatja a vastagságát.
    glm::vec3 eye{0.0f};
    float     world_per_px_unit = 0.001f;   // 1 px / 1 egység távolság

    float px(glm::vec3 at, float pixels) const {
        return pixels * world_per_px_unit * glm::length(at - eye);
    }

    // Kamerára fordított szalag a-tól b-ig, ÁLLANDÓ képernyő-vastagsággal. A két
    // végén más a világ-szélesség (a távolabbi vékonyabb), ezért trapéz lesz.
    void add_ribbon(glm::vec3 a, glm::vec3 b, float half_px, glm::vec3 view_dir) {
        glm::vec3 d = b - a;
        glm::vec3 n = glm::cross(d, view_dir);
        if (glm::dot(n, n) < 1e-12f) return;               // a szalag a nézőirányba mutat
        n = glm::normalize(n);
        glm::vec3 na = n * px(a, half_px);
        glm::vec3 nb = n * px(b, half_px);
        vertices.push_back(a - na); vertices.push_back(b - nb); vertices.push_back(b + nb);
        vertices.push_back(a - na); vertices.push_back(b + nb); vertices.push_back(a + na);
    }

    // Billboardozott nyílhegy a `tip` pontban, szintén képernyő-méretben.
    void add_arrow(glm::vec3 tip, glm::vec3 dir, float len_px, float half_px, glm::vec3 view_dir) {
        glm::vec3 base = tip - dir * px(tip, len_px);
        glm::vec3 n = glm::cross(dir, view_dir);
        if (glm::dot(n, n) < 1e-12f) return;
        n = glm::normalize(n) * px(tip, half_px);
        vertices.push_back(tip); vertices.push_back(base - n); vertices.push_back(base + n);
    }

    // Egy betű vonalszakaszai egységnégyzetben ([-1,1] x [-1,1]), szakaszpáronként.
    static std::vector<glm::vec2> const& glyph(char c) {
        static const std::vector<glm::vec2> X = {
            {-1,-1},{1,1},  {-1,1},{1,-1}
        };
        static const std::vector<glm::vec2> Y = {
            {-1,1},{0,0},  {1,1},{0,0},  {0,0},{0,-1}
        };
        static const std::vector<glm::vec2> Z = {
            {-1,1},{1,1},  {1,1},{-1,-1},  {-1,-1},{1,-1}
        };
        switch (c) { case 'X': return X; case 'Y': return Y; default: return Z; }
    }

    int mark() const { return static_cast<int>(vertices.size()); }

    void render(Camera const& camera) override {
        glm::vec3 const r   = camera.get_right();
        glm::vec3 const u   = camera.get_up();
        glm::vec3 const fwd = camera.get_front();

        eye = camera.get_eye();
        int const win_h = std::max(Window::get_height(), 1);
        world_per_px_unit = 2.0f * std::tan(camera.get_fov_deg() * 0.5f * 3.14159265f / 180.0f)
                          / static_cast<float>(win_h);

        glm::vec3 const axis_color[3] = {
            {0.85f, 0.22f, 0.26f},   // x — piros
            {0.20f, 0.62f, 0.28f},   // y — zöld
            {0.20f, 0.42f, 0.85f},   // z — kék
        };
        glm::vec3 const dir[3] = {{1,0,0}, {0,1,0}, {0,0,1}};
        char      const name[3] = {'X', 'Y', 'Z'};

        vertices.clear();
        batches.clear();

        // ---- 1. talajrács a z = 0 síkban ------------------------------------
        if (show_grid) {
            int  const n = static_cast<int>(grid_extent);
            auto push_grid = [&](bool major) {
                for (int i = -n; i <= n; ++i) {
                    if (i == 0) continue;                       // ezt a tengely rajzolja
                    if ((i % 5 == 0) != major) continue;
                    float f = static_cast<float>(i);
                    vertices.push_back({-grid_extent, f, 0.0f});
                    vertices.push_back({ grid_extent, f, 0.0f});
                    vertices.push_back({f, -grid_extent, 0.0f});
                    vertices.push_back({f,  grid_extent, 0.0f});
                }
            };
            int s = mark(); push_grid(false);
            batches.push_back({GL_LINES, s, mark() - s, {0.90f, 0.90f, 0.92f}});
            s = mark();     push_grid(true);
            batches.push_back({GL_LINES, s, mark() - s, {0.80f, 0.80f, 0.84f}});
        }

        // ---- 2. tengelyek: sávonként halványuló szalagok ---------------------
        // Közel telített, távolabb egyre világosabb — a hosszú tengely így nem
        // nyomja el a modellt, de a távolban is mutatja az irányt.
        // width: FÉL-vastagság képpontban (állandó a képernyőn, zoomtól függetlenül)
        struct Band { float from, to, fade_pos, fade_neg, width; };
        Band const bands[] = {
            {0.0f,        label_dist,       0.00f, 0.45f, 1.8f},
            {label_dist,  axis_length*0.4f, 0.45f, 0.70f, 1.2f},
            {axis_length*0.4f, axis_length, 0.75f, 0.88f, 0.9f},
        };

        for (int a = 0; a < 3; ++a) {
            for (auto const& b : bands) {
                int s = mark();
                add_ribbon(dir[a] * b.from, dir[a] * b.to, b.width, fwd);
                batches.push_back({GL_TRIANGLES, s, mark() - s, fade(axis_color[a], b.fade_pos)});

                s = mark();
                add_ribbon(dir[a] * -b.from, dir[a] * -b.to, b.width, fwd);
                batches.push_back({GL_TRIANGLES, s, mark() - s, fade(axis_color[a], b.fade_neg)});
            }
        }

        // ---- 3. nyílhegy + osztások -----------------------------------------
        for (int a = 0; a < 3; ++a) {
            glm::vec3 tip = dir[a] * label_dist;
            int s = mark();
            add_arrow(tip, dir[a], 16.0f, 5.5f, fwd);
            batches.push_back({GL_TRIANGLES, s, mark() - s, axis_color[a]});

            // egész értékeknél rövid, a nézőre merőleges osztás
            s = mark();
            glm::vec3 tick = glm::cross(dir[a], fwd);
            if (glm::dot(tick, tick) > 1e-12f) {
                tick = glm::normalize(tick);
                for (int i = 1; i < static_cast<int>(label_dist); ++i) {
                    float len_px = (i % 5 == 0) ? 7.0f : 3.5f;
                    for (int sgn = -1; sgn <= 1; sgn += 2) {
                        glm::vec3 p = dir[a] * static_cast<float>(i * sgn);
                        glm::vec3 t = tick * px(p, len_px);
                        vertices.push_back(p - t);
                        vertices.push_back(p + t);
                    }
                }
            }
            batches.push_back({GL_LINES, s, mark() - s, fade(axis_color[a], 0.3f)});
        }

        // ---- 4. betűk a nyílhegyek mögött, a kamerára billboardozva ----------
        // Szintén képernyő-méretben, hogy közelről se hízzanak el, távolról se
        // tűnjenek el.
        for (int a = 0; a < 3; ++a) {
            glm::vec3 center = dir[a] * label_dist + (u + r) * 0.0f;
            float h = px(center, label_size_px);
            center += (r * 0.0f) + u * (h * 2.2f);      // a nyílhegy fölé
            int s = mark();
            for (auto const& pt : glyph(name[a]))
                vertices.push_back(center + (pt.x * h) * r + (pt.y * h) * u);
            batches.push_back({GL_LINES, s, mark() - s, axis_color[a]});
        }

        update_buffers();

        glLineWidth(1.0f);
        for (auto const& b : batches) {
            if (b.count <= 0) continue;
            set_uniform("color", b.color);
            glDrawArrays(b.mode, b.start, b.count);
        }
    }

public:
    // A rács ki/be kapcsolható (a GUI-ból).
    bool show_grid = true;

    explicit Axes(float grid_extent = 8.0f, float axis_length = 60.0f,
                  float label_size_px = 9.0f)
        : grid_extent{grid_extent},
          axis_length{axis_length},
          label_dist{grid_extent * 0.75f},
          label_size_px{label_size_px} {
        update_buffers_on_draw = false; // a render() tölti fel a csúcsokat
        set_shader(Builder::get_or_build(SHADER_DIR "/vertex.vert",
                                         SHADER_DIR "/fragment.glsl"));
    }
};
