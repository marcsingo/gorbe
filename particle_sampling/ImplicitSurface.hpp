#ifndef GORBE_IMPLICITSURFACE_HPP
#define GORBE_IMPLICITSURFACE_HPP

#include <map>
#include <random>
#include <cmath>

#include "Particle.hpp"
#include "SpatialGrid.hpp"
#include "../model/Include.hpp"
#include "../matek/Kif.hpp"
#include "Occluders.hpp"


// A szükséges neveket a Surface.hpp már behozza célzott using-deklarációkkal;
// globális `using namespace` szándékosan nincs (lásd az ottani indoklást).


// A részecske-szimuláció hangolható paraméterei. A main-ből opcionálisan átadható
// az ImplicitSurface / App.show() hívásnak; ha nem adsz semmit, az alapértékek
// (a cikk eredeti beállításai) lépnek életbe.
struct SimParams {
    float d        = 4.0f;   // jellemző méretskála
    float alpha    = 6.0f;   // tűrési energia erőssége
    float sigma    = 1.0f;
    float phi      = 15.0f;  // felületre-húzó visszacsatolás
    float beta     = 10.0f;  // sigma-csillapítás
    float gamma    = 4.0f;   // egyensúly-küszöb a fisszióhoz/halálhoz
    float nu       = 0.2f;   // fisszió sűrűség-küszöbe
    float delta    = 0.7f;   // halál sűrűség-küszöbe
    float fraction = 0.001f;
};


// SurfaceT: a megjelenítendő implicit felület típusa (Sphere, Torus, Ellipsoid, ...).
// Az L paraméterszámot és a hozzá tartozó occludert automatikusan levezetjük, így
// a main-ben elég a felület típusát megadni.
template<class SurfaceT>
class ImplicitSurface {
    static constexpr size_t L = SurfaceT::param_count;
    using Occluder = typename OccluderFor<SurfaceT>::type;

    SurfaceT surface;
    Floaters<L> floaters;
    ControlPoints<L> controls;
    Occluder sphere_mesh;

    std::mt19937 rng;
    std::uniform_real_distribution<float> dist_R;

    // A szimuláció fut-e (alapból igen, hogy a show<T>() változatlanul működjön),
    // és a fix-lépésű integrátor időakkumulátora.
    bool  running   = true;
    float sim_accum = 0.0f;
    // Megjelenjen-e: a draw() ezt figyeli (a szimuláció attól még futhat a háttérben).
    bool  visible   = true;
public:
    explicit ImplicitSurface( Camera const &camera, SimParams params = {}) :
        // Nem a tiszta (0,0,1) / (1,0,0): a telített alapszínen az árnyalás alig
        // olvasható (a kék csatorna egyedül nem ad elég kontrasztot). Egy kissé
        // világosabb, kevertebb szín viszont szépen mutatja a formát.
        floaters{5, {0.20f, 0.45f, 0.90f}, camera},
        controls{10, {0.90f, 0.27f, 0.25f}, camera},
        sphere_mesh{surface, {0.85f, 0.85f, 0.85f}, camera},
        rng(std::random_device{}()),
        dist_R(0.0f, 1.0f),
        d(params.d), alpha(params.alpha), sigma(params.sigma), PHI(params.phi),
        E_v(0.8f * params.alpha), rho(params.phi), beta(params.beta), gamma(params.gamma),
        nu(params.nu), delta(params.delta), fraction(params.fraction)
    {
        // Egyetlen kezdő részecske — a globális fisszió (4.2 fejezet) ebből épít fel
        // egyenletes mintavételt anélkül, hogy előre el kellene helyezni a pontokat.
        // Particle<L> p0;
        // p0.p     = glm::vec3{surface.q.x, surface.q.y + surface.q.w, surface.q.z};
        // p0.sigma = sigma_max;
        // p0.F_x   = surface.grad(p0.p); // kezdeti normális a korong-rendereléshez
        // floaters.add_particle(p0);

        controls.set_surface(&surface);
        // A felület mostantól folyamatosan a saját átmérőjét írja a d-be.
        surface.bind_diameter(&d);
        spawn_random_particles(INITIAL_PARTICLES, 3);

        tick_sub = Window::Subscription(Window::add_time_passed_event([this](auto p) {
            if (!running) return;            // leállított állapotban nem szimulálunk
            sim_accum += p.dt;
            if (sim_accum >= 0.03f) {
                this->simulation(p.t, sim_accum);
                sim_accum = 0.0f;
            }
        }));

    }

