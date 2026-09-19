#ifndef GORBE_SCENE_VALIDATE_HPP
#define GORBE_SCENE_VALIDATE_HPP

#include <cstring>
#include <iterator>
#include <list>
#include <set>
#include <string>
#include <vector>

#include "Document.hpp"
#include "Names.hpp"
#include "Scope.hpp"

// Névellenőrzés. Minden frame-ben lefut (néhány tucat név, elhanyagolható), így a
// piros jelzés azonnal követi a gépelést, az Indít pedig tiltva marad, amíg baj van.
//
// HIBA csak azonos hatókörön BELÜL van (két azonos nevű paraméter ugyanabban a
// listában, két azonos nevű alakzat). A hatókörök KÖZÖTTI azonos név nem hiba,
// hanem ELFEDÉS — a belső nyer, mint C++-ban; ezt a panelen jelezzük (shadows).
struct Problems {
    std::vector<std::string> messages;  // névütközések emberi olvasásra
    std::set<void const*>    bad;       // a hibás sorok (Param*/Shape*) a piros jelzéshez

    bool empty() const { return messages.empty(); }
    bool is_bad(void const* row) const { return bad.count(row) != 0; }
};

inline Problems validate(std::list<Param> const& program_params, SceneDoc const& sc) {
    Problems pr;

    auto check = [&](char const* n, void const* row, std::string const& where) {
        if (bad_ident(n)) {
            pr.messages.push_back(where + ": ervenytelen nev (betuvel kezdodjon, utana betu/szam/_)");
            pr.bad.insert(row);
            return false;
        }
        if (is_reserved(n)) {
            pr.messages.push_back(where + ": a(z) '" + std::string(n) + "' foglalt nev (valtozo vagy fuggveny)");
            pr.bad.insert(row);
            return false;
        }
        return true;
    };
    auto clash = [&](void const* a, void const* b, std::string const& msg) {
        pr.messages.push_back(msg);
        pr.bad.insert(a);
        pr.bad.insert(b);
    };
    // Egy listán belüli ismétlődés keresése.
    auto unique_within = [&](std::list<Param> const& list, std::string const& what) {
        for (auto a = list.begin(); a != list.end(); ++a) {
            if (!check(a->name, &*a, what)) continue;
            for (auto b = std::next(a); b != list.end(); ++b)
                if (std::strcmp(a->name, b->name) == 0)
                    clash(&*a, &*b, what + ": a(z) '" + a->name + "' ketszer szerepel");
        }
    };

    unique_within(program_params, "program-szintu parameter");
    unique_within(sc.params,      "jelenet parametere");

    // Alakzatnevek: a jeleneten belül egyediek.
    for (auto a = sc.shapes.begin(); a != sc.shapes.end(); ++a) {
        if (!check(a->name, &*a, "alakzat")) continue;
        for (auto b = std::next(a); b != sc.shapes.end(); ++b)
            if (std::strcmp(a->name, b->name) == 0)
                clash(&*a, &*b, std::string("ket alakzat neve azonos: ") + a->name);
    }

    // A jelenet saját függvényei: a nevük egyedi, és a paramétereik is érvényes nevek.
    for (auto a = sc.funcs.begin(); a != sc.funcs.end(); ++a) {
        if (!check(a->name, &*a, "fuggveny")) continue;
        for (auto b = std::next(a); b != sc.funcs.end(); ++b)
            if (std::strcmp(a->name, b->name) == 0)
                clash(&*a, &*b, std::string("ket fuggveny neve azonos: ") + a->name);
        std::string ps = a->params;
        for (std::size_t from = 0; from <= ps.size();) {
            std::size_t to = ps.find(',', from);
            if (to == std::string::npos) to = ps.size();
            std::string p = ps.substr(from, to - from);
            p = p.substr(0, p.find('='));
            p.erase(0, p.find_first_not_of(' '));
            p.erase(p.find_last_not_of(' ') + 1);
            if (!p.empty() && (bad_ident(p.c_str()) || is_reserved(p.c_str()))) {
                pr.messages.push_back(std::string(a->name) + ": ervenytelen parameternev: " + p);
                pr.bad.insert(&*a);
            }
            from = to + 1;
        }
    }

    // Lokális paraméterek: alakzaton BELÜL egyediek. (Alakzatok között, és a
    // külső hatókörökkel szemben szabadon egyezhetnek — az elfedés.)
    for (auto const& s : sc.shapes) {
        std::string sn = s.name[0] ? s.name : "(nevtelen)";
        unique_within(s.locals, sn + " lokalis parametere");
    }
    return pr;
}

// Egy név elfed-e egy KÜLSŐBB hatókört? Csak jelzésre, nem hiba.
//
// `levels`: a név SAJÁT listája, majd kifelé a külsőbbek, pl. {lokálisok, jelenet,
// program}. A paraméterek ELŐBB oldódnak fel, mint az alakzatnevek, ezért egy
// paraméter egy azonos nevű alakzatot is elfed — ezt is jelezzük.
inline char const* shadows(char const* name,
                           std::vector<std::list<Param> const*> const& levels,
                           std::list<Param> const& scene_params,
                           std::list<Shape> const& shapes) {
    int const hit = Scope::shadowed(levels, name, 0);
    if (hit >= 0) return levels[hit] == &scene_params ? "elfedi: jelenet" : "elfedi: program";
    for (auto const& sh : shapes)
        if (sh.name[0] && std::strcmp(sh.name, name) == 0) return "elfedi: alakzat";
    return nullptr;
}

#endif //GORBE_SCENE_VALIDATE_HPP
