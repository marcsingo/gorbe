// Ter-transzformaciok (warpok) tesztje: az alakzatot NEM mozgatjuk, hanem az inverz
// lekepezest helyettesitjuk be F-be. A derivaltakat a szimbolikus derivalas intezi.
#include <cmath>
#include <cstdio>
#include <array>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#include "matek/Kif.hpp"
#include "particle_sampling/Transform.hpp"
#include "particle_sampling/WarpPresets.hpp"
#include "scene/Time.hpp"

using namespace Matek::Analizis;

static int failures = 0;
static float norm(glm::vec3 v) { return std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z); }

static void near(std::string const& what, float got, float exp, float tol = 1e-4f) {
    bool c = std::isfinite(got) && std::abs(got - exp) <= tol;
    if (!c) ++failures;
    std::printf("  %-46s %-8s got=%10.5f exp=%10.5f\n",
                what.c_str(), c ? "[OK]" : "[HIBA]", got, exp);
}
static void ok(std::string const& what, bool c, std::string const& info = "") {
    if (!c) ++failures;
    std::printf("  %-46s %-8s %s\n", what.c_str(), c ? "[OK]" : "[HIBA]", info.c_str());
}

static glm::vec3 grad(Kif const& f, glm::vec3 at) {
    return {f.derrive('x').at(at), f.derrive('y').at(at), f.derrive('z').at(at)};
}
static glm::vec3 num_grad(Kif const& f, glm::vec3 at, float h = 1e-3f) {
    return {(f.at(at + glm::vec3{h,0,0}) - f.at(at - glm::vec3{h,0,0})) / (2*h),
            (f.at(at + glm::vec3{0,h,0}) - f.at(at - glm::vec3{0,h,0})) / (2*h),
            (f.at(at + glm::vec3{0,0,h}) - f.at(at - glm::vec3{0,0,h})) / (2*h)};
}
static std::size_t tsize(Kif const& k) { std::ostringstream os; os << k; return os.str().size(); }

static float const PI = 3.14159265358979f;

