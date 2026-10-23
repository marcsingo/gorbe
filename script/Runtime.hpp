#ifndef GORBE_SCRIPT_RUNTIME_HPP
#define GORBE_SCRIPT_RUNTIME_HPP

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <exception>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../particle_sampling/ParticleSystem.hpp"
#include "../scene/Document.hpp"
#include "../scene/Time.hpp"
#include "../utils/Log.hpp"
#include "gorbe_api.hpp"

// ---------------------------------------------------------------------------
// Az alakzatokhoz írt C++ kód futtatása (a program oldala). A kód felülete a
// gorbe_api.hpp; itt töltjük ki a Host táblát, és itt élnek a példányok.
//
// Egy jelenet egy Runtime-ot használ. Egy Indítás menete:
//
//     rt.clear();                   // előző futás vége (a hívó előtte drop()-ol!)
//     rt.attach(shape, sc, program, factory);   // alakzatonként
//     rt.start_all();               // a start()-ok: warp, képlet, új paraméter
//     Build::build(sc, program);    // a kód kiegészítései már beépülnek
//     ... minden ciklusban: rt.update_all(t, dt);
//
// A GUI-n beállított értékek KEZDŐÉRTÉKEK: amihez a kód hozzányúl (paraméter,
// pozíció, forgatás, méret), annak az értékét az első hozzáféréskor feljegyezzük,
// és a clear() visszaállítja. Mentéshez a swap_initial() két hívás között a
// kezdőértékeket mutatja (lásd ott).
//
// Hiba (rossz fázisban hívott szerkezeti függvény, ismeretlen paraméter, a kód
// kivétele) nem állítja le a jelenetet: az `errors`-ba kerül, egy üzenet egyszer.
// Kivételnél az a példány leáll, a többi fut tovább.
//
// Más alakzatok (gorbe::ShapeRef): a Host függvényei egy `target`-et kapnak (a
// Shape*, átlátszatlanul; nullptr = a kód saját alakzata). Az F kiértékelése és az
// ütközés az alakzat FUTÓ mintavételezőjén megy (system_of): a lefordított felület-
// programmal, illetve a felületen ülő részecskékkel.
// ---------------------------------------------------------------------------
namespace Script {

    // A kód gyártófüggvényei (a DLL-ből, a fájl = osztály nevével).
    struct Factory {
        int                  (*version)()                    = nullptr;
        gorbe::ShapeScript*  (*create)()                     = nullptr;
        void                 (*destroy)(gorbe::ShapeScript*) = nullptr;
    };

    class Runtime {
    public:
        std::vector<std::string> errors;

        // Az alakzat futó mintavételezője (a felülete és a részecskéi), vagy nullptr.
        // A jelenet adja (app/Scene.hpp); nélküle az F és az ütközés nem érhető el.
        std::function<ParticleSystem*(Shape const*)> system_of;

        Runtime() = default;
        Runtime(Runtime const&) = delete;
        Runtime& operator=(Runtime const&) = delete;
        ~Runtime() { clear(); }

        bool empty() const { return items_.empty(); }

        // Egy példány az alakzathoz. Hamis (és hiba), ha a kód más API-verzióval
        // fordult, vagy hiányzik a gyártófüggvénye.
        bool attach(Shape& s, SceneDoc& sc, std::list<Param>& program, Factory f) {
            std::string const who = label(s);
            if (!f.version || !f.create || !f.destroy) {
                report(who + ": a kodban nincs GORBE_SHAPE a fajl nevevel egyezo osztalyra");
                return false;
            }
            if (int const v = f.version(); v != gorbe::API_VERSION) {
                report(who + ": a kod a gorbe_api " + std::to_string(v) + ". verziojaval fordult, "
                       "a program a " + std::to_string(gorbe::API_VERSION) + ".-at ismeri");
                return false;
            }
            auto& it = *items_.emplace_back(std::make_unique<Item>());
            it.owner = this;
            it.shape = &s; it.scene = &sc; it.program = &program;
            it.destroy = f.destroy;
            it.who = who;
            it.host = {gorbe::API_VERSION, &it, &h_param, &h_add_param, &h_transform,
                       &h_add_warp, &h_set_formula, &h_error, &h_log,
                       &h_find_shape, &h_shape_count, &h_shape_at, &h_shape_name,
                       &h_eval, &h_collide};
            try {
                it.obj = f.create();
            } catch (...) {
                report(who + ": a konstruktor kivetelt dobott");
            }
            if (!it.obj) { items_.pop_back(); return false; }
            it.obj->bind(&it.host);
            return true;
        }

