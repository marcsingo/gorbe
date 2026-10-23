// gorbe_api.hpp — az alakzatokhoz írt C++ kód felülete.
//
// A gorbe program írja ki a projekt mappájába: NE szerkeszd, minden Indításkor
// felülíródik. Egy alakzat kódja egy osztály, ami a ShapeScript-ből származik:
//
//     #include "../gorbe_api.hpp"
//
//     struct Gomb1 : gorbe::ShapeScript {
//         float* r = nullptr;
//         void start() override {                    // Indításkor, egyszer
//             r = &param("r");                       // a GUI-n beállított a kezdőérték
//             add_warp("x + 0.3*sin(3*y)", "y", "z");
//         }
//         void update(float t, float dt) override {  // minden ciklusban
//             *r = 2.0f + 0.5f * std::sin(t);
//             rotation().z += 0.5f * dt;
//         }
//     };
//     GORBE_SHAPE(Gomb1)
#ifndef GORBE_API_HPP
#define GORBE_API_HPP

#include <cmath>

#if defined(_WIN32)
#  define GORBE_EXPORT extern "C" __declspec(dllexport)
#else
#  define GORBE_EXPORT extern "C" __attribute__((visibility("default")))
#endif

namespace gorbe {

inline constexpr int API_VERSION = 1;

struct vec3 { float x = 0, y = 0, z = 0; };
static_assert(sizeof(vec3) == 3 * sizeof(float), "vec3 = float[3]");

// --- a határ: a program tölti ki ------------------------------------------------
// Csak C-típusok és függvénymutatók, így a kód a program belső fejléceit nem látja.
// A `ctx` átlátszatlan: a program saját, alakzatonkénti állapota.
//
// ponytail: a ShapeScript virtuális hívásai a C++ ABI-n mennek át, tehát a DLL-t
// ugyanazzal a fordítóval kell fordítani, mint a programot (MSVC <-> MSVC,
// g++ <-> g++). Ha ez kevés, a GORBE_SHAPE exportálhat C-s start/update hívókat is.
struct Host {
    int   version;                                                   // == API_VERSION
    void* ctx;
    float* (*param)(void* ctx, char const* name);                    // nullptr: nincs / képletes
    float* (*add_param)(void* ctx, char const* name, float value);   // csak start()
    float* (*transform)(void* ctx, int which);                       // 0 pos, 1 rot, 2 scale
    bool   (*add_warp)(void* ctx, char const* fx, char const* fy, char const* fz); // csak start()
    bool   (*set_formula)(void* ctx, char const* formula);           // csak start()
    void   (*error)(void* ctx, char const* msg);
};

// --- amiből a felhasználó származtat --------------------------------------------
class ShapeScript {
public:
    virtual ~ShapeScript() = default;

    // Egyszer, az Indításkor, a képletek felépítése ELŐTT: itt szabad szerkezetet
    // állítani (warp, képlet, új paraméter). A paraméterek és a pozíció ekkor még
    // a GUI-n beállított KEZDŐÉRTÉKEN állnak.
    virtual void start() {}

    // Minden ciklusban, a fő szálon, a részecskék léptetése előtt. Itt csak ÉRTÉKET
    // állíts (paraméter, pozíció, forgatás, méret): ez újraépítés nélkül, élőben hat.
    // `t` a program ideje (mint a képletekben), `dt` az előző ciklus óta eltelt idő.
    virtual void update(float t, float dt) { (void)t; (void)dt; }

    void bind(Host const* h) { host_ = h; }

protected:
    // A paraméter VÁLTOZÓJA: a képletbe a címe épül be, így az írás azonnal látszik.
    // Az alakzat saját, a jelenet és a program paraméterei közt keres (ebben a
    // sorrendben). A cím a futás végéig érvényes, nyugodtan eltehető a start()-ban.
    float& param(char const* name) { return deref(host_->param(host_->ctx, name)); }

    // Új paraméter, csak erre a futásra (nem mentődik). Ha már van ilyen nevű, azt
    // adja vissza a GUI-n beállított értékével — `value` csak az alapérték.
    float& add_param(char const* name, float value) {
        return deref(host_->add_param(host_->ctx, name, value));
    }

    vec3& position() { return *reinterpret_cast<vec3*>(host_->transform(host_->ctx, 0)); }
    vec3& rotation() { return *reinterpret_cast<vec3*>(host_->transform(host_->ctx, 1)); } // radián
    vec3& scale()    { return *reinterpret_cast<vec3*>(host_->transform(host_->ctx, 2)); }
    void  position(vec3 p) { position() = p; }

    // A warpok a GUI-n felvettek UTÁN hatnak; a jelentésük ugyanaz (visszafelé
    // leképezés, lásd a program súgóját).
    bool add_warp(char const* fx, char const* fy, char const* fz) {
        return host_->add_warp(host_->ctx, fx, fy, fz);
    }
    bool formula(char const* f) { return host_->set_formula(host_->ctx, f); }

    void error(char const* msg) { host_->error(host_->ctx, msg); }

private:
    Host const* host_ = nullptr;

    // Hiányzó paraméternél a program már jelezte a hibát; addig egy helyi változóba
    // írunk, hogy a kód ne omoljon össze.
    float& deref(float* p) { return p ? *p : dummy_; }
    float dummy_ = 0.0f;
};

} // namespace gorbe

// Egy alakzat-osztály regisztrálása. A program a fájl nevén (= az osztály neve)
// keresi a gyártófüggvényt. A new/delete a DLL-en BELÜL marad: Windowson a DLL-nek
// és a programnak külön heapje lehet.
#define GORBE_SHAPE(Class)                                                           \
    GORBE_EXPORT int gorbe_api_version_##Class() { return gorbe::API_VERSION; }      \
    GORBE_EXPORT gorbe::ShapeScript* gorbe_create_##Class() { return new Class; }    \
    GORBE_EXPORT void gorbe_destroy_##Class(gorbe::ShapeScript* s) { delete s; }

#endif // GORBE_API_HPP
