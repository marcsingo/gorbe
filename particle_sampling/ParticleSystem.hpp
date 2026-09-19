#ifndef GORBE_PARTICLESYSTEM_HPP
#define GORBE_PARTICLESYSTEM_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <utility>
#include <vector>

#include "DomainConstraint.hpp"
#include "Particle.hpp"
#include "SpatialGrid.hpp"
#include "Surface.hpp"
#include "../utils/Parallel.hpp"

// A részecske-szimuláció hangolható paraméterei; az alapértékek a cikk eredeti
// beállításai.
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

// ===========================================================================
// Egy térbeli objektum MODELLJE: a felület és a rajta mozgó részecskék, a
// Witkin–Heckbert-szimulációval (taszítás, fisszió, halál).
//
// Tiszta adat és logika — se GL, se ablak, se óra: a lépést a vezérlő hívja
// (object/ObjectController.hpp), a kirajzolás a nézeté (object/ObjectView.hpp).
// Ezért GL-kontextus nélkül is futtatható és tesztelhető (tests/test_particles.cpp).
// ===========================================================================
class ParticleSystem {
public:
    // A `seed` alapból véletlen; tesztben rögzíthető, hogy két futás összevethető legyen.
    explicit ParticleSystem(SimParams params = {}, unsigned seed = std::random_device{}()) :
        rng(seed),
        dist_R(0.0f, 1.0f),
        d(params.d), alpha(params.alpha), sigma(params.sigma), PHI(params.phi),
        E_v(0.8f * params.alpha), rho(params.phi), beta(params.beta), gamma(params.gamma),
        nu(params.nu), delta(params.delta), fraction(params.fraction) {}

    ParticleSystem(ParticleSystem const&) = delete;
    ParticleSystem& operator=(ParticleSystem const&) = delete;

    Surface&       surface()       { return surf; }
    Surface const& surface() const { return surf; }

    std::vector<Particle>&       particles()       { return floaters; }
    std::vector<Particle> const& particles() const { return floaters; }

    // A kontrollpontok: a felhasználó által letett, húzható jelölők.
    // ponytail: a felületre nincsenek hatással — a régi q-paraméteres megoldó a
    // képletből épülő felületnél hatástalan volt, ezért kikerült (regi-feluletek tag).
    std::vector<Particle> const& controls() const { return ctrl; }

    // Új futás: friss részecskékkel. A felület F-jének beállítása UTÁN hívandó
    // (Surface::set_tree), mert a kezdő gradiensekhez már az új F kell.
    void restart() {
        floaters.clear();
        ctrl.clear();
        spawn_random_particles(INITIAL_PARTICLES, 3);
    }

    // Minden részecske törlése (üres jelenet).
    void clear() {
        floaters.clear();
        ctrl.clear();
    }

    // Az alakzat jellemző mérete: ebből jön a részecskék cél-távolsága. A GUI
    // `d` csúszkája állítja.
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

    // Legfeljebb ennyi szálon számol egy lépés (0 = ahány mag van). Egy lépésen belül
    // a kiértékelés, a taszítás és a mozgás párhuzamos (lásd step()).
    std::size_t max_threads = 0;

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

    float curvature_scale(Particle const& p) const {
        if (curvature_repulsion <= 0.0f) return 1.0f;
        float s = 1.0f / (1.0f + curvature_repulsion * std::abs(p.K));
        return std::max(s, MIN_CURVATURE_SCALE);
    }

    // --- kontrollpontok ------------------------------------------------------

    // A kontrollpont FIX méretű: az elkapási sugara (CONTROL_RADIUS) is fix, a
    // kettőnek együtt kell mozognia, különben mellényúlna a felhasználó.
    static constexpr float CONTROL_RADIUS = 0.5f;

    void add_control(glm::vec3 pos) {
        Particle cp{pos};
        cp.sigma = CONTROL_RADIUS;
        cp.F_x = surf.grad(pos);
        ctrl.push_back(cp);
    }

    // Egy kontrollpont húzása a cél felé (csillapítva, mint egy rugó). A normálisát
    // is frissíti, hogy a korongja kövesse a felszínt.
    void drag_control(int idx, glm::vec3 target, float dt) {
        if (idx < 0 || idx >= static_cast<int>(ctrl.size())) return;
        auto& c = ctrl[static_cast<std::size_t>(idx)];
        c.p_dot = 10.0f * (target - c.p);
        c.p += c.p_dot * dt;
        c.F_x = surf.grad(c.p);
    }

