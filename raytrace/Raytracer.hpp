#ifndef GORBE_RAYTRACE_RAYTRACER_HPP
#define GORBE_RAYTRACE_RAYTRACER_HPP

#include <algorithm>
#include <cmath>
#include <thread>
#include <vector>

#include <glm.hpp>

#include "../matek/Kif.hpp"
#include "Material.hpp"
#include "Palette.hpp"

// ---------------------------------------------------------------------------
// Sugárkövetés implicit felületekre — ÖNÁLLÓ, leválasztható komponens.
//
// Nem függ semmitől a programból a kifejezésrendszeren (matek/) kívül: nincs benne
// OpenGL, ablak, ImGui, se a jelenet-modell. A hívó egy egyszerű leírást ad át
// (kifejezés + opcionális tartomány + szín + anyag), és egy RGB-tömböt kap vissza.
//
// A METSZÉS közelítő, ahogy kértük: a sugár mentén LÉPKEDÜNK, amíg elég közel nem
// kerülünk a felülethez, és ott metszünk el. A lépéshosszt nem fixáljuk, hanem a
// felülettől mért becsült GEOMETRIAI távolságból számoljuk (|F|/|∇F|, Taubin-féle
// elsőrendű közelítés) — így üres térben nagyot lépünk, a felület közelében pedig
// aprót. Előjelváltásnál felezéssel finomítunk.
//
// Fény: IRÁNYFÉNY (párhuzamos sugarak, mint a napfény), nem pontszerű.
// Anyag: ANYAGONKÉNT változó (lásd Material.hpp) — műanyag, gumi, kerámia, fém,
// króm, üveg, fa, márvány. A tükröződéshez és az üveg átlátszóságához a sugarat
// TOVÁBB kell követni, ezért a színszámítás rekurzív (`radiance`), legfeljebb
// `Settings::max_depth` mélységig.
//
// LEVÁLASZTÁS: töröld a raytrace/ mappát, a CMakeLists-ből a raytrace/* sorokat,
// a main.cpp-ből a "#include raytrace/..." sorokat és a "Fenykep" gombot kezelő
// blokkot, valamint a Shape::color_idx és Shape::material_idx mezőt. Semmi más
// nem hivatkozik rá.
// ---------------------------------------------------------------------------
namespace Raytrace {

    using Matek::Analizis::Kif;
    using Matek::Analizis::Program;

    // Egy rajzolandó objektum: VILÁGKOORDINÁTÁS implicit függvény (F=0 a felület,
    // F<0 belül), opcionális tartomány-feltétel (dom>0 = látható rész), szín és anyag.
    struct ObjectDesc {
        Kif       F;
        Kif       domain;
        bool      has_domain = false;
        glm::vec3 color{0.8f, 0.8f, 0.8f};
        Material  material = MATERIALS[0];      // alapértelmezés: műanyag
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

        // Háttér (függőleges átmenet). A tükröződő és átlátszó anyagok EZT is
        // visszaverik/átengedik, ezért nem mindegy, hogy mi van benne.
        glm::vec3 bg_top{0.92f, 0.94f, 0.97f};
        glm::vec3 bg_bottom{0.72f, 0.76f, 0.82f};

        // Hányszor követhet tovább egy sugár (tükrözés/törés). 0 = csak a helyi
        // árnyalás, tehát a fém és az üveg is átlátszatlan matt lenne.
        //
        // Üvegnél elvben minden szint KÉT sugárra ágazna (2^max_depth), de a
        // `min_weight` levágja az elhanyagolható ágakat, így a gyakorlatban egy
        // fő út marad — ezért engedhető meg ilyen nagy mélység. Kell is: egy
        // tömör üveggömbben a teljes visszaverődés több oda-vissza utat okoz, és
        // ha ezek elfogynak, sötét foltok maradnak a helyükön.
        int   max_depth = 8;
        // Ez alatti hozzájárulású ágat nem követünk tovább. MÉRVE (900x600, 3 alakzat,
        // árnyékkal): 0.02 -> műanyag 4073 ms, üveg 10758 ms; 0.05 -> 3091 ms és
        // 6478 ms. A 0.05 pont a dielektrikumok merőleges Fresnel-értéke (0.04)
        // fölött van, tehát a szemből alig látszó tükörképet elhagyja, a súrló
        // szögben felerősödő (és ott jól látható) tükröződést viszont megtartja.
        float min_weight = 0.05f;

