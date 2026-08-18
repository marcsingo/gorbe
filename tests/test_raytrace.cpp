// A sugarkoveto komponens tesztje. GL es ablak nelkul fut — pont ezert onallo.
#include <cmath>
#include <cstdio>
#include <string>
#include <filesystem>
#include <system_error>
#include <vector>

#include "raytrace/Raytracer.hpp"
#include "raytrace/Material.hpp"
#include "raytrace/Image.hpp"

using Matek::Analizis::make_kif;

static int failures = 0;
static void ok(std::string const& what, bool c, std::string const& info = "") {
    if (!c) ++failures;
    std::printf("  %-50s %-8s %s\n", what.c_str(), c ? "[OK]" : "[HIBA]", info.c_str());
}
static float lum(glm::vec3 c) { return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b; }
static float sat(glm::vec3 c) {
    float mx = std::max({c.r, c.g, c.b}), mn = std::min({c.r, c.g, c.b});
    return mx > 1e-6f ? (mx - mn) / mx : 0.0f;
}

// A kamera a -x fele nez az origora (Z-up bazis, mint a programban).
static Raytrace::CameraDesc cam_at(glm::vec3 eye) {
    Raytrace::CameraDesc c;
    c.eye   = eye;
    c.front = glm::normalize(-eye);
    c.right = glm::normalize(glm::cross(c.front, glm::vec3{0, 0, 1}));
    c.up    = glm::normalize(glm::cross(c.right, c.front));
    c.fov_deg = 45.0f;
    return c;
}