    // --- egy szimulációs lépés -------------------------------------------------

    // JACOBI-lépés: minden részecske a lépés ELEJI állapotot látja (a szomszédok
    // helyét és σ-ját), és csak a teljes erő kiszámolása után mozdul. (Korábban
    // Gauss–Seidel volt: a lépés közben elmozdult szomszédokat is látta, így az
    // eredmény a feldolgozási sorrendtől függött.) Ettől lehet minden párt EGYSZER
    // számolni, és ez az előfeltétele egy objektumon belüli párhuzamosításnak is.
    void step(float dt) {
        rebuild_grid();   // a taszítás szomszédkeresése ezen megy (O(n) a O(n²) helyett)

        // Az 1-3. fázis PÁRHUZAMOS: a részecskék összefüggő darabokra oszlanak (a
        // rendezés miatt ezek térben is összefüggők), darabonként egy szálon.
        std::size_t const C = chunk_count();
        if (ws.size() < C) ws.resize(C);

        // 1. Kiértékelés (F, gradiens, görbület, tartomány), és a még repülők
        //    ráhúzása a felületre. Részecskénként független; darabonként saját
        //    program-munkaterülettel.
        for_chunks(C, [&](std::size_t k, std::size_t b, std::size_t e) {
            for (std::size_t idx = b; idx < e; ++idx) {
                Particle& i = floaters[idx];
                calculate_particle(i, ws[k]);
                dist[idx] = surface_distance(i);
                if (i.state == ramozog) {
                    masik(i, dt);
                    // Geometriai közelség (nem nyers F): minden alakzatnál ugyanazt jelenti.
                    if (surface_distance(i) < 1e-2f) i.state = rajtamozog;
                }
            }
        });

        // 2. Taszítás: minden pár egyszer.
        accumulate_repulsion(C);

        // 3. Mozgás a teljes erőből. Részecskénként független.
        for_chunks(C, [&](std::size_t, std::size_t b, std::size_t e) {
            for (std::size_t idx = b; idx < e; ++idx)
                if (floaters[idx].state == rajtamozog) witkin_move(floaters[idx], dt);
        });

        // 4. Fisszió és halál — sorosan (a véletlenszámok sorrendje miatt).

        // 4. Fisszió és halál.
        std::vector<Particle> next;
        next.reserve(floaters.size() + 8);
        for (std::size_t idx = 0; idx < floaters.size(); ++idx) {
            Particle& i = floaters[idx];
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
            // rá nézve értelmetlen.
            bool const felszinen = (i.state == rajtamozog);
            // A fisszió két részecskét ad, ezért a plafon alatt egy hellyel korábban állunk
            // meg. A becslés a MÉG HÁTRALÉVŐKET is számolja (mindegyik legalább egyet ad):
            // korábban csak a már feldolgozottakat nézte, így a lista eleje minden lépésben
            // osztódhatott, és a plafon valójában nem korlátozott (mérve: 300-as plafonnal
            // egy végtelen síkon 6898 részecske — tests/test_particles.cpp).
            std::size_t const projected = next.size() + (floaters.size() - idx);
            bool const at_budget = static_cast<int>(projected) >= max_particles - 1;

            if (i.detah) {
                // halál: nem véges részecske, vagy a tartományba nem behozható
            } else if (uton || !felszinen) {
                next.push_back(i);
            } else if (at_budget) {
                // Elértük a részecske-plafont: nincs több fisszió. A szigmát is le
                // kell fogni, különben korlátlanul nőne (a sűrűség a cél alatt marad).
                i.sigma = std::min(i.sigma, smax);
                next.push_back(i);
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
                next.push_back(i);

                Particle child = i;
                child.p -= 2.0f * offset;
                next.push_back(child);

            } else if (
                glm::length(i.p_dot) < gamma*i.sigma &&
                i.sigma < delta*sv &&
                R > i.sigma/(delta*sv))
            {
                // halál: sűrűség alapú eliminálás
            } else {
                next.push_back(i);
            }
        }

        floaters = std::move(next);
    }

private:
    Surface surf;
    std::vector<Particle> floaters;   // a mintavételező részecskék
    std::vector<Particle> ctrl;       // a kontrollpontok

