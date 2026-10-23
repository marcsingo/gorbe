// GL-teszt: a GLSL-kiírás (scene/Glsl.hpp, Matek::Analizis::glsl_text) a VALÓDI GPU-n.
// A generált kód lefordul, és pontonként ugyanazt adja, mint a CPU-s kiértékelés —
// a transzformációval, warppal, CSG-vel, saját függvénnyel, `t`-vel, tartománnyal,
// negatív alapú hatvánnyal, a függvénytábla minden függvényével és egy variációs
// felülettel. A paraméterek (és a `t`) uniformként, újrafordítás nélkül hatnak.
// Rejtett ablakot nyit (GORBE_BUILD_GL_TESTS).
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include <glad/glad.h>
#include "utils/init.hpp"
#include "model/Window.hpp"
#include "particle_sampling/ParticleSystem.hpp"
#include "scene/Build.hpp"
#include "scene/Glsl.hpp"
#include "model/Camera.hpp"
#include "model/RayView.hpp"
#include "model/Viewport.hpp"

static int failures = 0;
static void ok(std::string const& what, bool c, std::string const& info = "") {
    if (!c) ++failures;
    std::printf("  %-55s %-8s %s\n", what.c_str(), c ? "[OK]" : "[HIBA]", info.c_str());
}

static GLuint compile(GLenum type, std::string const& src) {
    GLuint s = glCreateShader(type);
    char const* p = src.c_str();
    glShaderSource(s, 1, &p, nullptr);
    glCompileShader(s);
    GLint okc = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &okc);
    if (!okc) {
        char log[4096];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::printf("--- forditasi hiba:\n%s\n", log);
    }
    return s;
}

// A GPU-n kiértékelő "számológép": soronként egy alakzat (F, majd a tartomány),
// oszloponként egy mintapont; az eredmény egy R32F textúra.
struct Evaluator {
    GLuint prog = 0, fbo = 0, out = 0, pts = 0, vao = 0;
    int n = 0, rows = 0;
    bool compiled = false;

    Evaluator(Glsl::Code const& code, std::vector<glm::vec3> const& points) {
        n = static_cast<int>(points.size());
        rows = 2 * code.shapes;
        std::string fs = "#version 330 core\nuniform sampler2D u_pts;\nout float val;\n" + code.source +
                         "void main() {\n    ivec2 c = ivec2(gl_FragCoord.xy);\n"
                         "    vec3 q = texelFetch(u_pts, ivec2(c.x, 0), 0).xyz;\n    val = 0.0;\n";
        for (int i = 0; i < code.shapes; ++i) {
            fs += "    if (c.y == " + std::to_string(2 * i) + ") val = gp_F" + std::to_string(i) + "(q.x, q.y, q.z);\n";
            fs += "    if (c.y == " + std::to_string(2 * i + 1) + ") val = gp_D" + std::to_string(i) + "(q.x, q.y, q.z);\n";
        }
        fs += "}\n";
        GLuint const vs = compile(GL_VERTEX_SHADER,
            "#version 330 core\nvoid main() {\n"
            "    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);\n"
            "    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n}\n");
        GLuint const f = compile(GL_FRAGMENT_SHADER, fs);
        prog = glCreateProgram();
        glAttachShader(prog, vs);
        glAttachShader(prog, f);
        glLinkProgram(prog);
        GLint linked = 0;
        glGetProgramiv(prog, GL_LINK_STATUS, &linked);
        compiled = linked != 0;

        glGenTextures(1, &pts);
        glBindTexture(GL_TEXTURE_2D, pts);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, n, 1, 0, GL_RGB, GL_FLOAT, points.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glGenTextures(1, &out);
        glBindTexture(GL_TEXTURE_2D, out);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, n, rows, 0, GL_RED, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, out, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glGenVertexArrays(1, &vao);
    }

    // A paraméterek pillanatnyi értékével (újrafordítás nélkül) számol.
    std::vector<float> run(Glsl::Code const& code) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, n, rows);
        glDisable(GL_DEPTH_TEST);
        glUseProgram(prog);
        std::vector<float> const v = Glsl::values(code);
        glUniform1fv(glGetUniformLocation(prog, "u_p"), static_cast<GLsizei>(v.size()), v.data());
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, pts);
        glUniform1i(glGetUniformLocation(prog, "u_pts"), 0);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        std::vector<float> res(static_cast<std::size_t>(n) * rows);
        glReadPixels(0, 0, n, rows, GL_RED, GL_FLOAT, res.data());
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return res;
    }
};