        void start_all() {
            for (auto& it : items_) {
                it->phase = Phase::Start;
                call(*it, [&] { it->obj->start(); });
                it->phase = Phase::Update;
            }
        }

        // Az update()-ek, utána a részecskék vitele: amelyik alakzat pozícióját,
        // forgatását vagy méretét a kód átírta, annak a részecskéi ugyanazzal a merev
        // leképezéssel mennek vele (régi világ -> lokális -> új világ). Enélkül egy nagy
        // mozdulat (pl. az első update() a kezdőhelytől messzire teszi) után a
        // részecskék a régi helyükről csak lassan repülnének át — addig nincs fisszió, és
        // a mintavétel (az ütközés, a variációs szerkesztő) használhatatlan.
        // Csak a KÓD változtatására: a kontrollpont-megoldó a saját útján (q̇) viszi őket.
        void update_all(float t, float dt) {
            if (items_.empty()) return;
            SceneDoc& sc = *items_.front()->scene;
            before_.clear();
            for (auto const& s : sc.shapes) before_.push_back(s.xform);

            for (auto& it : items_) call(*it, [&] { it->obj->update(t, dt); });

            std::size_t i = 0;
            for (auto& s : sc.shapes) {
                TransformParams const& old = before_[i++];
                if (!s.warped || s.xform == old) continue;   // nem változott, vagy nem is hat
                if (old.scale[0] == 0.0f || old.scale[1] == 0.0f || old.scale[2] == 0.0f) continue;
                ParticleSystem* ps = system_of ? system_of(&s) : nullptr;
                if (!ps) continue;
                for (Particle& p : ps->particles()) p.p = to_world(s.xform, to_local(old, p.p));
            }
        }

        // A futás vége: a kezdőértékek vissza, a példányok törlése, és a kód
        // kiegészítései (script_params/warps/formula) ki az alakzatokból.
        // FONTOS: a hívó előtte drop()-oljon — a fák a script_params CÍMÉT tárolják.
        void clear() {
            restore();
            for (auto& it : items_) {
                if (it->obj) it->destroy(it->obj);
                it->shape->script_params.clear();
                it->shape->script_warps.clear();
                it->shape->script_formula.clear();
            }
            items_.clear();
            touched_.clear();
            for (Shape* s : moved_) s->script_moved = false;
            moved_.clear();
            warned_.clear();
        }

        // A kezdőértékek <-> a pillanatnyiak cseréje. Mentésnél a mentés köré:
        //     rt.swap_initial(); save(); rt.swap_initial();
        // így a fájlba a GUI-n beállított érték kerül, a futás pedig onnan folytatódik.
        void swap_initial() {
            for (auto& [p, v] : touched_) std::swap(*p, v);
        }

    private:
        enum class Phase { Start, Update };

        struct Item {
            Runtime*            owner   = nullptr;
            Shape*              shape   = nullptr;
            SceneDoc*           scene   = nullptr;
            std::list<Param>*   program = nullptr;
            gorbe::ShapeScript* obj     = nullptr;
            void (*destroy)(gorbe::ShapeScript*) = nullptr;
            gorbe::Host         host{};
            Phase               phase   = Phase::Start;
            std::string         who;
        };

        // unique_ptr: a Host a példány CÍMÉT kapja ctx-ként, tehát nem mozdulhat.
        std::vector<std::unique_ptr<Item>> items_;
        std::vector<std::pair<float*, float>> touched_;   // cím -> kezdőérték
        std::vector<Shape*> moved_;     // más alakzatok, amiknek a transzformációja beépül
        std::vector<Shape*> warned_;    // akikről már szóltunk, hogy nem mozgathatók
        Surface::Workspace  ws_;        // a kiértékeléshez (csak a fő szálról hívjuk)
        std::vector<TransformParams> before_;   // az update() előtti transzformációk

        static std::string label(Shape const& s) {
            return std::string(s.name[0] ? s.name : "(nevtelen)") + " [" + s.script + "]";
        }

        // Egy hiba a panelre és a konzolra — mindkettőre csak egyszer, akkor is, ha a
        // kód minden ciklusban ugyanazt a hibát váltja ki.
        void report(std::string msg) {
            if (std::find(errors.begin(), errors.end(), msg) != errors.end()) return;
            Log::add(Log::Level::Error, "", msg, SceneTime::value);
            errors.push_back(std::move(msg));
        }

