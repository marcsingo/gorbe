#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <list>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "App.hpp"

// A Kif / make_kif / Kifejezes a Matek::Analizis névtérből jön (a Surface.hpp
// globális `using namespace`-e miatt közvetlenül elérhető).
#include "scene/Scope.hpp"
#include "particle_sampling/Surface.hpp"
#include "particle_sampling/ImplicitSurface.hpp"
#include "particle_sampling/Transform.hpp"
#include "particle_sampling/WarpPresets.hpp"

// --- Sugarkoveto komponens (onallo, levalaszthato: lasd raytrace/Raytracer.hpp) ---
#include <filesystem>
#include "model/Viewport.hpp"
#include "raytrace/Raytracer.hpp"
#include "raytrace/Image.hpp"

// A bináris könyvtára (a CMake adja meg); ide mentjük a fényképeket.
#ifndef BINARY_DIR
#define BINARY_DIR "."
#endif

#include "imgui.h"

// ---------------------------------------------------------------------------
// Jelenet-modell
//
// Egy ALAKZAT = név + implicit képlet (F(x,y,z)=0) + saját, LOKÁLIS paraméterek.
// Emellett vannak GLOBÁLIS paraméterek, amiket minden alakzat lát.
//
// Névfeloldás a képletben: x/y/z a térbeli változók, minden más azonosítót
// ebben a sorrendben keresünk meg:
//   1. az alakzat saját LOKÁLIS paraméterei,
//   2. a GLOBÁLIS paraméterek,
//   3. egy, a listában KORÁBBAN álló alakzat neve (ekkor annak a teljes
//      kifejezésfája beépül ide — így lehet alakzatokat egymásból építeni).
//
// Ebből következik a névszabály: egy alakzat lokális neve nem ütközhet globális
// paraméterrel vagy alakzatnévvel, mert a képletben ugyanabban a névtérben élnek.
// Két KÜLÖNBÖZŐ alakzat lokális nevei viszont nyugodtan megegyezhetnek: azok
// külön scope-ok, a resolve mindig csak a saját alakzata lokálisait nézi.
//
// FONTOS az élettartam: a Parameter csomópont a `value` CÍMÉT tárolja
// (float const*), nem az értékét. Ezért kell std::list (a node-ok nem mozdulnak
// beszúráskor/törléskor), és ezért kell minden beparseolt képletet eldobni, ha
// egy paraméter vagy egy alakzat (a lokálisaival együtt) törlődik.
// ---------------------------------------------------------------------------

// Egy tér-warp: három kifejezés, amiket x, y, z helyére helyettesítünk.
// A jelentésük a VISSZAFELÉ (tér -> alakzat) leképezés — lásd Transform.hpp.
struct Warp {
    char name[32] = "";
    char fx[192]  = "x";
    char fy[192]  = "y";
    char fz[192]  = "z";
    bool enabled  = true;
};

struct Shape {
    char name[32]     = "";
    char formula[256] = "";
    // Opcionális tartomány-feltétel: a részecskék csak ott élnek, ahol ez teljesül
    // (pl. "x > 2 and x < 6"). Így lehet egy önmagában végtelen felületet — síkot,
    // hengert — véges darabon megjeleníteni, a CSG-vágás fedőlapjai nélkül.
    char domain[256]  = "";
    bool visible      = true;
    std::list<Param> locals;
    std::shared_ptr<Kifejezes const> tree;

    // Tér-transzformáció: az alakzatot nem mozgatjuk, hanem az inverz leképezést
    // helyettesítjük F-be (lásd particle_sampling/Transform.hpp). A mezők CÍME épül
    // be a kifejezésbe, ezért a csúszkák élőben mozgatják az alakzatot.
    TransformParams xform;
    // Beépült-e a warp az utolsó Indításkor? Egységtranszformációnál nem épül be
    // (hogy az egyszerű alakzatok olcsók maradjanak), ezért az első hozzányúláskor
    // újra kell építeni — különben a csúszka némán nem csinálna semmit.
    bool warped = false;

    // Warp-lánc. Az ELSŐ elem hat először az alakzatra; a lánc után jön a fenti
    // affin transzformáció, tehát a warpok az alakzat SAJÁT terében dolgoznak,
    // és a kész, deformált alakzatot helyezi el a pozíció/forgatás/méret.
    std::vector<Warp> warps;

    // --- a sugárkövető komponenshez ---
    // Szín a paletta-listából (Raytrace::PALETTE) és az Indításkor felépített,
    // KÉSZ tartomány-fa (globális ÉS saját, transzformálva) — hogy a fénykép
    // pontosan azt lássa, amit a szimuláció.
    int color_idx = 0;
    std::shared_ptr<Kifejezes const> dom_tree;
};

// ---------------------------------------------------------------------------
// Előre elkészített alakzatok (sablonok)
//
// Egy sablon = megjelenő név + az új alakzat nevének alapja + a képlet + a hozzá
// tartozó LOKÁLIS paraméterek kezdőértékkel. Hozzáadáskor ezekből egy teljesen
// közönséges alakzat születik, ami utána szabadon szerkeszthető.
//
// A képletek szándékosan POLINOMIÁLISAK (nincs bennük sqrt), ahol lehet: a
// sqrt(u) a parserben u^0.5-tá alakul, aminek a deriváltja u=0-ban végtelen —
// a felületen (F=0) ez pont a rossz hely lenne. A blend-sablonban a gyök alatt
// mindig van egy +k² tag, ezért ott biztonságos.
// ---------------------------------------------------------------------------
struct PresetParam {
    char const* name;
    float       value;
};

struct Preset {
    char const* group;       // csoportfejléc a lenyílóban
    char const* label;       // ami a lenyíló listában látszik
    char const* base_name;   // az új alakzat nevének alapja (gomb1, gomb2, ...)
    char const* formula;
    std::vector<PresetParam> params;
    char const* domain = ""; // opcionális tartomány-feltétel (üres = korlátlan)
};

