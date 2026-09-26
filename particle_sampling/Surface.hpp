#pragma once
#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <vector>

#include <glm.hpp>

#include "../matek/Kif.hpp"
#include "../scene/Time.hpp"

// Célzott using-deklarációk a régi globális `using namespace Matek::Analizis;` helyett:
// az utóbbi minden fordítási egységbe beszórta az `x`, `y`, `z` neveket is, ami ahogy
// nő a projekt, garantáltan ütközik. Az OPERÁTOROKAT (+ - * / ^) és a DSL-függvényeket
// (sin, cos, ...) nem kell felsorolni: azokat az argumentum típusa alapján az ADL
// megtalálja. A literál-operátort (2.0_k) viszont igen, arra nincs ADL.
using Matek::Analizis::Kif;
using Matek::Analizis::Kifejezes;
using Matek::Analizis::NameResolver;
using Matek::Analizis::Program;
using Matek::Analizis::make_kif;
using Matek::Analizis::kif_and;
using Matek::Analizis::x;
using Matek::Analizis::y;
using Matek::Analizis::z;
using Matek::Analizis::operator""_k;

// Egy implicit felület, F(x,y,z) = 0, a szimulációhoz előkészítve: a deriváltjai
// (gradiens, dF/dt, Hesse) és a belőlük lefordított lapos programok. Az F-et KÉSZ
// (már beparseolt) kifejezésfaként kapja (set_tree); egy felület EGY alakzatot
// mintavételez. GL és ablak nélkül működik.
struct Surface {
    Kif F_dx, F_dy, F_dz;
    // Második deriváltak (Hesse-mátrix) a görbület-számításhoz. Szimmetrikus, ezért
    // elég a felső háromszög: dxy = dyx, stb.
    Kif F_dxx, F_dxy, F_dxz, F_dyy, F_dyz, F_dzz;
    Kif F;

    // dF/dt — a felület SAJÁT mozgása, ha a képlet hivatkozik a `t` időre.
    // A Witkin-lépésben ez a tag tartja a részecskét a mozgó felületen; nélküle
    // csak a PHI*F visszacsatolás húzná vissza, ami láthatóan lemarad. (Ha a képlet
    // nem függ t-től, a szimbolikus deriválás konstans 0-t ad, tehát ingyen van.)
    Kif F_dt;

    Surface() {
        F = Kif(0.0f);    // üres placeholder, amíg nem kap képletet
        calculate();
    }

    // A programok a saját tagjaikra hivatkoznak; nincs értelme másolni.
    Surface(Surface const&) = delete;
    Surface& operator=(Surface const&) = delete;

    // Az F-et egy KÉSZ kifejezésfára állítja, és újraszámolja a deriváltakat.
    void set_tree(std::shared_ptr<Kifejezes const> tree) {
        F = Kif(std::move(tree));
        calculate();
    }

    glm::vec3 grad(glm::vec3 at) const {
        return glm::vec3{
            F_dx.at(at),
            F_dy.at(at),
            F_dz.at(at)
        };
    }

    // --- Tartomány-feltétel (opcionális) --------------------------------------
    // A részecske a "megfelelő térrészben" van, ahol Dom(x,y,z) > 0. Ez SZÁNDÉKOSAN
    // nem épül bele F-be:
    //   * F-be építve (metszet) egy zárt TEST határát kapnánk, tehát a vágólapok is
    //     megjelennének felületként — egy sík darabja helyett egy éket;
    //   * ráadásul F minden deriváltja felrobbanna (mérve: sík téglalapra vágva 4
    //     egymásba ágyazott metszettel 38x drágább, mint külön feltételként).
    // Így F érintetlen marad, és a feltételhez elég az ELSŐ derivált — az kell ahhoz,
    // hogy a részecskét a felület mentén tudjuk a jó térrész felé csúsztatni.
    Kif Dom;
    Kif Dom_dx, Dom_dy, Dom_dz;
    bool has_domain = false;