    std::mt19937 rng;
    std::uniform_real_distribution<float> dist_R;

    // Értéküket a konstruktor init-listája adja a SimParams-ból.
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
            Particle p;

            // Descartes-koordináták sorsolása a kockán belül. Ha van tartomány-feltétel,
            // elutasításos mintavétellel a JÓ térrészbe célzunk: a részecske ugyan a
            // felület mentén be tudna csúszni, de az lépésenként legfeljebb 0.25 szigma,
            // tehát egy távoli tartománynál sokáig tartana, amíg megjelenik bármi.
            int tries = 0;
            do {
                p.p = glm::vec3{dist_cube(rng), dist_cube(rng), dist_cube(rng)};
                ++tries;
            } while (surf.has_domain && tries < 64 && surf.Dom.at(p.p) <= 0.0f);

            p.sigma = sigma_v();

            // A kezdeti gradiens kiszámítása kritikus a ráhúzó ág miatt
            p.F_x = surf.grad(p.p);

            floaters.push_back(p);
        }
    }

    void calculate_particle(Particle& p, Surface::Workspace& w) {
        // A lefordított programokkal EGY menetben áll elő minden, amire szükség van,
        // a közös részkifejezések pedig csak egyszer futnak le (matek/Program.hpp).
        //
        // A görbületet (és vele a Hesse-mátrixot) csak akkor számoljuk, ha tényleg
        // kell: a Hesse a derivált-fák tömegének ~98%-a, és ha a görbület-taszítás
        // ki van kapcsolva, a curvature_scale() amúgy is 1-et ad.
        if (curvature_repulsion > 0.0f) {
            surf.eval_full(p.p, p.F, p.F_x, p.K, p.F_t, w);
            // Éles CSG-varraton (min/max) a Hesse nem véges — ilyenkor 0, mintha sík lenne.
            if (!std::isfinite(p.K)) p.K = 0.0f;
        } else {
            surf.eval_grad(p.p, p.F, p.F_x, p.F_t, w);
            p.K = 0.0f;
        }
        // Tartomány-feltétel (ha van): érték + gradiens + a felület menti irány.
        // Hesse NEM kell hozzá, ezért ez sokkal olcsóbb, mint a feltételt beépíteni F-be.
        if (surf.has_domain) {
            surf.eval_domain(p.p, p.dom, p.dom_x, w);
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

    static float sign(float x) {
        if (x == 0.0f) return 0.0f;
        return x > 0.0f ? 1.0f : -1.0f;
    }

    // A felülettől mért közelítő GEOMETRIAI távolság: |F| / |∇F| (elsőrendű, Taubin).
    // A nyers |F| skálafüggő (a tórusz kvartikus F-je a felülettől 0.05-re már ~5),
    // ezért a felület-közelség küszöböket erre normáljuk, hogy minden alakzatnál
    // ugyanazt jelentsék.
    static float surface_distance(Particle const& p) {
        float g = glm::length(p.F_x);
        return g > 1e-6f ? std::abs(p.F) / g : std::abs(p.F);
    }

    // A Gauss-kernel ennyi szigmán túl elhanyagolható: exp(-3²/2) ≈ 1.1%. Ennél
    // messzebb lévő párokat sem a rács nem ad vissza, sem a távolság-ellenőrzés
    // nem engedi át — így lesz a taszítás O(n²) helyett O(n).
    static constexpr float REPULSION_CUTOFF = 3.0f;

    SpatialGrid grid;
    float grid_cell = 1.0f;

    // A részecskék felülettől mért távolsága (surface_distance), részecskénként
    // EGYSZER számolva — a taszítás minden jelöltnél ezt nézi.
    std::vector<float> dist;

    // --- párhuzamosítás ----------------------------------------------------------

    // Egy darab legalább ennyi részecske: ennél kisebb munkánál a szálak
    // összehangolása többe kerül, mint amennyit hoz.
    static constexpr std::size_t MIN_CHUNK = 256;

    // Darabonként: a program-munkaterület (a kiértékeléshez), és a taszítás
    // gyűjtőtömbjei. Tagként, hogy lépésenként ne kelljen újrafoglalni.
    std::vector<Surface::Workspace> ws;
    struct Acc {
        std::vector<glm::vec3> P;
        std::vector<float> D, D_sigma;
        // A szomszéd-jelöltek (a 27 cella tartalma) az utoljára nézett cellához. A
        // részecskék cellák szerint rendezve jönnek, tehát egy cella jelöltjeit elég
        // EGYSZER összeszedni, nem részecskénként 27 hash-kereséssel.
        std::vector<int> cand;
    };
    std::vector<Acc> acc;

    std::size_t chunk_count() const {
        std::size_t t = Parallel::threads();
        if (max_threads > 0) t = std::min(t, max_threads);
        return std::clamp<std::size_t>(floaters.size() / MIN_CHUNK, 1, t);
    }

    // fn(k, eleje, vége) a k-adik darabra, a darabok párhuzamosan.
    template<class Fn>
    void for_chunks(std::size_t C, Fn&& fn) {
        std::size_t const n = floaters.size();
        Parallel::for_each(C, [&](std::size_t k) { fn(k, k * n / C, (k + 1) * n / C); }, C);
    }

    // A taszításhoz használt rács újraépítése a lépés eleji pozíciókkal.
    //
    // Előtte a részecskéket a CELLÁJUK szerint rendezzük: így a térben szomszédosak
    // a tömbben is egymás mellé kerülnek (jobb gyorsítótár-kihasználás), és a
    // accumulate_repulsion() cellánként egyszer gyűjti a jelölteket. Mérve 1.3–1.9x gyorsabb lépés,
    // változatlan mintavétellel (részecskeszám, a szomszédtávolság szórása).
    void rebuild_grid() {
        float max_sigma = 0.0f;
        for (auto& p : floaters) max_sigma = std::max(max_sigma, p.sigma);
        grid_cell = std::max(REPULSION_CUTOFF * max_sigma, 1e-4f);

        std::vector<std::pair<std::int64_t, std::size_t>> keyed(floaters.size());
        for (std::size_t k = 0; k < floaters.size(); ++k)
            keyed[k] = {SpatialGrid::key_at(floaters[k].p, grid_cell), k};
        std::sort(keyed.begin(), keyed.end());
        std::vector<Particle> sorted;
        sorted.reserve(floaters.size());
        for (auto const& kv : keyed) sorted.push_back(floaters[kv.second]);
        floaters.swap(sorted);

        grid.build(floaters.size(), [&](std::size_t k) { return floaters[k].p; }, grid_cell);

        dist.resize(floaters.size());
    }

    // A Witkin-taszítás összegei (P, D, D_sigma) a felületi részecskékre, MINDEN PÁRT
    // EGYSZER számolva. Az i-re ható tag (r/σi²·E_ij + r/σj²·E_ji) j-re ellentétes
    // előjellel hat (r_ji = -r_ij), és a két exp() is mindkét oldalt kiszolgálja —
    // így feleannyi exp() és távolság-számítás kell, mint részecskénként külön.
    //
    // Egy tag akkor számít egy részecskének, ha ő a felületen mozog, és a párja
    // közel van a felülethez (a még messze repülők nem taszítanak).
    //
    // Párhuzamosan: a j-re ható tag egy MÁSIK darab részecskéjére is eshet, ezért
    // minden darab a saját gyűjtőtömbjébe ír (nincs versenyhelyzet), és a végén
    // részecskénként összeadjuk őket, rögzített sorrendben.
    void accumulate_repulsion(std::size_t C) {
        std::size_t const n = floaters.size();
        if (acc.size() < C) acc.resize(C);

        for_chunks(C, [&](std::size_t k, std::size_t b, std::size_t e) {
            Acc& A = acc[k];
            A.P.assign(n, glm::vec3(0.0f));
            A.D.assign(n, 0.0f);
            A.D_sigma.assign(n, 0.0f);
            std::int64_t cand_key = 0;
            bool cand_valid = false;

            for (std::size_t a = b; a < e; ++a) {
                Particle const& i = floaters[a];

                std::int64_t const key = SpatialGrid::key_at(i.p, grid_cell);
                if (!cand_valid || key != cand_key) {
                    A.cand.clear();
                    grid.for_each_near(i.p, [&](int jdx) { A.cand.push_back(jdx); });
                    cand_key = key;
                    cand_valid = true;
                }
                for (int jdx : A.cand) {
                    auto const jb = static_cast<std::size_t>(jdx);
                    if (jb <= a) continue;                     // minden pár egyszer
                    Particle const& j = floaters[jb];
                    bool const to_i = i.state == rajtamozog && dist[jb] <= 5e-1f;
                    bool const to_j = j.state == rajtamozog && dist[a] <= 5e-1f;
                    if (!to_i && !to_j) continue;

                    auto  r  = i.p - j.p;
                    float r2 = glm::dot(r, r);
                    // A rács 27 cellája a hatósugárnál nagyobb területet fed le, ezért itt
                    // még pontosan is ellenőrizzük — ez a drága exp() elé kerülő olcsó szűrő.
                    float cut = REPULSION_CUTOFF * std::max(i.sigma, j.sigma);
                    if (r2 > cut * cut) continue;
                    float const E_ij = alpha*std::exp(-r2 / (i.sigma*i.sigma*2));
                    float const E_ji = alpha*std::exp(-r2 / (j.sigma*j.sigma*2));
                    glm::vec3 const f = r / (i.sigma*i.sigma) * E_ij + r / (j.sigma*j.sigma) * E_ji;
                    if (to_i) { A.P[a]  += f; A.D[a]  += E_ij; A.D_sigma[a]  += r2*E_ij; }
                    if (to_j) { A.P[jb] -= f; A.D[jb] += E_ji; A.D_sigma[jb] += r2*E_ji; }
                }
            }
        });

        // Összegzés részecskénként (a calculate_particle már lenullázta őket).
        for_chunks(C, [&](std::size_t, std::size_t b, std::size_t e) {
            for (std::size_t idx = b; idx < e; ++idx) {
                Particle& p = floaters[idx];
                for (std::size_t k = 0; k < C; ++k) {
                    p.P       += acc[k].P[idx];
                    p.D       += acc[k].D[idx];
                    p.D_sigma += acc[k].D_sigma[idx];
                }
            }
        });
    }

    // A Witkin-lépés többi része egy felületi részecskére, a már összegyűjtött
    // taszításból: σ-adaptáció, a felületre vetített sebesség, a tartomány, a mozgás.
    void witkin_move(Particle& i, float dt) {
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
        //  és a fisszió/halál küszöböket a step()-ben.)

        if (glm::length(i.F_x) > 1e-6f) {
            // A számlálóban a felület SAJÁT mozgása is benne van (dF/dt), ha a képlet
            // használja a `t`-t. Enélkül egy animált felület mögött a részecskék
            // lemaradnának: csak a PHI*F visszacsatolás húzná őket vissza, ami mindig
            // hibával követ.
            i.p_dot =
                i.P -
                    ((glm::dot(i.F_x, i.P) + i.F_t + PHI*i.F)
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
    //    legkisebb jellemző méretéhez van kötve, a 0.25·sigma-s korlát a görbületi
    //    sugárhoz képest is kicsi lépést jelent.
    static constexpr float DOMAIN_MAX_SLIDE = 0.25f;

    // A tartomány-feltétel érvényesítése a Witkin-lépés MÁSODIK kényszereként: a
    // részecske a jó térrész felé mozdul (illetve nem lép ki belőle), miközben végig
    // a felületen marad. A matek a DomainConstraint.hpp-ban van, hogy tesztelhető legyen.
    void apply_domain_constraint(Particle& i, float dt) {
        if (!surf.has_domain) return;

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

    void masik(Particle& i, float dt) {
        // Figueiredo-Gomes: a részecske nincs a felületen, rárepítjük.
        // A sebesség CSILLAPÍTVA halmozódik: eredetileg korlátlanul nőtt, ezért egy
        // messziről induló részecske végül átlőtte a felületet és oszcillált.
        i.p_dot = 0.8f * i.p_dot + i.delta * (-sign(i.F) * i.F_x);

        // Lépéshossz-korlát, hogy egy nagy |∇F| (pl. kvartikus tórusz) se lökje el.
        float step_len = glm::length(i.p_dot) * dt;
        float max_step = APPROACH_MAX_STEP * i.sigma;
        if (step_len > max_step && step_len > 1e-9f) i.p_dot *= max_step / step_len;

        auto uj_p = i.p + i.p_dot * dt;
        if (surf.F.at(uj_p) * i.F < 0.0f) {
            // Átlőttük a felületet: felezzük a lépésközt és álljunk meg.
            i.delta /= 2.0f;
            i.p_dot = glm::vec3{0};
        }
        i.p += i.p_dot * dt;
    }
};

#endif //GORBE_PARTICLESYSTEM_HPP
