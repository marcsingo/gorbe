// Mérés (nem teszt): a GPU-s sugárkövetés (model/RayView.hpp) képkocka-ideje tipikus
// jeleneteken, három felbontásban, befoglaló térfogat nélkül. A GPU-időt a GL saját
// időmérő lekérdezése adja (GL_TIME_ELAPSED), 20 képkocka átlagában.
//
//   cmake -B <dir> -DGORBE_BUILD_GL_TESTS=ON && cmake --build <dir> --target bench_raytrace
//   <dir>/bench_raytrace
#include <algorithm>
#include <cstdio>
#include <iterator>
#include <functional>
#include <string>
#include <vector>

#include <glad/glad.h>
#include "utils/init.hpp"
#include "model/Window.hpp"
#include "model/Camera.hpp"
#include "model/RayView.hpp"
#include "model/Viewport.hpp"
#include "scene/Build.hpp"

static Shape& shape(SceneDoc& sc, char const* n, char const* f, float x = 0, float y = 0, float z = 0) {
    auto& s = sc.shapes.emplace_back();
    std::snprintf(s.name, sizeof(s.name), "%s", n);
    std::snprintf(s.formula, sizeof(s.formula), "%s", f);
    s.xform.pos[0] = x; s.xform.pos[1] = y; s.xform.pos[2] = z;
    s.color_idx = static_cast<int>(sc.shapes.size()) % 8;
    return s;
}