    void set_domain(std::shared_ptr<Kifejezes const> tree) {
        Dom = Kif(std::move(tree));
        Dom_dx = Dom.derive('x');
        Dom_dy = Dom.derive('y');
        Dom_dz = Dom.derive('z');

        Kif const* all[4] = {&Dom, &Dom_dx, &Dom_dy, &Dom_dz};
        prog_dom = Program{};
        for (int i = 0; i < 4; ++i) out_dom[i] = all[i]->get()->compile(prog_dom);
        prog_dom.finish();

        has_domain = true;
    }

    void clear_domain() { has_domain = false; }

    glm::vec3 dom_grad(glm::vec3 at) const {
        return glm::vec3{
            Dom_dx.at(at),
            Dom_dy.at(at),
            Dom_dz.at(at)
        };
    }

    // Az implicit felület (F=0) KÖZEPES (mean) görbülete a pontban, a gradiensből és a
    // Hesse-mátrixból:
    //     H = ( ∇Fᵀ·Hess·∇F − |∇F|²·tr(Hess) ) / ( 2·|∇F|³ )
    // (Ez a div(∇F/|∇F|) képlete.) Előjeles: a normális irányától függ; az f<0 belül
    // konvencióval egy R sugarú gömbre H = −1/R, vagyis |H| = 1/R, a görbületi sugár 1/|H|.
    // Sík esetén (Hess=0) H=0. |∇F|→0 (kritikus pont) közelében 0-t adunk vissza.
    // A közepes görbület a gradiensből és a Hesse-elemekből (a fenti képlet).
    static float mean_curvature(glm::vec3 g,
                                float fxx, float fyy, float fzz,
                                float fxy, float fxz, float fyz) {
        float gn2 = glm::dot(g, g);
        if (gn2 < 1e-12f) return 0.0f;
        float ghg = g.x*g.x*fxx + g.y*g.y*fyy + g.z*g.z*fzz
                  + 2.0f*(g.x*g.y*fxy + g.x*g.z*fxz + g.y*g.z*fyz);
        float trace = fxx + fyy + fzz;
        return (ghg - gn2*trace) / (2.0f * gn2 * std::sqrt(gn2)); // 2·|∇F|³
    }

    float curvature(glm::vec3 at) const {
        return mean_curvature(grad(at),
                              F_dxx.at(at), F_dyy.at(at), F_dzz.at(at),
                              F_dxy.at(at), F_dxz.at(at), F_dyz.at(at));
    }

    // --- Lefordított, lapos programok (a szimuláció forró útja) ------------------
    // A fabejárás helyett a részecskénkénti kiértékelés ezeken megy: a szükséges
    // mennyiségek EGYETLEN lineáris menetben állnak elő, a közös részkifejezések
    // pedig csak egyszer futnak le (lásd matek/Program.hpp).
    //   prog_grad : F, ∂F/∂x, ∂F/∂y, ∂F/∂z
    //   prog_full : ugyanaz + a Hesse 6 eleme (csak ha kell a görbület)
    //   prog_dom  : a tartomány-feltétel és a gradiense
    Program prog_grad, prog_full, prog_dom;
    int out_grad[5]{};    // F, dF/dx, dF/dy, dF/dz, dF/dt
    int out_full[11]{};   // + a Hesse 6 eleme
    int out_dom[4]{};

    // A programok munkaterülete. Szálanként egy kell: a kiértékelés így több szálon
    // párhuzamosan futhat ugyanazon a felületen (lásd ParticleSystem::step).
    struct Workspace {
        std::vector<float> grad, full, dom, q;
        std::vector<float> dq;   // eval_params kimenete (a részecskénkénti foglalás elkerülésére)
    };

    // --- Paraméterek szerinti deriváltak (a kontrollpontok megoldójához) ---------
    // A q paraméterek CÍMEI (pl. az alakzat lokális paraméterei és a pozíciója): a
    // ∂F/∂q deriváltakból áll össze a megoldó egyenlete, és a részecskék ebből
    // követik a felület változását (q̇·∂F/∂q, mint a ∂F/∂t az időnél).
    std::vector<float const*> params;
    Program prog_q;                 // F, ∂F/∂x, ∂F/∂y, ∂F/∂z, majd ∂F/∂q_k
    std::vector<int> out_q;

    void set_params(std::vector<float const*> q) {
        params = std::move(q);
        compile_params();
    }