        template <class Fn>
        void call(Item& it, Fn&& fn) {
            if (!it.obj) return;
            try {
                fn();
            } catch (std::exception const& e) {
                stop(it, std::string("kivetel: ") + e.what());
            } catch (...) {
                stop(it, "ismeretlen kivetel");
            }
        }

        // A hibás példány leáll (többet nem hívjuk), de a kiegészítései maradnak a
        // clear()-ig: a fák még hivatkozhatnak rájuk.
        void stop(Item& it, std::string const& why) {
            report(it.who + ": " + why + " (a kod leallt)");
            it.destroy(it.obj);
            it.obj = nullptr;
        }

        void touch(float* p) {
            for (auto const& [q, v] : touched_) if (q == p) return;
            touched_.emplace_back(p, *p);
        }

        // --- a Host tábla függvényei (ctx = Item*) ------------------------------

        static Item& item(void* ctx) { return *static_cast<Item*>(ctx); }

        static bool in_start(Item& it, char const* fn) {
            if (it.phase == Phase::Start) return true;
            it.owner->report(it.who + ": " + fn + " csak a start()-ban hivhato");
            return false;
        }

        static Shape& target(Item& it, void* t) { return t ? *static_cast<Shape*>(t) : *it.shape; }

        static Param* find(Item& it, Shape& s, char const* name) {
            for (auto* lvl : {&s.locals, &s.script_params, &it.scene->params, it.program})
                for (auto& p : *lvl)
                    if (name && p.name[0] && std::strcmp(p.name, name) == 0) return &p;
            return nullptr;
        }

        static float* h_param(void* ctx, void* t, char const* name) {
            Item& it = item(ctx);
            Shape& s = target(it, t);
            Param* p = find(it, s, name);
            if (!p) {
                it.owner->report(it.who + ": nincs '" + (name ? name : "") + "' parameter" +
                                 (&s == it.shape ? std::string() : std::string(" (") + s.name + ")"));
                return nullptr;
            }
            if (p->derived()) {
                it.owner->report(it.who + ": a(z) '" + name + "' parameter kepletes, nem irhato");
                return nullptr;
            }
            it.owner->touch(&p->value);
            return &p->value;
        }

        static float* h_add_param(void* ctx, char const* name, float value) {
            Item& it = item(ctx);
            if (!in_start(it, "add_param")) return nullptr;
            if (!name || !name[0] || std::strlen(name) >= sizeof(Param::name)) {
                it.owner->report(it.who + ": add_param: ures vagy tul hosszu nev");
                return nullptr;
            }
            if (find(it, *it.shape, name)) return h_param(ctx, nullptr, name);   // a GUI-n beállított nyer
            Param& p = it.shape->script_params.emplace_back();
            std::snprintf(p.name, sizeof(p.name), "%s", name);
            p.value = value;
            p.fit_range_to_value();
            return &p.value;
        }

        // Egy MÁSIK alakzat transzformációja csak akkor épül a képletébe, ha az
        // Indításkor kellett (lásd Build: Shape::script_moved) — ezért a start()-ban
        // lekért alakzatot megjelöljük (mark_moved); ha az update()-ben először nyúlnak
        // a még be nem épült transzformációjához, a konzolon szólunk.
        static float* h_transform(void* ctx, void* t, int which) {
            Item& it = item(ctx);
            Shape& s = target(it, t);
            Runtime& rt = *it.owner;
            if (&s != it.shape && !s.script_moved) {
                if (it.phase == Phase::Start) {
                    mark_moved(it, s);
                } else if (!s.warped && std::find(rt.warned_.begin(), rt.warned_.end(), &s) == rt.warned_.end()) {
                    rt.warned_.push_back(&s);
                    Log::add(Log::Level::Warning, it.shape->name,
                             std::string("a(z) '") + s.name + "' pozicioja/forgatasa/merete csak akkor "
                             "mozgatja, ha a start()-ban mar lekered (pl. shape(...).position())",
                             SceneTime::value);
                }
            }
            TransformParams& x = s.xform;
            float* v = which == 0 ? x.pos : which == 1 ? x.rot : x.scale;
            for (int i = 0; i < 3; ++i) it.owner->touch(v + i);
            return v;
        }

