#pragma once
#include <algorithm>
#include <cmath>

#include "Include.hpp"
#include "Viewport.hpp"
#include "../scene/CameraDoc.hpp"

// A jelenet kamerája (scene/CameraDoc.hpp) a SZERKESZTŐ nézetben: egy gúla, aminek a
// csúcsa a kamera helye, az alapja a nézőirányban álló téglalap (a látószöggel és a
// nézet képarányával), és egy kis háromszög jelzi, merre van a kép "fel" iránya (így
// a dőlés is látszik). Futás közben, a jelenet kamerájából nézve nem rajzoljuk.
class CameraGizmo : public Model {
    CameraDoc const* doc = nullptr;

    void render(Camera const&) override {
        if (!doc) return;
        CameraDoc::Basis const b = doc->basis();
        glm::vec3 const eye = doc->position();
        float const len = 1.6f;                                   // a gúla hossza (egység)
        float const h = len * std::tan(doc->fov_deg() * 0.5f * 3.14159265f / 180.0f);
        float const w = h * Vp::aspect(Vp::current());
        glm::vec3 const c = eye + b.front * len;
        glm::vec3 const corner[4] = {c - b.right * w - b.up * h, c + b.right * w - b.up * h,
                                     c + b.right * w + b.up * h, c - b.right * w + b.up * h};
        vertices.clear();
        for (int i = 0; i < 4; ++i) {
            vertices.push_back(eye);       vertices.push_back(corner[i]);              // élek
            vertices.push_back(corner[i]); vertices.push_back(corner[(i + 1) % 4]);    // alap
        }
        glm::vec3 const top = c + b.up * (h * 1.6f);                                   // "fel"
        vertices.push_back(corner[3]); vertices.push_back(top);
        vertices.push_back(top);       vertices.push_back(corner[2]);
        update_buffers();
        glLineWidth(1.0f);
        set_uniform("color", glm::vec3{0.85f, 0.45f, 0.10f});
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices.size()));
    }

public:
    CameraGizmo() {
        update_buffers_on_draw = false;   // a render() tölti fel a csúcsokat
        set_shader(Builder::get_or_build(SHADER_DIR "/vertex.vert", SHADER_DIR "/fragment.glsl"));
    }

    void draw_doc(CameraDoc const& d, Camera const& view) {
        doc = &d;
        draw(view);
    }
};