    // F, a gradiens és a ∂F/∂q értékek egy menetben; a ∂F/∂q-k a `dq`-ba kerülnek.
    void eval_params(glm::vec3 at, float& F_out, glm::vec3& grad_out,
                     std::vector<float>& dq, Workspace& w) const {
        float const* s = run(prog_q, at, w.q);
        F_out    = s[out_q[0]];
        grad_out = {s[out_q[1]], s[out_q[2]], s[out_q[3]]};
        dq.resize(params.size());
        for (std::size_t k = 0; k < params.size(); ++k) dq[k] = s[out_q[4 + k]];
    }

    // F, a gradiens és az idő szerinti derivált egy menetben.
    void eval_grad(glm::vec3 at, float& F_out, glm::vec3& grad_out, float& Ft_out,
                   Workspace& w) const {
        float const* s = run(prog_grad, at, w.grad);
        F_out    = s[out_grad[0]];
        grad_out = {s[out_grad[1]], s[out_grad[2]], s[out_grad[3]]};
        Ft_out   = s[out_grad[4]];
    }

    // F, a gradiens és a közepes görbület egy menetben.
    void eval_full(glm::vec3 at, float& F_out, glm::vec3& grad_out, float& K_out,
                   float& Ft_out, Workspace& w) const {
        float const* s = run(prog_full, at, w.full);
        F_out    = s[out_full[0]];
        grad_out = {s[out_full[1]], s[out_full[2]], s[out_full[3]]};
        Ft_out   = s[out_full[4]];
        K_out = mean_curvature(grad_out,
                               s[out_full[5]],    // fxx
                               s[out_full[8]],    // fyy
                               s[out_full[10]],   // fzz
                               s[out_full[6]],    // fxy
                               s[out_full[7]],    // fxz
                               s[out_full[9]]);   // fyz
    }

    // A tartomány-feltétel és a gradiense egy menetben.
    void eval_domain(glm::vec3 at, float& dom_out, glm::vec3& grad_out, Workspace& w) const {
        float const* s = run(prog_dom, at, w.dom);
        dom_out  = s[out_dom[0]];
        grad_out = {s[out_dom[1]], s[out_dom[2]], s[out_dom[3]]};
    }

private:
    static float const* run(Program const& prog, glm::vec3 at, std::vector<float>& buf) {
        if (buf.size() < prog.size()) buf.resize(prog.size());
        prog.run(at, buf.data());
        return buf.data();
    }

public:

    void calculate() {
        F_dx = F.derive('x');
        F_dy = F.derive('y');
        F_dz = F.derive('z');

        // Az idő szerinti derivált: a `t` egy CÍM szerinti paraméter, ezért a cím
        // szerinti deriválást hívjuk (lásd Kif::derive(float const*)).
        F_dt = F.derive(SceneTime::ptr());

        // Hesse-mátrix (szimmetrikus): a már kész elsőrendű deriváltakat deriváljuk tovább.
        F_dxx = F_dx.derive('x');
        F_dxy = F_dx.derive('y');
        F_dxz = F_dx.derive('z');
        F_dyy = F_dy.derive('y');
        F_dyz = F_dy.derive('z');
        F_dzz = F_dz.derive('z');

        compile_programs();
        compile_params();
    }

    void compile_params() {
        std::vector<Kif> all = {F, F_dx, F_dy, F_dz};
        for (float const* q : params) all.push_back(F.derive(q));
        prog_q = Program{};
        out_q.clear();
        for (auto const& k : all) out_q.push_back(k.get()->compile(prog_q));
        prog_q.finish();
    }

    void compile_programs() {
        // A sorrend fix, es az eval_* ehhez indexel: F, grad(3), dF/dt, Hesse(6).
        Kif const* all[11] = {&F, &F_dx, &F_dy, &F_dz, &F_dt,
                              &F_dxx, &F_dxy, &F_dxz, &F_dyy, &F_dyz, &F_dzz};

        prog_grad = Program{};
        for (int i = 0; i < 5; ++i) out_grad[i] = all[i]->get()->compile(prog_grad);
        prog_grad.finish();

        prog_full = Program{};
        for (int i = 0; i < 11; ++i) out_full[i] = all[i]->get()->compile(prog_full);
        prog_full.finish();
    }
};