static Shape& shape(SceneDoc& sc, char const* n, char const* f) {
    auto& s = sc.shapes.emplace_back();
    std::snprintf(s.name, sizeof(s.name), "%s", n);
    std::snprintf(s.formula, sizeof(s.formula), "%s", f);
    return s;
}
static Param& param(std::list<Param>& l, char const* n, float v) {
    auto& p = l.emplace_back();
    std::snprintf(p.name, sizeof(p.name), "%s", n);
    p.value = v;
    return p;
}

int main() {
    Utils::init_glfw();
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    Window::init(64, 64, "test_gl_glsl");
    using Matek::Analizis::Kif;

    std::list<Param> program;
    SceneDoc sc;
    auto& g = sc.funcs.emplace_back();
    std::snprintf(g.name, 32, "g"); std::snprintf(g.params, 64, "u, k = 1"); std::snprintf(g.body, 256, "u^2 + k");
    param(sc.params, "s", 0.7f);

    Shape& gomb = shape(sc, "gomb", "x^2 + y^2 + z^2 - r^2");
    Param& r = param(gomb.locals, "r", 1.5f);
    gomb.xform.pos[0] = 2.0f; gomb.xform.rot[2] = 0.5f; gomb.xform.rot[0] = -0.3f; gomb.xform.scale[1] = 1.4f;

    Shape& torusz = shape(sc, "torusz", "(x^2 + y^2 + z^2 + R^2 - r^2)^2 - 4*R^2*(x^2 + y^2)");
    param(torusz.locals, "R", 2.0f); param(torusz.locals, "r", 0.6f);
    Warp w; std::snprintf(w.fx, 192, "x*cos(s*z) + y*sin(s*z)"); std::snprintf(w.fy, 192, "-x*sin(s*z) + y*cos(s*z)");
    torusz.warps.push_back(w);

    Shape& csg = shape(sc, "csg", "sunio(gomb, g(z) - 4 + sin(t), 0.5)");
    std::snprintf(csg.domain, 256, "x > -1 and z < 1.5 or not y > 2");

    shape(sc, "hatvanyok", "x^2 + x^3 + (x - 1)^5 - (2 + y^2)^(-2) + (1 + z^2)^0.7 + y^(-1)*0 + z^4");
    shape(sc, "fuggvenyek1",
          "sin(x) + cos(y) + tan(0.3*z) + ctg(1 + 0.2*sin(y)) + asin(0.3*sin(x)) + acos(0.3*cos(y))"
          " + atan(z) + atan2(y, x + 5) + sinh(0.5*x) + cosh(0.5*y) + tanh(z)");
    shape(sc, "fuggvenyek2",
          "exp(0.3*z) + ln(1 + x^2) + log(2 + y^2) + sqrt(1 + z^2) + cbrt(x*y) + abs(x) + sign(y)"
          " + floor(1.3*z) + ceil(0.7*x) + round(0.37*y) + min(x, y) + max(y, z)");
    shape(sc, "makrok1",
          "pow(x - 1, 3) + hypot(x, y) + length(x, y, z) + clamp(x, -1, 1) + mod(x, 1.5) + fract(y)"
          " + step(0.2, z) + smoothstep(-1, 1, x) + mix(x, y, 0.3)");
    shape(sc, "makrok2",
          "unio(x, y) + metszet(y, z) + kulonbseg(x, z) + smin(x, y) + smax(y, z, 0.3) + smetszet(x, z)"
          " + skulonbseg(y, x)");

    // Variációs felület (egy ellipszoid részecskéiből), elhelyezve.
    {
        ParticleSystem src({}, 9);
        src.d = 1.0f;
        src.surface().set_tree(Matek::Analizis::make_kif("x^2/2 + y^2 + z^2 - 1").get());
        src.restart();
        for (int i = 0; i < 300; ++i) src.step(0.03f);
        Shape& v = shape(sc, "vari", "x");
        v.vari = std::make_shared<Variational>(Variational::from_particles(src.particles(), {0, 0, 0}, 0.03f));
        v.vari->solve();
        v.xform.pos[2] = 0.5f; v.xform.rot[1] = 0.4f;
        std::printf("(variacios felulet: %zu kenyszer)\n", v.vari->centers.size());
    }

    std::string const err = Build::build(sc, program);
    ok("felepul", err.empty(), err);
    Glsl::Code const code = Glsl::generate(sc);
    std::printf("(GLSL: %zu karakter, %zu parameter, %d alakzat)\n", code.source.size(), code.params.size(), code.shapes);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> u(-3.0f, 3.0f);
    std::vector<glm::vec3> pts(256);
    for (auto& p : pts) p = {u(rng), u(rng), u(rng)};

    Evaluator ev(code, pts);
    ok("a generalt GLSL lefordul es linkel", ev.compiled);
    {
        // A textúrás változat (a sugárkövetőé): a variációs felület egy 3D textúrát olvas,
        // a doboza a felület kényszereit bőven körülveszi; a teljes jelenet így is lefordul.
        Glsl::Code const tex = Glsl::generate(sc, true);
        bool box = tex.volumes.size() == 1;
        if (box)
            for (glm::vec3 p : tex.volumes[0].v->centers)
                box &= glm::all(glm::greaterThan(p, tex.volumes[0].lo)) && glm::all(glm::lessThan(p, tex.volumes[0].hi));
        ok("textura-mod: egy terfogat, a doboz a kenyszerek korul", box);
        ok("... a kodban textura-olvasas, nincs osszeg", tex.source.find("sampler3D") != std::string::npos &&
           tex.source.find("gp_cube(length(") == std::string::npos);
        RayView all;
        DocCamera cam(sc.scene_camera);
        all.draw(sc, cam);
        ok("... a sugarkoveto a variacios alakzattal egyutt lefordul", all.error.empty(), all.error);
    }

    // Összevetés a CPU-val (relatív tűrés; ahol a CPU sem véges, az kimarad).
    auto compare = [&](char const* when) {
        std::vector<float> const gpu = ev.run(code);
        for (int i = 0; i < code.shapes; ++i) {
            Shape const& s = *code.shape_of[static_cast<std::size_t>(i)];
            float worst = 0.0f, worst_d = 0.0f;
            int checked = 0;
            for (int k = 0; k < ev.n; ++k) {
                float const cpu = Kif(s.tree).at(pts[static_cast<std::size_t>(k)]);
                float const gf = gpu[static_cast<std::size_t>(2 * i * ev.n + k)];
                if (!std::isfinite(cpu)) continue;
                ++checked;
                worst = std::max(worst, std::abs(gf - cpu) / (1.0f + std::abs(cpu)));
                if (s.dom_tree) {
                    float const cd = Kif(s.dom_tree).at(pts[static_cast<std::size_t>(k)]);
                    float const gd = gpu[static_cast<std::size_t>((2 * i + 1) * ev.n + k)];
                    worst_d = std::max(worst_d, std::abs(gd - cd) / (1.0f + std::abs(cd)));
                }
            }
            std::string const what = std::string(when) + ": " + s.name + (s.dom_tree ? " (+ tartomany)" : "");
            ok(what, checked > 200 && worst < 2e-3f && worst_d < 2e-3f,
               "max rel. elteres " + std::to_string(worst) + (s.dom_tree ? " / " + std::to_string(worst_d) : "") +
               ", " + std::to_string(checked) + " pont");
        }
    };
    compare("GPU = CPU");

    // Élő paraméterek: az r, a jelenet `s`-e, a pozíció és a `t` — újrafordítás NÉLKÜL.
    r.value = 2.2f;
    sc.params.front().value = 1.3f;
    gomb.xform.pos[1] = -1.0f;
    SceneTime::value = 1.7f;
    compare("parameter/pozicio/t valtozas utan");

    std::printf("=== Sugarkovetes (model/RayView.hpp): egy 64x64-es kep a GPU-n ===\n");
    {
        int const W = 64;
        GLuint fbo, color, depth;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glGenTextures(1, &color);
        glBindTexture(GL_TEXTURE_2D, color);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, W, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
        glGenRenderbuffers(1, &depth);
        glBindRenderbuffer(GL_RENDERBUFFER, depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, W, W);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
        Vp::set_current({0.0f, 0.0f, static_cast<float>(W), static_cast<float>(W)});

        SceneDoc s2;
        Shape& g = shape(s2, "g", "x^2 + y^2 + z^2 - r^2");
        Param& rr = param(g.locals, "r", 1.0f);
        Build::build(s2, program);
        s2.scene_camera.look_at({0.0f, -5.0f, 0.0f}, {0.0f, 0.0f, 0.0f});   // +y felé néz, jobbra = +x
        s2.scene_camera.fov = 45.0f;
        DocCamera cam(s2.scene_camera);
        RayView rays;

        struct Px { unsigned char r, g, b, a; };
        auto render = [&] {
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glViewport(0, 0, W, W);
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClearDepth(1.0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST);
            rays.draw(s2, cam);
            std::vector<Px> img(static_cast<std::size_t>(W) * W);
            glReadPixels(0, 0, W, W, GL_RGBA, GL_UNSIGNED_BYTE, img.data());
            return img;
        };
        auto drawn = [&](std::vector<Px> const& img, int x, int y) { return img[static_cast<std::size_t>(y * W + x)].a > 0; };

        auto img = render();
        ok("lefordul", rays.error.empty(), rays.error);
        ok("kozepen a gomb, a sarokban a hatter", drawn(img, 32, 32) && !drawn(img, 2, 2));
        // r = 1, 5 egységről, 45°-os látószöggel a sziluett sugara ~15 px.
        ok("r = 1: 20 px-re a kozeptol mar hatter", !drawn(img, 52, 32));
        float dz = 0.0f;
        glReadPixels(32, 32, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &dz);
        glm::vec4 const clip = cam.get_matrix() * glm::vec4(0.0f, -1.0f, 0.0f, 1.0f);   // a gömb elülső pontja
        float const expect = clip.z / clip.w * 0.5f + 0.5f;
        ok("a melyseg a gomb eleje (a kamera elott 4 egyseg)", std::abs(dz - expect) < 2e-4f,
           std::to_string(dz) + " / " + std::to_string(expect));

        rr.value = 1.5f;                                   // élő paraméter: újrafordítás nélkül
        img = render();
        ok("r = 1.5: nagyobb, ujraforditas nelkul", drawn(img, 52, 32) && rays.compiles == 1,
           std::to_string(rays.compiles) + " forditas");

        std::snprintf(g.domain, sizeof(g.domain), "x > 0");    // szerkezet: új fa -> új shader
        Build::build(s2, program);
        img = render();
        ok("tartomany x > 0: a bal fele nem latszik, a jobb igen", !drawn(img, 25, 32) && drawn(img, 40, 32) &&
           rays.compiles == 2, std::to_string(rays.compiles) + " forditas");

        g.visible = false;
        img = render();
        ok("lathatatlan: nem rajzolodik", !drawn(img, 40, 32));
        g.visible = true;

        // Két alakzat: a közelebbi (y = -2.5) takarja a távolabbit; más a színük.
        g.domain[0] = 0;
        g.color_idx = 0;
        Shape& elol = shape(s2, "elol", "(x)^2 + (y + 2.5)^2 + z^2 - 0.3^2");
        elol.color_idx = 5;
        Build::build(s2, program);
        img = render();
        Px const c = img[32 * W + 32];
        glm::vec3 const blue = Raytrace::palette_color(5);
        ok("ket alakzat: kozepen a kozelebbi (az o szinevel)", c.b > c.r && blue.b > blue.r,
           std::to_string(c.r) + "," + std::to_string(c.g) + "," + std::to_string(c.b));

        // Variációs felület (egy 1 sugarú gömb részecskéiből), 3D textúrából: ugyanúgy a
        // kép közepén, a mélység a gömb elülső pontjáé (a rács felbontásáig).
        SceneDoc s3;
        {
            ParticleSystem src({}, 4);
            src.d = 0.8f;
            src.surface().set_tree(Matek::Analizis::make_kif("x^2 + y^2 + z^2 - 1").get());
            src.restart();
            for (int i = 0; i < 300; ++i) src.step(0.03f);
            Shape& v = shape(s3, "v", "x");
            v.vari = std::make_shared<Variational>(Variational::from_particles(src.particles(), {0, 0, 0}, 0.03f));
            v.vari->solve();
        }
        Build::build(s3, program);
        s3.scene_camera = s2.scene_camera;
        DocCamera cam3(s3.scene_camera);
        RayView vr;
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, W, W);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        vr.draw(s3, cam3);
        std::vector<Px> vimg(static_cast<std::size_t>(W) * W);
        glReadPixels(0, 0, W, W, GL_RGBA, GL_UNSIGNED_BYTE, vimg.data());
        ok("variacios (3D texturabol): lefordul, kozepen latszik, a sarokban nem",
           vr.error.empty() && drawn(vimg, 32, 32) && !drawn(vimg, 2, 2) && !drawn(vimg, 52, 32), vr.error);
        glReadPixels(32, 32, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &dz);
        // Az elülső pont mélysége ~ y = -1 (a variációs felület a gömböt közelíti, a
        // textúra a rács felbontásáig): kb. 0.05 egységen belül.
        glm::vec4 const near_ = cam3.get_matrix() * glm::vec4(0.0f, -1.05f, 0.0f, 1.0f);
        glm::vec4 const far_  = cam3.get_matrix() * glm::vec4(0.0f, -0.95f, 0.0f, 1.0f);
        float const dn = near_.z / near_.w * 0.5f + 0.5f, df = far_.z / far_.w * 0.5f + 0.5f;
        ok("... a melyseg a gomb eleje (y = -1 +- 0.05)", dz >= dn && dz <= df,
           std::to_string(dz) + " in [" + std::to_string(dn) + ", " + std::to_string(df) + "]");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    Window::destroy_window();
    std::printf(failures ? "\n%d HIBA\n" : "\nminden rendben\n", failures);
    return failures ? 1 : 0;
}