    // A `this`-t kapó eseménykezelő élettartama a példányhoz kötve, ezért az
    // ImplicitSurface immár szabadon megszüntethető (nem marad utána "lógó" lambda).
    Window::Subscription tick_sub;

    ImplicitSurface(ImplicitSurface const&) = delete;
    ImplicitSurface& operator=(ImplicitSurface const&) = delete;

    // --- Futásidejű vezérlés (egyetlen, állandó életű példányhoz) ----------------
    // A felület eseménykezelői (Window::add_*_event) a konstruktorban, egyszer
    // regisztrálódnak és erre a példányra mutatnak. Ezért a szimulációt NEM a példány
    // megsemmisítésével/újraépítésével indítjuk-állítjuk (az dangling lambdákat hagyna),
    // hanem ezekkel a kapcsolókkal.

    SurfaceT&       get_surface()       { return surface; }
    SurfaceT const& get_surface() const { return surface; }

    bool is_running() const { return running; }
    void stop()  { running = false; }

    // A hatterben levo fulek szimulacioja all, de a reszecskek allapota megmarad —
    // fulvaltaskor onnan folytatodik.
    void set_running(bool v) { running = v; if (!v) sim_accum = 0.0f; }

    // Csak az aktiv jelenet kontrollpontjai reagaljanak a bevitelre.
    void set_input_enabled(bool v) { controls.input_enabled = v; }

    // Megjelenítés ki/be (a szimulációt nem állítja le, csak a rajzolást hagyja ki).
    void set_visible(bool v) { visible = v; }
    bool is_visible() const  { return visible; }

    // Új futás: friss részecskékkel, futó állapotban. A felület F-jének beállítása UTÁN
    // hívandó (pl. StringSurface::set_tree), mert a kezdő gradiensekhez már az új F kell.
    void restart() {
        floaters.ps().clear();
        controls.ps().clear();
        sim_accum = 0.0f;
        spawn_random_particles(INITIAL_PARTICLES, 3);
        running = true;
    }

    // Minden részecske törlése és leállítás (üres, álló jelenet).
    void clear() {
        running = false;
        floaters.ps().clear();
        controls.ps().clear();
        sim_accum = 0.0f;
    }

    // Az alakzat aktuális átmérője. NEM const: a felület (Surface) folyamatosan
    // ide írja a valódi átmérőt (lásd surface.bind_diameter(&d) a konstruktorban),
    // így a belőle számolt skálák (sigma_v, sigma_max) követik a felület változását.
    float d;

    // Felső korlát a részecskeszámra. Ez a garancia, hogy SEMMILYEN beírt képlet ne
    // tudja lefagyasztani a programot: önmagában végtelen felületnél (sík, henger) a
    // konstans mintavételi sűrűség végtelen sok részecskét jelentene, mert a
    // sűrűséghiány sosem szűnik meg, tehát a fisszió sosem áll le.
    // A plafon elérésekor a fisszió leáll, és a szigmát is meg kell fogni: különben
    // korlátlanul nőne (D < E_v marad), és a korongok gigantikusra hízva jelennének meg.
    int max_particles = 4000;

    // Ennyi kezdő részecskét szórunk. Kettő KEVÉS volt: a ráhúzó ág (masik) a
    // gradiens mentén repíti a részecskét a felületre, a gradiens viszont a felület
    // "tengelyénél" (kritikus pontoknál) eltűnik, és az i.delta lépésköz minden
    // átlövéskor feleződik, sosem nő vissza. Így egy szerencsétlenül induló részecske
    // csigalassúsággal csordogál — ha mind a kettő ilyen, a jelenet üres marad, és a
    // fisszió sem indul be (ahhoz felületi részecske kell). Mérve: ez ritkán, de
    // előfordult. Nyolccal a "mind beragad" esély elhanyagolható.
    static constexpr int INITIAL_PARTICLES = 8;

    // A görbület-adaptív taszítás erőssége (futásidőben állítható, pl. GUI-csúszka).
    // 0 = kikapcsolva (egyenletes mintavétel); nagyobb érték = a görbült helyek erősebben
    // sűrűsödnek. A curvature_scale() ezt használja.
    float curvature_repulsion = 1.0f;

    // d-ből származó, ezért menet közben is helyes méretskálák.
    float sigma_v()   const { return d / 4.0f; }
    float sigma_max() const { return std::max(d / 2.0f, 1.5f * sigma_v()); }