        static bool h_add_warp(void* ctx, char const* fx, char const* fy, char const* fz) {
            Item& it = item(ctx);
            if (!in_start(it, "add_warp")) return false;
            Warp w;
            for (auto [dst, src] : {std::pair{w.fx, fx}, {w.fy, fy}, {w.fz, fz}}) {
                if (!src || std::strlen(src) >= sizeof(w.fx)) {
                    it.owner->report(it.who + ": add_warp: ures vagy tul hosszu kifejezes");
                    return false;
                }
                std::snprintf(dst, sizeof(w.fx), "%s", src);
            }
            std::snprintf(w.name, sizeof(w.name), "kod %zu", it.shape->script_warps.size() + 1);
            it.shape->script_warps.push_back(w);
            return true;
        }

        static bool h_set_formula(void* ctx, char const* f) {
            Item& it = item(ctx);
            if (!in_start(it, "formula")) return false;
            if (!f || !f[0]) {
                it.owner->report(it.who + ": formula: ures keplet");
                return false;
            }
            it.shape->script_formula = f;
            return true;
        }

        static void h_error(void* ctx, char const* msg) {
            Item& it = item(ctx);
            it.owner->report(it.who + ": " + (msg ? msg : ""));
        }

        static void h_log(void* ctx, char const* msg) {
            Item& it = item(ctx);
            Log::add(Log::Level::Info, it.shape->name[0] ? it.shape->name : "(nevtelen)",
                     msg ? msg : "", SceneTime::value);
        }

        // A start()-ban lekért alakzat transzformációja beépül a képletébe (Build), így
        // a kód később mozgathatja is — akkor is, ha a lekéréskor ezt még nem tudjuk.
        static void mark_moved(Item& it, Shape& s) {
            if (&s == it.shape || s.script_moved || it.phase != Phase::Start) return;
            s.script_moved = true;
            it.owner->moved_.push_back(&s);
        }

        static void* h_find_shape(void* ctx, char const* name) {
            Item& it = item(ctx);
            for (auto& s : it.scene->shapes)
                if (name && std::strcmp(s.name, name) == 0) { mark_moved(it, s); return &s; }
            it.owner->report(it.who + ": nincs '" + (name ? name : "") + "' nevu alakzat a jelenetben");
            return nullptr;
        }

        static int h_shape_count(void* ctx) { return static_cast<int>(item(ctx).scene->shapes.size()); }

        static void* h_shape_at(void* ctx, int i) {
            Item& it = item(ctx);
            if (i < 0 || i >= h_shape_count(ctx)) {
                it.owner->report(it.who + ": shape(" + std::to_string(i) + "): nincs ilyen sorszamu alakzat");
                return nullptr;
            }
            Shape& s = *std::next(it.scene->shapes.begin(), i);
            mark_moved(it, s);
            return &s;
        }

        static char const* h_shape_name(void* ctx, void* t) { return target(item(ctx), t).name; }

        // A futó, felépített mintavételező, vagy nullptr (a start()-ban még nincs fa).
        static ParticleSystem* running(Item& it, Shape& s, char const* fn) {
            if (it.phase == Phase::Start) {
                it.owner->report(it.who + ": " + fn + " csak az update()-ben hivhato "
                                 "(a start()-ban meg nem epultek fel a kepletek)");
                return nullptr;
            }
            ParticleSystem* ps = it.owner->system_of ? it.owner->system_of(&s) : nullptr;
            return ps && s.tree ? ps : nullptr;
        }

        static bool h_eval(void* ctx, void* t, float const p[3], float out[4], bool* in_domain) {
            Item& it = item(ctx);
            ParticleSystem* ps = running(it, target(it, t), "value/gradient/contains/distance");
            if (!ps) return false;
            Surface const& surf = ps->surface();
            glm::vec3 const q{p[0], p[1], p[2]};
            float F = 0.0f, Ft = 0.0f;
            glm::vec3 g{0.0f};
            surf.eval_grad(q, F, g, Ft, it.owner->ws_);
            out[0] = F; out[1] = g.x; out[2] = g.y; out[3] = g.z;
            if (in_domain) {
                float dom = 1.0f;
                glm::vec3 dg{0.0f};
                if (surf.has_domain) surf.eval_domain(q, dom, dg, it.owner->ws_);
                *in_domain = dom > 0.0f;
            }
            return true;
        }

        // Egy pont vetülete a felületre, Newton-lépésekkel a gradiens mentén:
        // p <- p - F·∇F/|∇F|². A felület közelében gyorsan konvergál; mélyen bent (ahol
        // |∇F| kicsi) a -F/|∇F| becslés elszállna, a vetítés viszont visszatalál.
        static glm::vec3 project(Surface const& s, glm::vec3 p, Surface::Workspace& w) {
            for (int k = 0; k < 16; ++k) {
                float F = 0.0f, Ft = 0.0f;
                glm::vec3 g{0.0f};
                s.eval_grad(p, F, g, Ft, w);
                float const g2 = glm::dot(g, g);
                if (!(g2 > 1e-20f) || std::abs(F) < 1e-5f * std::sqrt(g2)) break;
                p -= (F / g2) * g;
            }
            return p;
        }