// A halmazműveletek a listában ELŐTTE álló két alakzatra hivatkoznak `f1`/`f2` néven —
// ezeket a saját alakzataid nevére kell átírni. Az F<0 = belül konvenció miatt az unió
// min és a metszet max (a BlobTree-cikk fordítva írja, mert ott a potenciál belül nagy).
static std::vector<Preset> const PRESETS = {
    {"Alapalakzatok", "Gomb",                 "gomb",        "x^2 + y^2 + z^2 - r^2",
        {{"r", 1.0f}}},
    {"Alapalakzatok", "Ellipszoid",           "ellipszoid",  "x^2/a^2 + y^2/b^2 + z^2/c^2 - 1",
        {{"a", 3.0f}, {"b", 2.0f}, {"c", 1.0f}}},
    {"Alapalakzatok", "Torusz",               "torusz",      "(x^2 + y^2 + z^2 + R^2 - r^2)^2 - 4*R^2*(x^2 + y^2)",
        {{"R", 3.0f}, {"r", 1.0f}}},
    {"Alapalakzatok", "Ellipszis (ell. henger)", "ellipszis", "x^2/a^2 + y^2/b^2 - 1",
        {{"a", 3.0f}, {"b", 1.5f}}},
    {"Alapalakzatok", "Henger",               "henger",      "x^2 + y^2 - r^2",
        {{"r", 2.0f}}},
    {"Alapalakzatok", "Kup",                  "kup",         "x^2 + y^2 - a^2*z^2",
        {{"a", 1.0f}}},
    {"Alapalakzatok", "Hiperboloid (1 kopeny)", "hiperboloid", "x^2/a^2 + y^2/b^2 - z^2/c^2 - 1",
        {{"a", 1.0f}, {"b", 1.0f}, {"c", 1.0f}}},
    {"Alapalakzatok", "Lekerekitett kocka",   "kocka",       "x^4 + y^4 + z^4 - a^4",
        {{"a", 1.5f}}},

    // Onmagaban vegtelen feluletek, tartomany-feltetellel veges darabra szoritva.
    // A feltetel NEM epul be F-be: igy nyers szelu felulet-darabot kapunk, nem
    // egy zart test hatarat (ami a vagolapokat is tartalmazna).
    {"Vegtelen + tartomany", "Sik (negyzet darab)", "sik", "z", {{"m", 3.0f}},
        "x > 0 - m and x < m and y > 0 - m and y < m"},
    {"Vegtelen + tartomany", "Henger (veges hosszu)", "cso", "x^2 + y^2 - r^2",
        {{"r", 1.0f}, {"h", 3.0f}}, "z > 0 - h and z < h"},

    // Éles (C0) halmazműveletek: a varraton törés van, a gradiens ugrik.
    {"Eles muveletek", "Unio  (f1 U f2)",       "unio",       "unio(f1, f2)",       {}},
    {"Eles muveletek", "Metszet  (f1 ^ f2)",    "metszet",    "metszet(f1, f2)",    {}},
    {"Eles muveletek", "Kulonbseg  (f1 - f2)",  "kulonbseg",  "kulonbseg(f1, f2)",  {}},

    // Sima (C^inf) halmazműveletek: k a lekerekítés mértéke. Ezek gradiense a
    // varraton is véges, ezért a részecske-szimulációhoz ezek a biztonságosak.
    {"Sima muveletek", "Sima unio (blend)",     "sunio",      "sunio(f1, f2, k)",      {{"k", 0.5f}}},
    {"Sima muveletek", "Sima metszet",          "smetszet",   "smetszet(f1, f2, k)",   {{"k", 0.5f}}},
    {"Sima muveletek", "Sima kulonbseg",        "skulonbseg", "skulonbseg(f1, f2, k)", {{"k", 0.5f}}},
};

// A parser által lefoglalt nevek: a térbeli változók és a beépített függvények.
static bool is_reserved(char const* n) {
    static char const* const R[] = {
        "x", "y", "z",
        "sin", "cos", "tan", "tg", "ctg", "cot", "ln", "log", "sqrt", "abs", "sign",
        "min", "max", "unio", "union", "metszet", "intersect", "kulonbseg", "subtract",
        "smin", "smax", "sunio", "sunion", "smetszet", "sintersect", "skulonbseg", "ssubtract",
        "and", "or", "not", "pi"};
    for (auto r : R)
        if (std::strcmp(n, r) == 0) return true;
    return false;
}

// Azonosító-formátum: betűvel kezdődik, utána betű/szám/aláhúzás (ezt tudja a parser).
static bool bad_ident(char const* n) {
    if (!std::isalpha(static_cast<unsigned char>(n[0]))) return true;
    for (char const* c = n; *c; ++c)
        if (!std::isalnum(static_cast<unsigned char>(*c)) && *c != '_') return true;
    return false;
}

// Szabad "prefix + sorszám" név keresése (g1, g2, ... / p1, p2, ... / f1, f2, ...).
// Bármilyen `name` mezős elemekből álló konténerre működik.
template<class Container>
static void next_name(Container const& items, char const* prefix, char* out, size_t n) {
    for (int k = 1;; ++k) {
        char candidate[32];
        std::snprintf(candidate, sizeof(candidate), "%s%d", prefix, k);
        bool taken = false;
        for (auto const& it : items)
            if (std::strcmp(it.name, candidate) == 0) { taken = true; break; }
        if (!taken) { std::snprintf(out, n, "%s", candidate); return; }
    }
}


// ---------------------------------------------------------------------------
// Egy JELENET = egy fül. Önálló: saját alakzatok, saját paraméterek, saját munkatér,
// saját kamera és saját mintavételezők.
//
// A háttérben lévő fülek szimulációja ÁLL (a részecskék állapota megmarad, tehát
// visszaváltáskor onnan folytatódik), és a bevitelt sem kapják meg — enélkül minden
// fül kamerája együtt mozogna.
// ---------------------------------------------------------------------------
struct Scene {
    using EqSurface = ImplicitSurface<StringSurface>;

    char name[32] = "";

    // JELENET-szintű paraméterek. A hatókör kívülről befelé: program -> jelenet ->
    // alakzat; a belső ELFEDI a külsőt (mint C++-ban).
    std::list<Param> params;

    // A jelenet munkatere: az a térrész, amiben egyáltalán értelmezzük az alakzatokat.
    // Minden alakzatra érvényes, a saját tartomány-feltételével ÉS-kapcsolatban.
    char domain[256] = "";

    std::list<Shape> shapes;
    Shape* selected = nullptr;

    float d_ui    = 2.0f;   // közös méretskála minden samplerre
    float curv_ui = 1.0f;   // görbület-adaptív taszítás (0 = egyenletes)

    std::string error;      // parse-hiba az utolsó Indításból

    // Fülönként saját kamera: a nézet megmarad fülváltáskor.
    Camera3D camera{glm::vec4(0.0f, 0.0f, 1200.0f, 800.0f), App::DEFAULT_EYE, -90.0f, 45.0f};

    // Alakzatonként egy önálló mintavételező. A lista i-edik alakzata a pool i-edik
    // felületére kerül; a méret együtt mozog (az ImplicitSurface a destruktorában
    // leiratkozik az ablak eseményeiről, ezért szabadon megszüntethető).
    std::vector<std::unique_ptr<EqSurface>> pool;

    Scene() { camera.look_at(App::DEFAULT_EYE, glm::vec3(0.0f)); }

    // Aktív fül: fut a szimuláció és megkapja a bevitelt.
    void set_active(bool a) {
        camera.input_enabled = a;
        for (auto& p : pool) {
            p->set_running(a);
            p->set_input_enabled(a);
        }
    }

    void draw() {
        for (auto& p : pool) p->draw(camera);
    }
};