    // Görbület-adaptív skálatényező (0,1]: lapos helyen (|K|→0) 1, görbült helyen (nagy
    // |K|) kisebb. Ezzel SZOROZZUK a cél-méretskálákat (sigma_v/sigma_max) a fisszió/halál
    // küszöbeiben: görbültebb helyen kisebb a cél-σ -> hamarabb fisszionál, kevésbé hal ->
    // SŰRŰBB mintavétel. Így a taszítás (a cél-távolság) fordítottan arányos a görbülettel.
    // Ennél kisebbre nem mehet a skálatényező: legfeljebb 10x sűrűbb mintavétel a
    // görbült helyeken. Az éles CSG-operátorok (min/max) miatt kötelező a levágás: a
    // varraton a felület csak C0, ezért a MÁSODIK deriváltak ott értelmetlenül nagyok,
    // |K| elszállna, a cél-szigma nullába menne, és korlátlan fisszió indulna.
    static constexpr float MIN_CURVATURE_SCALE = 0.1f;

    float curvature_scale(Particle<L> const& p) const {
        if (curvature_repulsion <= 0.0f) return 1.0f;
        float s = 1.0f / (1.0f + curvature_repulsion * std::abs(p.K));
        return std::max(s, MIN_CURVATURE_SCALE);
    }

    // Az átmérőt (d) alapból a felület folyamatosan felülírja a valódi átmérőjével
    // (lásd surface.bind_diameter(&d) a konstruktorban). Ezzel kézi vezérlésre lehet
    // váltani: ekkor a d szabadon állítható (pl. ImGui-csúszkáról), a felület már
    // nem írja felül. Visszakapcsolva újra a felület átmérőjét követi.
    void set_manual_diameter(bool on) { surface.bind_diameter(on ? nullptr : &d); }

    // Értéküket a konstruktor init-listája adja a SimParams-ból (lásd fentebb).
    float const alpha;
    float const sigma;
    float const PHI;
    float const E_v;
    float const rho;
    float const beta;
    float const gamma;
    float const nu;
    float const delta;
    float const fraction;

    void spawn_random_particles(int n, float cube_size) {
        // A kocka közepe az origó, így a határok -méret/2 és +méret/2 között lesznek
        float half_size = cube_size / 2.0f;
        std::uniform_real_distribution<float> dist_cube(-half_size, half_size);

        for (int i = 0; i < n; ++i) {
            Particle<L> p;

            // Descartes-koordináták sorsolása a kockán belül. Ha van tartomány-feltétel,
            // elutasításos mintavétellel a JÓ térrészbe célzunk: a részecske ugyan a
            // felület mentén be tudna csúszni, de az lépésenként legfeljebb 0.25 szigma,
            // tehát egy távoli tartománynál sokáig tartana, amíg megjelenik bármi.
            int tries = 0;
            do {
                p.p = glm::vec3{dist_cube(rng), dist_cube(rng), dist_cube(rng)};
                ++tries;
            } while (surface.has_domain && tries < 64 && surface.Dom.at(p.p) <= 0.0f);

            p.sigma = sigma_v();

            // A kezdeti gradiens kiszámítása kritikus a ráhúzó ág miatt
            p.F_x = surface.grad(p.p);

            floaters.add_particle(p);
        }
    }

