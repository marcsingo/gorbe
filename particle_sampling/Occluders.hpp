#pragma once
#include "Particle.hpp"
#include "../model/Include.hpp"


class SphereOccluder : public Model {
    Sphere const& sphere;
    glm::vec3 color;

    static constexpr int   RINGS   = 24;
    static constexpr int   SEGS    = 24;
    static constexpr float PI      = 3.14159265359f;
    static constexpr float TWO_PI  = 6.28318530718f;

    void render(const Camera&) override {
        vertices.clear();
        glm::vec3 c{sphere.q.x, sphere.q.y, sphere.q.z};
        float r = sphere.q.w;

        auto v = [&](float phi, float theta) -> glm::vec3 {
            return c + r * glm::vec3{
                std::sin(phi) * std::cos(theta),
                std::cos(phi),
                std::sin(phi) * std::sin(theta)
            };
        };

        for (int ri = 0; ri < RINGS; ++ri) {
            float p0 = PI * float(ri)     / float(RINGS);
            float p1 = PI * float(ri + 1) / float(RINGS);
            for (int si = 0; si < SEGS; ++si) {
                float t0 = TWO_PI * float(si)     / float(SEGS);
                float t1 = TWO_PI * float(si + 1) / float(SEGS);
                vertices.push_back(v(p0, t0));
                vertices.push_back(v(p1, t0));
                vertices.push_back(v(p1, t1));
                vertices.push_back(v(p0, t0));
                vertices.push_back(v(p1, t1));
                vertices.push_back(v(p0, t1));
            }
        }

        update_buffers();
        set_uniform("color", color);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 1.0f);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertices.size());
        glDisable(GL_POLYGON_OFFSET_FILL);
    }

public:
    SphereOccluder(Sphere const& s, glm::vec3 col, Camera const&)
        : sphere{s}, color{col}
    {
        update_buffers_on_draw = false;
        set_shader(Builder::get_or_build(SHADER_DIR "/vertex.vert",
                                         SHADER_DIR "/fragment.glsl"));
    }
};


class TorusOccluder : public Model {
    Torus const& torus;
    glm::vec3 color;

    static constexpr int   RINGS   = 32;
    static constexpr int   SEGS    = 32;
    static constexpr float TWO_PI  = 6.28318530718f;

    void render(const Camera&) override {
        vertices.clear();
        float R = torus.q.x;
        float r = torus.q.y;

        auto v = [&](float u, float vv) -> glm::vec3 {
            return glm::vec3{
                (R + r * std::cos(vv)) * std::cos(u),
                (R + r * std::cos(vv)) * std::sin(u),
                r * std::sin(vv)
            };
        };

        for (int si = 0; si < SEGS; ++si) {
            float u0 = TWO_PI * float(si)     / float(SEGS);
            float u1 = TWO_PI * float(si + 1) / float(SEGS);
            for (int ri = 0; ri < RINGS; ++ri) {
                float v0 = TWO_PI * float(ri)     / float(RINGS);
                float v1 = TWO_PI * float(ri + 1) / float(RINGS);
                vertices.push_back(v(u0, v0));
                vertices.push_back(v(u1, v0));
                vertices.push_back(v(u1, v1));
                vertices.push_back(v(u0, v0));
                vertices.push_back(v(u1, v1));
                vertices.push_back(v(u0, v1));
            }
        }

        update_buffers();
        set_uniform("color", color);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 1.0f);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertices.size());
        glDisable(GL_POLYGON_OFFSET_FILL);
    }

public:
    TorusOccluder(Torus const& t, glm::vec3 col, Camera const&)
        : torus{t}, color{col}
    {
        update_buffers_on_draw = false;
        set_shader(Builder::get_or_build(SHADER_DIR "/vertex.vert",
                                         SHADER_DIR "/fragment.glsl"));
    }
};


// Ellipszoid: q = {cx, cy, cz, a, b, c}
// Parametrizáció: x=cx+a·sin(φ)cos(θ), y=cy+b·cos(φ), z=cz+c·sin(φ)sin(θ)
class EllipsoidOccluder : public Model {
    Ellipsoid const& ellipsoid;
    glm::vec3 color;

    static constexpr int   RINGS   = 24;
    static constexpr int   SEGS    = 24;
    static constexpr float PI      = 3.14159265359f;
    static constexpr float TWO_PI  = 6.28318530718f;

    void render(const Camera&) override {
        vertices.clear();
        float a = ellipsoid.q.x, b = ellipsoid.q.y, c = ellipsoid.q.z;

        auto v = [&](float phi, float theta) -> glm::vec3 {
            return glm::vec3{
                a * std::sin(phi) * std::cos(theta),
                b * std::cos(phi),
                c * std::sin(phi) * std::sin(theta)
            };
        };

        for (int ri = 0; ri < RINGS; ++ri) {
            float p0 = PI     * float(ri)     / float(RINGS);
            float p1 = PI     * float(ri + 1) / float(RINGS);
            for (int si = 0; si < SEGS; ++si) {
                float t0 = TWO_PI * float(si)     / float(SEGS);
                float t1 = TWO_PI * float(si + 1) / float(SEGS);
                vertices.push_back(v(p0, t0));
                vertices.push_back(v(p1, t0));
                vertices.push_back(v(p1, t1));
                vertices.push_back(v(p0, t0));
                vertices.push_back(v(p1, t1));
                vertices.push_back(v(p0, t1));
            }
        }

        update_buffers();
        set_uniform("color", color);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 1.0f);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertices.size());
        glDisable(GL_POLYGON_OFFSET_FILL);
    }