int main() {
    Kif const gomb = make_kif("x^2 + y^2 + z^2 - 1");     // egyseggomb az origoban
    Kif const sik  = make_kif("x - 1");                    // x = 1 sik

    std::printf("=== 1. Eltolas ===\n");
    {
        TransformParams t; t.pos[0] = 3.0f;
        Kif f = apply_transform(gomb, t);
        near("kozeppont (3,0,0) -> F = -1", f.at({3, 0, 0}), -1.0f);
        near("felszin  (4,0,0) -> F = 0",   f.at({4, 0, 0}),  0.0f);
        near("felszin  (2,0,0) -> F = 0",   f.at({2, 0, 0}),  0.0f);
        near("felszin  (3,1,0) -> F = 0",   f.at({3, 1, 0}),  0.0f);
        near("origo (kivul)   -> F = 8",    f.at({0, 0, 0}),  8.0f);
    }

    std::printf("\n=== 2. Skalazas ===\n");
    {
        TransformParams t; t.scale[0] = 2.0f; t.scale[2] = 0.5f;
        Kif f = apply_transform(gomb, t);   // ellipszoid: a=2, b=1, c=0.5
        near("(2,0,0) a feluleten",   f.at({2.0f, 0, 0}), 0.0f);
        near("(0,1,0) a feluleten",   f.at({0, 1.0f, 0}), 0.0f);
        near("(0,0,0.5) a feluleten", f.at({0, 0, 0.5f}), 0.0f);
        ok("(1.5,0,0) belul", f.at({1.5f, 0, 0}) < 0.0f);
    }

    std::printf("\n=== 3. Forgatas ===\n");
    {
        // a gomb forgatasra invarians -> nem valtozhat
        TransformParams t; t.rot[0] = 0.7f; t.rot[1] = -1.3f; t.rot[2] = 2.1f;
        Kif f = apply_transform(gomb, t);
        near("forgatott gomb valtozatlan (1,0,0)", f.at({1, 0, 0}), 0.0f);
        near("forgatott gomb valtozatlan (0,0,1)", f.at({0, 0, 1}), 0.0f);
        near("forgatott gomb kozeppontja",         f.at({0, 0, 0}), -1.0f);
    }
    {
        // az x = 1 sik z korul +90 fokkal -> y = 1 sik
        TransformParams t; t.rot[2] = PI / 2.0f;
        Kif f = apply_transform(sik, t);
        near("x=1 sik, rz=+90 -> (0,1,0) a feluleten", f.at({0, 1, 0}), 0.0f);
        near("... (0,2,0) kivul  (tavolsag 1)",        f.at({0, 2, 0}), 1.0f);
        near("... (1,0,0) mar NEM a feluleten",        f.at({1, 0, 0}), -1.0f);
    }
    {
        // az x = 1 sik y korul +90 fokkal -> z = -1 sik
        // (Ry(90) a +x tengelyt a -z-be viszi)
        TransformParams t; t.rot[1] = PI / 2.0f;
        Kif f = apply_transform(sik, t);
        near("x=1 sik, ry=+90 -> (0,0,-1) a feluleten", f.at({0, 0, -1}), 0.0f);
    }

    std::printf("\n=== 4. Osszetett: skala -> forgatas -> eltolas ===\n");
    {
        TransformParams t;
        t.scale[0] = 2.0f;                 // x iranyban nyujtott gomb
        t.rot[2]   = PI / 2.0f;            // z korul 90 fok -> a hossztengely +y lesz
        t.pos[1]   = 5.0f;                 // eltolva y-ban
        Kif f = apply_transform(gomb, t);
        near("kozeppont (0,5,0)",            f.at({0, 5, 0}), -1.0f);
        near("hossztengely vege (0,7,0)",    f.at({0, 7, 0}),  0.0f);
        near("hossztengely vege (0,3,0)",    f.at({0, 3, 0}),  0.0f);
        near("rovid tengely (1,5,0)",        f.at({1, 5, 0}),  0.0f);
        near("rovid tengely (0,5,1)",        f.at({0, 5, 1}),  0.0f);
    }

    std::printf("\n=== 5. Derivalt: szimbolikus vs. numerikus a transzformalt alakon ===\n");
    {
        TransformParams t;
        t.pos[0] = 1.5f; t.pos[1] = -2.0f; t.pos[2] = 0.7f;
        t.rot[0] = 0.4f; t.rot[1] = 0.9f;  t.rot[2] = -0.6f;
        t.scale[0] = 1.7f; t.scale[1] = 0.8f; t.scale[2] = 1.2f;

        char const* forms[] = {"x^2 + y^2 + z^2 - 1",
                               "x^4 + y^4 + z^4 - 1",
                               "sunio(x^2+y^2+z^2-1, (x-1)^2+y^2+z^2-1, 0.5)"};
        glm::vec3 pts[] = {{1.5f,-2.0f,0.7f}, {2.5f,-1.0f,1.5f}, {0.0f,0.0f,0.0f}};
        for (auto* src : forms) {
            Kif f = apply_transform(make_kif(src), t);
            float worst = 0.0f;
            for (auto p : pts) {
                glm::vec3 a = grad(f, p), b = num_grad(f, p);
                worst = std::max(worst, norm(a - b));
            }
            ok(std::string("gradiens egyezik: ") + std::string(src).substr(0, 22),
               std::isfinite(worst) && worst < 5e-2f,
               "max elteres=" + std::to_string(worst));
        }
    }

    std::printf("\n=== 6. A parameterek ELOBEN hatnak (nem kell ujraparseolni) ===\n");
    {
        // A warp CIM szerint hivatkozik a TransformParams mezoire, ezert eleg az
        // ERTEKET atirni: a mar felepitett (es lefordított) kifejezes koveti.
        TransformParams t;
        t.pos[0] = 1.0f;                      // NEM egyseg -> a warp beepul
        Kif f = apply_transform(gomb, t);     // egyszer epul fel
        near("kezdetben (2,0,0) a feluleten", f.at({2, 0, 0}), 0.0f);

        t.pos[0] = 10.0f;                     // csak az erteket irjuk at
        near("eltolas utan (11,0,0)",  f.at({11, 0, 0}),  0.0f);
        near("eltolas utan (10,0,0)",  f.at({10, 0, 0}), -1.0f);

        t.scale[1] = 3.0f;
        near("skalazas utan (10,3,0)", f.at({10, 3, 0}),  0.0f);

        t.rot[2] = PI / 2.0f;                 // forgatas is elo
        near("forgatas utan (10,1,0)", f.at({10, 1, 0}),  0.0f);
        near("forgatas utan (13,0,0)", f.at({13, 0, 0}),  0.0f);
    }

    std::printf("\n=== 7. Egysegtranszformacio nem valtoztat semmit ===\n");
    {
        TransformParams t;
        ok("is_identity() igaz", t.is_identity());
        Kif f = apply_transform(gomb, t);
        near("F valtozatlan", f.at({0.3f, -0.4f, 0.9f}), gomb.at({0.3f, -0.4f, 0.9f}));

        // FONTOS viselkedes: egysegtranszformacional a warp NEM epul be (igy az
        // egyszeru alakzatok olcsok maradnak). Ezert az ilyenkor felepitett kifejezes
        // NEM koveti a kesobbi valtoztatast — a hivonak ujra kell epitenie.
        // (A GUI ezert epiti ujra az alakzatot, amint eloszor hozzanyulsz a
        //  transzformacios vezerlokhoz.)
        float before = f.at({5, 0, 0});
        t.pos[0] = 4.0f;
        near("egysegkent epult -> nem koveti a valtozast", f.at({5, 0, 0}), before);
        ok("de a t mar nem egyseg", !t.is_identity());
        near("ujraepitve viszont helyes", apply_transform(gomb, t).at({5, 0, 0}), 0.0f);
    }

    std::printf("\n=== 8. Meret: mennyivel no a fa es a lefordított program ===\n");
    {
        TransformParams t;
        t.pos[0] = 1.0f; t.rot[2] = 0.5f; t.scale[0] = 2.0f;
        char const* forms[] = {"x^2 + y^2 + z^2 - 1",
                               "(x^2+y^2+z^2+9-1)^2 - 4*9*(x^2+y^2)",
                               "sunio(x^2+y^2+z^2-1, (x-1)^2+y^2+z^2-1, 0.5)"};
        std::printf("  %-30s %10s %10s %9s %9s\n", "keplet", "F fa", "F fa (T)", "prog", "prog (T)");
        for (auto* src : forms) {
            Kif a = make_kif(src);
            Kif b = apply_transform(a, t);
            auto prog_size = [](Kif const& k) {
                Kif fx = k.derrive('x'), fy = k.derrive('y'), fz = k.derrive('z');
                Kif all[10] = {k, fx, fy, fz,
                               fx.derrive('x'), fx.derrive('y'), fx.derrive('z'),
                               fy.derrive('y'), fy.derrive('z'), fz.derrive('z')};
                Program p;
                for (auto& e : all) e.get()->compile(p);
                p.finish();
                return p.size();
            };
            std::printf("  %-30s %10zu %10zu %9zu %9zu\n",
                        std::string(src).substr(0, 30).c_str(),
                        tsize(a), tsize(b), prog_size(a), prog_size(b));
        }
    }

    std::printf("\n=== 9. Warp: csavaras (twist) ===\n");
    {
        // A warp-sablon a VISSZAFELE lekepezes, ezert a "+a"-val csavaro deformacio
        // inverze szerepel benne. Teszteljuk az egyseghengert (x^2+y^2-1): a csavaras
        // z korul forgat, tehat a hengert VALTOZATLANUL kell hagynia.
        float a = 0.3f;
        Kif henger = make_kif("x^2 + y^2 - 1");
        Kif wx = make_kif("x*cos(0.3*z) + y*sin(0.3*z)");
        Kif wy = make_kif("0 - x*sin(0.3*z) + y*cos(0.3*z)");
        Kif wz = make_kif("z");
        Kif f = apply_warp(henger, wx, wy, wz);
        near("csavart henger valtozatlan (1,0,0)",  f.at({1, 0, 0}), 0.0f);
        near("csavart henger valtozatlan (1,0,5)",  f.at({1, 0, 5}), 0.0f);
        near("csavart henger valtozatlan (0,1,-3)", f.at({0, 1, -3}), 0.0f);

        // Egy z-tol fuggo alakzaton viszont LATSZIK a csavaras: a "sik" x=0 (az yz sik)
        // z magassagban a*z szoggel elfordul.
        Kif sik = make_kif("x");
        Kif g = apply_warp(sik, wx, wy, wz);
        float zz = 2.0f, th = a * zz;             // ennyivel fordul el
        // az elfordult sikon rajta van a (sin(th)*t, ... ) irany: ellenorizzuk, hogy
        // a (-sin(th), cos(th)) irany a feluleten van z=zz-nel
        near("csavart sik: elfordult irany rajta van",
             g.at({-std::sin(th), std::cos(th), zz}), 0.0f, 1e-4f);
        ok("csavart sik: az eredeti y irany MAR NEM rajta", std::abs(g.at({0, 1, zz})) > 1e-3f,
           "F=" + std::to_string(g.at({0, 1, zz})));
    }

    std::printf("\n=== 10. Warp: nyiras es hullam (egyszeru inverzek) ===\n");
    {
        // nyiras: a forward x' = x + k*z, tehat az inverz x -> x - k*z
        float k = 0.5f;
        Kif sik = make_kif("x");                       // az x=0 sik
        Kif f = apply_warp(sik, make_kif("x - 0.5*z"), make_kif("y"), make_kif("z"));
        near("nyirt sik z=0-nal x=0",  f.at({0.0f, 0, 0}), 0.0f);
        near("nyirt sik z=2-nel x=1",  f.at({k * 2.0f, 0, 2}), 0.0f);
        near("nyirt sik z=-2-nel x=-1", f.at({-k * 2.0f, 0, -2}), 0.0f);

        // hullam: forward z' = z + a*sin(w*x); inverz z -> z - a*sin(w*x)
        Kif zsik = make_kif("z");                      // a z=0 sik
        Kif h = apply_warp(zsik, make_kif("x"), make_kif("y"), make_kif("z - 0.3*sin(1.0*x)"));
        near("hullamos sik x=0-nal z=0", h.at({0, 0, 0.0f}), 0.0f);
        float xx = 1.2f;
        near("hullamos sik a hullamhegyen", h.at({xx, 0, 0.3f * std::sin(xx)}), 0.0f);
    }

    std::printf("\n=== 11. Warp-LANC: a sorrend szamit ===\n");
    {
        // Ket warp: (A) eltolas x-ben 1-gyel, (B) ketszerezo skalazas x-ben.
        // A behelyettesitesek egymasba agyazodnak: az elso elem hat eloszor az alakzatra.
        Kif gomb0 = make_kif("x^2 + y^2 + z^2 - 1");
        Kif A_x = make_kif("x - 1"), Iy = make_kif("y"), Iz = make_kif("z");
        Kif B_x = make_kif("x/2");

        // lanc: A, majd B
        Kif ab = apply_warp(apply_warp(gomb0, A_x, Iy, Iz), B_x, Iy, Iz);
        // lanc: B, majd A
        Kif ba = apply_warp(apply_warp(gomb0, B_x, Iy, Iz), A_x, Iy, Iz);

        // ab(p) = F((p/2) - 1)  -> kozeppont ott, ahol p/2 - 1 = 0, azaz p = 2
        near("A majd B: kozeppont x=2", ab.at({2, 0, 0}), -1.0f);
        // ba(p) = F((p-1)/2)    -> kozeppont ott, ahol (p-1)/2 = 0, azaz p = 1
        near("B majd A: kozeppont x=1", ba.at({1, 0, 0}), -1.0f);
        ok("a ket sorrend KULONBOZO eredmenyt ad",
           std::abs(ab.at({2, 0, 0}) - ba.at({2, 0, 0})) > 1e-3f);
    }

    std::printf("\n=== 12. Warp + affin transzformacio egyutt ===\n");
    {
        // A program eloszor a warp-lancot alkalmazza (az alakzat SAJAT tereben),
        // utana az affin transzformaciot. Igy a mar deformalt alakzatot helyezi el.
        Kif henger = make_kif("x^2 + y^2 - 1");
        Kif wx = make_kif("x*cos(0.3*z) + y*sin(0.3*z)");
        Kif wy = make_kif("0 - x*sin(0.3*z) + y*cos(0.3*z)");
        Kif wz = make_kif("z");

        TransformParams t; t.pos[0] = 5.0f;
        Kif f = apply_transform(apply_warp(henger, wx, wy, wz), t);

        near("eltolt csavart henger palastja (6,0,0)", f.at({6, 0, 0}), 0.0f);
        near("eltolt csavart henger palastja (4,0,2)", f.at({4, 0, 2}), 0.0f);
        near("eltolt csavart henger tengelye",         f.at({5, 0, 0}), -1.0f);

        // gradiens ellenorzes a deformalt+elhelyezett alakon
        glm::vec3 p{6, 0, 1};
        float err = norm(grad(f, p) - num_grad(f, p));
        ok("gradiens egyezik a warp+transzformacio utan is",
           std::isfinite(err) && err < 5e-2f, "elteres=" + std::to_string(err));
    }

    std::printf("\n=== 13. Warp-lanc merete (a CSE mennyit szed vissza) ===\n");
    {
        Kif gomb1 = make_kif("x^2 + y^2 + z^2 - 1");
        Kif wx = make_kif("x*cos(0.3*z) + y*sin(0.3*z)");
        Kif wy = make_kif("0 - x*sin(0.3*z) + y*cos(0.3*z)");
        Kif wz = make_kif("z");
        auto prog_size = [](Kif const& k) {
            Kif fx = k.derrive('x'), fy = k.derrive('y'), fz = k.derrive('z');
            Kif all[10] = {k, fx, fy, fz,
                           fx.derrive('x'), fx.derrive('y'), fx.derrive('z'),
                           fy.derrive('y'), fy.derrive('z'), fz.derrive('z')};
            Program p;
            for (auto& e : all) e.get()->compile(p);
            p.finish();
            return p.size();
        };
        std::printf("  %-28s %10s %10s\n", "lanc hossza", "fa (kar.)", "program");
        Kif f = gomb1;
        for (int n = 0; n <= 3; ++n) {
            std::printf("  %-28d %10zu %10zu\n", n, tsize(f), prog_size(f));
            f = apply_warp(f, wx, wy, wz);
        }
    }

    std::printf("\n=== 14. `pi` allando a parserben ===\n");
    {
        near("pi",        make_kif("pi").at({0, 0, 0}),        3.14159265f, 1e-5f);
        near("90*pi/180", make_kif("90*pi/180").at({0, 0, 0}), 1.5707963f,  1e-5f);
        near("cos(180*pi/180)", make_kif("cos(180*pi/180)").at({0, 0, 0}), -1.0f, 1e-5f);
    }

    std::printf("\n=== 15. A WARP-SABLONOK (pontosan azok, amiket a program hasznal) ===\n");
    {
        // Egy sablon felepitese ugyanugy, ahogy a GUI teszi: parameternevek
        // behelyettesitese a $1/$2/... helyere, majd parseolas.
        struct Built { Kif fx, fy, fz; std::vector<float> vals; };
        auto build = [](WarpPresets::Preset const& p, std::vector<float> const& values,
                        std::vector<float>& storage) {
            storage = values;
            std::vector<std::string> names;
            for (std::size_t i = 0; i < storage.size(); ++i) names.push_back("p" + std::to_string(i));
            char bx[256], by[256], bz[256];
            WarpPresets::fill_template(bx, sizeof(bx), p.fx, names);
            WarpPresets::fill_template(by, sizeof(by), p.fy, names);
            WarpPresets::fill_template(bz, sizeof(bz), p.fz, names);
            auto res = [&](std::string const& nm) -> std::shared_ptr<Kifejezes const> {
                for (std::size_t i = 0; i < storage.size(); ++i)
                    if (nm == "p" + std::to_string(i)) return Kif(&storage[i]).get();
                return nullptr;
            };
            return std::array<Kif, 3>{make_kif(bx, res), make_kif(by, res), make_kif(bz, res)};
        };
        auto find = [](char const* label) -> WarpPresets::Preset const& {
            for (auto const& p : WarpPresets::ALL)
                if (std::string(p.label).rfind(label, 0) == 0) return p;
            std::printf("  NINCS ILYEN SABLON: %s\n", label);
            std::exit(1);
        };

        Kif gomb2 = make_kif("x^2 + y^2 + z^2 - 1");
        Kif sikx  = make_kif("x - 1");     // az x=1 sik
        Kif siky  = make_kif("y - 1");
        Kif sikz  = make_kif("z - 1");

        // --- Eltolas ---
        {
            std::vector<float> v;
            auto w = build(find("Eltolas"), {3.0f, -2.0f, 1.0f}, v);
            Kif f = apply_warp(gomb2, w[0], w[1], w[2]);
            near("eltolas: kozeppont (3,-2,1)", f.at({3, -2, 1}), -1.0f);
            near("eltolas: felszin (4,-2,1)",   f.at({4, -2, 1}),  0.0f);
            v[0] = 10.0f;    // eloben kovesse az erteket
            near("eltolas: eloben all (10,-2,1)", f.at({10, -2, 1}), -1.0f);
        }
        // --- Skalazas ---
        {
            std::vector<float> v;
            auto w = build(find("Skalazas"), {2.0f, 1.0f, 0.5f}, v);
            Kif f = apply_warp(gomb2, w[0], w[1], w[2]);
            near("skalazas: (2,0,0) a feluleten",   f.at({2.0f, 0, 0}), 0.0f);
            near("skalazas: (0,1,0) a feluleten",   f.at({0, 1.0f, 0}), 0.0f);
            near("skalazas: (0,0,0.5) a feluleten", f.at({0, 0, 0.5f}), 0.0f);
        }
        // --- Forgatasok, FOKBAN ---
        {
            std::vector<float> v;
            auto w = build(find("Forgatas z"), {90.0f}, v);
            Kif f = apply_warp(sikx, w[0], w[1], w[2]);
            near("forgatas z, +90 fok: x=1 sik -> y=1", f.at({0, 1, 0}), 0.0f);
            near("... es (1,0,0) mar nem a feluleten",   f.at({1, 0, 0}), -1.0f);
            // a gomb forgatasra invarians
            near("forgatott gomb valtozatlan",
                 apply_warp(gomb2, w[0], w[1], w[2]).at({1, 0, 0}), 0.0f);
        }
        {
            std::vector<float> v;
            auto w = build(find("Forgatas x"), {90.0f}, v);
            Kif f = apply_warp(siky, w[0], w[1], w[2]);
            near("forgatas x, +90 fok: y=1 sik -> z=1", f.at({0, 0, 1}), 0.0f);
        }
        {
            std::vector<float> v;
            auto w = build(find("Forgatas y"), {90.0f}, v);
            Kif f = apply_warp(sikz, w[0], w[1], w[2]);
            near("forgatas y, +90 fok: z=1 sik -> x=1", f.at({1, 0, 0}), 0.0f);
        }
        // --- Minden sablon alapertekkel is ertelmes (nincs 0-val osztas, NaN) ---
        {
            for (auto const& p : WarpPresets::ALL) {
                std::vector<float> vals;
                for (auto const& pd : p.params) vals.push_back(pd.value);
                std::vector<float> v;
                auto w = build(p, vals, v);
                Kif f = apply_warp(gomb2, w[0], w[1], w[2]);
                bool fin = true;
                glm::vec3 pts[] = {{0,0,0}, {1,0,0}, {0.5f,-0.5f,0.7f}, {2,3,-1}};
                for (auto q : pts) {
                    if (!std::isfinite(f.at(q))) fin = false;
                    auto g = grad(f, q);
                    if (!std::isfinite(g.x) || !std::isfinite(g.y) || !std::isfinite(g.z)) fin = false;
                }
                ok(std::string("alapertekkel veges: ") + std::string(p.label).substr(0, 24), fin);
            }
        }
    }

    std::printf("\n=== 16. A `t` IDO a warpokban ===\n");
    {
        // A warp-kifejezesek UGYANAZT a nevfeloldot hasznaljak, mint a keplet, tehat
        // a `t` bennuk is mukodik. A lenyeges kerdes, hogy a dF/dt a warpon KERESZTUL
        // is helyes-e — a szimbolikus derivalas ehhez a lancszabalyt kell alkalmazza.
        auto res = [](std::string const& nm) -> std::shared_ptr<Kifejezes const> {
            if (nm == "t") return Kif(SceneTime::ptr()).get();
            return nullptr;
        };

        // Idoben csavarodo, ellipszis keresztmetszetu cso. A csavaras szoge a*t*z,
        // tehat a warp maga fugg az idotol.
        Kif henger = make_kif("x^2/4 + y^2 - 1");
        Kif wx = make_kif("x*cos(0.3*t*z) + y*sin(0.3*t*z)", res);
        Kif wy = make_kif("0 - x*sin(0.3*t*z) + y*cos(0.3*t*z)", res);
        Kif wz = make_kif("z", res);
        Kif f  = apply_warp(henger, wx, wy, wz);

        // t=0-nal a csavaras szoge 0 -> az eredeti alakzat.
        SceneTime::value = 0.0f;
        near("t=0: (2,0,3) a feluleten (nincs csavarodas)", f.at({2, 0, 3}), 0.0f);
        near("t=0: (0,1,3) a feluleten",                    f.at({0, 1, 3}), 0.0f);

        // t>0-nal a z=3 magassagban a keresztmetszet 0.3*t*3 szoggel fordul el.
        SceneTime::value = 2.0f;
        float const th = 0.3f * 2.0f * 3.0f;
        near("t=2: az ELFORDULT hossztengely a feluleten",
             f.at({2.0f * std::cos(th), 2.0f * std::sin(th), 3.0f}), 0.0f, 1e-3f);
        ok("t=2: az eredeti irany mar NEM a feluleten",
           std::abs(f.at({2, 0, 3})) > 1e-2f, "F=" + std::to_string(f.at({2, 0, 3})));

        // A LENYEG: dF/dt a warpon keresztul is helyes (lancszabaly).
        Kif ft = f.derrive(SceneTime::ptr());
        float const h = 1e-3f;
        glm::vec3 pts[] = {{1.6f, 0.5f, 2.0f}, {0.3f, -0.9f, -1.5f}, {2.0f, 0.0f, 3.0f}};
        float worst = 0.0f;
        for (auto q : pts) {
            SceneTime::value = 2.0f + h; float fp = f.at(q);
            SceneTime::value = 2.0f - h; float fm = f.at(q);
            SceneTime::value = 2.0f;
            worst = std::max(worst, std::abs(ft.at(q) - (fp - fm) / (2.0f * h)));
        }
        ok("dF/dt a warpon keresztul: szimbolikus == numerikus",
           std::isfinite(worst) && worst < 5e-2f, "max elteres=" + std::to_string(worst));

        // Warp-LANC: idofuggo csavaras + idofuggo eltolas egyutt.
        Kif tx = make_kif("x - 1.5*t", res);
        Kif f2 = apply_warp(f, tx, make_kif("y"), make_kif("z"));
        SceneTime::value = 2.0f;
        near("lancban az eltolas is kovet (kozeppont x=3)", f2.at({3.0f, 0.0f, 0.0f}), -1.0f);
        Kif f2t = f2.derrive(SceneTime::ptr());
        {
            glm::vec3 q{3.4f, 0.3f, 1.0f};
            SceneTime::value = 2.0f + h; float fp = f2.at(q);
            SceneTime::value = 2.0f - h; float fm = f2.at(q);
            SceneTime::value = 2.0f;
            ok("dF/dt a teljes LANCON keresztul is helyes",
               std::abs(f2t.at(q) - (fp - fm) / (2.0f * h)) < 5e-2f,
               "elteres=" + std::to_string(std::abs(f2t.at(q) - (fp - fm) / (2.0f * h))));
        }

        SceneTime::value = 0.0f;
    }

    std::printf("\n%s (%d hiba)\n", failures ? ">>> SIKERTELEN" : ">>> MINDEN TESZT OK", failures);
    return failures != 0;
}
