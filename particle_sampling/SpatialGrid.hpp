#ifndef GORBE_SPATIALGRID_HPP
#define GORBE_SPATIALGRID_HPP

#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include <glm.hpp>

// Egyenletes térbeli rács a részecske-taszításhoz.
//
// A Witkin-taszítás eredetileg minden párt végigjárt: O(n²). A Gauss-kernel viszont
// 3σ-n túl elhanyagolható (exp(-4.5) ≈ 1.1%), tehát elég a közeli szomszédokat nézni.
// A rács cellamérete a legnagyobb hatósugár, így a 27 szomszédos cella biztosan
// tartalmaz minden olyan részecskét, ami még számít -> O(n).
//
// A cellák egy hash-táblában vannak (nem tömbben), mert a részecskék tetszőlegesen
// messze kerülhetnek az origótól — végtelen felületeknél ez tényleg előfordul.
class SpatialGrid {
    float cell_size = 1.0f;
    std::unordered_map<std::int64_t, std::vector<int>> cells;

    static int coord(float v, float cell) {
        return static_cast<int>(std::floor(v / cell));
    }

    // 21-21-21 bites pakolás eltolt koordinátákkal. |c| < 2^20 tartományban ütközésmentes;
    // azon túl a kulcs átfordulhat, de az csak fölösleges szomszéd-jelöltet jelent —
    // a hívó távolság-ellenőrzése kiszűri, tehát az eredmény nem romlik el.
    static std::int64_t key(int cx, int cy, int cz) {
        std::uint64_t x = static_cast<std::uint64_t>(cx + (1 << 20)) & 0x1FFFFFull;
        std::uint64_t y = static_cast<std::uint64_t>(cy + (1 << 20)) & 0x1FFFFFull;
        std::uint64_t z = static_cast<std::uint64_t>(cz + (1 << 20)) & 0x1FFFFFull;
        return static_cast<std::int64_t>((x << 42) | (y << 21) | z);
    }

public:
    // Újraépítés. A get_pos(i) adja az i-edik elem pozícióját.
    // A cellák vektorait csak ürítjük (a kapacitásuk megmarad), így a lépésenkénti
    // újraépítés nem allokál folyamatosan.
    template<class GetPos>
    void build(std::size_t n, GetPos&& get_pos, float cell) {
        cell_size = (cell > 1e-4f) ? cell : 1e-4f;
        for (auto& kv : cells) kv.second.clear();
        for (std::size_t i = 0; i < n; ++i) {
            glm::vec3 p = get_pos(i);
            cells[key(coord(p.x, cell_size), coord(p.y, cell_size), coord(p.z, cell_size))]
                .push_back(static_cast<int>(i));
        }
    }

    // Meghívja fn(j)-t minden olyan j indexre, ami a p körüli 27 cellában van.
    // (Lehet köztük a hatósugáron kívüli is — a szűrés a hívó dolga.)
    template<class Fn>
    void for_each_near(glm::vec3 p, Fn&& fn) const {
        int cx = coord(p.x, cell_size);
        int cy = coord(p.y, cell_size);
        int cz = coord(p.z, cell_size);
        for (int dx = -1; dx <= 1; ++dx)
            for (int dy = -1; dy <= 1; ++dy)
                for (int dz = -1; dz <= 1; ++dz) {
                    auto it = cells.find(key(cx + dx, cy + dy, cz + dz));
                    if (it == cells.end()) continue;
                    for (int j : it->second) fn(j);
                }
    }
};

#endif //GORBE_SPATIALGRID_HPP
