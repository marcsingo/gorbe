#ifndef GORBE_RAYTRACE_IMAGE_HPP
#define GORBE_RAYTRACE_IMAGE_HPP

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include <glm.hpp>

// Kép kiírása és megnyitása a rendszer képnézegetőjében.
//
// BMP-t írunk, mert egyetlen külső könyvtárat sem igényel (a PNG-hez zlib vagy
// stb_image_write kellene). Cserébe tömörítetlen: egy 900x600-as kép ~1.6 MB —
// egy alkalmi "fényképnél" ez nem számít.
namespace Raytrace {

    // Lineáris [0,1] szín -> 8 bites, gamma-korrigált érték.
    // A gamma nélkül a kép látványosan sötét lenne: a fényszámítás lineáris térben
    // megy, a képernyő viszont ~sRGB-t vár.
    inline std::uint8_t to_srgb_byte(float v) {
        if (!(v > 0.0f)) v = 0.0f;          // NaN is ide esik
        if (v > 1.0f)    v = 1.0f;
        float s = std::pow(v, 1.0f / 2.2f);
        int   i = static_cast<int>(s * 255.0f + 0.5f);
        return static_cast<std::uint8_t>(i < 0 ? 0 : (i > 255 ? 255 : i));
    }

    // 24 bites BMP. A sorok ALULRÓL FELFELÉ következnek és 4 bájtra vannak
    // igazítva, a csatornák sorrendje BGR — ez a formátum előírása.
    inline bool write_bmp(std::string const& path, int w, int h,
                          std::vector<glm::vec3> const& pixels) {
        if (w <= 0 || h <= 0 || static_cast<std::size_t>(w) * h != pixels.size()) return false;

        int const row_raw = w * 3;
        int const pad     = (4 - (row_raw % 4)) % 4;
        int const row     = row_raw + pad;
        std::uint32_t const data_size = static_cast<std::uint32_t>(row) * h;
        std::uint32_t const file_size = 14 + 40 + data_size;

        std::FILE* f = std::fopen(path.c_str(), "wb");
        if (!f) return false;

        auto u16 = [&](std::uint16_t v) { std::fwrite(&v, 2, 1, f); };
        auto u32 = [&](std::uint32_t v) { std::fwrite(&v, 4, 1, f); };
        auto i32 = [&](std::int32_t  v) { std::fwrite(&v, 4, 1, f); };

        // BITMAPFILEHEADER
        std::fputc('B', f); std::fputc('M', f);
        u32(file_size); u16(0); u16(0); u32(14 + 40);
        // BITMAPINFOHEADER
        u32(40); i32(w); i32(h); u16(1); u16(24);
        u32(0); u32(data_size); i32(2835); i32(2835); u32(0); u32(0);

        std::vector<std::uint8_t> line(static_cast<std::size_t>(row), 0);
        for (int y = h - 1; y >= 0; --y) {                 // alulról felfelé
            for (int x = 0; x < w; ++x) {
                glm::vec3 const c = pixels[static_cast<std::size_t>(y) * w + x];
                line[x * 3 + 0] = to_srgb_byte(c.b);       // BGR
                line[x * 3 + 1] = to_srgb_byte(c.g);
                line[x * 3 + 2] = to_srgb_byte(c.r);
            }
            std::fwrite(line.data(), 1, line.size(), f);
        }
        return std::fclose(f) == 0;
    }

    // Új képfájl útvonala a megadott könyvtárban (létrehozza, ha nincs).
    //
    // A HELYET a hívó dönti el — a komponens nem tudja (és nem is akarja tudni),
    // hol van a program. A NÉV időbélyeges, ezért két kép sosem írja felül egymást,
    // még a program újraindítása után sem. Ha egy másodpercen belül több kép
    // készül, egy sorszám kerül a végére.
    inline std::string image_path(std::filesystem::path const& dir) {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);   // hiba esetén az fopen bukik majd

        std::time_t t = std::time(nullptr);
        std::tm tm{};
#if defined(_WIN32)
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        char stamp[32];
        std::strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &tm);

        std::filesystem::path p = dir / ("kep_" + std::string(stamp) + ".bmp");
        for (int i = 2; std::filesystem::exists(p) && i < 1000; ++i)
            p = dir / ("kep_" + std::string(stamp) + "_" + std::to_string(i) + ".bmp");
        return p.string();
    }

    // Megnyitás a rendszer alapértelmezett képnézegetőjében — ez adja az "új ablakot".
    // Szándékosan nem nyitunk saját GLFW-ablakot: az a fő ablak statikus
    // állapotához és az ImGui backendhez nyúlna hozzá, és ezzel a komponens
    // nem lenne többé egyszerűen leválasztható.
    inline void open_in_viewer(std::string const& path) {
#if defined(_WIN32)
        // A "start" első idézőjeles argumentuma az ABLAKCÍM — enélkül a szóközös
        // útvonalat címnek nézné, és nem nyitna meg semmit.
        std::string cmd = "start \"\" \"" + path + "\"";
#elif defined(__APPLE__)
        std::string cmd = "open \"" + path + "\"";
#else
        std::string cmd = "xdg-open \"" + path + "\" &";
#endif
        std::system(cmd.c_str());
    }

}

#endif //GORBE_RAYTRACE_IMAGE_HPP
