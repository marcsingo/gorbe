#ifndef GORBE_APP_PHOTO_HPP
#define GORBE_APP_PHOTO_HPP

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include "../raytrace/Image.hpp"
#include "../raytrace/Material.hpp"
#include "../raytrace/Raytracer.hpp"
#include "Scene.hpp"

// A bináris könyvtára (a CMake adja meg); ide mentjük a fényképeket.
#ifndef BINARY_DIR
#define BINARY_DIR "."
#endif

// Sugárkövetett fénykép (a leválasztható raytrace/ komponens bekötése).
namespace Photo {

    // A választható felbontások. EGY táblában, hogy a lenyíló felirata és a
    // tényleges méret ne csúszhasson szét.
    struct Resolution { char const* label; int w, h; };
    inline constexpr Resolution RESOLUTIONS[] = {
        {"640x420", 640, 420}, {"900x600", 900, 600},
        {"1280x850", 1280, 850}, {"1920x1280", 1920, 1280}};
    inline constexpr int RESOLUTION_COUNT = sizeof(RESOLUTIONS) / sizeof(RESOLUTIONS[0]);

    struct Settings {
        int         size_idx = 1;
        bool        shadows  = true;
        std::string status;      // az utolsó fénykép eredménye a panelre
    };

    // A render szinkron (az ablak addig áll), ezért van több szálon és mérsékelt
    // alapfelbontással. Az eredményt a `status`-ba írja.
    inline void take(Scene const& sc, Settings& ps) {
        ps.status.clear();

        std::vector<Raytrace::ObjectDesc> objs;
        for (auto const& s : sc.shapes) {
            if (!s.visible || !s.tree) continue;
            Raytrace::ObjectDesc o;
            o.F = Kif(s.tree);
            if (s.dom_tree) { o.domain = Kif(s.dom_tree); o.has_domain = true; }
            o.color    = Raytrace::palette_color(s.color_idx);
            o.material = Raytrace::material_of(s.material_idx);
            objs.push_back(std::move(o));
        }
        if (objs.empty()) {
            ps.status = "Nincs mit fenykepezni (nyomj Indit-ot).";
            return;
        }

        auto const& cam = sc.camera;
        Raytrace::CameraDesc rc;
        rc.eye     = cam.get_position();
        rc.front   = cam.get_front();
        rc.right   = cam.get_right();
        rc.up      = cam.get_up();
        rc.fov_deg = cam.get_fov_deg();

        Raytrace::Settings rs;
        rs.width   = RESOLUTIONS[ps.size_idx].w;
        rs.height  = RESOLUTIONS[ps.size_idx].h;
        rs.shadows = ps.shadows;

        auto t0  = std::chrono::steady_clock::now();
        auto img = Raytrace::render(objs, rc, rs);
        auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - t0).count();

        // A képek a BINÁRIS mellé, a `kepek/` almappába kerülnek — nem a
        // munkakönyvtárba, mert az indítástól függően bárhol lehet.
        std::string path = Raytrace::image_path(std::filesystem::path(BINARY_DIR) / "kepek");
        if (Raytrace::write_bmp(path, rs.width, rs.height, img)) {
            Raytrace::open_in_viewer(path);
            ps.status = "Kesz (" + std::to_string(ms) + " ms): " + path;
        } else {
            ps.status = "A kep mentese nem sikerult: " + path;
        }
    }

}

#endif //GORBE_APP_PHOTO_HPP