public:
    EllipsoidOccluder(Ellipsoid const& e, glm::vec3 col, Camera const&)
        : ellipsoid{e}, color{col}
    {
        update_buffers_on_draw = false;
        set_shader(Builder::get_or_build(SHADER_DIR "/vertex.vert",
                                         SHADER_DIR "/fragment.glsl"));
    }
};


// Ellipszis: q = {cx, cy, a, b}  — lapos korong az x-y síkban
class EllipseOccluder : public Model {
    Ellipse const& ellipse;
    glm::vec3 color;

    static constexpr int   SEGS   = 48;
    static constexpr float TWO_PI = 6.28318530718f;

    void render(const Camera&) override {
        vertices.clear();
        float cx = ellipse.q.x;
        float cy = ellipse.q.y;
        float a  = ellipse.q.z;
        float b  = ellipse.q.w;
        glm::vec3 center{cx, cy, 0.0f};

        for (int i = 0; i < SEGS; ++i) {
            float t0 = TWO_PI * float(i)     / float(SEGS);
            float t1 = TWO_PI * float(i + 1) / float(SEGS);
            vertices.push_back(center);
            vertices.push_back({cx + a * std::cos(t0), cy + b * std::sin(t0), 0.0f});
            vertices.push_back({cx + a * std::cos(t1), cy + b * std::sin(t1), 0.0f});
        }

        update_buffers();
        set_uniform("color", color);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 1.0f);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertices.size());
        glDisable(GL_POLYGON_OFFSET_FILL);
    }

public:
    EllipseOccluder(Ellipse const& e, glm::vec3 col, Camera const&)
        : ellipse{e}, color{col}
    {
        update_buffers_on_draw = false;
        set_shader(Builder::get_or_build(SHADER_DIR "/vertex.vert",
                                         SHADER_DIR "/fragment.glsl"));
    }
};


// Teszt: két egységgömb sima uniója a (±0.5, ±0.5, ±0.5) középpontokkal.
// Az occluder csak vizuális segédlet, ezért egyszerűen a két gömböt rajzoljuk ki.
class TesztOccluder : public Model {
    Teszt const& teszt;
    glm::vec3 color;

    static constexpr int   RINGS   = 24;
    static constexpr int   SEGS    = 24;
    static constexpr float PI      = 3.14159265359f;
    static constexpr float TWO_PI  = 6.28318530718f;

    void render(const Camera&) override {
        vertices.clear();

        auto sphere = [&](glm::vec3 c, float r) {
            auto v = [&](float phi, float theta) -> glm::vec3 {
                return c + r * glm::vec3{
                    std::sin(phi) * std::cos(theta),
                    std::cos(phi),
                    std::sin(phi) * std::sin(theta)
                };
            };
            for (int ri = 0; ri < RINGS; ++ri) {
                float p0 = PI * float(ri)     / float(RINGS);
                float p1 = PI * float(ri + 1) / float(RINGS);
                for (int si = 0; si < SEGS; ++si) {
                    float t0 = TWO_PI * float(si)     / float(SEGS);
                    float t1 = TWO_PI * float(si + 1) / float(SEGS);
                    vertices.push_back(v(p0, t0));
                    vertices.push_back(v(p1, t0));
                    vertices.push_back(v(p1, t1));
                    vertices.push_back(v(p0, t0));
                    vertices.push_back(v(p1, t1));
                    vertices.push_back(v(p0, t1));
                }
            }
        };

        sphere(glm::vec3{ 0.5f,  0.5f,  0.5f}, 1.0f);
        sphere(glm::vec3{-0.5f, -0.5f, -0.5f}, 1.0f);

        update_buffers();
        set_uniform("color", color);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 1.0f);
        //glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertices.size());
        glDisable(GL_POLYGON_OFFSET_FILL);
    }

public:
    TesztOccluder(Teszt const& t, glm::vec3 col, Camera const&)
        : teszt{t}, color{col}
    {
        update_buffers_on_draw = false;
        set_shader(Builder::get_or_build(SHADER_DIR "/vertex.vert",
                                         SHADER_DIR "/fragment.glsl"));
    }
};


// Üres occluder: bármelyik felülethez passzol, és nem rajzol ki semmit.
// Nem állít be shadert, így a Model::draw a shaderProgram==0 ágon azonnal kilép
// (nincs mesh, nincs rajzolás). A render() override csak azért kell, hogy a
// (különben absztrakt) osztály példányosítható legyen.
template<class S>
class NullOccluder : public Model {
    void render(Camera const&) override {}
public:
    NullOccluder(S const&, glm::vec3, Camera const&) {}
};


// Felület -> hozzá tartozó occluder (referencia-mesh) párosítás.
// Az ALAPÉRTELMEZÉS a NullOccluder: minden felület, aminek nincs saját occludere,
// automatikusan "semmit nem rajzol" (így nem is kötelező occludert írni hozzá).
// Saját mesh-hez vegyél fel egy specializációt a Sphere/Torus mintájára.
template<class S> struct OccluderFor      { using type = NullOccluder<S>;   };
template<> struct OccluderFor<Sphere>    { using type = SphereOccluder;    };
template<> struct OccluderFor<Torus>     { using type = TorusOccluder;     };
template<> struct OccluderFor<Ellipsoid> { using type = EllipsoidOccluder; };
template<> struct OccluderFor<Ellipse>   { using type = EllipseOccluder;   };