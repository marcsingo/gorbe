#ifndef GORBE_MODEL_RAYVIEW_HPP
#define GORBE_MODEL_RAYVIEW_HPP

#include <string>
#include <vector>

#include <glad/glad.h>
#include <gtc/type_ptr.hpp>

#include "Camera.hpp"
#include "Viewport.hpp"
#include "../raytrace/Palette.hpp"
#include "../scene/Glsl.hpp"

// ---------------------------------------------------------------------------
// GPU-s sugárkövetés: a jelenet alakzatai TISZTÁN az implicit egyenletükből, befoglaló
// térfogat nélkül. Egy teljes képernyős háromszög, amelynek a fragment shadere
// pixelenként menetel a sugár mentén (scene/Glsl.hpp adja az alakzatok gp_F / gp_D
// függvényeit).
//
// A menetelés a CPU-s sugárkövetőé (raytrace/Raytracer.hpp, march): a lépés a becsült
// távolság (|F| / |∇F|) egy része, korlátok között; ha F előjelet vált, felezéssel
// pontosítunk, és a találat csak a tartomány-feltételen belül érvényes. A gradiens
// numerikus (3 extra kiértékelés). Több alakzatnál a sugár a már megtalált legközelebbi
// találatig keres tovább (ami mögötte van, úgysem látszik).
//
// A shader csak a SZERKEZET változásakor fordul újra (amikor a felépített fák
// lecserélődnek); a paraméterek, a pozíció és a `t` képkockánként uniformként mennek fel.
// A mélységet is kiírja, így a sugárkövetett kép a többi rajzolttal helyesen takar.
// ---------------------------------------------------------------------------
class RayView {
public:
    // A menetelés beállításai (a CPU-s sugárkövetőéi, raytrace::Settings).
    struct Settings {
        float t_min = 0.02f, t_max = 200.0f;
        int   max_steps = 300;
        float min_step = 1e-3f, max_step = 2.0f, step_safety = 0.75f;
    };
    Settings settings;

    std::string error;           // az utolsó fordítás hibája (üres: rendben)
    std::size_t compiles = 0;    // hányszor fordult újra (a tesztnek)

    RayView() = default;
    RayView(RayView const&) = delete;
    RayView& operator=(RayView const&) = delete;
    ~RayView() { release(); }

    // A felépített jelenet rajzolása a kamerából. Ha a fák lecserélődtek (Indítás,
    // előnézet), előbb újragenerálja és lefordítja a shadert.
    void draw(SceneDoc const& sc, Camera const& cam) {
        sync(sc);
        if (!prog_ || code_.shapes == 0) return;

        GLint vp[4];
        glGetIntegerv(GL_VIEWPORT, vp);
        glm::mat4 const m = cam.get_matrix();
        glm::mat4 const inv = glm::inverse(m);
        glm::vec3 const eye = cam.get_eye();

        std::vector<float> const p = Glsl::values(code_);
        std::vector<glm::vec3> colors;
        std::vector<float> visible;
        for (Shape const* s : code_.shape_of) {
            colors.push_back(Raytrace::palette_color(s->color_idx));
            visible.push_back(s->visible ? 1.0f : 0.0f);
        }

        glUseProgram(prog_);
        glUniform1fv(loc("u_p"), static_cast<GLsizei>(p.size()), p.data());
        glUniform3fv(loc("u_color"), static_cast<GLsizei>(colors.size()), glm::value_ptr(colors[0]));
        glUniform1fv(loc("u_visible"), static_cast<GLsizei>(visible.size()), visible.data());
        glUniformMatrix4fv(loc("u_vp"), 1, GL_FALSE, glm::value_ptr(m));
        glUniformMatrix4fv(loc("u_inv_vp"), 1, GL_FALSE, glm::value_ptr(inv));
        glUniform3fv(loc("u_eye"), 1, glm::value_ptr(eye));
        glUniform2f(loc("u_res"), static_cast<float>(vp[2]), static_cast<float>(vp[3]));
        glUniform2f(loc("u_t"), settings.t_min, settings.t_max);
        glUniform3f(loc("u_step"), settings.min_step, settings.max_step, settings.step_safety);
        glUniform1i(loc("u_max_steps"), settings.max_steps);

        glBindVertexArray(vao_);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);
    }

