#pragma once
#include "Include.hpp"

// Koordináta-tengelyek a térben:  piros = x,  zöld = y,  kék = z.
// Az origóból a +irányba húzott vonalak (GL_LINES), a tengelyek végén pedig
// X / Y / Z betűcímkék — szintén vonalakból, a kamerára billboardozva (mindig
// szemből látszanak). Mindent külön rajzhívás színez, mert a fragment-shader
// egyetlen `color` uniformmal dolgozik.
class Axes : public Model {
    float length;
    float label_size; // a betűk fél-magassága világegységben

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

    void render(Camera const& camera) override {
        glm::vec3 r = camera.get_right();
        glm::vec3 u = camera.get_up();

        vertices.clear();
        // 3 tengely (6 csúcs)
        vertices.push_back({0,0,0}); vertices.push_back({length,0,0}); // x
        vertices.push_back({0,0,0}); vertices.push_back({0,length,0}); // y
        vertices.push_back({0,0,0}); vertices.push_back({0,0,length}); // z

        // Betűk a tengelyvégeknél, kicsit túlnyúlva, a kamera síkjába billboardozva.
        glm::vec3 tips[3]    = {{length,0,0}, {0,length,0}, {0,0,length}};
        char      names[3]   = {'X', 'Y', 'Z'};
        int label_start[3], label_count[3];
        for (int a = 0; a < 3; ++a) {
            glm::vec3 center = tips[a] + glm::normalize(tips[a]) * (2.5f * label_size);
            auto const& g = glyph(names[a]);
            label_start[a] = (int)vertices.size();
            for (auto const& pt : g)
                vertices.push_back(center + (pt.x * label_size) * r + (pt.y * label_size) * u);
            label_count[a] = (int)vertices.size() - label_start[a];
        }
        update_buffers();

        glm::vec3 colors[3] = {{1,0,0}, {0,1,0}, {0,0,1}}; // x=piros, y=zöld, z=kék
        glLineWidth(2.0f);
        for (int a = 0; a < 3; ++a) {
            set_uniform("color", colors[a]);
            glDrawArrays(GL_LINES, 2 * a, 2);                       // tengely
            glDrawArrays(GL_LINES, label_start[a], label_count[a]); // címke
        }
    }

public:
    explicit Axes(float length = 5.0f, float label_size = 0.3f)
        : length{length}, label_size{label_size} {
        update_buffers_on_draw = false; // a render() tölti fel a csúcsokat
        Builder::ShaderBuilder builder;
        set_shader(builder
            .add_vertex_shader  (SHADER_DIR "/vertex.vert")
            .add_fragment_shader(SHADER_DIR "/fragment.glsl")
            .build());
    }
};
