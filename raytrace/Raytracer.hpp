#ifndef GORBE_RAYTRACE_RAYTRACER_HPP
#define GORBE_RAYTRACE_RAYTRACER_HPP

#include <algorithm>
#include <cmath>
#include <thread>
#include <vector>

#include <glm.hpp>

#include "../matek/Kif.hpp"
#include "Palette.hpp"

// ---------------------------------------------------------------------------
// Sugárkövetés implicit felületekre — ÖNÁLLÓ, leválasztható komponens.
//
// Nem függ semmitől a programból a kifejezésrendszeren (matek/) kívül: nincs benne
// OpenGL, ablak, ImGui, se a jelenet-modell. A hívó egy egyszerű leírást ad át
// (kifejezés + opcionális tartomány + szín), és egy RGB-tömböt kap vissza.
//
// A METSZÉS közelítő, ahogy kértük: a sugár mentén LÉPKEDÜNK, amíg elég közel nem
// kerülünk a felülethez, és ott metszünk el. A lépéshosszt nem fixáljuk, hanem a
// felülettől mért becsült GEOMETRIAI távolságból számoljuk (|F|/|∇F|, Taubin-féle
// elsőrendű közelítés) — így üres térben nagyot lépünk, a felület közelében pedig
// aprót. Előjelváltásnál felezéssel finomítunk.
//
// Fény: IRÁNYFÉNY (párhuzamos sugarak, mint a napfény), nem pontszerű.
// Anyag: MŰANYAG — színes diffúz + FEHÉR csúcsfény. (A fehér csúcsfény az, amitől
// műanyagnak látszik: a fémeknél a csúcsfény is felveszi az anyag színét.)
//
// LEVÁLASZTÁS: töröld a raytrace/ mappát, a CMakeLists-ből a raytrace/* sorokat,
// a main.cpp-ből a "#include raytrace/..." sorokat és a "Fenykep" gombot kezelő
// blokkot, valamint a Shape::color_idx mezőt. Semmi más nem hivatkozik rá.
// ---------------------------------------------------------------------------
namespace Raytrace {

    using Matek::Analizis::Kif;
    using Matek::Analizis::Program;

    // Egy rajzolandó objektum: VILÁGKOORDINÁTÁS implicit függvény (F=0 a felület,
    // F<0 belül), opcionális tartomány-feltétel (dom>0 = látható rész), és egy szín.
    struct ObjectDesc {
        Kif       F;
        Kif       domain;
        bool      has_domain = false;
        glm::vec3 color{0.8f, 0.8f, 0.8f};
    };

    struct CameraDesc {
        glm::vec3 eye{0.0f};
        glm::vec3 front{1.0f, 0.0f, 0.0f};
        glm::vec3 right{0.0f, -1.0f, 0.0f};
        glm::vec3 up{0.0f, 0.0f, 1.0f};
        float     fov_deg = 45.0f;
    };

    struct Settings {
        int   width  = 900;
        int   height = 600;

        // IRÁNYFÉNY: a fény TERJEDÉSI iránya (a fényforrástól a jelenet felé).
        // Nem pontszerű, tehát nincs távolság-csökkenés és nincs helye.
        glm::vec3 light_dir{-0.45f, 0.55f, -0.70f};
        glm::vec3 light_color{1.0f, 1.0f, 1.0f};
        float     light_intensity = 1.15f;

        // Féggömb-ambiens: felfelé néző felületek az "égbolt", lefelé nézők a "föld"
        // színéből kapnak egy keveset. Olcsó, de sokat dob a plasztikusságon.
        glm::vec3 sky_color{0.55f, 0.62f, 0.75f};
        glm::vec3 ground_color{0.28f, 0.26f, 0.24f};
        float     ambient = 0.30f;

        // Háttér (függőleges átmenet).
        glm::vec3 bg_top{0.92f, 0.94f, 0.97f};
        glm::vec3 bg_bottom{0.72f, 0.76f, 0.82f};

        // MŰANYAG anyagjellemzők.
        float diffuse   = 0.80f;
        float specular  = 0.38f;
        float shininess = 48.0f;

        // Menetelés.
        float t_min     = 0.02f;
        float t_max     = 200.0f;
        int   max_steps = 300;
        float hit_eps   = 2e-3f;   // ennyire közel már "elértük" a felületet
        float min_step  = 1e-3f;
        float max_step  = 2.0f;
        float step_safety = 0.75f; // a becsült távolság ekkora részét lépjük

        bool  shadows      = true;
        int   shadow_steps = 140;

        int   threads = 0;         // 0 = a gép magjainak száma
    };

    namespace detail {

        // Egy objektum lefordított alakja: F és a gradiens EGY menetben (matek/Program.hpp),
        // plusz a tartomány értéke (ahhoz nem kell gradiens).
        struct Compiled {
            Program   prog;
            int       out[4]{};
            Program   dom;
            int       dom_out = 0;
            bool      has_domain = false;
            glm::vec3 color{0.8f};
        };