int main() {
    App app{1200, 800, "Particle sampling"};

    // PROGRAM-szintű paraméterek: minden jelenet (fül) látja őket. Ez a legkülső
    // hatókör; a jelenet- és az alakzat-szintű nevek elfedhetik (mint C++-ban).
    std::list<Param> program_params;

    // A jelenetek (fülek) és az éppen aktív. std::list, mert a Scene címe stabil kell
    // legyen: a mintavételezők és a kamera eseménykezelői rá mutatnak.
    std::list<Scene> scenes;
    Scene* cur = nullptr;
    {
        auto& s0 = scenes.emplace_back();
        std::snprintf(s0.name, sizeof(s0.name), "Jelenet 1");
        cur = &s0;
        cur->set_active(true);
    }

    int         preset_idx = 0;    // a sablon-lenyílóban kiválasztott alakzat

    // A sugárkövető komponens állapota (a gomb csak jelez; a render a GUI után fut,
    // hogy ne egy félig felépített ImGui-frame közben blokkoljuk a programot).
    bool        photo_requested = false;
    int         photo_size_idx  = 1;
    bool        photo_shadows   = true;
    std::string photo_status;

    // ---------------------------------------------------------------------
    // Fix elrendezes: a panelek nem lebegnek, hanem a foablak meretehez igazodnak.
    // A ket oldalso sav szelessege es a bennuk levo vizszintes osztas huzhato.
    // ---------------------------------------------------------------------
    struct Layout {
        float left_w   = 340.0f;   // bal sav szelessege
        float right_w  = 340.0f;   // jobb sav szelessege
        float left_top = 0.46f;    // a bal sav felso paneljenek aranya
        float right_top= 0.52f;
    } layout;

    constexpr float PAD   = 6.0f;   // panelek kozti res (ez egyben az elvalaszto is)
    constexpr float MINW  = 220.0f;

    // Egy vekony, huzhato elvalaszto sav. Sajat, keret nelkuli ImGui-ablak a resben:
    // igy pontosan ott fogja az egeret, ahol a hezag van, es nem zavarja a paneleket.
    auto splitter = [&](char const* id, ImVec2 pos, ImVec2 size, bool vertical,
                        float* value, float lo, float hi, float sign) {
        if (size.x < 1.0f || size.y < 1.0f) return;
        ImGui::SetNextWindowPos(pos);
        ImGui::SetNextWindowSize(size);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin(id, nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);
        ImGui::InvisibleButton("##grip", size);
        bool const hot = ImGui::IsItemHovered() || ImGui::IsItemActive();
        if (hot) ImGui::SetMouseCursor(vertical ? ImGuiMouseCursor_ResizeEW
                                                : ImGuiMouseCursor_ResizeNS);
        if (ImGui::IsItemActive()) {
            float d = vertical ? ImGui::GetIO().MouseDelta.x : ImGui::GetIO().MouseDelta.y;
            *value = std::clamp(*value + d * sign, lo, hi);
        }
        ImU32 col = hot ? ImGui::GetColorU32(ImGuiCol_SeparatorHovered)
                        : ImGui::GetColorU32(ImGuiCol_Separator);
        ImGui::GetWindowDrawList()->AddRectFilled(
            pos, ImVec2(pos.x + size.x, pos.y + size.y), col, 2.0f);
        ImGui::End();
        ImGui::PopStyleVar();
    };

    // Fix panel: nem mozgathato, nem atmeretezheto, nem csukhato ossze.
    auto fixed_panel = [](char const* title, ImVec2 pos, ImVec2 size) {
        ImGui::SetNextWindowPos(pos);
        ImGui::SetNextWindowSize(size);
        ImGui::Begin(title, nullptr,
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);
    };

    // A rajzolas mindig az AKTUALIS ful tartalmat mutatja, a sajat kamerajaval.
    app.set_draw([&] {
        app.get_axes().draw(cur->camera);
        cur->draw();
    });

    app.set_gui([&] {
        // Az AKTUALIS jelenet allapota. A lenti kod ezeken a neveken dolgozik, tehat
        // mindig a kivalasztott ful adatait szerkeszti.
        Scene& sc = *cur;
        auto& shapes        = sc.shapes;
        auto& selected      = sc.selected;
        auto& pool          = sc.pool;
        auto& scene_params  = sc.params;
        auto& scene_domain  = sc.domain;
        auto& d_ui          = sc.d_ui;
        auto& curv_ui       = sc.curv_ui;
        auto& error         = sc.error;

        std::vector<std::string> problems;  // névütközések emberi olvasásra
        std::set<void const*>    bad;       // a hibás sorok (Param*/Shape*) a piros jelzéshez

    // Minden képlet eldobása + a szimuláció leállítása. Akkor is KÖTELEZŐ hívni, ha
    // egy paraméter vagy alakzat törlődik: a már beparseolt fák a törölt float
    // CÍMÉT tárolják, onnantól felszabadított memóriára mutatnának.
    // A samplerek számát az alakzatokéhoz igazítja (kell-e új, vagy elhagyható egy).
        auto sync_pool = [&] {
            while (pool.size() > shapes.size()) pool.pop_back();
            while (pool.size() < shapes.size()) {
                // A jelenet SAJAT kameraja: a mintavetelezok kontrollpontjai ehhez
                // vetitenek vissza, tehat fulenkent kulon kell.
                auto p = std::make_unique<Scene::EqSurface>(sc.camera);
                p->set_manual_diameter(true);  // a d-t a GUI allitja
                p->clear();                    // indulaskor ures, allo
                pool.push_back(std::move(p));
            }
        };

        auto drop_all = [&] {
            for (auto& p : pool) {
            p->clear();
            p->get_surface().set_tree(Kif(0.0f).get());
            p->get_surface().clear_domain();
        }
        for (auto& s : shapes) { s.tree.reset(); s.dom_tree.reset(); }
    };

        // Névellenőrzés. Minden frame-ben lefut (néhány tucat név, elhanyagolható), így a
        // piros jelzés azonnal követi a gépelést, az Indít pedig tiltva marad, amíg baj van.
        //
        // HIBA csak azonos hatókörön BELÜL van (két azonos nevű paraméter ugyanabban a
        // listában, két azonos nevű alakzat). A hatókörök KÖZÖTTI azonos név nem hiba,
        // hanem ELFEDÉS — a belső nyer, mint C++-ban; ezt a panelen jelezzük.
        auto validate = [&] {
            problems.clear();
            bad.clear();

            auto check = [&](char const* n, void const* row, std::string const& where) {
                if (bad_ident(n)) {
                    problems.push_back(where + ": ervenytelen nev (betuvel kezdodjon, utana betu/szam/_)");
                    bad.insert(row);
                    return false;
                }
                if (is_reserved(n)) {
                    problems.push_back(where + ": a(z) '" + std::string(n) + "' foglalt nev (valtozo vagy fuggveny)");
                    bad.insert(row);
                    return false;
                }
                return true;
            };
            auto clash = [&](void const* a, void const* b, std::string const& msg) {
                problems.push_back(msg);
                bad.insert(a);
                bad.insert(b);
            };
            // Egy listán belüli ismétlődés keresése.
            auto unique_within = [&](std::list<Param>& list, char const* what) {
                for (auto a = list.begin(); a != list.end(); ++a) {
                    if (!check(a->name, &*a, what)) continue;
                    for (auto b = std::next(a); b != list.end(); ++b)
                        if (std::strcmp(a->name, b->name) == 0)
                            clash(&*a, &*b, std::string(what) + ": a(z) '" + a->name + "' ketszer szerepel");
                }
            };

            unique_within(program_params, "program-szintu parameter");
            unique_within(scene_params,   "jelenet parametere");

            // Alakzatnevek: a jeleneten belül egyediek.
            for (auto a = shapes.begin(); a != shapes.end(); ++a) {
                if (!check(a->name, &*a, "alakzat")) continue;
                for (auto b = std::next(a); b != shapes.end(); ++b)
                    if (std::strcmp(a->name, b->name) == 0)
                        clash(&*a, &*b, std::string("ket alakzat neve azonos: ") + a->name);
            }

            // Lokális paraméterek: alakzaton BELÜL egyediek. (Alakzatok között, és a
            // külső hatókörökkel szemben szabadon egyezhetnek — az elfedés.)
            for (auto& s : shapes) {
                std::string sn = s.name[0] ? s.name : "(nevtelen)";
                unique_within(s.locals, (sn + " lokalis parametere").c_str());
            }
        };

        // Egy név elfed-e egy KÜLSŐBB hatókört? Csak jelzésre, nem hiba.
        // (A paraméterek előbb oldódnak fel, mint az alakzatnevek, ezért egy paraméter
        //  egy azonos nevű alakzatot is elfed — ezt is kiírjuk.)
        // `level`: 0 = program, 1 = jelenet, 2 = alakzat. A Scope kifele varja a
        // szinteket, ezert megforditva adjuk at (legbelso eloszor).
        auto shadows = [&](char const* name, int level) -> char const* {
            std::vector<std::list<Param> const*> levels;
            if (level >= 2) levels.push_back(selected ? &selected->locals : nullptr);
            if (level >= 1) levels.push_back(&scene_params);
            levels.push_back(&program_params);
            int const from = (level >= 2) ? 0 : (level >= 1 ? 0 : 0);
            int const hit  = Scope::shadowed(levels, name, from);
            if (hit >= 0) {
                bool const outer_is_scene = (level >= 2 && hit == 1);
                return outer_is_scene ? "elfedi: jelenet" : "elfedi: program";
            }
            // A parameterek ELOBB oldodnak fel, mint az alakzatnevek, tehat egy
            // azonos nevu alakzatot is elfednek.
            for (auto& sh : shapes)
                if (sh.name[0] && std::strcmp(sh.name, name) == 0) return "elfedi: alakzat";
            return nullptr;
        };

    // Névfeloldó a parsernek: lokális paraméter -> globális paraméter -> korábbi alakzat.
        // HAROM HATOKOR, kifele haladva; az elso talalat nyer, tehat a belso ELFEDI
        // a kulsot (mint C++-ban):
        //     alakzat lokalisai -> jelenet parameterei -> program-szintuek
        // A vegen a NALA KORABBI alakzatok neve (igy lehet oket egymasbol epiteni).
        auto resolve = [&](Shape const& owner, std::string const& nm) -> std::shared_ptr<Kifejezes const> {
            if (float const* v = Scope::find({&owner.locals, &scene_params, &program_params},
                                             nm.c_str()))
                return Kif(v).get();
            for (auto& s : shapes) {
                if (&s == &owner) break;               // csak a nála korábbiakra hivatkozhat
                if (s.name[0] && nm == s.name && s.tree) return s.tree;
            }
            return nullptr;                            // ismeretlen név -> a parser hibát dob
        };

        // A jelenet munkatere alakzat-lokalist nem lathat (nincs "sajat" alakzata),
        // de a jelenet- es program-szintu parametereket igen.
        auto resolve_global = [&](std::string const& nm) -> std::shared_ptr<Kifejezes const> {
            if (float const* v = Scope::find({&scene_params, &program_params}, nm.c_str()))
                return Kif(v).get();
            return nullptr;
        };

    // Egy kifejezés "elhelyezése": előbb a warp-lánc (a lista sorrendjében, tehát az
    // első elem hat először az alakzatra), utána az affin transzformáció. Így a warpok
    // az alakzat SAJÁT terében dolgoznak, és a kész, deformált alakzatot mozgatja a
    // pozíció/forgatás/méret — ez az, amit egy modellezőtől elvárunk.
    auto place = [&](Kif f, Shape const& s) {
        for (auto const& w : s.warps) {
            if (!w.enabled) continue;
            auto r = [&](std::string const& nm) { return resolve(s, nm); };
            f = apply_warp(f, make_kif(w.fx, r), make_kif(w.fy, r), make_kif(w.fz, r));
        }
        return apply_transform(f, s.xform);
    };

    // Az összes alakzat beparseolása és elindítása (a lista sorrendjében, hogy a
    // későbbiek hivatkozhassanak a korábbiak már kész fájára).
    auto build_all = [&] {
        error.clear();
        sync_pool();
        try {
            // A globális tartományt előbb ÖNMAGÁBAN is beparseoljuk: így az itteni hiba
            // nem egy véletlenszerű alakzat nevével jelenik meg, és egyben ellenőrizzük,
            // hogy tényleg csak globális paramétert használ.
            if (scene_domain[0]) {
                try {
                    make_kif(scene_domain, resolve_global);
                } catch (std::exception const& e) {
                    throw std::runtime_error(std::string("globalis tartomany: ") + e.what());
                }
            }

            int i = 0;
            for (auto& s : shapes) {
                try {
                    s.tree = make_kif(s.formula,
                                      [&](std::string const& nm) { return resolve(s, nm); }).get();

                    // Tér-transzformáció: az alakzat saját képletén ÉS a saját
                    // tartományán is alkalmazzuk (a "véges hosszú henger" végei
                    // együtt mozogjanak a hengerrel), a GLOBÁLIS tartományon viszont
                    // NEM — az a világ munkatere, nem az alakzaté.
                    //
                    // A transzformált alakot tesszük vissza s.tree-be, hogy a rá
                    // HIVATKOZÓ későbbi alakzatok is a már elhelyezett formát lássák
                    // (két elhelyezett gömb uniója a helyükön legyen).
                    s.warped = !s.xform.is_identity();
                    s.tree = place(Kif(s.tree), s).get();
                    pool[i]->get_surface().set_tree(s.tree);

                    // Tartomány = GLOBÁLIS és a (transzformált) SAJÁT feltétel ÉS-kapcsolata.
                    Kif dom;
                    bool has_dom = false;
                    if (s.domain[0]) {
                        // A tartomány UGYANAZT a warp-láncot és transzformációt kapja,
                        // mint a képlet — így a levágott rész együtt mozog/deformálódik
                        // az alakzattal (a "véges hosszú henger" végei a hengerrel).
                        dom = place(
                            make_kif(s.domain, [&](std::string const& nm) { return resolve(s, nm); }),
                            s);
                        has_dom = true;
                    }
                    if (scene_domain[0]) {
                        Kif g = make_kif(scene_domain, resolve_global);
                        dom = has_dom ? kif_and(g, dom) : g;   // ÉS = min
                        has_dom = true;
                    }

                    if (has_dom) pool[i]->get_surface().set_domain(dom.get());
                    else         pool[i]->get_surface().clear_domain();
                    s.dom_tree = has_dom ? dom.get() : nullptr;   // a fénykép ezt használja
                } catch (std::exception const& e) {
                    throw std::runtime_error(std::string(s.name[0] ? s.name : "(nevtelen)")
                                             + ": " + e.what());
                }
                pool[i]->restart();   // saját kezdő részecskék + futó állapot
                ++i;
            }
        } catch (std::exception const& e) {
            error = e.what();
            drop_all();
        }
    };

    // Új alakzat felvétele sablonból: a képlet és a lokális paraméterek is bemásolódnak.
    // Innentől teljesen közönséges alakzat, szabadon szerkeszthető.
    auto add_preset = [&](Preset const& pr) {
        auto& s = shapes.emplace_back();
        next_name(shapes, pr.base_name, s.name, sizeof(s.name));
        s.color_idx = (static_cast<int>(shapes.size()) - 1) % Raytrace::PALETTE_COUNT;
        std::snprintf(s.formula, sizeof(s.formula), "%s", pr.formula);
        std::snprintf(s.domain,  sizeof(s.domain),  "%s", pr.domain);
        for (auto const& pp : pr.params) {
            auto& p = s.locals.emplace_back();
            std::snprintf(p.name, sizeof(p.name), "%s", pp.name);
            p.value = pp.value;
        }
        selected = &s;
    };

    // --- Warp-lánc szerkesztő ------------------------------------------------
    // A `rebuild`-et akkor állítja igazra, ha a lánc SZERKEZETE vagy egy kifejezés
    // szövege változott: ilyenkor újra kell parseolni. A warp PARAMÉTEREI viszont az
    // alakzat lokálisai, tehát cím szerint épülnek be — azokat a csúszka élőben
    // állítja, újraépítés nélkül.
    int warp_preset_idx = 0;
    auto draw_warps = [&](Shape& s, bool& rebuild) {
        if (!ImGui::CollapsingHeader("Warpok (lancban)", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::BeginCombo("##warpsablon", WarpPresets::ALL[warp_preset_idx].label)) {
            for (int k = 0; k < static_cast<int>(WarpPresets::ALL.size()); ++k) {
                bool sel = (k == warp_preset_idx);
                if (ImGui::Selectable(WarpPresets::ALL[k].label, sel)) warp_preset_idx = k;
                if (sel) ImGui::SetItemDefaultFocus();
                if (ImGui::IsItemHovered() && WarpPresets::ALL[k].hint[0])
                    ImGui::SetTooltip("%s", WarpPresets::ALL[k].hint);
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Warp hozzaad")) {
            auto const& wp = WarpPresets::ALL[warp_preset_idx];
            Warp w;
            std::snprintf(w.name, sizeof(w.name), "%s", wp.label);
            // A sablon paraméterei az alakzat lokálisai közé kerülnek, ütközésmentes
            // néven; a $1/$2 helyére ezek a nevek kerülnek a kifejezésekbe.
            std::vector<std::string> names;
            for (auto const& pp : wp.params) {
                auto& p = s.locals.emplace_back();
                next_name(s.locals, pp.name, p.name, sizeof(p.name));
                p.value = pp.value;
                names.emplace_back(p.name);
            }
            WarpPresets::fill_template(w.fx, sizeof(w.fx), wp.fx, names);
            WarpPresets::fill_template(w.fy, sizeof(w.fy), wp.fy, names);
            WarpPresets::fill_template(w.fz, sizeof(w.fz), wp.fz, names);
            s.warps.push_back(w);
            rebuild = true;
        }
        if (s.warps.empty())
            ImGui::TextDisabled("Nincs warp. A lancban az ELSO hat eloszor.");

        int move_from = -1, move_to = -1, erase = -1;
        for (int i = 0; i < static_cast<int>(s.warps.size()); ++i) {
            Warp& w = s.warps[i];
            ImGui::PushID(i);
            if (ImGui::Checkbox("##on", &w.enabled)) rebuild = true;
            ImGui::SameLine();
            bool open = ImGui::TreeNodeEx("##w", ImGuiTreeNodeFlags_DefaultOpen,
                                          "%d. %s", i + 1, w.name);
            ImGui::SameLine();
            if (ImGui::SmallButton("^") && i > 0)                        { move_from = i; move_to = i - 1; }
            ImGui::SameLine();
            if (ImGui::SmallButton("v") && i + 1 < (int)s.warps.size())  { move_from = i; move_to = i + 1; }
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) erase = i;

            if (open) {
                ImGui::SetNextItemWidth(-40.0f);
                ImGui::InputText("x' =", w.fx, sizeof(w.fx));
                if (ImGui::IsItemDeactivatedAfterEdit()) rebuild = true;
                ImGui::SetNextItemWidth(-40.0f);
                ImGui::InputText("y' =", w.fy, sizeof(w.fy));
                if (ImGui::IsItemDeactivatedAfterEdit()) rebuild = true;
                ImGui::SetNextItemWidth(-40.0f);
                ImGui::InputText("z' =", w.fz, sizeof(w.fz));
                if (ImGui::IsItemDeactivatedAfterEdit()) rebuild = true;
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        if (move_from >= 0) { std::swap(s.warps[move_from], s.warps[move_to]); rebuild = true; }
        if (erase >= 0)     { s.warps.erase(s.warps.begin() + erase);          rebuild = true; }

        ImGui::TextDisabled("A harom kifejezes a ter -> alakzat lekepezes:");
        ImGui::TextDisabled("'hol keressuk ki az alakzatot ehhez a ponthoz'.");
        ImGui::TextDisabled("Ezert a sablonok a deformacio INVERZET tartalmazzak.");
    };

    // Egy paramétertábla (név | érték | törlés). A globális és a lokális lista UI-ja
    // ugyanaz. Igazat ad vissza, ha törölt sort — ilyenkor a hívónak drop_all()-t KELL
    // hívnia, mert a beparseolt fák a felszabadított float címét tárolják.
        // `level`: 1 = jelenet, 2 = alakzat (a kulsobb hatokorok elfedesenek jelzesehez);
        // 0 = program-szint, ott nincs mit elfedni.
        auto draw_params = [&](std::list<Param>& list, char const* prefix, int level) -> bool {
            if (ImGui::Button("Uj parameter")) {
                auto& p = list.emplace_back();
                next_name(list, prefix, p.name, sizeof(p.name));
            }
            bool erased = false;
            for (auto it = list.begin(); it != list.end();) {
                ImGui::PushID(&*it);
                bool warn = bad.count(&*it) != 0;
                if (warn) ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.45f, 0.12f, 0.12f, 1.0f));
                ImGui::SetNextItemWidth(90.0f);
                ImGui::InputText("##nev", it->name, sizeof(it->name));
                if (warn) ImGui::PopStyleColor();
                ImGui::SameLine();
                ImGui::SetNextItemWidth(90.0f);
                ImGui::InputFloat("##ertek", &it->value);
                ImGui::SameLine();
                bool del = ImGui::Button("X");
                // Elfedes-jelzes: nem hiba, de ne legyen nema meglepetes.
                if (char const* sh = shadows(it->name, level)) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("%s", sh);
                }
                ImGui::PopID();
                if (del) { it = list.erase(it); erased = true; }
                else     { ++it; }
            }
            return erased;
        };


        validate();

        // --- az elrendezes kiszamitasa a foablak meretebol ---
        ImGuiViewport const* vp = ImGui::GetMainViewport();
        ImVec2 const O = vp->WorkPos;
        ImVec2 const S = vp->WorkSize;
        float  const max_side = std::max(MINW, (S.x - 3.0f * PAD - 320.0f) * 0.5f);
        layout.left_w  = std::clamp(layout.left_w,  MINW, max_side);
        layout.right_w = std::clamp(layout.right_w, MINW, max_side);

        float const inner_h = S.y - 3.0f * PAD;
        float const cx = O.x + PAD + layout.left_w + PAD;                 // kozep bal szele
        float const cw = S.x - layout.left_w - layout.right_w - 4.0f * PAD;
        float const lt_h = inner_h * layout.left_top;
        float const rt_h = inner_h * layout.right_top;

        // Ha a transzformacios vezerlokhoz eloszor nyulunk hozza, az alakzatot
        // ujra kell epiteni (lasd a Tulajdonsagok panelnel).
        bool needs_rebuild = false;

        // ------------------------------------------------------------------
        // 1. ablak: a jelenet alakzatai (lista + kijelölés) és a futtatás.
        // ------------------------------------------------------------------
        fixed_panel("Alakzatok", {O.x + PAD, O.y + PAD}, {layout.left_w, lt_h});
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Separator();

        if (ImGui::Button("Uj alakzat")) {
            auto& s = shapes.emplace_back();
            next_name(shapes, "f", s.name, sizeof(s.name));
            s.color_idx = (static_cast<int>(shapes.size()) - 1) % Raytrace::PALETTE_COUNT;
            selected = &s;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(%d alakzat)", static_cast<int>(shapes.size()));

        // Sablonok: kész alakzat (képlet + lokális paraméterek) hozzáadása egy kattintással.
        ImGui::SetNextItemWidth(190.0f);
        if (ImGui::BeginCombo("##sablon", PRESETS[preset_idx].label)) {
            char const* current_group = nullptr;
            for (int k = 0; k < static_cast<int>(PRESETS.size()); ++k) {
                if (current_group == nullptr || std::strcmp(current_group, PRESETS[k].group) != 0) {
                    current_group = PRESETS[k].group;
                    if (k > 0) ImGui::Separator();
                    ImGui::TextDisabled("%s", current_group);
                }
                bool is_selected = (k == preset_idx);
                if (ImGui::Selectable(PRESETS[k].label, is_selected)) preset_idx = k;
                if (is_selected) ImGui::SetItemDefaultFocus();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("F = %s", PRESETS[k].formula);
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Hozzaad")) add_preset(PRESETS[preset_idx]);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("F = %s", PRESETS[preset_idx].formula);
        ImGui::PopTextWrapPos();

        ImGui::Separator();

        for (auto it = shapes.begin(); it != shapes.end();) {
            Shape& s = *it;
            ImGui::PushID(&s);
            ImGui::Checkbox("##show", &s.visible);          // láthatóság, élőben hat
            ImGui::SameLine();

            bool warn = bad.count(&s) != 0;
            if (warn) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.45f, 0.45f, 1.0f));
            if (ImGui::Selectable(s.name[0] ? s.name : "(nevtelen)", selected == &s,
                                  0, ImVec2(180.0f, 0.0f)))
                selected = &s;
            if (warn) ImGui::PopStyleColor();

            ImGui::SameLine();
            bool del = ImGui::Button("X");
            ImGui::PopID();

            if (del) {
                if (selected == &s) selected = nullptr;
                it = shapes.erase(it);
                drop_all();          // a törölt alakzat lokálisaira mutathatnak fák
                error.clear();
            } else {
                ++it;
            }
        }

        // A samplerek számát az alakzatokéhoz igazítjuk (felvétel/törlés után),
        // majd a pool i-edik felülete a lista i-edik alakzatához tartozik.
        sync_pool();
        {
            std::size_t i = 0;
            for (auto& s : shapes) pool[i++]->set_visible(s.visible);
        }

        ImGui::Separator();
        ImGui::BeginDisabled(!problems.empty());
        if (ImGui::Button("Indit")) build_all();
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Torol")) { drop_all(); error.clear(); }

        // --- Sugárkövetett fénykép (a leválasztható raytrace/ komponens) ---
        ImGui::BeginDisabled(shapes.empty());
        if (ImGui::Button("Fenykep keszitese")) photo_requested = true;
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        // Kulon tomb, NEM a NUL-lal elvalasztott string-tulterhelest hasznaljuk:
        // ott a nulla-escape utani szamjegyek oktalis escape-pe olvadnanak, es
        // a lista nemaan elromlana.
        static char const* const RES_LABELS[] = {"640x420", "900x600", "1280x850", "1920x1280"};
        ImGui::Combo("##felbontas", &photo_size_idx, RES_LABELS, IM_ARRAYSIZE(RES_LABELS));
        ImGui::SameLine();
        ImGui::Checkbox("arnyek", &photo_shadows);
        if (!photo_status.empty())
            ImGui::TextDisabled("%s", photo_status.c_str());

        if (!error.empty())
            ImGui::TextColored(ImVec4(1.0f, 0.40f, 0.40f, 1.0f), "%s", error.c_str());
        for (auto& p : problems)
            ImGui::TextColored(ImVec4(1.0f, 0.60f, 0.35f, 1.0f), "%s", p.c_str());

        ImGui::Separator();
        ImGui::SliderFloat("d (meretskala)", &d_ui, 0.5f, 10.0f);
        ImGui::SliderFloat("gorbulet-taszitas", &curv_ui, 0.0f, 5.0f);
        for (auto& p : pool) {
            p->d = d_ui;
            p->curvature_repulsion = curv_ui;
        }
        ImGui::End();

        // ------------------------------------------------------------------
        // 2. ablak: a kijelölt alakzat adatai (képlet + lokális paraméterek).
        // ------------------------------------------------------------------
        fixed_panel("Tulajdonsagok", {O.x + PAD, O.y + PAD + lt_h + PAD},
                    {layout.left_w, inner_h - lt_h});
        if (selected == nullptr) {
            ImGui::TextDisabled("Valassz egy alakzatot az \"Alakzatok\" listabol.");
        } else {
            Shape& s = *selected;

            bool name_warn = bad.count(&s) != 0;
            if (name_warn) ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.45f, 0.12f, 0.12f, 1.0f));
            ImGui::SetNextItemWidth(200.0f);
            ImGui::InputText("nev", s.name, sizeof(s.name));
            if (name_warn) ImGui::PopStyleColor();

            // A panel szekcioi osszecsukhatok: kulonben a lentebbi reszek (warpok,
            // parameterek) lelognanak a panel aljarol es eszrevehetetlenek lennenek.
            if (ImGui::CollapsingHeader("Keplet", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("F(x, y, z) =");
            ImGui::InputTextMultiline("##keplet", s.formula, sizeof(s.formula),
                                      ImVec2(-1.0f, ImGui::GetTextLineHeight() * 3.5f));
            ImGui::TextDisabled("Valtozok: x y z | sin cos tan ctg ln log sqrt abs sign");
            ImGui::TextDisabled("Eles: unio(a,b) metszet(a,b) kulonbseg(a,b) min max");
            ImGui::TextDisabled("Sima: sunio(a,b,k) smetszet(a,b,k) skulonbseg(a,b,k)");
            ImGui::TextDisabled("Hivatkozhatsz a listaban ELOTTE allo alakzatok nevere is.");
            }

            // Szín a paletta-listából (a sugárkövetett fényképhez).
            ImGui::SetNextItemWidth(200.0f);
            if (ImGui::BeginCombo("szin", Raytrace::palette_name(s.color_idx))) {
                for (int k = 0; k < Raytrace::PALETTE_COUNT; ++k) {
                    glm::vec3 c = Raytrace::palette_color(k);
                    ImGui::ColorButton("##c", ImVec4(c.r, c.g, c.b, 1.0f),
                                       ImGuiColorEditFlags_NoTooltip, ImVec2(14, 14));
                    ImGui::SameLine();
                    if (ImGui::Selectable(Raytrace::palette_name(k), k == s.color_idx))
                        s.color_idx = k;
                }
                ImGui::EndCombo();
            }

            if (ImGui::CollapsingHeader("Tartomany")) {
            ImGui::Text("Csak itt jelenjen meg (opcionalis):");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputText("##tartomany", s.domain, sizeof(s.domain));
            ImGui::TextDisabled("pl. x > 2 and x < 6 and y > -2 and y < 2");
            ImGui::TextDisabled("Operatorok: > < >= <= and or not (&& || ! is jo)");
            ImGui::TextDisabled("A reszecskek a FELULETEN csusznak be a jo terreszbe,");
            ImGui::TextDisabled("es a peremen nem lepnek at. Ures = korlatlan.");
            }

            // --- Tér-transzformáció ---------------------------------------------
            if (ImGui::CollapsingHeader("Transzformacio", ImGuiTreeNodeFlags_DefaultOpen)) {
            bool xf_edited = false;
            xf_edited |= ImGui::DragFloat3("pozicio", s.xform.pos, 0.05f);
            xf_edited |= ImGui::SliderAngle("forgatas x", &s.xform.rot[0], -180.0f, 180.0f);
            xf_edited |= ImGui::SliderAngle("forgatas y", &s.xform.rot[1], -180.0f, 180.0f);
            xf_edited |= ImGui::SliderAngle("forgatas z", &s.xform.rot[2], -180.0f, 180.0f);
            xf_edited |= ImGui::DragFloat3("meret", s.xform.scale, 0.02f, 0.01f, 100.0f);
            if (ImGui::SmallButton("Alaphelyzet")) { s.xform.reset(); xf_edited = true; }
            ImGui::SameLine();
            ImGui::TextDisabled(s.warped ? "(eloben mozog)" : "(elso mozgatasra ujraepul)");

            // Egységtranszformációval a warp NEM épül be (így az egyszerű alakzatok
            // olcsók maradnak: a gömb programja 15 utasítás a warpos 153 helyett).
            // Ezért amikor ELŐSZÖR nyúlsz a vezérlőkhöz, újra kell építeni — utána
            // a paraméterek címe már benne van, és a csúszkák élőben hatnak.
            if (xf_edited && !s.warped && !s.xform.is_identity()) needs_rebuild = true;
            }

            draw_warps(s, needs_rebuild);

            if (ImGui::CollapsingHeader("Lokalis parameterek", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextDisabled("Csak ez az alakzat latja oket.");
            if (draw_params(s.locals, "p", 2)) { drop_all(); error.clear(); }
            }
        }
        ImGui::End();

        // ------------------------------------------------------------------
        // 3. ablak: nézet, jelmagyarázat és irányítás — a program használata
        //    közben végig látható súgó.
        // ------------------------------------------------------------------
        float const rx = O.x + S.x - PAD - layout.right_w;
        fixed_panel("Nezet es sugo", {rx, O.y + PAD}, {layout.right_w, rt_h});

        auto swatch = [](ImVec4 c, char const* text) {
            ImGui::ColorButton("##sw", c,
                               ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
                               ImVec2(14, 14));
            ImGui::SameLine();
            ImGui::TextUnformatted(text);
        };

        if (ImGui::CollapsingHeader("Jelmagyarazat", ImGuiTreeNodeFlags_DefaultOpen)) {
            swatch(ImVec4(0.85f, 0.22f, 0.26f, 1.0f), "X tengely");
            swatch(ImVec4(0.20f, 0.62f, 0.28f, 1.0f), "Y tengely");
            swatch(ImVec4(0.20f, 0.42f, 0.85f, 1.0f), "Z tengely  (ez a 'fuggoleges')");
            ImGui::Spacing();
            swatch(ImVec4(0.00f, 0.00f, 1.00f, 1.0f), "mintavetelezo reszecskek");
            swatch(ImVec4(1.00f, 0.00f, 0.00f, 1.0f), "kontrollpontok");
            ImGui::Spacing();
            ImGui::TextDisabled("A racs a z = 0 sikban van, 1 egyseg osztassal");
            ImGui::TextDisabled("(minden 5. vonal es osztas hangsulyos).");
            ImGui::Checkbox("Racs mutatasa", &app.get_axes().show_grid);
        }

        if (ImGui::CollapsingHeader("Nezet", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& cam = cur->camera;
            // A nézetváltás a kamera aktuális origótól mért távolságát megtartja.
            float dist = glm::length(cam.get_position());
            if (dist < 1.0f) dist = 15.0f;

            // Z-up kameranal a SZINTE FUGGOLEGES nezes a hatareset (pitch -> -90), ezert a
            // felulnezet kap egy pici y-eltolast: igy a kepernyon +x jobbra, +y felfele all.
            if (ImGui::Button("Felulnezet")) cam.look_at({0.0f, -0.02f * dist, dist});
            ImGui::SameLine();
            if (ImGui::Button("3/4 nezet"))  cam.look_at(glm::normalize(App::DEFAULT_EYE) * dist);
            ImGui::SameLine();
            if (ImGui::Button("Oldalrol"))   cam.look_at({dist, 0.0f, 0.0f});

            if (ImGui::Button("Elolrol"))    cam.look_at({0.0f, -dist, 0.0f});
            ImGui::SameLine();
            if (ImGui::Button("Alapnezet"))  cam.look_at(App::DEFAULT_EYE);

            ImGui::SetNextItemWidth(150.0f);
            if (ImGui::SliderFloat("tavolsag", &dist, 3.0f, 60.0f))
                cam.look_at(glm::normalize(cam.get_position()) * dist);
        }

        if (ImGui::CollapsingHeader("Iranyitas", ImGuiTreeNodeFlags_DefaultOpen)) {
            struct Row { char const* input; char const* effect; };
            static Row const camera_rows[] = {
                {"jobb egergomb + huzas", "nezet forgatasa"},
                {"Alt + bal gomb + huzas", "nezet forgatasa"},
                {"W / S",                  "kamera elore / hatra"},
                {"A / D",                  "kamera balra / jobbra"},
                {"egergorgo",              "zoom (latoszog 1-45 fok)"},
                {"Esc",                    "kilepes"},
            };
            static Row const point_rows[] = {
                {"Shift + bal kattintas",  "uj kontrollpont"},
                {"bal kattintas + huzas",  "pont mozgatasa"},
                {"bal gomb elengedese",    "pont elengedese"},
            };
            // Fix oszlop-eltolás, nem ImGui-tábla: a monospace alapfonttal így biztosan
            // nem vágódik el a hosszabb bevitel-leírás (a táblás arányos osztás elvágta).
            auto table = [](char const*, Row const* rows, int n) {
                for (int i = 0; i < n; ++i) {
                    ImGui::TextUnformatted(rows[i].input);
                    ImGui::SameLine(178.0f);
                    ImGui::TextDisabled("%s", rows[i].effect);
                }
            };
            ImGui::SeparatorText("Kamera");
            table("##cam", camera_rows, IM_ARRAYSIZE(camera_rows));
            ImGui::SeparatorText("Kontrollpontok");
            table("##pts", point_rows, IM_ARRAYSIZE(point_rows));
            ImGui::TextDisabled("A UI folott az eger/billentyu a panelt vezerli.");
        }
        ImGui::End();

        // ------------------------------------------------------------------
        // 4. ablak: paraméterek — jelenet- és program-szinten.
        // ------------------------------------------------------------------
        fixed_panel("Parameterek", {rx, O.y + PAD + rt_h + PAD},
                    {layout.right_w, inner_h - rt_h});

        ImGui::TextDisabled("Hatokor kifele: alakzat -> jelenet -> program.");
        ImGui::TextDisabled("A belso ELFEDI a kulsot (mint C++-ban).");

        ImGui::SeparatorText("Jelenet parameterei");
        ImGui::TextDisabled("Ebben a fulben minden alakzat latja.");
        if (draw_params(scene_params, "s", 1)) { drop_all(); error.clear(); }

        ImGui::SeparatorText("Program-szintu parameterek");
        ImGui::TextDisabled("MINDEN fulben lathatok.");
        if (draw_params(program_params, "g", 0)) {
            // Program-szintu valtozas MINDEN jelenetet erint: a mar beparseolt fak a
            // torolt float CIMET tartjak, ezert mindenhol el kell dobni oket.
            for (auto& other : scenes) {
                for (auto& p : other.pool) {
                    p->clear();
                    p->get_surface().set_tree(Kif(0.0f).get());
                    p->get_surface().clear_domain();
                }
                for (auto& sh : other.shapes) { sh.tree.reset(); sh.dom_tree.reset(); }
                other.error.clear();
            }
        }

        // --- A jelenet munkatere ---
        ImGui::SeparatorText("Jelenet munkatere");
        ImGui::TextWrapped("Az a terresz, amiben egyaltalan ertelmezzuk az alakzatokat. "
                           "Ebben a fulben minden alakzatra ervenyes, a sajat "
                           "tartomanyaval ES-kapcsolatban. Ures = korlatlan.");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##gdom", scene_domain, sizeof(scene_domain));

        // Gyorsgombok: a rács ±8 kiterjedéséhez igazodnak, hogy a beallitas lathato legyen.
        if (ImGui::SmallButton("Doboz")) {
            std::snprintf(scene_domain, sizeof(scene_domain),
                          "x > 0 - 8 and x < 8 and y > 0 - 8 and y < 8 and z > 0 - 8 and z < 8");
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Gomb")) {
            std::snprintf(scene_domain, sizeof(scene_domain), "x^2 + y^2 + z^2 < 64");
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Torol")) scene_domain[0] = '\0';
        ImGui::TextDisabled("Jelenet- es program-szintu parametert hasznalhat.");
        ImGui::End();

        // ------------------------------------------------------------------
        // Kozepso ablak: a 3D nezet (a jelenet texturaja), fulekkel.
        // ------------------------------------------------------------------
        ImGui::SetNextWindowPos({cx, O.y + PAD});
        ImGui::SetNextWindowSize({cw, inner_h + PAD});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("##nezet", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);

        bool image_hovered = false;
        Scene* want_scene = cur;        // a fulvaltast a ciklus UTAN hajtjuk vegre
        Scene* close_scene = nullptr;

        if (ImGui::BeginTabBar("##jelenetek", ImGuiTabBarFlags_Reorderable |
                                              ImGuiTabBarFlags_AutoSelectNewTabs)) {
            for (auto& one : scenes) {
                bool open = true;
                // Csak akkor adunk bezaro gombot, ha van mit bezarni.
                bool* p_open = (scenes.size() > 1) ? &open : nullptr;
                ImGui::PushID(&one);
                if (ImGui::BeginTabItem(one.name, p_open)) {
                    want_scene = &one;

                    ImVec2 avail = ImGui::GetContentRegionAvail();
                    ImVec2 pos   = ImGui::GetCursorScreenPos();
                    int const iw = std::max(16, static_cast<int>(avail.x));
                    int const ih = std::max(16, static_cast<int>(avail.y));

                    // A KOVETKEZO frame-re kerjuk a meretet (lasd App::request_viewport_size).
                    app.request_viewport_size(iw, ih);
                    Vp::set_current({pos.x, pos.y, static_cast<float>(iw), static_cast<float>(ih)});

                    // A GL-textura alulrol felfele all, ezert az UV-t megforditjuk.
                    ImGui::Image(static_cast<ImTextureID>(app.viewport_texture()),
                                 ImVec2(static_cast<float>(iw), static_cast<float>(ih)),
                                 ImVec2(0, 1), ImVec2(1, 0));
                    image_hovered = ImGui::IsItemHovered();
                    ImGui::EndTabItem();
                }
                ImGui::PopID();
                if (!open) close_scene = &one;
            }
            // "+" ful: uj jelenet
            if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing |
                                          ImGuiTabItemFlags_NoTooltip)) {
                auto& ns = scenes.emplace_back();
                next_name(scenes, "Jelenet ", ns.name, sizeof(ns.name));
                ns.set_active(false);
                want_scene = &ns;
            }
            ImGui::EndTabBar();
        }
        ImGui::End();

        // --- Fulvaltas / bezaras ------------------------------------------
        if (close_scene) {
            bool const closing_current = (close_scene == cur);
            for (auto it = scenes.begin(); it != scenes.end(); ++it)
                if (&*it == close_scene) { scenes.erase(it); break; }
            if (closing_current || want_scene == close_scene) {
                cur = &scenes.front();
                cur->set_active(true);
            }
        } else if (want_scene != cur) {
            cur->set_active(false);      // a reszecskek allapota megmarad
            cur = want_scene;
            cur->set_active(true);
        }
        ImGui::PopStyleVar();

        // --- Bemenet-kapu ---------------------------------------------------
        // A jelenet akkor kap egeret, ha a kurzor a kepen van. HUZAS-RETESZ: ha a
        // huzas a kepen indult, a gomb elengedeseig akkor is oda megy, ha az eger
        // kicsuszik — kulonben forgatas kozben a panel fole erve megallna a nezet.
        {
            static bool drag_latch = false;
            ImGuiIO& io = ImGui::GetIO();
            bool const any_down = ImGui::IsMouseDown(ImGuiMouseButton_Left)  ||
                                  ImGui::IsMouseDown(ImGuiMouseButton_Right) ||
                                  ImGui::IsMouseDown(ImGuiMouseButton_Middle);
            if (image_hovered && any_down) drag_latch = true;
            if (!any_down)                 drag_latch = false;

            Vp::set_scene_mouse(image_hovered || drag_latch);
            Vp::set_scene_keyboard(!io.WantCaptureKeyboard);
        }

        // --- Huzhato elvalasztok --------------------------------------------
        splitter("##split_left",  {O.x + PAD + layout.left_w, O.y + PAD},
                 {PAD, inner_h + PAD}, true,  &layout.left_w,  MINW, max_side, +1.0f);
        splitter("##split_right", {rx - PAD, O.y + PAD},
                 {PAD, inner_h + PAD}, true,  &layout.right_w, MINW, max_side, -1.0f);
        {
            float lt_px = lt_h, rt_px = rt_h;
            splitter("##split_lh", {O.x + PAD, O.y + PAD + lt_h},
                     {layout.left_w, PAD}, false, &lt_px, 120.0f, inner_h - 120.0f, +1.0f);
            splitter("##split_rh", {rx, O.y + PAD + rt_h},
                     {layout.right_w, PAD}, false, &rt_px, 120.0f, inner_h - 120.0f, +1.0f);
            layout.left_top  = std::clamp(lt_px / std::max(inner_h, 1.0f), 0.15f, 0.85f);
            layout.right_top = std::clamp(rt_px / std::max(inner_h, 1.0f), 0.15f, 0.85f);
        }

        if (needs_rebuild) build_all();

        // ------------------------------------------------------------------
        // Sugárkövetett fénykép (leválasztható komponens — raytrace/).
        //
        // A gomb csak jelez, a render itt fut: így nem egy félig felépített
        // ImGui-frame közben blokkoljuk a programot. A render szinkron (az ablak
        // addig áll), ezért van több szálon és mérsékelt alapfelbontással.
        // ------------------------------------------------------------------
        if (photo_requested) {
            photo_requested = false;
            photo_status.clear();

            std::vector<Raytrace::ObjectDesc> objs;
            for (auto& s : shapes) {
                if (!s.visible || !s.tree) continue;
                Raytrace::ObjectDesc o;
                o.F = Kif(s.tree);
                if (s.dom_tree) { o.domain = Kif(s.dom_tree); o.has_domain = true; }
                o.color = Raytrace::palette_color(s.color_idx);
                objs.push_back(std::move(o));
            }

            if (objs.empty()) {
                photo_status = "Nincs mit fenykepezni (nyomj Indit-ot).";
            } else {
                auto& cam = cur->camera;
                Raytrace::CameraDesc rc;
                rc.eye     = cam.get_position();
                rc.front   = cam.get_front();
                rc.right   = cam.get_right();
                rc.up      = cam.get_up();
                rc.fov_deg = cam.get_fov_deg();

                static int const RES_W[] = {640, 900, 1280, 1920};
                static int const RES_H[] = {420, 600,  850, 1280};
                Raytrace::Settings rs;
                rs.width   = RES_W[photo_size_idx];
                rs.height  = RES_H[photo_size_idx];
                rs.shadows = photo_shadows;

                auto t0  = std::chrono::steady_clock::now();
                auto img = Raytrace::render(objs, rc, rs);
                auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now() - t0).count();

                // A képek a BINÁRIS mellé, a `kepek/` almappába kerülnek — nem a
                // munkakönyvtárba, mert az indítástól függően bárhol lehet.
                std::string path = Raytrace::image_path(
                    std::filesystem::path(BINARY_DIR) / "kepek");
                if (Raytrace::write_bmp(path, rs.width, rs.height, img)) {
                    Raytrace::open_in_viewer(path);
                    photo_status = "Kesz (" + std::to_string(ms) + " ms): " + path;
                } else {
                    photo_status = "A kep mentese nem sikerult: " + path;
                }
            }
        }
    });

    app.run();

    // A jelenetek (bennuk a Model-ek: VAO/VBO) felszabaditasa MEG elo GL-kontextussal,
    // csak utana az ablak es a shader-cache.
    scenes.clear();
    app.shutdown();
    return 0;
}
