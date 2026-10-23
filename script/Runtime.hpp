#ifndef GORBE_SCRIPT_RUNTIME_HPP
#define GORBE_SCRIPT_RUNTIME_HPP

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <exception>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../scene/Document.hpp"
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
                report(who + ": nincs GORBE_SHAPE(" + s.script + ") a kodban");
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
                       &h_add_warp, &h_set_formula, &h_error};
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

        void update_all(float t, float dt) {
            for (auto& it : items_) call(*it, [&] { it->obj->update(t, dt); });
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

        static std::string label(Shape const& s) {
            return std::string(s.name[0] ? s.name : "(nevtelen)") + " [" + s.script + "]";
        }

        void report(std::string msg) {
            if (std::find(errors.begin(), errors.end(), msg) == errors.end())
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

        static Param* find(Item& it, char const* name) {
            for (auto* lvl : {&it.shape->locals, &it.shape->script_params, &it.scene->params, it.program})
                for (auto& p : *lvl)
                    if (name && p.name[0] && std::strcmp(p.name, name) == 0) return &p;
            return nullptr;
        }

        static float* h_param(void* ctx, char const* name) {
            Item& it = item(ctx);
            Param* p = find(it, name);
            if (!p) {
                it.owner->report(it.who + ": nincs '" + (name ? name : "") + "' parameter");
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
            if (find(it, name)) return h_param(ctx, name);   // a GUI-n beállított nyer
            Param& p = it.shape->script_params.emplace_back();
            std::snprintf(p.name, sizeof(p.name), "%s", name);
            p.value = value;
            p.fit_range_to_value();
            return &p.value;
        }

        static float* h_transform(void* ctx, int which) {
            Item& it = item(ctx);
            TransformParams& x = it.shape->xform;
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

        void restore() {
            for (auto const& [p, v] : touched_) *p = v;
        }
    };

}

#endif //GORBE_SCRIPT_RUNTIME_HPP