        // Ütközés az implicit egyenlettel: A részecskéi A felületén ülnek, és amelyikre
        // F_B < 0 (és B tartományán belül van), az B-ben van. Ugyanez fordítva is (B
        // részecskéi A-ban), így akkor is jelez, ha az egyik a másikon belül van.
        //
        // A LEGMÉLYEBB pontot (az elsőrendű -F/|∇F| becslés szerint) a másik felületére
        // vetítjük (project): a mélység a valódi elmozdulás, a normális pedig az, amerre
        // A-nak mozdulnia kell — A pontjánál a kilépés iránya (p' - p), B pontjánál
        // fordítva (q - q'), mert ott A felülete kell a pontig hátráljon.
        //
        // ponytail: részecskénként egy kiértékelés (O(N_A + N_B)); sok alakzatpárnál
        // befoglaló dobozzal előszűrhető.
        static int h_collide(void* ctx, void* a, void* b, float out[7]) {
            Item& it = item(ctx);
            Shape& sa = target(it, a);
            Shape& sb = target(it, b);
            if (&sa == &sb) return 0;
            ParticleSystem* pa = running(it, sa, "collide");
            ParticleSystem* pb = pa ? running(it, sb, "collide") : nullptr;
            if (!pa || !pb) return 0;

            int count = 0;
            float best = -1.0f;
            glm::vec3 deepest{0.0f};
            Surface const* deepest_in = nullptr;
            float deepest_sign = 1.0f;
            auto probe = [&](ParticleSystem const& from, Surface const& into, float sign) {
                Surface const& own = from.surface();
                for (Particle const& p : from.particles()) {
                    if (Domain::is_outside(p.dom_dist, p.sigma)) continue;

                    // A részecske a SAJÁT felületének pillanatnyi helyére igazítva (két
                    // Newton-lépés): ha a kód épp mozgatja az alakzatot, a részecskék
                    // késve követik, és a régi helyükről hamis (túl mély) találatot
                    // adnának. Ami messzebb van a részecske sugaránál (pl. az Indítás után
                    // még úton van), az nem számít.
                    glm::vec3 q = p.p;
                    float F = 0.0f, Ft = 0.0f;
                    glm::vec3 g{0.0f};
                    own.eval_grad(q, F, g, Ft, it.owner->ws_);
                    float g2 = glm::dot(g, g);
                    if (!(g2 > 1e-20f) || !(std::abs(F) < p.sigma * std::sqrt(g2))) continue;
                    q -= (F / g2) * g;
                    own.eval_grad(q, F, g, Ft, it.owner->ws_);
                    g2 = glm::dot(g, g);
                    if (g2 > 1e-20f) q -= (F / g2) * g;

                    into.eval_grad(q, F, g, Ft, it.owner->ws_);
                    if (!(F < 0.0f)) continue;
                    if (into.has_domain) {
                        float dom = 0.0f;
                        glm::vec3 dg{0.0f};
                        into.eval_domain(q, dom, dg, it.owner->ws_);
                        if (!(dom > 0.0f)) continue;
                    }
                    ++count;
                    float const gl = glm::length(g);
                    float const est = gl > 1e-12f ? -F / gl : 1e30f;
                    if (est <= best) continue;
                    best = est;
                    deepest = q;
                    deepest_in = &into;
                    deepest_sign = sign;
                }
            };
            probe(*pa, pb->surface(), +1.0f);    // A pontjai B-ben: A a kilépés irányába
            probe(*pb, pa->surface(), -1.0f);    // B pontjai A-ban: A visszafelé
            if (!count) return 0;

            glm::vec3 const moved = project(*deepest_in, deepest, it.owner->ws_) - deepest;
            float const depth = glm::length(moved);
            glm::vec3 const n = depth > 1e-12f ? moved * (deepest_sign / depth) : glm::vec3(0.0f);
            out[0] = deepest.x; out[1] = deepest.y; out[2] = deepest.z;
            out[3] = n.x;       out[4] = n.y;       out[5] = n.z;
            out[6] = depth;
            return count;
        }

        void restore() {
            for (auto const& [p, v] : touched_) *p = v;
        }
    };

}

#endif //GORBE_SCRIPT_RUNTIME_HPP
