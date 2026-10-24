// gorbe_api.hpp — az alakzatokhoz írt C++ kód felülete.
//
// A gorbe program írja ki a projekt mappájába: NE szerkeszd, minden Indításkor
// felülíródik. Egy alakzat kódja egy osztály, ami a ShapeScript-ből származik:
//
//     #include "../gorbe_api.hpp"
//
//     struct Gomb1 : gorbe::ShapeScript {
//         float* r = nullptr;
//         gorbe::ShapeRef fal;
//         void start() override {                    // Indításkor, egyszer
//             r = &param("r");                       // a GUI-n beállított a kezdőérték
//             fal = shape("Fal");                    // egy másik alakzat
//             add_warp("x + 0.3*sin(3*y)", "y", "z");
//         }
//         void update(float t, float dt) override {  // minden ciklusban
//             *r = 2.0f + 0.5f * std::sin(t);
//             if (gorbe::Hit h = collide(fal))       // ütközés: kitolja magát
//                 position() += h.normal * h.depth;
//             print("r = %.2f", *r);                 // a konzolra
//         }
//     };
//     GORBE_SHAPE(Gomb1)
#ifndef GORBE_API_HPP
#define GORBE_API_HPP

#include <cmath>
#include <cstdarg>
#include <cstdio>

#if defined(_WIN32)
#  define GORBE_EXPORT extern "C" __declspec(dllexport)
#else
#  define GORBE_EXPORT extern "C" __attribute__((visibility("default")))
#endif

