// Ter-transzformaciok (warpok) tesztje: az alakzatot NEM mozgatjuk, hanem az inverz
// lekepezest helyettesitjuk be F-be. A derivaltakat a szimbolikus derivalas intezi.
#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>

#include "matek/Kif.hpp"
#include "particle_sampling/Transform.hpp"

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

    std::printf("\n%s (%d hiba)\n", failures ? ">>> SIKERTELEN" : ">>> MINDEN TESZT OK", failures);
    return failures != 0;
}
