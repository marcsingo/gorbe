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
#include "particle_sampling/Surface.hpp"
#include "particle_sampling/Transform.hpp"
#include "particle_sampling/WarpPresets.hpp"

// --- Sugarkoveto komponens (onallo, levalaszthato: lasd raytrace/Raytracer.hpp) ---
#include <filesystem>
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
struct Param {
    char  name[32] = "";
    float value    = 0.0f;
};

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

int main() {
    App app{1200, 800, "Particle sampling"};

    // MINDEN alakzat egy önálló ImplicitSurface-t kap, saját kezdő részecskékkel
    // (külön mintavételezve). A lista i-edik alakzata a pool i-edik felületére kerül;
    // a pool mérete együtt mozog az alakzatokéval (az ImplicitSurface a destruktorában
    // leiratkozik az ablak eseményeiről, ezért szabadon megszüntethető).
    std::vector<App::EqSurface*> pool;

    std::list<Param> globals;

    // Globális tartomány: az a térrész, amiben egyáltalán értelmezzük az alakzatokat.
    // Minden alakzatra érvényes, a saját tartomány-feltételével ÉS-kapcsolatban.
    // Üresen hagyva korlátlan. Csak globális paramétereket használhat (alakzatnevet
    // nem — az körkörös lenne).
    char global_domain[256] = "";
    std::list<Shape> shapes;
    Shape* selected = nullptr;   // a Tulajdonságok ablakban szerkesztett alakzat

    float       d_ui      = 2.0f;  // közös méretskála minden samplerre
    float       curv_ui   = 1.0f;  // görbület-adaptív taszítás erőssége (0 = egyenletes)
    int         preset_idx = 0;    // a sablon-lenyílóban kiválasztott alakzat

    // A sugárkövető komponens állapota (a gomb csak jelez; a render a GUI után fut,
    // hogy ne egy félig felépített ImGui-frame közben blokkoljuk a programot).
    bool        photo_requested = false;
    int         photo_size_idx  = 1;
    bool        photo_shadows   = true;
    std::string photo_status;
    std::string error;             // parse-hiba az utolsó Indításból

    std::vector<std::string> problems;  // névütközések emberi olvasásra
    std::set<void const*>    bad;       // a hibás sorok (Param*/Shape*) a piros jelzéshez

    // Minden képlet eldobása + a szimuláció leállítása. Akkor is KÖTELEZŐ hívni, ha
    // egy paraméter vagy alakzat törlődik: a már beparseolt fák a törölt float
    // CÍMÉT tárolják, onnantól felszabadított memóriára mutatnának.
    // A samplerek számát az alakzatokéhoz igazítja (kell-e új, vagy elhagyható egy).
    auto sync_pool = [&] {
        if (pool.size() != shapes.size())
            pool = app.resize_equation_surfaces(shapes.size());
    };

    auto drop_all = [&] {
        for (auto* p : pool) {
            p->clear();
            p->get_surface().set_tree(Kif(0.0f).get());
            p->get_surface().clear_domain();
        }
        for (auto& s : shapes) { s.tree.reset(); s.dom_tree.reset(); }
    };

    // Névellenőrzés. Minden frame-ben lefut (néhány tucat név, elhanyagolható), így a
    // piros jelzés azonnal követi a gépelést, az Indít pedig tiltva marad, amíg baj van.
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
                problems.push_back(where + ": a(z) '" + n + "' foglalt nev (valtozo vagy fuggveny)");
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

        // Globális paraméterek: érvényes név, egymás közt egyediek.
        for (auto a = globals.begin(); a != globals.end(); ++a) {
            if (!check(a->name, &*a, "globalis parameter")) continue;
            for (auto b = std::next(a); b != globals.end(); ++b)
                if (std::strcmp(a->name, b->name) == 0)
                    clash(&*a, &*b, std::string("ket globalis parameter neve azonos: ") + a->name);
        }

        // Alakzatnevek: egyediek, és nem ütköznek globális paraméterrel.
        for (auto a = shapes.begin(); a != shapes.end(); ++a) {
            if (!check(a->name, &*a, "alakzat")) continue;
            for (auto b = std::next(a); b != shapes.end(); ++b)
                if (std::strcmp(a->name, b->name) == 0)
                    clash(&*a, &*b, std::string("ket alakzat neve azonos: ") + a->name);
            for (auto& g : globals)
                if (std::strcmp(a->name, g.name) == 0)
                    clash(&*a, &g, std::string("alakzat es globalis parameter neve azonos: ") + a->name);
        }

        // Lokális paraméterek: alakzaton BELÜL egyediek, és nem ütköznek globális
        // paraméterrel vagy alakzatnévvel. Alakzatok KÖZÖTT viszont ütközhetnek.
        for (auto& s : shapes) {
            std::string sn = s.name[0] ? s.name : "(nevtelen)";
            for (auto a = s.locals.begin(); a != s.locals.end(); ++a) {
                if (!check(a->name, &*a, sn + " lokalis parametere")) continue;
                for (auto b = std::next(a); b != s.locals.end(); ++b)
                    if (std::strcmp(a->name, b->name) == 0)
                        clash(&*a, &*b, sn + ": a(z) '" + a->name + "' lokalis parameter ketszer szerepel");
                for (auto& g : globals)
                    if (std::strcmp(a->name, g.name) == 0)
                        clash(&*a, &g, sn + "." + a->name + ": lokalis es globalis parameter neve nem egyezhet meg");
                for (auto& o : shapes)
                    if (std::strcmp(a->name, o.name) == 0)
                        clash(&*a, &o, sn + "." + a->name + ": ez mar egy alakzat neve");
            }
        }
    };

    // Névfeloldó a parsernek: lokális paraméter -> globális paraméter -> korábbi alakzat.
    auto resolve = [&](Shape const& owner, std::string const& nm) -> std::shared_ptr<Kifejezes const> {
        for (auto& p : owner.locals)
            if (p.name[0] && nm == p.name) return Kif(&p.value).get();
        for (auto& g : globals)
            if (g.name[0] && nm == g.name) return Kif(&g.value).get();
        for (auto& s : shapes) {
            if (&s == &owner) break;                   // csak a nála korábbiakra hivatkozhat
            if (s.name[0] && nm == s.name && s.tree) return s.tree;
        }
        return nullptr;                                // ismeretlen név -> a parser hibát dob
    };

    // A globális tartomány CSAK globális paramétert láthat.
    auto resolve_global = [&](std::string const& nm) -> std::shared_ptr<Kifejezes const> {
        for (auto& g : globals)
            if (g.name[0] && nm == g.name) return Kif(&g.value).get();
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
            if (global_domain[0]) {
                try {
                    make_kif(global_domain, resolve_global);
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
                    if (global_domain[0]) {
                        Kif g = make_kif(global_domain, resolve_global);
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
    auto draw_params = [&](std::list<Param>& list, char const* prefix) -> bool {
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
            ImGui::SetNextItemWidth(110.0f);
            ImGui::InputFloat("##ertek", &it->value);
            ImGui::SameLine();
            bool del = ImGui::Button("X");
            ImGui::PopID();
            if (del) { it = list.erase(it); erased = true; }
            else     { ++it; }
        }
        return erased;
    };

    app.set_gui([&] {
        validate();
        // Ha a transzformacios vezerlokhoz eloszor nyulunk hozza, az alakzatot
        // ujra kell epiteni (lasd a Tulajdonsagok panelnel).
        bool needs_rebuild = false;

        // ------------------------------------------------------------------
        // 1. ablak: a jelenet alakzatai (lista + kijelölés) és a futtatás.
        // ------------------------------------------------------------------
        ImGui::SetNextWindowPos({16, 16}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({330, 400}, ImGuiCond_FirstUseEver);
        ImGui::Begin("Alakzatok");
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
        for (auto* p : pool) {
            p->d = d_ui;
            p->curvature_repulsion = curv_ui;
        }
        ImGui::End();

        // ------------------------------------------------------------------
        // 2. ablak: a kijelölt alakzat adatai (képlet + lokális paraméterek).
        // ------------------------------------------------------------------
        ImGui::SetNextWindowPos({16, 428}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({330, 356}, ImGuiCond_FirstUseEver);
        ImGui::Begin("Tulajdonsagok");
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
            if (draw_params(s.locals, "p")) { drop_all(); error.clear(); }
            }
        }
        ImGui::End();

        // ------------------------------------------------------------------
        // 3. ablak: nézet, jelmagyarázat és irányítás — a program használata
        //    közben végig látható súgó.
        // ------------------------------------------------------------------
        ImGui::SetNextWindowPos({824, 16}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({360, 530}, ImGuiCond_FirstUseEver);
        ImGui::Begin("Nezet es sugo");

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
            auto& cam = app.get_camera();
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
        // 4. ablak: globális paraméterek (minden alakzat látja őket).
        // ------------------------------------------------------------------
        ImGui::SetNextWindowPos({824, 558}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({360, 226}, ImGuiCond_FirstUseEver);
        ImGui::Begin("Globalis parameterek");
        ImGui::TextWrapped("Minden alakzat lathatja oket. A nevuk nem egyezhet meg egyetlen "
                           "lokalis parameter vagy alakzat nevevel sem.");
        ImGui::Separator();
        if (draw_params(globals, "g")) { drop_all(); error.clear(); }

        // --- Globális tartomány: a "munkatér", amiben az alakzatokat értelmezzük ---
        ImGui::SeparatorText("Globalis tartomany");
        ImGui::TextWrapped("Az a terresz, amiben egyaltalan ertelmezzuk az alakzatokat. "
                           "Minden alakzatra ervenyes, a sajat tartomanyaval ES-kapcsolatban. "
                           "Ures = korlatlan.");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##gdom", global_domain, sizeof(global_domain));

        // Gyorsgombok: a rács ±8 kiterjedéséhez igazodnak, hogy a beallitas lathato legyen.
        if (ImGui::SmallButton("Doboz")) {
            std::snprintf(global_domain, sizeof(global_domain),
                          "x > 0 - 8 and x < 8 and y > 0 - 8 and y < 8 and z > 0 - 8 and z < 8");
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Gomb")) {
            std::snprintf(global_domain, sizeof(global_domain), "x^2 + y^2 + z^2 < 64");
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Torol")) global_domain[0] = '\0';
        ImGui::TextDisabled("Csak globalis parametert hasznalhat.");
        ImGui::End();

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
                auto& cam = app.get_camera();
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
    return 0;
}
