#ifndef GORBE_SCOPE_HPP
#define GORBE_SCOPE_HPP

#include <cstring>
#include <list>
#include <vector>

// Nevesitett parameter es a HATOKOR-feloldas.
//
// Harom szint van, kifele haladva:  alakzat -> jelenet -> program.
// Az elso talalat nyer, tehat a BELSO ELFEDI a kulsot — ugyanaz a szabaly, mint
// C++-ban. Az elfedes NEM hiba; a GUI csak jelzi.
//
// Kulon fejlecben van (nem a main.cpp-ben), hogy GL es ablak nelkul tesztelheto
// legyen: ez a fajta "melyik nev mit jelent" logika csendben romlik el.
struct Param {
    char  name[32] = "";
    float value    = 0.0f;
};

namespace Scope {

    // A szintek listaja LEGBELSOTOL kifele. A talalt parameter ERTEKENEK CIME jon
    // vissza (a kifejezesfa cim szerint hivatkozik ra, ezert kell a cim), vagy
    // nullptr, ha egyik szinten sincs ilyen nev.
    inline float const* find(std::vector<std::list<Param> const*> const& levels,
                             char const* name) {
        if (!name || !name[0]) return nullptr;
        for (auto const* lvl : levels) {
            if (!lvl) continue;
            for (auto const& p : *lvl)
                if (p.name[0] && std::strcmp(p.name, name) == 0) return &p.value;
        }
        return nullptr;
    }

    // Hanyadik szinten van a nev (0 = legbelso), vagy -1 ha sehol.
    inline int level_of(std::vector<std::list<Param> const*> const& levels,
                        char const* name) {
        if (!name || !name[0]) return -1;
        for (std::size_t i = 0; i < levels.size(); ++i) {
            if (!levels[i]) continue;
            for (auto const& p : *levels[i])
                if (p.name[0] && std::strcmp(p.name, name) == 0) return static_cast<int>(i);
        }
        return -1;
    }

    // Elfed-e a `from` szinten levo nev egy KULSOBB szintet? Az elso elfedett szint
    // indexe, vagy -1 ha nincs elfedes.
    inline int shadowed(std::vector<std::list<Param> const*> const& levels,
                        char const* name, int from) {
        if (!name || !name[0]) return -1;
        for (std::size_t i = static_cast<std::size_t>(from) + 1; i < levels.size(); ++i) {
            if (!levels[i]) continue;
            for (auto const& p : *levels[i])
                if (p.name[0] && std::strcmp(p.name, name) == 0) return static_cast<int>(i);
        }
        return -1;
    }

}

#endif //GORBE_SCOPE_HPP