    void calculate_particle(Particle<L>& p) {
        // A lefordított programokkal EGY menetben áll elő minden, amire szükség van,
        // a közös részkifejezések pedig csak egyszer futnak le (matek/Program.hpp).
        //
        // A görbületet (és vele a Hesse-mátrixot) csak akkor számoljuk, ha tényleg
        // kell: a Hesse a derivált-fák tömegének ~98%-a, és ha a görbület-taszítás
        // ki van kapcsolva, a curvature_scale() amúgy is 1-et ad.
        if (curvature_repulsion > 0.0f) {
            surface.eval_full(p.p, p.F, p.F_x, p.K);
            // Éles CSG-varraton (min/max) a Hesse nem véges — ilyenkor 0, mintha sík lenne.
            if (!std::isfinite(p.K)) p.K = 0.0f;
        } else {
            surface.eval_grad(p.p, p.F, p.F_x);
            p.K = 0.0f;
        }
        // Tartomány-feltétel (ha van): érték + gradiens + a felület menti irány.
        // Hesse NEM kell hozzá, ezért ez sokkal olcsóbb, mint a feltételt beépíteni F-be.
        if (surface.has_domain) {
            surface.eval_domain(p.p, p.dom, p.dom_x);
            p.dom_g    = Domain::tangential_gradient(p.dom_x, p.F_x);
            p.dom_dist = Domain::distance(p.dom, p.dom_g);
        } else {
            p.dom      = 1.0f;
            p.dom_x    = glm::vec3{0};
            p.dom_g    = glm::vec3{0};
            p.dom_dist = 1e30f;
        }
        p.P = glm::vec3{0};
        p.D = 0.0f;
        p.D_sigma = 0.0f;
        // A NEM VÉGES részecske azonnal kiesik. Ez fontos: a felhasználó képlete
        // adhat NaN-t (ln(0), 0/0, negatív alap törtkitevővel), és a taszítás
        // szomszédszűrője NaN-nal hamis eredményt ad -> a NaN egyetlen részecskéről
        // az ÖSSZES szomszédra átterjedne, és tönkretenné a teljes szimulációt.
        p.detah = !(std::isfinite(p.F) && std::isfinite(p.F_x.x) &&
                    std::isfinite(p.F_x.y) && std::isfinite(p.F_x.z) &&
                    std::isfinite(p.p.x) && std::isfinite(p.p.y) && std::isfinite(p.p.z));
    }

    float sign(float x) {
        if (x == 0.0f) return 0.0f;
        return x > 0.0f ? 1.0f : -1.0f;
    }

    // A felülettől mért közelítő GEOMETRIAI távolság: |F| / |∇F| (elsőrendű, Taubin).
    // A nyers |F| skálafüggő (a tórusz kvartikus F-je a felülettől 0.05-re már ~5),
    // ezért a felület-közelség küszöböket erre normáljuk, hogy minden alakzatnál
    // ugyanazt jelentsék.
    static float surface_distance(Particle<L> const& p) {
        float g = glm::length(p.F_x);
        return g > 1e-6f ? std::abs(p.F) / g : std::abs(p.F);
    }

    // A Gauss-kernel ennyi szigmán túl elhanyagolható: exp(-3²/2) ≈ 1.1%. Ennél
    // messzebb lévő párokat sem a rács nem ad vissza, sem a távolság-ellenőrzés
    // nem engedi át — így lesz a taszítás O(n²) helyett O(n).
    static constexpr float REPULSION_CUTOFF = 3.0f;

    SpatialGrid grid;

    // A taszításhoz használt rács újraépítése a lépés eleji pozíciókkal.
    void rebuild_grid() {
        auto& ps = floaters.ps();
        float max_sigma = 0.0f;
        for (auto& p : ps) max_sigma = std::max(max_sigma, p.sigma);
        grid.build(ps.size(), [&](std::size_t k) { return ps[k].p; },
                   REPULSION_CUTOFF * max_sigma);
    }

    void witkin(int idx, float dt) {
        auto& ps = floaters.ps();
        Particle<L>& i = ps[static_cast<std::size_t>(idx)];

        grid.for_each_near(i.p, [&](int jdx) {
            if (jdx == idx) return;
            Particle<L>& j = ps[static_cast<std::size_t>(jdx)];
            if (surface_distance(j) > 5e-1f) return;
            auto  r  = i.p - j.p;
            float r2 = glm::dot(r, r);
            // A rács 27 cellája a hatósugárnál nagyobb területet fed le, ezért itt
            // még pontosan is ellenőrizzük — ez a drága exp() elé kerülő olcsó szűrő.
            float cut = REPULSION_CUTOFF * std::max(i.sigma, j.sigma);
            if (r2 > cut * cut) return;
            auto E_ij = alpha*std::exp(-r2 / (i.sigma*i.sigma*2) );
            auto E_ji = alpha*std::exp(-r2 / (j.sigma*j.sigma*2) );
            i.P += r / (i.sigma*i.sigma) * E_ij + r / (j.sigma*j.sigma) * E_ji;
            i.D += E_ij;
            i.D_sigma += r2*E_ij;
        });
        i.P *= i.sigma*i.sigma;

        i.D_dot = -rho*(i.D - E_v);
        i.D_sigma *= (1/(i.sigma*i.sigma*i.sigma));

        float sigma_update = (i.D_dot / (i.D_sigma + beta)) * dt;
        // Egy lépésben legfeljebb 30%-ot csökkenhet, hogy ne zuhanjon
        // halálküszöb alá azonnali D-spike miatt (pl. egyszerre érkező részecskék).
        sigma_update = std::max(sigma_update, -0.3f * i.sigma) ;
        i.sigma += sigma_update;
        i.sigma = std::max(i.sigma, 1e-3f);
        // (A görbület-adaptáció NEM itt, σ felülírásával történik — az tönkretenné a
        //  fenti sűrűség-visszacsatolást és előjelhibás lenne. Lásd curvature_scale()-t
        //  és a fisszió/halál küszöböket a simulation()-ben.)

        if (glm::length(i.F_x) > 1e-6f) {
            i.p_dot =
                i.P -
                    ((glm::dot(i.F_x, i.P) + glm::dot(surface.q_dot, surface.get_F_q(i.p)) + PHI*i.F)
                        /
                    glm::dot(i.F_x, i.F_x)) * i.F_x;
        } else {
            i.p_dot = glm::vec3(0,0,0);
        }

        apply_domain_constraint(i, dt);

        i.p += i.p_dot * dt;
    }