        inline std::vector<Compiled> compile(std::vector<ObjectDesc> const& objs) {
            std::vector<Compiled> out;
            out.reserve(objs.size());
            for (auto const& o : objs) {
                Compiled c;
                Kif fx = o.F.derrive('x'), fy = o.F.derrive('y'), fz = o.F.derrive('z');
                Kif const* all[4] = {&o.F, &fx, &fy, &fz};
                for (int i = 0; i < 4; ++i) c.out[i] = all[i]->get()->compile(c.prog);
                c.prog.finish();

                c.has_domain = o.has_domain;
                if (o.has_domain) {
                    c.dom_out = o.domain.get()->compile(c.dom);
                    c.dom.finish();
                }
                c.color = o.color;
                out.push_back(std::move(c));
            }
            return out;
        }

        struct Sample {
            float     F;
            glm::vec3 grad;
        };

        inline Sample eval(Compiled const& c, glm::vec3 p) {
            c.prog.run(p);
            return Sample{c.prog.slot(c.out[0]),
                          {c.prog.slot(c.out[1]), c.prog.slot(c.out[2]), c.prog.slot(c.out[3])}};
        }

        inline bool in_domain(Compiled const& c, glm::vec3 p) {
            if (!c.has_domain) return true;
            c.dom.run(p);
            return c.dom.slot(c.dom_out) > 0.0f;
        }

        // A felülettől mért becsült geometriai távolság (Taubin). A nyers |F| skálafüggő
        // (a tórusz kvartikus F-je a felülettől 0.05-re már ~5), ezért osztunk |∇F|-fel.
        inline float surface_distance(Sample const& s) {
            float g = std::sqrt(glm::dot(s.grad, s.grad));
            return g > 1e-9f ? std::abs(s.F) / g : std::abs(s.F);
        }

        struct Hit {
            bool      hit = false;
            float     t   = 0.0f;
            glm::vec3 normal{0.0f};
            glm::vec3 color{0.0f};
        };

        // Menetelés EGY objektumon. `t` a sugár mentén, `any_hit` esetén az első
        // találatnál azonnal visszatérünk (árnyéksugár — ott nem kell normális).
        inline bool march(Compiled const& c, glm::vec3 ro, glm::vec3 rd,
                          Settings const& st, float t_start, float t_end,
                          bool any_hit, float& hit_t) {
            float t  = t_start;
            Sample s = eval(c, ro + rd * t);
            if (!std::isfinite(s.F)) return false;

            for (int i = 0; i < st.max_steps && t < t_end; ++i) {
                float d    = surface_distance(s);
                float step = std::clamp(d * st.step_safety, st.min_step, st.max_step);

                float  t2 = t + step;
                Sample s2 = eval(c, ro + rd * t2);
                if (!std::isfinite(s2.F)) { t = t2; s = s2; continue; }

                bool const crossed  = (s.F <= 0.0f) != (s2.F <= 0.0f);
                bool const close    = surface_distance(s2) < st.hit_eps;

                if (crossed || close) {
                    float th = t2;
                    if (crossed) {
                        // Felezés az előjelváltás közé: ez adja a pontos metszéspontot.
                        float a = t, b = t2, fa = s.F;
                        for (int k = 0; k < 24; ++k) {
                            float m  = 0.5f * (a + b);
                            float fm = eval(c, ro + rd * m).F;
                            if ((fa <= 0.0f) != (fm <= 0.0f)) { b = m; }
                            else                              { a = m; fa = fm; }
                        }
                        th = 0.5f * (a + b);
                    }
                    if (in_domain(c, ro + rd * th)) { hit_t = th; return true; }
                    // A tartományon kívül esik: nem látszik, megyünk tovább. Egy kicsit
                    // túllépünk, hogy ne ugyanazt a metszéspontot találjuk meg újra.
                    t = th + std::max(st.min_step * 4.0f, step);
                    s = eval(c, ro + rd * t);
                    if (any_hit) continue;
                    continue;
                }
                t = t2;
                s = s2;
            }
            return false;
        }

        inline Hit trace(std::vector<Compiled> const& objs, glm::vec3 ro, glm::vec3 rd,
                         Settings const& st) {
            Hit best;
            float best_t = st.t_max;
            for (auto const& c : objs) {
                float t;
                if (march(c, ro, rd, st, st.t_min, best_t, false, t) && t < best_t) {
                    best_t = t;
                    best.hit = true;
                    best.t = t;
                    best.color = c.color;
                    Sample s = eval(c, ro + rd * t);
                    glm::vec3 n = s.grad;
                    float len = std::sqrt(glm::dot(n, n));
                    n = (len > 1e-9f) ? n / len : glm::vec3(0.0f, 0.0f, 1.0f);
                    // A normális a NÉZŐ felé nézzen (F<0 belül konvenció mellett a
                    // gradiens kifelé mutat, de belülről nézve fordítva kell).
                    if (glm::dot(n, rd) > 0.0f) n = -n;
                    best.normal = n;
                }
            }
            return best;
        }