struct Target {
    GLuint fbo = 0, color = 0, depth = 0;
    int w = 0, h = 0;
    void resize(int W, int H) {
        if (fbo) { glDeleteFramebuffers(1, &fbo); glDeleteTextures(1, &color); glDeleteRenderbuffers(1, &depth); }
        w = W; h = H;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glGenTextures(1, &color);
        glBindTexture(GL_TEXTURE_2D, color);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
        glGenRenderbuffers(1, &depth);
        glBindRenderbuffer(GL_RENDERBUFFER, depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    }
};

int main() {
    Utils::init_glfw();
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    Window::init(64, 64, "bench_raytrace");
    std::printf("GPU: %s\n\n", reinterpret_cast<char const*>(glGetString(GL_RENDERER)));

    std::list<Param> program;
    struct Case { char const* name; std::function<void(SceneDoc&)> make; };
    std::vector<Case> cases = {
        {"1 gomb", [](SceneDoc& s) { shape(s, "a", "x^2 + y^2 + z^2 - 4"); }},
        {"1 torusz", [](SceneDoc& s) { shape(s, "a", "(x^2 + y^2 + z^2 + 9 - 1)^2 - 36*(x^2 + y^2)"); }},
        {"1 csavart ellipszoid (warp)", [](SceneDoc& s) {
            Shape& a = shape(s, "a", "x^2/9 + y^2 + z^2/4 - 1");
            Warp w; std::snprintf(w.fx, 192, "x*cos(0.6*z) + y*sin(0.6*z)"); std::snprintf(w.fy, 192, "-x*sin(0.6*z) + y*cos(0.6*z)");
            a.warps.push_back(w);
            a.xform.rot[2] = 0.4f; }},
        {"CSG: sima unio, 2 gomb", [](SceneDoc& s) {
            shape(s, "a", "sunio((x-1)^2 + y^2 + z^2 - 2.25, (x+1)^2 + y^2 + z^2 - 2.25, 0.8)"); }},
        {"3 egyszeru alakzat", [](SceneDoc& s) {
            shape(s, "a", "x^2 + y^2 + z^2 - 2", -4, 0, 0);
            shape(s, "b", "(x^2 + y^2 + z^2 + 4 - 0.36)^2 - 16*(x^2 + y^2)", 0, 0, 0);
            Shape& c = shape(s, "c", "x^2 + y^2 - 1", 4, 0, 0); std::snprintf(c.domain, 256, "z > -2 and z < 2"); }},
        {"8 alakzat (vegyes)", [](SceneDoc& s) {
            for (int i = 0; i < 8; ++i) {
                float const x = -6.0f + 4.0f * static_cast<float>(i % 4), y = i < 4 ? -2.5f : 2.5f;
                if (i % 2) shape(s, "t", "(x^2 + y^2 + z^2 + 1 - 0.09)^2 - 4*(x^2 + y^2)", x, y, 0);
                else       shape(s, "g", "x^2 + y^2 + z^2 - 1.2", x, y, 0);
            } }},
    };
    struct Res { char const* name; int w, h; };
    Res const res[] = {{"960x540", 960, 540}, {"1280x720", 1280, 720}, {"1920x1080", 1920, 1080}};

    // Két beállítás: a CPU-s sugárkövetőé (max. lépés 2, t_max 200), és a RayView
    // alapértelmezése, a "gyors" (max. lépés 8, t_max 100 — a kamera úgyis csak 100-ig
    // lát). Az utóbbinál azt is mérjük, hány pixelen más a kép (találat / nem találat).
    // (A variációs felületet a RayView 3D textúrából rajzolja — lásd RayView::bake.)
    RayView::Settings base{};
    base.max_step = 2.0f;
    base.t_max = 200.0f;
    RayView::Settings const fast{};

    GLuint q;
    glGenQueries(1, &q);
    Target tgt;
    // A két beállítást FELVÁLTVA mérjük (képkockánként), és a mediánt vesszük: a GPU-t
    // más is használhatja (böngésző, videó), és a laptop órajele is változhat — így a
    // zaj mindkettőt egyformán éri, és egy-egy kiugró képkocka nem torzít.
    std::printf("%-30s", "jelenet");
    for (auto const& r : res) std::printf("  %34s", r.name);
    std::printf("\n%-30s", "");
    for (std::size_t i = 0; i < std::size(res); ++i) std::printf("  %34s", "alap -> gyors (elterő pixel)");
    std::printf("\n");
    {
        int const pass = 0; (void)pass;
        for (auto const& cs : cases) {
            SceneDoc sc;
            cs.make(sc);
            std::string const err = Build::build(sc, program);
            if (!err.empty()) { std::printf("%s: %s\n", cs.name, err.c_str()); continue; }
            sc.scene_camera.look_at({-9.0f, -11.0f, 7.0f}, {0.0f, 0.0f, 0.0f});
            sc.scene_camera.fov = 45.0f;
            DocCamera cam(sc.scene_camera);
            RayView rays;
            std::printf("%-30s", cs.name);
            for (auto const& r : res) {
                tgt.resize(r.w, r.h);
                Vp::set_current({0.0f, 0.0f, static_cast<float>(r.w), static_cast<float>(r.h)});
                auto frame = [&] {
                    glBindFramebuffer(GL_FRAMEBUFFER, tgt.fbo);
                    glViewport(0, 0, r.w, r.h);
                    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                    glEnable(GL_DEPTH_TEST);
                    rays.draw(sc, cam);
                };
                auto mask = [&] {
                    std::vector<unsigned char> px(static_cast<std::size_t>(r.w) * r.h * 4);
                    glReadPixels(0, 0, r.w, r.h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
                    std::vector<bool> m(static_cast<std::size_t>(r.w) * r.h);
                    for (std::size_t i = 0; i < m.size(); ++i) m[i] = px[i * 4 + 3] > 0;
                    return m;
                };
                auto timed = [&](RayView::Settings const& st) {
                    rays.settings = st;
                    glBeginQuery(GL_TIME_ELAPSED, q);
                    frame();
                    glEndQuery(GL_TIME_ELAPSED);
                    GLuint64 ns = 0;
                    glGetQueryObjectui64v(q, GL_QUERY_RESULT, &ns);
                    return static_cast<double>(ns) * 1e-6;
                };
                rays.settings = base;
                for (int i = 0; i < 5; ++i) frame();           // bemelegítés (+ a shader fordítása)
                glFinish();
                std::vector<bool> const ref = mask();
                rays.settings = fast;
                frame();
                glFinish();
                std::vector<bool> const now = mask();
                std::vector<double> tb, tf;
                for (int i = 0; i < 15; ++i) { tb.push_back(timed(base)); tf.push_back(timed(fast)); }
                auto median = [](std::vector<double> v) { std::sort(v.begin(), v.end()); return v[v.size() / 2]; };
                double const mb = median(tb), mf = median(tf);
                std::size_t diff = 0, hits = 0;
                for (std::size_t i = 0; i < now.size(); ++i) { diff += now[i] != ref[i]; hits += ref[i]; }
                char cell[64];
                std::snprintf(cell, sizeof(cell), "%6.1f -> %6.1f ms (%.2f%%)", mb, mf,
                              hits ? 100.0 * static_cast<double>(diff) / static_cast<double>(hits) : 0.0);
                std::printf("  %34s", cell);
                if (hits == 0)
                    std::printf(" [nincs talalat! hiba: '%s', forditas: %zu, gl hiba: 0x%x, forras: %zu kar., fa: %d]",
                                rays.error.c_str(), rays.compiles, glGetError(), Glsl::generate(sc).source.size(),
                                sc.shapes.front().tree ? 1 : 0);
                if (!rays.error.empty()) std::printf(" HIBA: %s", rays.error.c_str());
            }
            std::printf("\n");
        }
    }
    Window::destroy_window();
    return 0;
}