int main() {
    int const W = 120, H = 90;
    auto px = [&](std::vector<glm::vec3> const& img, int x, int y) {
        return img[static_cast<std::size_t>(y) * W + x];
    };

    Raytrace::Settings st;
    st.width = W; st.height = H;
    st.threads = 1;                    // determinisztikus, gyors kis kepnel

    std::printf("=== 1. Gomb: talalat kozepen, hatter a sarkokban ===\n");
    std::vector<glm::vec3> img;
    {
        Raytrace::ObjectDesc o;
        o.F = make_kif("x^2 + y^2 + z^2 - 1");
        o.color = Raytrace::palette_color(5);          // kek
        img = Raytrace::render({o}, cam_at({0, -6, 0}), st);

        ok("a kep merete stimmel", img.size() == static_cast<std::size_t>(W) * H);
        glm::vec3 c = px(img, W / 2, H / 2);
        glm::vec3 corner = px(img, 2, 2);
        ok("kozepen az alakzat szine (kekes)", c.b > c.r && c.b > 0.05f,
           "rgb=(" + std::to_string(c.r) + "," + std::to_string(c.g) + "," + std::to_string(c.b) + ")");
        ok("a sarok hatter (vilagos, kekes-szurke)", lum(corner) > 0.5f,
           "lum=" + std::to_string(lum(corner)));
        ok("kozep != sarok", lum(c) != lum(corner));
    }

    std::printf("\n=== 2. IRANYFENY: a megvilagitott oldal vilagosabb ===\n");
    {
        // A feny -x, -y, -z fele halad -> a +x,+y,+z felol erkezik.
        // A kamera -y-bol nez, tehat a gomb JOBB felso resze van a feny fele.
        Raytrace::ObjectDesc o;
        o.F = make_kif("x^2 + y^2 + z^2 - 1");
        o.color = Raytrace::palette_color(8);          // vilagosszurke
        Raytrace::Settings s2 = st;
        s2.light_dir = glm::normalize(glm::vec3{-1.0f, 1.0f, -1.0f});
        auto im = Raytrace::render({o}, cam_at({0, -6, 0}), s2);

        // Pontok a gombon belul: a kepernyon jobbra-fel vs. balra-le
        glm::vec3 lit   = px(im, W / 2 + 12, H / 2 - 9);
        glm::vec3 shade = px(im, W / 2 - 12, H / 2 + 9);
        ok("a fenyes oldal vilagosabb az arnyekosnal", lum(lit) > lum(shade) + 0.05f,
           "lit=" + std::to_string(lum(lit)) + " shade=" + std::to_string(lum(shade)));

        // Iranyfeny: a tavolsag NEM szamit. Ugyanaz az alakzat ketszer akkora
        // tavolsagbol nezve/megvilagitva ugyanolyan fenyes legyen a kozepen.
        Raytrace::ObjectDesc big;
        big.F = make_kif("x^2 + y^2 + z^2 - 100");     // r=10
        big.color = o.color;
        auto im_big = Raytrace::render({big}, cam_at({0, -60, 0}), s2);
        float a = lum(px(im,     W / 2 + 12, H / 2 - 9));
        float b = lum(px(im_big, W / 2 + 12, H / 2 - 9));
        ok("nincs tavolsag-csokkenes (vektorszeru feny)", std::abs(a - b) < 0.05f,
           "kozeli=" + std::to_string(a) + " tavoli=" + std::to_string(b));
    }

    std::printf("\n=== 3. MUANYAG: a csucsfeny FEHER (nem az anyag szine) ===\n");
    {
        Raytrace::ObjectDesc o;
        o.F = make_kif("x^2 + y^2 + z^2 - 1");
        o.color = Raytrace::palette_color(0);          // eros piros
        Raytrace::Settings s3 = st;
        s3.width = 240; s3.height = 180;
        s3.light_dir = glm::normalize(glm::vec3{0.0f, 1.0f, -0.35f});  // szemből-felulrol
        s3.shadows = false;
        auto im = Raytrace::render({o}, cam_at({0, -6, 0}), s3);

        // A legvilagosabb pixel a gombon a csucsfeny; a legtelitettebb a tiszta diffuz.
        float best_lum = -1.0f; glm::vec3 hi{0};
        float base_sat = 0.0f;  glm::vec3 lo{0};
        for (int y = 0; y < s3.height; ++y)
            for (int x = 0; x < s3.width; ++x) {
                glm::vec3 c = im[static_cast<std::size_t>(y) * s3.width + x];
                if (c.r > c.b + 0.05f) {               // az alakzatra esik (pirosas)
                    if (lum(c) > best_lum) { best_lum = lum(c); hi = c; }
                    if (sat(c) > base_sat) { base_sat = sat(c); lo = c; }
                }
            }
        ok("van csucsfeny (vilagosabb a diffuznal)", best_lum > lum(lo),
           "csucs=" + std::to_string(best_lum) + " diffuz=" + std::to_string(lum(lo)));
        ok("a csucsfeny KEVESBE telitett -> feher, azaz muanyag",
           sat(hi) < sat(lo) - 0.05f,
           "csucs telitettseg=" + std::to_string(sat(hi)) + " diffuz=" + std::to_string(sat(lo)));
    }

    std::printf("\n=== 4. Tartomany-feltetel: a levagott resz nem latszik ===\n");
    {
        Raytrace::ObjectDesc o;
        o.F = make_kif("x^2 + y^2 + z^2 - 1");
        o.color = Raytrace::palette_color(3);
        auto full = Raytrace::render({o}, cam_at({0, -6, 0}), st);

        o.domain = make_kif("z > 0");                  // csak a felso felgomb
        o.has_domain = true;
        auto half = Raytrace::render({o}, cam_at({0, -6, 0}), st);

        auto is_shape = [](glm::vec3 c) { return c.g > c.r + 0.02f && c.g > c.b + 0.02f; };
        int n_full = 0, n_half = 0, n_low_full = 0, n_low_half = 0;
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                if (is_shape(full[static_cast<std::size_t>(y) * W + x])) {
                    ++n_full; if (y > H / 2 + 6) ++n_low_full;
                }
                if (is_shape(half[static_cast<std::size_t>(y) * W + x])) {
                    ++n_half; if (y > H / 2 + 6) ++n_low_half;
                }
            }
        ok("a levagott valtozat kevesebb pixelt fed", n_half < n_full && n_half > 0,
           "teljes=" + std::to_string(n_full) + " fel=" + std::to_string(n_half));
        ok("az also fel eltunt", n_low_half == 0 && n_low_full > 0,
           "also teljes=" + std::to_string(n_low_full) + " also fel=" + std::to_string(n_low_half));
    }

    std::printf("\n=== 5. Tobb alakzat: a KOZELEBBI takar ===\n");
    {
        Raytrace::ObjectDesc near_o, far_o;
        near_o.F = make_kif("x^2 + (y+2)^2 + z^2 - 1");   // kozelebb a kamerahoz (-y felol)
        near_o.color = Raytrace::palette_color(0);        // piros
        far_o.F  = make_kif("x^2 + (y-2)^2 + z^2 - 1");
        far_o.color  = Raytrace::palette_color(3);        // zold
        auto im = Raytrace::render({far_o, near_o}, cam_at({0, -8, 0}), st);
        glm::vec3 c = px(im, W / 2, H / 2);
        ok("a kozelebbi (piros) latszik, nem a tavolabbi", c.r > c.g,
           "rgb=(" + std::to_string(c.r) + "," + std::to_string(c.g) + "," + std::to_string(c.b) + ")");
    }

    std::printf("\n=== 6. BMP mentes ===\n");
    {
        auto dir = std::filesystem::temp_directory_path() / "gorbe_test_kepek";
        std::string path = Raytrace::image_path(dir);
        bool w = Raytrace::write_bmp(path, W, H, img);
        ok("sikeres iras", w, path);
        if (w) {
            std::FILE* f = std::fopen(path.c_str(), "rb");
            ok("megnyithato", f != nullptr);
            if (f) {
                unsigned char hdr[54] = {0};
                std::size_t got = std::fread(hdr, 1, 54, f);
                std::fseek(f, 0, SEEK_END);
                long size = std::ftell(f);
                std::fclose(f);
                ok("BM fejlec", got == 54 && hdr[0] == 'B' && hdr[1] == 'M');
                ok("24 bites", hdr[28] == 24);
                // sorhossz 4 bajtra igazitva
                long row = ((W * 3 + 3) / 4) * 4;
                ok("a fajlmeret a fejleccel egyezik", size == 54 + row * H,
                   "meret=" + std::to_string(size) + " vart=" + std::to_string(54 + row * H));
                std::remove(path.c_str());
                std::error_code ec; std::filesystem::remove_all(dir, ec);
            }
        }
        // Ket egymas utani kep NEM irhatja felul egymast.
        {
            auto d2 = std::filesystem::temp_directory_path() / "gorbe_test_kepek2";
            std::string p1 = Raytrace::image_path(d2);
            Raytrace::write_bmp(p1, W, H, img);
            std::string p2 = Raytrace::image_path(d2);
            ok("ket kep neve kulonbozik (nincs felulirás)", p1 != p2,
               "p1=" + std::filesystem::path(p1).filename().string() +
               " p2=" + std::filesystem::path(p2).filename().string());
            ok("a nev idobelyeges (kep_ eloteggel)",
               std::filesystem::path(p1).filename().string().rfind("kep_", 0) == 0,
               std::filesystem::path(p1).filename().string());
            ok("a konyvtar letrejott", std::filesystem::exists(d2));
            std::error_code ec; std::filesystem::remove_all(d2, ec);
        }

        // gamma: a 0.5 linearis vilagosabb legyen a felenel (sRGB)
        ok("gamma-korrekcio (0.5 linearis -> ~188)",
           Raytrace::to_srgb_byte(0.5f) > 170 && Raytrace::to_srgb_byte(0.5f) < 200,
           "ertek=" + std::to_string(Raytrace::to_srgb_byte(0.5f)));
        ok("levagas: 0 es 1", Raytrace::to_srgb_byte(-3.0f) == 0 && Raytrace::to_srgb_byte(9.0f) == 255);
        ok("NaN nem tor el semmit", Raytrace::to_srgb_byte(std::nanf("")) == 0);
    }

    std::printf("\n=== 7. Tobbszalu render == egyszalu ===\n");
    {
        Raytrace::ObjectDesc o;
        o.F = make_kif("(x^2 + y^2 + z^2 + 9 - 1)^2 - 4*9*(x^2 + y^2)");   // torusz
        o.color = Raytrace::palette_color(6);
        Raytrace::Settings a = st; a.threads = 1;
        Raytrace::Settings b = st; b.threads = 4;
        auto ia = Raytrace::render({o}, cam_at({0, -12, 7}), a);
        auto ib = Raytrace::render({o}, cam_at({0, -12, 7}), b);
        float worst = 0.0f;
        for (std::size_t i = 0; i < ia.size(); ++i)
            worst = std::max(worst, std::abs(lum(ia[i]) - lum(ib[i])));
        ok("a szalak szama nem valtoztat az eredmenyen", worst < 1e-6f,
           "max elteres=" + std::to_string(worst));
    }

    std::printf("\n=== 8. ANYAGOK: a tablazat es az index-feloldas ===\n");
    // Az anyagot NEVVEL keressuk ki, nem sorszammal: igy a lista atrendezese
    // nem torzitja el csendben azt, hogy melyik tesztet melyik anyagon futtatjuk.
    auto idx_of = [](char const* n) {
        for (int i = 0; i < Raytrace::MATERIAL_COUNT; ++i)
            if (std::string(Raytrace::material_name(i)) == n) return i;
        return -1;
    };
    int const M_PLASTIC = idx_of("Muanyag");
    int const M_RUBBER  = idx_of("Gumi (matt)");
    int const M_CERAMIC = idx_of("Keramia");
    int const M_METAL   = idx_of("Fem");
    int const M_CHROME  = idx_of("Krom (tukor)");
    int const M_GLASS   = idx_of("Uveg");
    int const M_WOOD    = idx_of("Fa");
    int const M_MARBLE  = idx_of("Marvany");
    {
        ok("minden vart anyag megvan",
           M_PLASTIC >= 0 && M_RUBBER >= 0 && M_CERAMIC >= 0 && M_METAL >= 0 &&
           M_CHROME >= 0 && M_GLASS >= 0 && M_WOOD >= 0 && M_MARBLE >= 0,
           std::to_string(Raytrace::MATERIAL_COUNT) + " anyag");

        bool table_ok = true;
        for (int i = 0; i < Raytrace::MATERIAL_COUNT; ++i) {
            auto const& m = Raytrace::material_of(i);
            if (!m.name || !m.name[0])                       table_ok = false;
            if (!(m.ior >= 1.0f))                            table_ok = false;
            if (m.diffuse < 0.0f || m.specular < 0.0f)       table_ok = false;
            if (!(m.shininess > 0.0f))                       table_ok = false;
            if (m.reflectivity < 0.0f || m.reflectivity > 1.0f) table_ok = false;
            if (m.transparency < 0.0f || m.transparency > 1.0f) table_ok = false;
            if (!(m.pattern_scale > 0.0f))                   table_ok = false;
        }
        ok("minden anyag ertelmes ertekekkel", table_ok);
        ok("a muanyag az alapertelmezett (0. index)", M_PLASTIC == 0);
        ok("hatarokon kivuli index -> muanyag",
           std::string(Raytrace::material_name(-1))  == "Muanyag" &&
           std::string(Raytrace::material_name(999)) == "Muanyag");
        ok("ObjectDesc alapertelmezese is muanyag",
           std::string(Raytrace::ObjectDesc{}.material.name) == "Muanyag");
    }

    // Egy egysegnyi gomb adott anyaggal es szinnel, szemből-felulrol vilagitva.
    auto sphere_with = [&](int mat, int color, Raytrace::Settings s) {
        Raytrace::ObjectDesc o;
        o.F        = make_kif("x^2 + y^2 + z^2 - 1");
        o.color    = Raytrace::palette_color(color);
        o.material = Raytrace::material_of(mat);
        return Raytrace::render({o}, cam_at({0, -6, 0}), s);
    };
    // A legvilagosabb es a legtelitettebb pixel az ALAKZATON (piros szin mellett).
    struct Stat { float max_lum = -1.0f, max_sat = -1.0f; glm::vec3 hi{0}, sc{0}; int n = 0; };
    auto scan_red = [](std::vector<glm::vec3> const& im, int w, int h) {
        Stat s;
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                glm::vec3 c = im[static_cast<std::size_t>(y) * w + x];
                if (c.r <= c.b + 0.05f) continue;             // hatter/nem alakzat
                ++s.n;
                if (lum(c) > s.max_lum) { s.max_lum = lum(c); s.hi = c; }
                if (sat(c) > s.max_sat) { s.max_sat = sat(c); s.sc = c; }
            }
        return s;
    };

    std::printf("\n=== 9. MATT vs. FENYES: a gumin nincs csucsfeny ===\n");
    {
        Raytrace::Settings s = st;
        s.width = 240; s.height = 180;
        s.light_dir = glm::normalize(glm::vec3{0.0f, 1.0f, -0.35f});
        s.shadows = false;

        Stat rub = scan_red(sphere_with(M_RUBBER,  0, s), s.width, s.height);
        Stat cer = scan_red(sphere_with(M_CERAMIC, 0, s), s.width, s.height);
        ok("a gumi legvilagosabb pontja sotetebb a keramianal",
           rub.max_lum < cer.max_lum - 0.05f,
           "gumi=" + std::to_string(rub.max_lum) + " keramia=" + std::to_string(cer.max_lum));
        ok("mindketto ugyanakkora feluletet fed (csak a fenye mas)",
           std::abs(rub.n - cer.n) < std::max(20, cer.n / 20),
           "gumi=" + std::to_string(rub.n) + " keramia=" + std::to_string(cer.n));
    }

    std::printf("\n=== 10. FEM: a csucsfeny SZINEZETT (a muanyage feher) ===\n");
    {
        Raytrace::Settings s = st;
        s.width = 240; s.height = 180;
        s.light_dir = glm::normalize(glm::vec3{0.0f, 1.0f, -0.35f});
        s.shadows = false;

        Stat pla = scan_red(sphere_with(M_PLASTIC, 0, s), s.width, s.height);
        Stat met = scan_red(sphere_with(M_METAL,   0, s), s.width, s.height);
        ok("a fem csucsfenye telitettebb a muanyagenal",
           sat(met.hi) > sat(pla.hi) + 0.05f,
           "fem=" + std::to_string(sat(met.hi)) + " muanyag=" + std::to_string(sat(pla.hi)));
        // A muanyagnal a csucsfeny KIFEHEREDIK a diffuzhoz kepest, a femnel nem.
        ok("muanyag: a csucs kevesbe telitett a diffuznal", sat(pla.hi) < sat(pla.sc) - 0.05f,
           "csucs=" + std::to_string(sat(pla.hi)) + " diffuz=" + std::to_string(sat(pla.sc)));
        ok("fem: a csucs NEM feheredik ki", sat(met.hi) > sat(met.sc) - 0.05f,
           "csucs=" + std::to_string(sat(met.hi)) + " diffuz=" + std::to_string(sat(met.sc)));
    }

    std::printf("\n=== 11. UVEG: atlatszik rajta a mogotte levo alakzat ===\n");
    {
        Raytrace::Settings s = st;
        s.shadows = false;                       // csak az atlatszosag szamitson

        Raytrace::ObjectDesc back;               // nagy piros gomb HATUL
        back.F        = make_kif("x^2 + (y-8)^2 + z^2 - 9");
        back.color    = Raytrace::palette_color(0);
        back.material = Raytrace::material_of(M_PLASTIC);

        auto front = [&](int mat) {
            Raytrace::ObjectDesc o;
            o.F        = make_kif("x^2 + y^2 + z^2 - 1");
            o.color    = Raytrace::palette_color(5);   // kek
            o.material = Raytrace::material_of(mat);
            return o;
        };
        // A KOZEPSO doboz biztosan az elso gombon van (annak sugara ~18 pixel).
        auto center_diff = [&](std::vector<glm::vec3> const& a,
                               std::vector<glm::vec3> const& b) {
            float worst = 0.0f;
            for (int y = H / 2 - 6; y <= H / 2 + 6; ++y)
                for (int x = W / 2 - 8; x <= W / 2 + 8; ++x) {
                    glm::vec3 d = a[static_cast<std::size_t>(y) * W + x]
                                - b[static_cast<std::size_t>(y) * W + x];
                    worst = std::max({worst, std::abs(d.r), std::abs(d.g), std::abs(d.b)});
                }
            return worst;
        };

        float const opaque = center_diff(Raytrace::render({front(M_PLASTIC)}, cam_at({0, -6, 0}), s),
                                         Raytrace::render({back, front(M_PLASTIC)}, cam_at({0, -6, 0}), s));
        ok("KONTROLL: atlatszatlan elottel a hatso gomb nem latszik", opaque < 1e-6f,
           "elteres=" + std::to_string(opaque));

        float const glassy = center_diff(Raytrace::render({front(M_GLASS)}, cam_at({0, -6, 0}), s),
                                         Raytrace::render({back, front(M_GLASS)}, cam_at({0, -6, 0}), s));
        ok("uvegen KERESZTUL latszik a hatso gomb", glassy > 0.05f,
           "elteres=" + std::to_string(glassy));

        // Ha nem koveinkjuk tovabb a sugarat, az uveg is atlatszatlan lesz —
        // vagyis a max_depth tenylegesen ezt a mechanizmust kapcsolja.
        Raytrace::Settings s0 = s; s0.max_depth = 0;
        float const flat = center_diff(Raytrace::render({front(M_GLASS)}, cam_at({0, -6, 0}), s0),
                                       Raytrace::render({back, front(M_GLASS)}, cam_at({0, -6, 0}), s0));
        ok("max_depth=0 mellett az uveg is atlatszatlan", flat < 1e-6f,
           "elteres=" + std::to_string(flat));
    }

    std::printf("\n=== 12. KROM: visszatukrozi a kornyezetet ===\n");
    {
        Raytrace::Settings s = st;
        s.shadows = false;

        Raytrace::ObjectDesc ground;             // nagy piros "talaj" a gomb alatt
        ground.F        = make_kif("x^2 + y^2 + (z+21)^2 - 400");
        ground.color    = Raytrace::palette_color(0);
        ground.material = Raytrace::material_of(M_PLASTIC);

        auto ball = [&](int mat) {
            Raytrace::ObjectDesc o;
            o.F        = make_kif("x^2 + y^2 + z^2 - 1");
            o.color    = Raytrace::palette_color(8);   // vilagosszurke
            o.material = Raytrace::material_of(mat);
            return o;
        };
        // A gomb ALSO resze nez a talaj fele — ott latszik a tukorkep.
        auto redness = [&](std::vector<glm::vec3> const& im) {
            float sum = 0.0f; int n = 0;
            for (int y = H / 2 + 3; y <= H / 2 + 9; ++y)
                for (int x = W / 2 - 5; x <= W / 2 + 5; ++x) {
                    glm::vec3 c = im[static_cast<std::size_t>(y) * W + x];
                    sum += c.r - c.b; ++n;
                }
            return n ? sum / n : 0.0f;
        };

        float const dull  = redness(Raytrace::render({ground, ball(M_PLASTIC)}, cam_at({0, -6, 2}), s));
        float const shiny = redness(Raytrace::render({ground, ball(M_CHROME)},  cam_at({0, -6, 2}), s));
        ok("a krom gombon megjelenik a piros talaj", shiny > dull + 0.05f,
           "krom=" + std::to_string(shiny) + " muanyag=" + std::to_string(dull));
    }

    std::printf("\n=== 13. MINTAZAT: a fa es a marvany feluleti rajzot kap ===\n");
    {
        // Nagyobb gomb, hogy a minta tobb periodusa laccon a kepen.
        Raytrace::Settings s = st;
        s.width = 240; s.height = 180;
        s.shadows = false;

        auto big = [&](int mat, glm::vec3 eye) {
            Raytrace::ObjectDesc o;
            o.F        = make_kif("x^2 + y^2 + z^2 - 9");     // r=3
            o.color    = Raytrace::palette_color(1);          // narancs
            o.material = Raytrace::material_of(mat);
            return Raytrace::render({o}, cam_at(eye), s);
        };
        auto is_shape = [](glm::vec3 c) { return c.r > c.b + 0.05f; };

        // "Nagyfrekvencias" valtozas: a szomszedos pixelek kozti LEGNAGYOBB ugras
        // az alakzat belsejeben. A sima arnyalas lassan valtozik, a mintazat eles
        // vonalakat rajzol — ezert a MAXIMUMOT nezzuk, nem az atlagot: egy keskeny
        // sotet evgyuru az atlagban elveszne.
        auto hf = [&](std::vector<glm::vec3> const& im) {
            float worst = 0.0f;
            for (int y = s.height / 2 - 20; y <= s.height / 2 + 20; y += 4)
                for (int x = s.width / 2 - 25; x < s.width / 2 + 25; ++x) {
                    glm::vec3 a = im[static_cast<std::size_t>(y) * s.width + x];
                    glm::vec3 b = im[static_cast<std::size_t>(y) * s.width + x + 1];
                    if (is_shape(a) && is_shape(b))
                        worst = std::max(worst, std::abs(lum(a) - lum(b)));
                }
            return worst;
        };

        // A fa evgyurui a z tengely KORUL futnak, tehat egy gomb EGYENLITOJEN
        // majdnem allandoak (ott a tengelytol mert tavolsag alig valtozik) —
        // FELULROL nezve latszanak koncentrikus korokkent. A szempont kicsit
        // ferde, mert pont a z tengelyrol nezve a kamera bazisa elfajulna.
        glm::vec3 const top{2.0f, 2.0f, 11.0f};
        float const p_top = hf(big(M_PLASTIC, top)), w_top = hf(big(M_WOOD, top));
        ok("a fa evgyurui latszanak (a sima muanyagnal sokkal valtozekonyabb)",
           w_top > p_top * 2.5f + 0.002f,
           "fa=" + std::to_string(w_top) + " muanyag=" + std::to_string(p_top));

        // A marvany erei az x menten valtoznak: oldalrol nezve vizszintesen
        // keresztezzuk oket.
        glm::vec3 const side{0.0f, -12.0f, 0.0f};
        float const p_side = hf(big(M_PLASTIC, side)), m_side = hf(big(M_MARBLE, side));
        ok("a marvany erezete latszik", m_side > p_side * 2.5f + 0.002f,
           "marvany=" + std::to_string(m_side) + " muanyag=" + std::to_string(p_side));

        // A minta a TERBOL szamol, tehat determinisztikus: ugyanaz a render
        // ketszer ugyanazt adja (nem sercen ket fenykep kozott).
        auto wood  = big(M_WOOD, top);
        auto wood2 = big(M_WOOD, top);
        float worst = 0.0f;
        for (std::size_t i = 0; i < wood.size(); ++i)
            worst = std::max(worst, std::abs(lum(wood[i]) - lum(wood2[i])));
        ok("a mintazat determinisztikus", worst == 0.0f, "max elteres=" + std::to_string(worst));

        // Mintazat nelkuli anyagnal a szin valtozatlan marad.
        glm::vec3 c{0.7f, 0.3f, 0.2f};
        ok("mintazat nelkul az alapszin valtozatlan",
           Raytrace::pattern_albedo(c, Raytrace::material_of(M_PLASTIC), {1.3f, -2.1f, 0.7f}) == c);
        ok("a fa mintaja pontrol pontra MAS",
           Raytrace::pattern_albedo(c, Raytrace::material_of(M_WOOD), {1.3f, -2.1f, 0.7f})
           != Raytrace::pattern_albedo(c, Raytrace::material_of(M_WOOD), {2.9f, 0.4f, -1.2f}));
    }

    std::printf("\n=== 14. Tobbszalu render TUKROZESSEL es UVEGGEL is egyezik ===\n");
    {
        Raytrace::ObjectDesc g, m, w;
        g.F = make_kif("x^2 + (y+1.5)^2 + z^2 - 1");
        g.color = Raytrace::palette_color(4); g.material = Raytrace::material_of(M_GLASS);
        m.F = make_kif("(x-2)^2 + y^2 + z^2 - 1");
        m.color = Raytrace::palette_color(9); m.material = Raytrace::material_of(M_CHROME);
        w.F = make_kif("(x+2)^2 + y^2 + z^2 - 1");
        w.color = Raytrace::palette_color(1); w.material = Raytrace::material_of(M_WOOD);

        Raytrace::Settings a = st; a.threads = 1;
        Raytrace::Settings b = st; b.threads = 4;
        auto ia = Raytrace::render({g, m, w}, cam_at({0, -9, 3}), a);
        auto ib = Raytrace::render({g, m, w}, cam_at({0, -9, 3}), b);
        float worst = 0.0f;
        for (std::size_t i = 0; i < ia.size(); ++i)
            worst = std::max(worst, std::abs(lum(ia[i]) - lum(ib[i])));
        ok("a szalak szama itt sem valtoztat semmin", worst < 1e-6f,
           "max elteres=" + std::to_string(worst));
    }

    std::printf("\n%s (%d hiba)\n", failures ? ">>> SIKERTELEN" : ">>> MINDEN TESZT OK", failures);
    return failures != 0;
}
