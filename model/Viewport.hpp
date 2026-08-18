#ifndef GORBE_VIEWPORT_HPP
#define GORBE_VIEWPORT_HPP

#include <algorithm>
#include <glm.hpp>

// A 3D nézet téglalapja a főablakon belül, és az ide tartozó koordináta-átváltás.
//
// Amíg a jelenet a teljes ablakra rajzolódott, elég volt az ablakméret. Most viszont
// a nézet egy ImGui-ablak BELSEJÉBEN van, tehát a képarányt és az egér normalizált
// koordinátáját ehhez a téglalaphoz kell számolni — különben a kép megnyúlik, és a
// kontrollpont-elkapás elcsúszik az egérhez képest.
//
// A tiszta átváltó függvények (to_ndc / aspect / contains) szándékosan állapot
// nélküliek, hogy GL és ablak nélkül tesztelhetők legyenek.
namespace Vp {

    // Képernyő-koordinátában (bal felső origó, y lefelé nő) — ugyanabban a térben,
    // amit az ImGui és a glfwGetCursorPos használ.
    struct Rect {
        float x = 0.0f;
        float y = 0.0f;
        float w = 1.0f;
        float h = 1.0f;
    };

    // Egérpozíció -> normalizált eszközkoordináta [-1,1] a téglalapon belül.
    // Az y-t megfordítjuk: a képernyőn lefelé nő, az OpenGL-ben felfelé.
    inline glm::vec2 to_ndc(Rect const& r, double mx, double my) {
        float const w = (r.w > 1e-6f) ? r.w : 1e-6f;
        float const h = (r.h > 1e-6f) ? r.h : 1e-6f;
        return {
             2.0f * (static_cast<float>(mx) - r.x) / w - 1.0f,
             1.0f - 2.0f * (static_cast<float>(my) - r.y) / h
        };
    }

    inline float aspect(Rect const& r) {
        return (r.h > 1e-6f) ? (r.w / r.h) : 1.0f;
    }

    inline bool contains(Rect const& r, double mx, double my) {
        float const x = static_cast<float>(mx), y = static_cast<float>(my);
        return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
    }

    // --- Az aktuális nézet állapota -----------------------------------------
    // Egyszerre csak egy nézet aktív (a kiválasztott fülé), ezért elég egy példány.
    struct State {
        inline static Rect rect{};
        // Megkapja-e a JELENET az egeret? Igaz, ha a kurzor a képen van, VAGY ha a
        // húzás a képen indult és még tart (lásd a "húzás-retesz"-t a main-ben).
        inline static bool mouse = false;
        // Megkapja-e a jelenet a billentyűzetet? (Hamis, amíg egy UI-mezőbe gépelünk.)
        inline static bool keyboard = false;
    };

    inline Rect current()                 { return State::rect; }
    inline void set_current(Rect const& r){ State::rect = r; }

    inline bool scene_mouse()             { return State::mouse; }
    inline void set_scene_mouse(bool v)   { State::mouse = v; }

    inline bool scene_keyboard()          { return State::keyboard; }
    inline void set_scene_keyboard(bool v){ State::keyboard = v; }

}

#endif //GORBE_VIEWPORT_HPP