namespace gorbe {

// 2: Host::log (a konzolra írás, ShapeScript::print)
// 3: más alakzatok (ShapeRef), F kiértékelése, ütközés; a Host függvényei célpontot kapnak
// 4: bemenet (billentyűzet, egér) és a jelenet kamerája (CameraRef)
// 5: másolat (clone, clone_start, clone_update) és láthatóság (visible)
inline constexpr int API_VERSION = 5;

// --- vektor -----------------------------------------------------------------------
struct vec3 { float x = 0, y = 0, z = 0; };
static_assert(sizeof(vec3) == 3 * sizeof(float), "vec3 = float[3]");

inline vec3  operator+(vec3 a, vec3 b)   { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline vec3  operator-(vec3 a, vec3 b)   { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline vec3  operator-(vec3 a)           { return {-a.x, -a.y, -a.z}; }
inline vec3  operator*(vec3 a, float s)  { return {a.x * s, a.y * s, a.z * s}; }
inline vec3  operator*(float s, vec3 a)  { return a * s; }
inline vec3  operator/(vec3 a, float s)  { return {a.x / s, a.y / s, a.z / s}; }
inline vec3& operator+=(vec3& a, vec3 b) { return a = a + b; }
inline vec3& operator-=(vec3& a, vec3 b) { return a = a - b; }
inline vec3& operator*=(vec3& a, float s) { return a = a * s; }
inline float dot(vec3 a, vec3 b)         { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float length(vec3 a)              { return std::sqrt(dot(a, a)); }
inline vec3  normalize(vec3 a)           { float const l = length(a); return l > 0 ? a / l : vec3{}; }

struct vec2 { float x = 0, y = 0; };

// Egy sugár: kezdőpont és egységnyi irány (pl. a kamerából az egér alatti pont felé).
struct Ray { vec3 origin, dir; };

// --- bemenet ------------------------------------------------------------------------
// Billentyűkódok: a betűk és a számok a NAGYBETŰS ASCII kódjukkal ('W', 'A', '1'; a
// kisbetűt is elfogadjuk), a többi az alábbi néven. (A számok a GLFW kódjai.)
inline constexpr int KEY_SPACE = 32, KEY_ESCAPE = 256, KEY_ENTER = 257, KEY_TAB = 258,
                     KEY_BACKSPACE = 259, KEY_RIGHT = 262, KEY_LEFT = 263, KEY_DOWN = 264,
                     KEY_UP = 265, KEY_LEFT_SHIFT = 340, KEY_LEFT_CONTROL = 341,
                     KEY_LEFT_ALT = 342, KEY_RIGHT_SHIFT = 344, KEY_RIGHT_CONTROL = 345;
inline constexpr int MOUSE_LEFT = 0, MOUSE_RIGHT = 1, MOUSE_MIDDLE = 2;

// A bemenet egy ciklusra (a program tölti; a ShapeScript függvényei olvassák).
// Bitek billentyűnként és egérgombonként: 1 = lenyomva, 2 = ebben a ciklusban nyomták
// le, 4 = ebben a ciklusban engedték fel.
struct InputState {
    static constexpr int KEYS = 512, BUTTONS = 8;
    unsigned char keys[KEYS] = {};
    unsigned char buttons[BUTTONS] = {};
    float mouse_x = 0, mouse_y = 0;       // képpont a 3D nézetben (bal felső sarok = 0, 0)
    float mouse_dx = 0, mouse_dy = 0;     // elmozdulás az előző ciklus óta
    float scroll = 0;                     // görgetés ebben a ciklusban
    float view_w = 1, view_h = 1;         // a 3D nézet mérete képpontban
    float ray_origin[3] = {}, ray_dir[3] = {0, 0, -1};   // a kamerából az egér alá
    bool  active = false;                 // a jelenet kapja-e a bemenetet (fut + jelenet kamera)
};

// Két alakzat ütközése (ShapeRef::collide). `normal`: amerre az ELSŐ alakzatot tolni
// kell, hogy kiszabaduljon; `depth`: a legmélyebb behatolás (becsült távolság);
// `point`: hol; `count`: hány mintapont van a másikban. Feltételként is használható:
//     if (gorbe::Hit h = collide(fal)) position() += h.normal * h.depth;
struct Hit {
    bool  hit = false;
    int   count = 0;
    vec3  point, normal;
    float depth = 0.0f;
    explicit operator bool() const { return hit; }
};

// --- a határ: a program tölti ki ------------------------------------------------
// Csak C-típusok és függvénymutatók, így a kód a program belső fejléceit nem látja.
// A `ctx` átlátszatlan: a program saját, alakzatonkénti állapota. A `target` egy
// alakzat (find_shape / shape_at adja); nullptr = a kód SAJÁT alakzata.
//
// ponytail: a ShapeScript virtuális hívásai a C++ ABI-n mennek át, tehát a DLL-t
// ugyanazzal a fordítóval kell fordítani, mint a programot (MSVC <-> MSVC,
// g++ <-> g++). Ha ez kevés, a GORBE_SHAPE exportálhat C-s start/update hívókat is.
struct Host {
    int   version;                                                   // == API_VERSION
    void* ctx;
    float* (*param)(void* ctx, void* target, char const* name);      // nullptr: nincs / képletes
    float* (*add_param)(void* ctx, char const* name, float value);   // csak start()
    float* (*transform)(void* ctx, void* target, int which);         // 0 pos, 1 rot, 2 scale
    bool   (*add_warp)(void* ctx, char const* fx, char const* fy, char const* fz); // csak start()
    bool   (*set_formula)(void* ctx, char const* formula);           // csak start()
    void   (*error)(void* ctx, char const* msg);
    void   (*log)(void* ctx, char const* msg);                       // a konzolra
    void*  (*find_shape)(void* ctx, char const* name);               // nullptr: nincs ilyen
    int    (*shape_count)(void* ctx);
    void*  (*shape_at)(void* ctx, int index);
    char const* (*shape_name)(void* ctx, void* target);
    // F és a gradiense egy világ-pontban: out = {F, dF/dx, dF/dy, dF/dz}, és hogy a pont
    // a tartomány-feltételen belül van-e. Hamis, ha még nincs felépítve (start()-ban).
    bool   (*eval)(void* ctx, void* target, float const p[3], float out[4], bool* in_domain);
    // Ütközés: out = {pont(3), normális(3), mélység}; a visszatérés a pontok száma.
    int    (*collide)(void* ctx, void* a, void* b, float out[7]);
    InputState const* (*input)(void* ctx);                          // a mostani ciklus bemenete
    float* (*camera)(void* ctx, int which);                          // 0 pos, 1 yaw/pitch/roll, 2 fov
    void*  (*clone)(void* ctx, void* target);                        // az új alakzat; nullptr: hiba
    bool*  (*visible)(void* ctx, void* target);
    bool   is_clone;                                                 // ez a példány egy másolaté
};

// --- a jelenet kamerája ------------------------------------------------------------
// Futás közben ebből látunk. Ugyanúgy írható, mint egy alakzat pozíciója: a GUI-n
// beállított a kezdőérték, a futás végén visszaáll.
//   position()  a kamera helye
//   rotation()  x = yaw (azimut, 0 = +x felé), y = pitch (emelkedés, negatív = lefelé
//               néz, ±89°), z = roll (dőlés) — RADIÁNBAN
//   fov()       a függőleges látószög FOKBAN (5..120)
class CameraRef {
public:
    CameraRef() = default;
    explicit CameraRef(Host const* h) : host_(h) {}

    vec3&  position() { return vec(0); }
    vec3&  rotation() { return vec(1); }
    float& fov()      { float* p = host_ ? host_->camera(host_->ctx, 2) : nullptr; return p ? *p : dummy_[0]; }

    // A nézőirány és a kép jobbra/felfelé iránya a világban (a dőléssel együtt).
    vec3 forward() { vec3 const r = rotation(); return dir(r.x, r.y); }
    vec3 right() {
        vec3 const r = rotation(), f = dir(r.x, r.y);
        vec3 const rr = normalize(vec3{f.y, -f.x, 0}), u = cross(rr, f);   // f x (0,0,1), majd felfelé
        return rr * std::cos(r.z) + u * std::sin(r.z);
    }
    vec3 up() {
        vec3 const r = rotation(), f = dir(r.x, r.y);
        vec3 const rr = normalize(vec3{f.y, -f.x, 0}), u = cross(rr, f);
        return u * std::cos(r.z) - rr * std::sin(r.z);
    }

    // A `target` felé fordul (a helye és a dőlése marad).
    void look_at(vec3 target) {
        vec3 const d = normalize(target - position());
        if (length(d) == 0) return;
        rotation().x = std::atan2(d.y, d.x);
        rotation().y = std::asin(d.z < -1 ? -1 : d.z > 1 ? 1 : d.z);
    }

private:
    Host const* host_ = nullptr;
    float dummy_[3] = {0, 0, 0};

    vec3& vec(int which) {
        float* p = host_ ? host_->camera(host_->ctx, which) : nullptr;
        return *reinterpret_cast<vec3*>(p ? p : dummy_);
    }
    static vec3 cross(vec3 a, vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
    static vec3 dir(float yaw, float pitch) {
        float const lim = 1.5533430f;      // ±89°, mint a programban
        pitch = pitch < -lim ? -lim : pitch > lim ? lim : pitch;
        return {std::cos(yaw) * std::cos(pitch), std::sin(yaw) * std::cos(pitch), std::sin(pitch)};
    }
};

// --- egy alakzat (a saját vagy egy másik) -----------------------------------------
// Kicsi érték, nyugodtan eltehető tagváltozóba (pl. a start()-ban). Ha a keresett
// alakzat nem létezik, érvénytelen: `if (masik)` hamis, a hívásai nem csinálnak semmit.
//
// Ugyanaz érhető el rajta, mint a saját alakzaton: paraméter, pozíció, forgatás,
// méret (írható is — a futás végén ezek is visszaállnak a kezdőértékre), és az
// implicit függvénye: F(p) < 0 BELÜL, F(p) = 0 a felületen, F(p) > 0 kívül.
class ShapeRef {
public:
    ShapeRef() = default;
    ShapeRef(Host const* h, void* target, bool self) : host_(h), target_(target), self_(self) {}

    explicit operator bool() const { return host_ && (self_ || target_); }

    char const* name() const { return *this ? host_->shape_name(host_->ctx, t()) : ""; }

    // A paraméter VÁLTOZÓJA, az alakzat hatókörében (lokális -> jelenet -> program).
    float& param(char const* name) { return deref(*this ? host_->param(host_->ctx, t(), name) : nullptr); }

    vec3& position() { return vec(0); }
    vec3& rotation() { return vec(1); }      // radián
    vec3& scale()    { return vec(2); }

    // Látható-e. A rejtett alakzat nem látszik (a fényképen sem), és nem ütközik:
    // a collide() rá és vele hamisat ad. A futás végén ez is visszaáll.
    bool& visible() {
        bool* p = *this ? host_->visible(host_->ctx, t()) : nullptr;
        return p ? *p : dummy_vis_;
    }

    // Másolat erről az alakzatról, erre a futásra (nem mentődik, a futás végén
    // törlődik). Ugyanaz a képlet, paraméter-értékek, warpok és transzformáció, mint
    // az eredetinek MOST; a részecskéi is onnan indulnak. Ha az alakzatnak van kódja,
    // a másolat saját példányt kap belőle, amiben a start()/update() HELYETT a
    // clone_start()/clone_update() fut. A visszakapott ShapeRef-fel rögtön írható
    // (pl. a helye): a másolat a ciklus végén épül fel, már az új értékekkel.
    ShapeRef clone() { return {host_, *this ? host_->clone(host_->ctx, t()) : nullptr, false}; }

    // Az implicit függvény a világ egy pontjában. A start()-ban még nem érhető el
    // (a képletek ott épülnek fel): ilyenkor NaN, illetve nullvektor.
    float value(vec3 p) const {
        float out[4];
        return eval(p, out, nullptr) ? out[0] : NAN;
    }
    vec3 gradient(vec3 p) const {
        float out[4];
        return eval(p, out, nullptr) ? vec3{out[1], out[2], out[3]} : vec3{};
    }
    // Belül van-e a pont: F(p) < 0, és a tartomány-feltételen belül.
    bool contains(vec3 p) const {
        float out[4];
        bool dom = false;
        return eval(p, out, &dom) && out[0] < 0.0f && dom;
    }
    // Előjeles távolság-becslés a felülettől: F / |∇F| (felület közelében pontos;
    // negatív = belül).
    float distance(vec3 p) const {
        float out[4];
        if (!eval(p, out, nullptr)) return NAN;
        float const g = std::sqrt(out[1] * out[1] + out[2] * out[2] + out[3] * out[3]);
        return g > 1e-12f ? out[0] / g : out[0];
    }

    // Ütközik-e EZ az alakzat a másikkal. Az implicit egyenletre épül: ennek a
    // részecskéi a felületén ülnek, és amelyikre a másik F-je negatív, az a másikban
    // van (és fordítva, a másik részecskéi ebben). Csak futó alakzatokon működik.
    // Önmagával sosem ütközik (így minden alakzaton végig lehet menni).
    Hit collide(ShapeRef const& other) const {
        Hit h;
        if (!*this || !other) return h;
        float out[7];
        h.count = host_->collide(host_->ctx, t(), other.t(), out);
        h.hit = h.count > 0;
        if (h.hit) {
            h.point  = {out[0], out[1], out[2]};
            h.normal = {out[3], out[4], out[5]};
            h.depth  = out[6];
        }
        return h;
    }

private:
    Host const* host_ = nullptr;
    void* target_ = nullptr;
    bool  self_ = false;
    float dummy_[3] = {0, 0, 0};    // hiba esetén ide írunk, hogy a kód ne omoljon össze
    bool  dummy_vis_ = true;

    void* t() const { return self_ ? nullptr : target_; }
    float& deref(float* p) { return p ? *p : dummy_[0]; }
    vec3& vec(int which) {
        float* p = *this ? host_->transform(host_->ctx, t(), which) : nullptr;
        return *reinterpret_cast<vec3*>(p ? p : dummy_);
    }
    bool eval(vec3 p, float out[4], bool* dom) const {
        float const q[3] = {p.x, p.y, p.z};
        return *this && host_->eval(host_->ctx, t(), q, out, dom);
    }
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

    // A MÁSOLAT példányán (clone()) a start()/update() helyett ezek futnak. A
    // clone_start() egyszer, a clone() hívásakor (még a felépítés előtt, tehát itt
    // is szabad add_warp, formula, add_param); a clone_update() minden ciklusban.
    // A tagváltozók az alapértéküken indulnak, nem az eredeti példányéból másolódnak.
    virtual void clone_start() {}
    virtual void clone_update(float t, float dt) { (void)t; (void)dt; }

    void bind(Host const* h) { host_ = h; self_ = ShapeRef(h, nullptr, true); camera_ = CameraRef(h); }

protected:
    // A saját alakzat (ugyanaz, mint a lenti rövidítések).
    ShapeRef& self() { return self_; }

    // Egy másik alakzat ugyanebben a jelenetben, név szerint. Ha nincs ilyen, hibát ír,
    // és érvénytelen ShapeRef jön vissza. Elég egyszer lekérni (pl. a start()-ban).
    // FONTOS: a start()-ban kérd le. Csak így lehet a pozícióját/forgatását/méretét
    // írni is: az update()-ben először lekért alakzat transzformációja (ha egység volt)
    // nem épült be a képletébe, és az írás nem látszana (erről a konzol szól).
    ShapeRef shape(char const* name) { return {host_, host_->find_shape(host_->ctx, name), false}; }

    // A jelenet összes alakzata, sorrendben (a saját is köztük van):
    //     for (int i = 0; i < shape_count(); ++i) { ShapeRef s = shape(i); ... }
    int      shape_count()  { return host_->shape_count(host_->ctx); }
    ShapeRef shape(int i)   { return {host_, host_->shape_at(host_->ctx, i), false}; }

    // A paraméter VÁLTOZÓJA: a képletbe a címe épül be, így az írás azonnal látszik.
    // Az alakzat saját, a jelenet és a program paraméterei közt keres (ebben a
    // sorrendben). A cím a futás végéig érvényes, nyugodtan eltehető a start()-ban.
    float& param(char const* name) { return self_.param(name); }

    // Új paraméter, csak erre a futásra (nem mentődik). Ha már van ilyen nevű, azt
    // adja vissza a GUI-n beállított értékével — `value` csak az alapérték.
    float& add_param(char const* name, float value) {
        float* p = host_->add_param(host_->ctx, name, value);
        return p ? *p : dummy_;
    }

    vec3& position() { return self_.position(); }
    vec3& rotation() { return self_.rotation(); }    // radián
    vec3& scale()    { return self_.scale(); }
    void  position(vec3 p) { position() = p; }
    bool& visible()  { return self_.visible(); }

    // Másolat a saját alakzatról (lásd ShapeRef::clone); más alakzatról: shape("B").clone().
    ShapeRef clone() { return self_.clone(); }
    // Ez a példány egy másolaté-e (a clone_start/clone_update fut benne).
    bool is_clone() const { return host_ && host_->is_clone; }

    // Ütközés a saját alakzat és egy másik között (lásd ShapeRef::collide).
    Hit collide(ShapeRef const& other) { return self_.collide(other); }

    // --- a jelenet kamerája (lásd CameraRef) ---------------------------------------
    CameraRef& camera() { return camera_; }

    // --- bemenet ---------------------------------------------------------------------
    // Csak FUTÁS közben és a jelenet kamerájának nézetében kapja a kód (a szerkesztő
    // kamera nézetében a billentyűk a kamerát mozgatják); máskor minden hamis / nulla.
    // Csak akkor, ha az egér a 3D nézet fölött van, és nem egy mezőbe gépelsz.
    //   key_down('W')            lenyomva van (folyamatos mozgáshoz)
    //   key_pressed(KEY_SPACE)   ebben a ciklusban nyomták le (egyszeri akcióhoz)
    //   key_released(...)        ebben a ciklusban engedték fel
    bool key_down(int k)     { return key_bits(k) & 1; }
    bool key_pressed(int k)  { return key_bits(k) & 2; }
    bool key_released(int k) { return key_bits(k) & 4; }
    bool mouse_down(int b)     { return button_bits(b) & 1; }   // MOUSE_LEFT / RIGHT / MIDDLE
    bool mouse_pressed(int b)  { return button_bits(b) & 2; }
    bool mouse_released(int b) { return button_bits(b) & 4; }
    vec2  mouse_position() { InputState const& s = input(); return {s.mouse_x, s.mouse_y}; }
    vec2  mouse_delta()    { InputState const& s = input(); return {s.mouse_dx, s.mouse_dy}; }
    float scroll()         { return input().scroll; }
    vec2  view_size()      { InputState const& s = input(); return {s.view_w, s.view_h}; }
    bool  input_active()   { return input().active; }
    // Sugár a kamerából az egér alatti pont felé (kattintással kiválasztáshoz):
    //     Ray r = mouse_ray();  vec3 p = r.origin + r.dir * t;
    Ray mouse_ray() {
        InputState const& s = input();
        return {{s.ray_origin[0], s.ray_origin[1], s.ray_origin[2]},
                {s.ray_dir[0], s.ray_dir[1], s.ray_dir[2]}};
    }

    // A warpok a GUI-n felvettek UTÁN hatnak; a jelentésük ugyanaz (visszafelé
    // leképezés, lásd a program súgóját).
    bool add_warp(char const* fx, char const* fy, char const* fz) {
        return host_->add_warp(host_->ctx, fx, fy, fz);
    }
    bool formula(char const* f) { return host_->set_formula(host_->ctx, f); }

    // Hiba: az Alakzatok panelre és a konzolra, pirossal (ugyanaz az üzenet egyszer).
    void error(char const* msg) { host_->error(host_->ctx, msg); }

    // Egy sor a konzolra (a jelenet alatt), printf-formátummal:
    //     print("r = %.2f, t = %.1f", r, t);
    // Egymás után ugyanaz a sor nem ismétlődik, csak a számlálója nő. (Nem `log` a
    // neve, mert az elfedné a <cmath> log() függvényét az osztályon belül.)
#if defined(__GNUC__)
    __attribute__((format(printf, 2, 3)))
#endif
    void print(char const* fmt, ...) {
        char buf[1024];
        va_list args;
        va_start(args, fmt);
        std::vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        host_->log(host_->ctx, buf);
    }

private:
    Host const* host_ = nullptr;
    ShapeRef self_;
    CameraRef camera_;
    float dummy_ = 0.0f;       // hiányzó paraméternél ide írunk (a program már jelezte)

    InputState const& input() {
        static InputState const none{};
        InputState const* s = host_->input(host_->ctx);
        return s ? *s : none;
    }
    int key_bits(int k) {
        if (k >= 'a' && k <= 'z') k -= 'a' - 'A';
        return k >= 0 && k < InputState::KEYS ? input().keys[k] : 0;
    }
    int button_bits(int b) { return b >= 0 && b < InputState::BUTTONS ? input().buttons[b] : 0; }
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