private:
    GLuint prog_ = 0, vao_ = 0;
    Glsl::Code code_;
    std::vector<void const*> built_;   // melyik fákból készült a shader

    GLint loc(char const* name) const { return glGetUniformLocation(prog_, name); }

    void release() {
        if (prog_) glDeleteProgram(prog_);
        if (vao_) glDeleteVertexArrays(1, &vao_);
        prog_ = vao_ = 0;
    }

    // A felépített fák azonossága: ha bármelyik lecserélődött (új felépítés), a
    // shadert újra kell generálni — a paraméterek CÍME is változhatott.
    void sync(SceneDoc const& sc) {
        std::vector<void const*> trees;
        for (auto const& s : sc.shapes) { trees.push_back(s.tree.get()); trees.push_back(s.dom_tree.get()); }
        if (trees == built_ && (prog_ || !error.empty())) return;
        built_ = trees;
        code_ = Glsl::generate(sc);
        if (prog_) glDeleteProgram(prog_);
        prog_ = 0;
        error.clear();
        if (code_.shapes == 0) return;
        if (!vao_) glGenVertexArrays(1, &vao_);
        prog_ = build(vertex_source(), fragment_source(code_));
        ++compiles;
    }

    static std::string vertex_source() {
        return "#version 330 core\n"
               "void main() {\n"
               "    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);\n"
               "    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
               "}\n";
    }

    static std::string fragment_source(Glsl::Code const& c) {
        std::string const n = std::to_string(c.shapes);
        std::string s = "#version 330 core\n" + c.source;
        // Alakzat-választó: a menetelés egy ciklusban megy végig az alakzatokon.
        s += "float gp_F(int i, vec3 p) {\n    switch (i) {\n";
        for (int i = 0; i < c.shapes; ++i)
            s += "        case " + std::to_string(i) + ": return gp_F" + std::to_string(i) + "(p.x, p.y, p.z);\n";
        s += "    }\n    return 1.0;\n}\n";
        s += "float gp_D(int i, vec3 p) {\n    switch (i) {\n";
        for (int i = 0; i < c.shapes; ++i)
            s += "        case " + std::to_string(i) + ": return gp_D" + std::to_string(i) + "(p.x, p.y, p.z);\n";
        s += "    }\n    return 1.0;\n}\n";
        s += "const int N_SHAPES = " + n + ";\n"
             "uniform vec3 u_color[" + n + "];\n"
             "uniform float u_visible[" + n + "];\n";
        s += R"(
uniform mat4 u_vp, u_inv_vp;
uniform vec3 u_eye;
uniform vec2 u_res;          // a nézet mérete képpontban
uniform vec2 u_t;            // t_min, t_max
uniform vec3 u_step;         // min_step, max_step, step_safety
uniform int  u_max_steps;
out vec4 frag;

// A gradiens numerikusan (előrelépő különbség; F-et a hívó már ismeri).
vec3 gp_grad(int i, vec3 p, float F) {
    const float h = 1e-3;
    return vec3(gp_F(i, p + vec3(h, 0.0, 0.0)) - F,
                gp_F(i, p + vec3(0.0, h, 0.0)) - F,
                gp_F(i, p + vec3(0.0, 0.0, h)) - F) / h;
}

bool gp_bad(float v) { return isnan(v) || isinf(v); }

// Menetelés egy alakzaton (mint a CPU-s Raytracer::march): t_end-ig keres.
bool gp_march(int i, vec3 ro, vec3 rd, float t_end, out float hit_t) {
    float t = u_t.x;
    float F = gp_F(i, ro + rd * t);
    for (int k = 0; k < u_max_steps && t < t_end; ++k) {
        float d = u_step.y;
        if (!gp_bad(F)) {
            float g = length(gp_grad(i, ro + rd * t, F));
            d = g > 1e-9 ? abs(F) / g : abs(F);
            if (gp_bad(d)) d = u_step.y;
        }
        float step = clamp(d * u_step.z, u_step.x, u_step.y);
        float t2 = t + step;
        float F2 = gp_F(i, ro + rd * t2);
        if (!gp_bad(F) && !gp_bad(F2) && ((F <= 0.0) != (F2 <= 0.0))) {
            // Felezés az előjelváltás közé: ez adja a pontos metszéspontot.
            float a = t, b = t2, fa = F;
            for (int j = 0; j < 20; ++j) {
                float m = 0.5 * (a + b);
                float fm = gp_F(i, ro + rd * m);
                if ((fa <= 0.0) != (fm <= 0.0)) b = m; else { a = m; fa = fm; }
            }
            float th = 0.5 * (a + b);
            if (gp_D(i, ro + rd * th) > 0.0) { hit_t = th; return true; }
            // A tartományon kívül esik: nem látszik, megyünk tovább.
            t = th + max(u_step.x * 4.0, step);
            F = gp_F(i, ro + rd * t);
            continue;
        }
        t = t2;
        F = F2;
    }
    return false;
}

void main() {
    vec2 ndc = gl_FragCoord.xy / u_res * 2.0 - 1.0;
    vec4 a = u_inv_vp * vec4(ndc, -1.0, 1.0);
    vec4 b = u_inv_vp * vec4(ndc, 1.0, 1.0);
    vec3 ro = u_eye;
    vec3 rd = normalize(b.xyz / b.w - a.xyz / a.w);

    float best = u_t.y;
    int hit = -1;
    for (int i = 0; i < N_SHAPES; ++i) {
        if (u_visible[i] < 0.5) continue;
        float th;
        if (gp_march(i, ro, rd, best, th) && th < best) { best = th; hit = i; }
    }
    if (hit < 0) discard;

    vec3 p = ro + rd * best;
    vec3 n = normalize(gp_grad(hit, p, gp_F(hit, p)));
    if (dot(n, rd) > 0.0) n = -n;                    // a nézőnek háttal: a túloldalát látjuk
    vec3 l = normalize(vec3(0.35, 0.25, 1.0));       // fény felülről, kicsit oldalról
    vec3 v = -rd;
    float diff = max(dot(n, l), 0.0);
    float head = max(dot(n, v), 0.0);                // fejlámpa: a sötét oldal se legyen fekete
    float spec = pow(max(dot(n, normalize(l + v)), 0.0), 48.0);
    vec3 c = u_color[hit] * (0.18 + 0.62 * diff + 0.25 * head) + vec3(0.30 * spec);
    frag = vec4(c, 1.0);

    vec4 clip = u_vp * vec4(p, 1.0);
    gl_FragDepth = clip.z / clip.w * 0.5 + 0.5;
}
)";
        return s;
    }

    GLuint compile(GLenum type, std::string const& src) {
        GLuint sh = glCreateShader(type);
        char const* p = src.c_str();
        glShaderSource(sh, 1, &p, nullptr);
        glCompileShader(sh);
        GLint ok = 0;
        glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[2048];
            glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
            error += log;
        }
        return sh;
    }

    GLuint build(std::string const& vs, std::string const& fs) {
        GLuint const v = compile(GL_VERTEX_SHADER, vs), f = compile(GL_FRAGMENT_SHADER, fs);
        GLuint p = glCreateProgram();
        glAttachShader(p, v);
        glAttachShader(p, f);
        glLinkProgram(p);
        glDeleteShader(v);
        glDeleteShader(f);
        GLint ok = 0;
        glGetProgramiv(p, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[2048];
            glGetProgramInfoLog(p, sizeof(log), nullptr, log);
            error += log;
            glDeleteProgram(p);
            return 0;
        }
        return p;
    }
};

#endif //GORBE_MODEL_RAYVIEW_HPP
