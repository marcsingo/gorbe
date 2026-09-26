#ifndef GORBE_UI_REQUESTS_HPP
#define GORBE_UI_REQUESTS_HPP

#include <list>
#include <string>

#include "../scene/Document.hpp"
#include "../scene/Scope.hpp"

struct Scene;

namespace Ui {

    // A nézet -> vezérlő irány. A panelek csak JELEZNEK, a végrehajtás a frame
    // végén, a Controller::apply-ban történik.
    //
    // A szabály: a nézet élőben írhatja az ÉRTÉKEKET (csúszka, szövegmező), és a
    // lista VÉGÉRE is szúrhat (std::list: a meglévő elemek címe nem mozdul). Amit
    // viszont egy kifejezésfa címmel tarthat (paraméter, alakzat, jelenet), azt
    // SOSEM szabadítja fel: a törlés előtt el kell dobni a fákat, és ezt a vezérlő
    // végzi, egy helyen.
    //
    // A késleltetésnek ImGui-os oka is van: a fülsáv ciklusa minden frame-ben
    // felülírja a kiválasztást, a fénykép pedig ne egy félig felépített frame
    // közben blokkolja a programot.
    struct Requests {
        bool build = false;     // Indít (vagy a transzformáció első mozdítása)
        bool drop  = false;     // Töröl: a képletek eldobása
        bool photo = false;     // sugárkövetett fénykép
        bool cancel_job = false;   // a folyamatban lévő hosszú munka megszakítása

        Shape*            erase_shape = nullptr;
        std::list<Param>* erase_from  = nullptr;   // melyik listából ...
        Param*            erase_param = nullptr;   // ... melyik paramétert
        Shape*            edit_shape  = nullptr;   // variációs szerkesztő fül nyitása

        bool   new_scene   = false;
        Scene* want_scene  = nullptr;   // a fülsávon kiválasztott fül
        Scene* close_scene = nullptr;

        // Projekt (Fájl menü). Az új projekt és a betöltés MINDEN jelenetet lecserél.
        bool        new_project = false;
        std::string load_path;          // üres = nincs kérés
        std::string save_path;
    };

    // A csak a nézethez tartozó, frame-ek KÖZÖTT megmaradó állapot. Nem lehet a
    // panelfüggvények helyi változója: az minden frame-ben visszaállna.
    struct Layout {
        // A bal sáv alapértelmezett szélességét az irányítás-tábla szabja meg: a
        // második oszlop 178 px-nél kezdődik, tehát ennél keskenyebben elvágódna.
        float left_w  = 372.0f;
        float right_w = 380.0f;    // jobb sáv (Tulajdonsagok) — ide kerül a képlet
        // A BAL sáv HÁROM panelre oszlik (Alakzatok / Nezet / Parameterek); ez a két
        // arány a felső kettő magassága, a harmadik a maradék. A JOBB sáv egyetlen,
        // teljes magasságú panel.
        float left_f1 = 0.30f;
        float left_f2 = 0.42f;
    };

    struct UiState {
        Layout layout;
        int    preset_idx      = 0;   // alakzat-sablon a lenyílóban
        int    warp_preset_idx = 0;   // warp-sablon a lenyílóban
        bool   drag_latch      = false;

        // A Fájl menü útvonal-ablaka (Megnyitás / Mentés másként).
        enum class FileDialog { None, Open, SaveAs };
        FileDialog file_dialog = FileDialog::None;
        char       path_buf[512] = "";
    };

}

#endif //GORBE_UI_REQUESTS_HPP