        inline bool in_shadow(std::vector<Compiled> const& objs, glm::vec3 p,
                              glm::vec3 to_light, Settings const& st) {
            Settings sh = st;
            sh.max_steps = st.shadow_steps;
            float t;
            for (auto const& c : objs)
                if (march(c, p, to_light, sh, st.t_min, st.t_max, true, t)) return true;
            return false;
        }

        // MŰANYAG árnyalás: színes diffúz + FEHÉR csúcsfény (Blinn-Phong), plusz
        // féggömb-ambiens. Irányfénnyel, tehát távolság-csökkenés nélkül.
        inline glm::vec3 shade(Hit const& h, glm::vec3 view_dir, bool shadowed,
                               Settings const& st) {
            glm::vec3 const N = h.normal;
            glm::vec3 const L = -glm::normalize(st.light_dir);   // a fény FELÉ mutat
            glm::vec3 const V = -view_dir;
            glm::vec3 const H = glm::normalize(L + V);

            float const ndl = std::max(0.0f, glm::dot(N, L));
            float const ndh = std::max(0.0f, glm::dot(N, H));

            // Féggömb-ambiens: a felfelé néző részek az égbolt, a lefelé nézők a
            // föld színéből kapnak. (A jelenetben a z a függőleges.)
            float const up_amount = 0.5f * (N.z + 1.0f);
            glm::vec3 amb = st.ambient * glm::mix(st.ground_color, st.sky_color, up_amount);

            float const vis = shadowed ? 0.0f : 1.0f;
            glm::vec3 col = h.color * (amb + st.diffuse * ndl * vis * st.light_intensity
                                             * st.light_color);
            // A csúcsfény NEM veszi fel az anyag színét -> műanyag hatás.
            col += st.light_color * (st.specular * std::pow(ndh, st.shininess) * vis
                                     * st.light_intensity);
            return col;
        }

    } // namespace detail

    // A teljes kép elkészítése. Visszaadott tömb: sor-folytonos, [0] a BAL FELSŐ pixel.
    inline std::vector<glm::vec3> render(std::vector<ObjectDesc> const& objects,
                                         CameraDesc const& cam,
                                         Settings const& st) {
        int const W = std::max(1, st.width);
        int const H = std::max(1, st.height);
        std::vector<glm::vec3> img(static_cast<std::size_t>(W) * H);

        auto compiled = detail::compile(objects);

        float const aspect = static_cast<float>(W) / static_cast<float>(H);
        float const tan_half = std::tan(cam.fov_deg * 0.5f * 3.14159265358979f / 180.0f);

        glm::vec3 const fw = glm::normalize(cam.front);
        glm::vec3 const rt = glm::normalize(cam.right);
        glm::vec3 const upv = glm::normalize(cam.up);

        int nthreads = st.threads > 0 ? st.threads
                                      : static_cast<int>(std::thread::hardware_concurrency());
        nthreads = std::clamp(nthreads, 1, 32);

        auto worker = [&](int thread_idx) {
            // MINDEN szál SAJÁT másolatot kap a lefordított programokból: a Program
            // futtatása közös munkaterületre ír, tehát egy példány nem futtatható
            // párhuzamosan több szálon.
            std::vector<detail::Compiled> local = compiled;

            for (int y = thread_idx; y < H; y += nthreads) {
                for (int x = 0; x < W; ++x) {
                    // Pixel közepe -> [-1,1] vászonkoordináta (y felfelé nő).
                    float const sx = (2.0f * (static_cast<float>(x) + 0.5f) / W - 1.0f)
                                   * aspect * tan_half;
                    float const sy = (1.0f - 2.0f * (static_cast<float>(y) + 0.5f) / H)
                                   * tan_half;
                    glm::vec3 const rd = glm::normalize(fw + rt * sx + upv * sy);

                    detail::Hit h = detail::trace(local, cam.eye, rd, st);

                    glm::vec3 col;
                    if (h.hit) {
                        glm::vec3 const p = cam.eye + rd * h.t;
                        bool shadowed = false;
                        if (st.shadows) {
                            // Eltoljuk a felülettől, különben a saját felületét találná el.
                            glm::vec3 const o = p + h.normal * 1e-2f;
                            shadowed = detail::in_shadow(local, o, -glm::normalize(st.light_dir), st);
                        }
                        col = detail::shade(h, rd, shadowed, st);
                    } else {
                        float const t = 0.5f * (rd.z + 1.0f);   // z = függőleges
                        col = glm::mix(st.bg_bottom, st.bg_top, std::clamp(t, 0.0f, 1.0f));
                    }
                    img[static_cast<std::size_t>(y) * W + x] = col;
                }
            }
        };

        if (nthreads == 1) {
            worker(0);
        } else {
            std::vector<std::thread> pool;
            pool.reserve(nthreads);
            for (int i = 0; i < nthreads; ++i) pool.emplace_back(worker, i);
            for (auto& t : pool) t.join();
        }
        return img;
    }

}

#endif //GORBE_RAYTRACE_RAYTRACER_HPP