    // Milyen erősen csúsztatjuk vissza a rossz térrészbe került részecskét (1/s).
    static constexpr float DOMAIN_PULL = 6.0f;
    // Egy lépésben legfeljebb ennyi szigmányit csúszhat. Két dolog miatt kell:
    //  * túl nagy lépés átlőné a tartományt;
    //  * a csúsztatás ÉRINTŐ irányú, görbült felületen tehát másodrendben kivisz
    //    (v·dt hosszú lépés R görbületi sugárnál ~(v·dt)²/2R eltérést okoz), amit a
    //    felület-visszacsatolás csak késleltetve hoz vissza. Mivel sigma a felület
    //    legkisebb jellemző méretéhez van kötve (lásd diameter()), a 0.25·sigma-s
    //    korlát a görbületi sugárhoz képest is kicsi lépést jelent.
    static constexpr float DOMAIN_MAX_SLIDE = 0.25f;

    // A tartomány-feltétel érvényesítése a Witkin-lépés MÁSODIK kényszereként: a
    // részecske a jó térrész felé mozdul (illetve nem lép ki belőle), miközben végig
    // a felületen marad. A matek a DomainConstraint.hpp-ban van, hogy tesztelhető legyen.
    void apply_domain_constraint(Particle<L>& i, float dt) {
        if (!surface.has_domain) return;

        if (glm::dot(i.dom_g, i.dom_g) < 1e-12f) {
            // A feltétel gradiense párhuzamos a felület normálisával: a felület mentén
            // csúszva a dom értéke nem változik, ezt a részecskét nem lehet behozni.
            if (i.dom < 0.0f) i.detah = true;
            return;
        }

        i.p_dot = Domain::constrain(i.p_dot, i.dom_x, i.dom_g, i.dom_dist,
                                    i.sigma, dt, DOMAIN_PULL, DOMAIN_MAX_SLIDE);
    }

    // Egy lépésben legfeljebb ennyi szigmányit haladhat a felület felé repülő
    // részecske; enélkül a lenti sebesség-akkumuláció elszállna.
    static constexpr float APPROACH_MAX_STEP = 0.5f;

    void masik(Particle<L>& i, float dt) {
        // Figueiredo-Gomes: a részecske nincs a felületen, rárepítjük.
        // A sebesség CSILLAPÍTVA halmozódik: eredetileg korlátlanul nőtt, ezért egy
        // messziről induló részecske végül átlőtte a felületet és oszcillált.
        i.p_dot = 0.8f * i.p_dot + i.delta * (-sign(i.F) * i.F_x);

        // Lépéshossz-korlát, hogy egy nagy |∇F| (pl. kvartikus tórusz) se lökje el.
        float step = glm::length(i.p_dot) * dt;
        float max_step = APPROACH_MAX_STEP * i.sigma;
        if (step > max_step && step > 1e-9f) i.p_dot *= max_step / step;

        auto uj_p = i.p + i.p_dot * dt;
        if (surface.F.at(uj_p) * i.F < 0.0f) {
            // Átlőttük a felületet: felezzük a lépésközt és álljunk meg.
            i.delta /= 2.0f;
            i.p_dot = glm::vec3{0};
        }
        i.p += i.p_dot * dt;
    }