        // Menetelés.
        float t_min     = 0.02f;
        float t_max     = 200.0f;
        int   max_steps = 300;
        float hit_eps   = 2e-3f;   // ennyire közel már "elértük" a felületet
        float min_step  = 1e-3f;
        float max_step  = 2.0f;
        float step_safety = 0.75f; // a becsült távolság ekkora részét lépjük
        float bias      = 1e-2f;   // ennyivel lépünk el a felülettől új sugárnál

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
            Material  mat = MATERIALS[0];
        };

        inline std::vector<Compiled> compile(std::vector<ObjectDesc> const& objs) {
            std::vector<Compiled> out;
            out.reserve(objs.size());
            for (auto const& o : objs) {
                Compiled c;
                Kif fx = o.F.derive('x'), fy = o.F.derive('y'), fz = o.F.derive('z');
                Kif const* all[4] = {&o.F, &fx, &fy, &fz};
                for (int i = 0; i < 4; ++i) c.out[i] = all[i]->get()->compile(c.prog);
                c.prog.finish();

                c.has_domain = o.has_domain;
                if (o.has_domain) {
                    c.dom_out = o.domain.get()->compile(c.dom);
                    c.dom.finish();
                }
                c.color = o.color;
                c.mat   = o.material;
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
            bool            hit = false;
            float           t   = 0.0f;
            glm::vec3       normal{0.0f};
            glm::vec3       color{0.0f};
            Material const* mat = nullptr;
            // A sugár BELÜLRŐL érte el a felületet (a normálist meg kellett
            // fordítani). Üvegnél ez dönti el a törésmutató irányát, és ez jelzi,
            // hogy a mögöttünk hagyott szakasz az anyagban futott.
            bool            inside = false;
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
                    best.mat = &c.mat;
                    Sample s = eval(c, ro + rd * t);
                    glm::vec3 n = s.grad;
                    float len = std::sqrt(glm::dot(n, n));
                    n = (len > 1e-9f) ? n / len : glm::vec3(0.0f, 0.0f, 1.0f);
                    // A normális a NÉZŐ felé nézzen (F<0 belül konvenció mellett a
                    // gradiens kifelé mutat, de belülről nézve fordítva kell).
                    best.inside = glm::dot(n, rd) > 0.0f;
                    if (best.inside) n = -n;
                    best.normal = n;
                }
            }
            return best;
        }

        // Mennyi fény jut el a fényforrástól a pontig: 1 = semmi sem takarja,
        // 0 = teljes árnyék. ÁTLÁTSZÓ takaró nem olt ki teljesen, csak tompít —
        // e nélkül az üveg ugyanolyan koromfekete árnyékot vetne, mint a kő.
        inline float light_visibility(std::vector<Compiled> const& objs, glm::vec3 p,
                                      glm::vec3 to_light, Settings const& st) {
            Settings sh = st;
            sh.max_steps = st.shadow_steps;
            float vis = 1.0f;
            float t;
            for (auto const& c : objs) {
                if (!march(c, p, to_light, sh, st.t_min, st.t_max, true, t)) continue;
                if (c.mat.transparency > 0.01f) vis *= 0.15f + 0.75f * c.mat.transparency;
                else                            return 0.0f;
            }
            return vis;
        }

        // Helyi árnyalás: színes diffúz + csúcsfény (Blinn-Phong), plusz féggömb-
        // ambiens. Irányfénnyel, tehát távolság-csökkenés nélkül.
        //
        // Az anyag dönti el, hogy a CSÚCSFÉNY fehér-e (műanyag, kerámia, üveg) vagy
        // felveszi az anyag színét (fém, króm) — ez a legerősebb vizuális jelzés
        // arról, hogy mit lát az ember.
        inline glm::vec3 shade(glm::vec3 N, glm::vec3 albedo, glm::vec3 view_dir,
                               float vis, Material const& m, Settings const& st) {
            glm::vec3 const L = -glm::normalize(st.light_dir);   // a fény FELÉ mutat
            glm::vec3 const V = -view_dir;
            glm::vec3 const H = glm::normalize(L + V);

            float const ndl = std::max(0.0f, glm::dot(N, L));
            float const ndh = std::max(0.0f, glm::dot(N, H));

            // Féggömb-ambiens: a felfelé néző részek az égbolt, a lefelé nézők a
            // föld színéből kapnak. (A jelenetben a z a függőleges.)
            float const up_amount = 0.5f * (N.z + 1.0f);
            glm::vec3 amb = st.ambient * glm::mix(st.ground_color, st.sky_color, up_amount);

            glm::vec3 col = albedo * (amb + m.diffuse * ndl * vis * st.light_intensity
                                            * st.light_color);
            // Fémnél a csúcsfény színezett, egyébként fehér marad -> műanyag hatás.
            glm::vec3 const spec_tint = m.metallic ? albedo : glm::vec3(1.0f);
            col += spec_tint * st.light_color
                 * (m.specular * std::pow(ndh, m.shininess) * vis * st.light_intensity);
            return col;
        }

        inline glm::vec3 background(glm::vec3 rd, Settings const& st) {
            float const t = 0.5f * (rd.z + 1.0f);   // z = függőleges
            return glm::mix(st.bg_bottom, st.bg_top, std::clamp(t, 0.0f, 1.0f));
        }

        // Schlick-közelítés: súrló szögben minden anyag tükröz. Enélkül az üveg
        // pereme nem világosodna ki, és laposnak látszana.
        inline float fresnel(float cos_theta, float ior) {
            float r0 = (1.0f - ior) / (1.0f + ior);
            r0 *= r0;
            float const c = std::clamp(1.0f - cos_theta, 0.0f, 1.0f);
            return r0 + (1.0f - r0) * c * c * c * c * c;
        }

        // Egy sugár által hozott szín. `depth` a már megtett tükrözések/törések
        // száma, `weight` pedig az, hogy ez a sugár mekkora súllyal számít bele a
        // VÉGSŐ pixelbe. A súly az, ami az elágazást kordában tartja: egy üveg
        // homlokfelületén a visszavert ág súlya ~0.04, tehát azonnal elhal, míg az
        // átmenő ág ~0.96-tal megy tovább — így az exponenciális szétágazás
        // helyett a gyakorlatban egyetlen fő út marad.
        inline glm::vec3 radiance(std::vector<Compiled> const& objs, glm::vec3 ro,
                                  glm::vec3 rd, Settings const& st, int depth,
                                  float weight = 1.0f) {
            Hit const h = trace(objs, ro, rd, st);
            if (!h.hit || !h.mat) return background(rd, st);

            Material const& m = *h.mat;
            glm::vec3 const p = ro + rd * h.t;
            glm::vec3 const albedo = pattern_albedo(h.color, m, p);

            float vis = 1.0f;
            if (st.shadows) {
                glm::vec3 const L = -glm::normalize(st.light_dir);
                vis = light_visibility(objs, p + h.normal * st.bias, L, st);
            }
            glm::vec3 col = shade(h.normal, albedo, rd, vis, m, st);

            if (depth < st.max_depth) {
                float const cosi = std::clamp(glm::dot(-rd, h.normal), 0.0f, 1.0f);

                if (m.transparency > 0.01f) {
                    // Üveg: a fény egy része visszaverődik, a többi megtörik.
                    // A törésmutató iránya attól függ, hogy be- vagy kilépünk.
                    float const eta = h.inside ? m.ior : 1.0f / m.ior;
                    glm::vec3 const T = glm::refract(rd, h.normal, eta);
                    float F = fresnel(cosi, m.ior);
                    if (glm::dot(T, T) < 1e-9f) F = 1.0f;      // teljes visszaverődés

                    float const wr = weight * m.transparency * F;
                    float const wt = weight * m.transparency * (1.0f - F);
                    glm::vec3 through(0.0f);
                    bool any = false;
                    if (wr > st.min_weight) {
                        through += F * radiance(objs, p + h.normal * st.bias,
                                                glm::reflect(rd, h.normal), st, depth + 1, wr);
                        any = true;
                    }
                    if (wt > st.min_weight) {
                        through += (1.0f - F) * radiance(objs, p - h.normal * st.bias,
                                                         glm::normalize(T), st, depth + 1, wt);
                        any = true;
                    }
                    // Ha MINDKÉT ág elhanyagolható, a helyi árnyalást hagyjuk meg:
                    // a nullával való keverés fekete foltot festene oda.
                    if (any) col = glm::mix(col, through, m.transparency);
                } else if (m.reflectivity > 0.001f) {
                    // Fémnél a tükörkép is felveszi az anyag színét (a réz sárgásan
                    // tükröz), egyébként színezetlen marad.
                    float const k = std::clamp(m.reflectivity
                                    + (1.0f - m.reflectivity) * std::pow(1.0f - cosi, 5.0f),
                                    0.0f, 1.0f);
                    if (weight * k > st.min_weight) {
                        glm::vec3 const refl = radiance(objs, p + h.normal * st.bias,
                                                        glm::reflect(rd, h.normal), st,
                                                        depth + 1, weight * k);
                        glm::vec3 const tint = m.metallic ? albedo : glm::vec3(1.0f);
                        col = glm::mix(col, refl * tint, k);
                    }
                }
            }

            // Beer-féle elnyelés: ha a sugár az ANYAGON BELÜL tette meg az utat
            // (most lép ki), a vastagabb rész telítettebb színű. Ettől lesz az
            // üvegnek mélysége a lapos átlátszóság helyett.
            if (h.inside && m.absorb > 0.0f) {
                glm::vec3 const a = (glm::vec3(1.0f) - albedo) * (m.absorb * h.t);
                col *= glm::vec3(std::exp(-a.x), std::exp(-a.y), std::exp(-a.z));
            }
            return col;
        }

    } // namespace detail

    // A felületek lefordítva, a render_rows() bemenete.
    using Prepared = std::vector<detail::Compiled>;

    inline Prepared prepare(std::vector<ObjectDesc> const& objects) { return detail::compile(objects); }

    // A kép [y0, y1) sorai az `img`-be (W*H elemű, sor-folytonos). Így a render
    // sávokra bontható, és a sávok között a hívó folyamatjelzőt rajzolhat; az
    // eredmény bitre ugyanaz, mint egyben.
    inline void render_rows(Prepared const& compiled, CameraDesc const& cam, Settings const& st,
                            int y0, int y1, std::vector<glm::vec3>& img) {
        int const W = std::max(1, st.width);
        int const H = std::max(1, st.height);
        y1 = std::min(y1, H);

        float const aspect = static_cast<float>(W) / static_cast<float>(H);
        float const tan_half = std::tan(cam.fov_deg * 0.5f * 3.14159265358979f / 180.0f);

        glm::vec3 const fw = glm::normalize(cam.front);
        glm::vec3 const rt = glm::normalize(cam.right);
        glm::vec3 const upv = glm::normalize(cam.up);

        int nthreads = st.threads > 0 ? st.threads
                                      : static_cast<int>(std::thread::hardware_concurrency());
        nthreads = std::clamp(nthreads, 1, std::max(1, std::min(32, y1 - y0)));

        auto worker = [&](int thread_idx) {
            // MINDEN szál SAJÁT másolatot kap a lefordított programokból: a Program
            // futtatása közös munkaterületre ír, tehát egy példány nem futtatható
            // párhuzamosan több szálon.
            std::vector<detail::Compiled> local = compiled;

            for (int y = y0 + thread_idx; y < y1; y += nthreads) {
                for (int x = 0; x < W; ++x) {
                    // Pixel közepe -> [-1,1] vászonkoordináta (y felfelé nő).
                    float const sx = (2.0f * (static_cast<float>(x) + 0.5f) / W - 1.0f)
                                   * aspect * tan_half;
                    float const sy = (1.0f - 2.0f * (static_cast<float>(y) + 0.5f) / H)
                                   * tan_half;
                    glm::vec3 const rd = glm::normalize(fw + rt * sx + upv * sy);

                    img[static_cast<std::size_t>(y) * W + x] =
                        detail::radiance(local, cam.eye, rd, st, 0);
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
    }

    // A teljes kép elkészítése. Visszaadott tömb: sor-folytonos, [0] a BAL FELSŐ pixel.
    inline std::vector<glm::vec3> render(std::vector<ObjectDesc> const& objects,
                                         CameraDesc const& cam,
                                         Settings const& st) {
        std::vector<glm::vec3> img(static_cast<std::size_t>(std::max(1, st.width)) * std::max(1, st.height));
        render_rows(prepare(objects), cam, st, 0, std::max(1, st.height), img);
        return img;
    }

}

#endif //GORBE_RAYTRACE_RAYTRACER_HPP
