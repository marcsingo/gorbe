#ifndef MATEK_KIFEJEZES_HPP
#define MATEK_KIFEJEZES_HPP

#include <map>
#include <memory>
#include <iostream>
#include <functional>
#include <string>
#include <variant>
#include "vec3.hpp"
#include "Program.hpp"

namespace Matek {
    namespace Analizis {

        struct Kifejezes;
        using Tree = std::shared_ptr<Kifejezes const>;

        // Változó -> részkifejezés csere táblája (lásd Kifejezes::substitute).
        using SubstMap = std::map<char, Tree>;

        // Paraméter -> név (a névfeloldó inverze), a kiíráshoz. Üres szöveg = nincs
        // neve; ilyenkor a paraméter pillanatnyi értéke íródik ki.
        using ParamNamer = std::function<std::string(float const*)>;

        // Ami szerint deriválni lehet: egy térbeli változó ('x', 'y', 'z'), egy
        // paraméter (a float CÍME), vagy egy tetszőleges részfa (azonosság szerint).
        using Var = std::variant<char, float const*, Kifejezes const*>;

        struct Kifejezes {
            Kifejezes() = default;
            virtual float at(glm::vec3 const v) const = 0;
            float operator()(glm::vec3 const v) const { return at(v); }

            // Szimbolikus derivált (egyszerűsítés nélkül; azt a Kif::derive végzi).
            virtual Tree derive(Var const& var) const = 0;

            // Kiírás a parser nyelvén (matek/Parser.hpp): a kimenet újra beolvasható.
            virtual void print(std::ostream& os, ParamNamer const& namer) const = 0;
            virtual Tree simplify() const = 0;

            // SZERKEZETI egyezés: ugyanaz a fa-alak ugyanazokkal a levelekkel. Az
            // egyszerűsítő használja (a - a = 0, a * a = a^2).
            virtual bool same(Kifejezes const& o) const = 0;

            // A részfa hozzáfordítása a laposított programhoz; a visszatérési érték az
            // a slot, ahova az eredmény kerül. A gyerekeket előbb kell fordítani (így
            // adódik a topologikus sorrend), a közös részkifejezéseket a Program::emit
            // ismeri fel. Lásd matek/Program.hpp.
            virtual int compile(Program& prog) const = 0;

            // A megadott VÁLTOZÓK helyére részkifejezéseket helyettesít, és az így
            // kapott új fát adja vissza (az eredetit nem módosítja).
            //
            // Ez a tér-transzformációk (warpok) egész gépezete: ha F-ben x,y,z helyére
            // a világ->lokális leképezés kifejezéseit tesszük, akkor F(w(p))-t kapunk,
            // azaz az elmozgatott/forgatott/skálázott alakzatot. A deriváltakat NEM kell
            // külön kezelni: a szimbolikus deriválás a láncszabályt magától elvégzi.
            // (A BlobTree-cikk 3.4-e ehhez explicit Jacobi-mátrixot számol.)
            virtual Tree substitute(SubstMap const& m) const = 0;

            virtual ~Kifejezes() = default;
        };

    }
}

#endif //MATEK_KIFEJEZES_HPP