    void simulation(float t, float dt) {
        // bool is_particle_on_surface =
        //     std::any_of(floaters.ps().begin(),
        //                 floaters.ps().end(),
        //                 [this](auto& p) {
        //                     calculate_particle(p);
        //                     return std::abs(p.F) < 1e-6f;
        //                 });
        // is_particle_on_surface = false;
        rebuild_grid();   // a taszítás szomszédkeresése ezen megy (O(n) a O(n²) helyett)


        std::vector<Particle<L>> particles;
        auto& ps = floaters.ps();
        particles.reserve(ps.size() + 8);
        for (std::size_t idx = 0; idx < ps.size(); ++idx) {
            Particle<L>& i = ps[idx];
            calculate_particle(i);
            if (i.state == ramozog) {
                masik(i, dt);
                // Geometriai közelség (nem nyers F): minden alakzatnál ugyanazt jelenti.
                if (surface_distance(i) < 1e-2f) {
                    i.state = rajtamozog;
                }
            }
            if (i.state == rajtamozog) {
                witkin(static_cast<int>(idx), dt);
            }


            float R = dist_R(rng);

            // Görbület-adaptív cél-méretskálák: görbült helyen kisebbek -> sűrűbb mintavétel.
            float cs    = curvature_scale(i);
            float sv    = sigma_v()   * cs;
            float smax  = sigma_max() * cs;

            // A tartomány-feltételen kívüli részecske még ÚTON van (a felület mentén
            // csúszik befelé): addig se nem osztódik, se nem hal meg sűrűség alapján —
            // különben a peremen "churn" alakulna ki (kilökődik, meghal, a szomszéd
            // fisszionál a helyére, azt is kilöki, ...).
            bool const uton = Domain::is_outside(i.dom_dist, i.sigma);

            // A fisszió és a sűrűség-alapú halál CSAK a felületen mozgó részecskékre
            // értelmes: a még repülő (ramozog) részecske p_dot-ja a ráhúzó ágból jön,
            // nem a taszításból, tehát a `|p_dot| < gamma*sigma` egyensúly-feltétel
            // rá nézve értelmetlen. (A kód eddig ezt nem szűrte, a komment viszont
            // már akkor is "csak felületi részecskéknél"-t írt.)
            bool const felszinen = (i.state == rajtamozog);
            // A fisszió két részecskét ad, ezért a plafon alatt egy hellyel korábban állunk meg.
            bool const at_budget = static_cast<int>(particles.size()) >= max_particles - 1;

            if (i.detah) {
                // halál: nem véges részecske, vagy a tartományba nem behozható
            } else if (uton || !felszinen) {
                particles.push_back(i);
            } else if (at_budget) {
                // Elértük a részecske-plafont: nincs több fisszió. A szigmát is le
                // kell fogni, különben korlátlanul nőne (a sűrűség a cél alatt marad).
                i.sigma = std::min(i.sigma, smax);
                particles.push_back(i);
            } else if (glm::length(i.p_dot) < gamma*i.sigma &&
                (i.sigma > smax || (i.D > nu * E_v && i.sigma > sv)))
            {
                // fisszió: csak felületi részecskéknél
                i.sigma /= std::sqrt(2.0f);
                i.delta = 0.01f;  // delta reset a gyerekeknek

                glm::vec3 normal = (glm::length(i.F_x) > 1e-6f)
                    ? glm::normalize(i.F_x) : glm::vec3(0, 1, 0);
                glm::vec3 rand_vec = {
                    dist_R(rng) - 0.5f,
                    dist_R(rng) - 0.5f,
                    dist_R(rng) - 0.5f
                };
                glm::vec3 tangent = rand_vec - glm::dot(rand_vec, normal) * normal;
                if (glm::length(tangent) > 1e-6f) tangent = glm::normalize(tangent);

                glm::vec3 offset = tangent * (0.5f * i.sigma);
                i.p += offset;
                i.p_dot = glm::vec3{0};
                particles.push_back(i);

                Particle<L> child = i;
                child.p -= 2.0f * offset;
                particles.push_back(child);

            } else if (
                glm::length(i.p_dot) < gamma*i.sigma &&
                i.sigma < delta*sv &&
                R > i.sigma/(delta*sv))
            {
                // halál: sűrűség alapú eliminálás
            } else {
                particles.push_back(i);
            }

        }

        floaters.ps() = particles;
    }

    void draw(const Camera &camera) {
        if (!visible) return;
        sphere_mesh.draw(camera);
        floaters.draw(camera);
        controls.draw(camera);
    }


};

#endif //GORBE_IMPLICITSURFACE_HPP

