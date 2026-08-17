// A sugarkoveto komponens tesztje. GL es ablak nelkul fut — pont ezert onallo.
#include <cmath>
#include <cstdio>
#include <string>
#include <filesystem>
#include <system_error>
#include <vector>

#include "raytrace/Raytracer.hpp"
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

    std::printf("\n%s (%d hiba)\n", failures ? ">>> SIKERTELEN" : ">>> MINDEN TESZT OK", failures);
    return failures != 0;
}
