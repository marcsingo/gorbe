// GL-teszt: a reszecske-korongok INSTANCINGGAL rajzolva ugyanazt a kepet adjak,
// mint a korabbi, CPU-n felepitett korongok. Valodi (rejtett) ablakot es
// GL-kontextust nyit, ezert kulon kapcsolora fordul (GORBE_BUILD_GL_TESTS).
//
// A regi rajzolast itt, a tesztben tartjuk meg referencianak (OldDisks): a
// kepkockakat pixelenkent vetjuk ossze. Pontos egyezes nem varhato — a korong
// sikja a CPU-n es a GPU-n kulon lebegopontos uton szamolodik, ami az eleken
// egy-egy pixelnyi elterest adhat —, de a lefedett pixelek elhanyagolhato
// resze terhet el.
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include <glad/glad.h>
#include "utils/init.hpp"
#include "model/Window.hpp"
#include "model/Camera.hpp"
#include "model/Framebuffer.hpp"
#include "model/Viewport.hpp"
#include "object/ObjectView.hpp"

static int failures = 0;
static void ok(std::string const& what, bool c, std::string const& info = "") {
    if (!c) ++failures;
    std::printf("  %-50s %-8s %s\n", what.c_str(), c ? "[OK]" : "[HIBA]", info.c_str());
}

// A korabbi rajzolas (a CPU epiti fel reszecskenkent a 16 haromszoget).
class OldDisks : public Model {
    std::vector<Particle> const* source = nullptr;
    glm::vec3 color;
protected:
    void render(Camera const&) override {
        vertices.clear();
        normals.clear();
        for (auto const& p : *source) {
            if (Domain::is_outside(p.dom_dist, p.sigma)) continue;
            glm::vec3 N = (glm::length(p.F_x) > 1e-6f) ? glm::normalize(p.F_x) : glm::vec3(0, 1, 0);
            glm::vec3 helper = (std::abs(N.x) < 0.9f) ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
            glm::vec3 T = glm::normalize(glm::cross(N, helper));
            glm::vec3 B = glm::cross(N, T);
            float const R = ParticleView::disk_radius(p.sigma);
            if (R <= 0.0f) continue;
            for (int i = 0; i < 16; i++) {
                float a0 = 6.28318530718f * float(i) / 16.0f;
                float a1 = 6.28318530718f * float(i + 1) / 16.0f;
                vertices.push_back(p.p);
                vertices.push_back(p.p + R * (std::cos(a0) * T + std::sin(a0) * B));
                vertices.push_back(p.p + R * (std::cos(a1) * T + std::sin(a1) * B));
                normals.push_back(N); normals.push_back(N); normals.push_back(N);
            }
        }
        update_buffers();
        set_uniform("color", color);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertices.size());
    }
public:
    explicit OldDisks(glm::vec3 c) : color(c) {
        update_buffers_on_draw = false;
        set_shader(Builder::get_or_build(SHADER_DIR "/vertex.vert", SHADER_DIR "/fragment.glsl"));
    }
    void draw(std::vector<Particle> const& ps, Camera const& cam) { source = &ps; Model::draw(cam); }
};

static std::vector<unsigned char> render(Framebuffer& fb, int w, int h, auto&& draw) {
    fb.bind();
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    draw();
    std::vector<unsigned char> px(static_cast<std::size_t>(w) * h * 4);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    Framebuffer::unbind();
    return px;
}

int main() {
    int const W = 640, H = 480;
    Utils::init_glfw();
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    Window::init(W, H, "test_gl_disks");
    glEnable(GL_DEPTH_TEST);
    Vp::set_current({0.0f, 0.0f, float(W), float(H)});

    Camera3D cam{glm::vec4(0, 0, W, H), glm::vec3(-9, -9, 7), -90.0f, 45.0f};
    cam.look_at({-9, -9, 7}, glm::vec3(0.0f));

    // Veletlen reszecskek egy gombon, a felulet normalisaval; kozottuk szandekosan
    // nulla normalis (a (0,1,0) tartalek-ag) es a tartomanyon kivuli is.
    std::mt19937 rng(7);
    std::normal_distribution<float> g(0.0f, 1.0f);
    std::vector<Particle> ps(600);
    for (auto& p : ps) {
        glm::vec3 d = glm::normalize(glm::vec3(g(rng), g(rng), g(rng)));
        p.p = 3.0f * d;
        p.F_x = 2.0f * p.p;
        p.sigma = 0.45f;
    }
    ps[0].F_x = glm::vec3(0.0f);
    ps[1].dom_dist = -10.0f;

    {   // a framebuffer az ablak (GL-kontextus) elengedese ELOTT szunjon meg
    Framebuffer fb;
    fb.resize(W, H);

    std::printf("=== Korongok: instancing == a korabbi CPU-s rajzolas ===\n");
    for (float gap : {0.0f, 0.4f}) {
        ParticleView::gap = gap;
        OldDisks old_d({0.20f, 0.45f, 0.90f});
        ParticleDisks new_d({0.20f, 0.45f, 0.90f}, true);
        auto a = render(fb, W, H, [&] { old_d.draw(ps, cam); });
        auto b = render(fb, W, H, [&] { new_d.draw(ps, cam); });

        long covered = 0, differ = 0;
        for (std::size_t i = 0; i < a.size(); i += 4) {
            bool ca = a[i] != 255 || a[i + 1] != 255 || a[i + 2] != 255;
            bool cb = b[i] != 255 || b[i + 1] != 255 || b[i + 2] != 255;
            if (ca || cb) ++covered;
            int dmax = 0;
            for (int k = 0; k < 3; ++k) dmax = std::max(dmax, std::abs(int(a[i + k]) - int(b[i + k])));
            if (dmax > 8) ++differ;
        }
        double ratio = covered ? double(differ) / double(covered) : 1.0;
        char info[96];
        std::snprintf(info, sizeof(info), "lefedett %ld px, elter %ld (%.3f%%)", covered, differ, 100 * ratio);
        ok("hezag " + std::to_string(gap).substr(0, 3) + ": van rajz", covered > 10000, info);
        ok("hezag " + std::to_string(gap).substr(0, 3) + ": a kep lenyegeben azonos", ratio < 0.005, info);
    }
    ParticleView::gap = 0.0f;
    }

    Builder::clear_shader_cache();
    Window::destroy_window();
    std::printf("\n%s (%d hiba)\n", failures ? "SIKERTELEN" : "MINDEN RENDBEN", failures);
    return failures ? 1 : 0;
}
