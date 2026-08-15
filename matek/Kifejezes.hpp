#ifndef MATEK_KIFEJEZES_HPP
#define MATEK_KIFEJEZES_HPP

#include <map>
#include <memory>
#include <iostream>
#include <glad/glad.h>
#include "vec3.hpp"
#include "Program.hpp"

namespace Matek {
    namespace Analizis {

        struct Kifejezes;

        // Változó -> részkifejezés csere táblája (lásd Kifejezes::substitute).
        using SubstMap = std::map<char, std::shared_ptr<Kifejezes const>>;

        struct Kifejezes {
            Kifejezes() = default;
            virtual float at(glm::vec3 const v) const = 0;
            float operator()(glm::vec3 const v) const { return at(v); }
            virtual std::shared_ptr<Kifejezes const> derrivate(char var) const = 0;
            virtual std::shared_ptr<Kifejezes const> derrivate(std::shared_ptr<Kifejezes const> var) const = 0;
            virtual std::shared_ptr<Kifejezes const> derrivate(float const * var) const = 0;
            virtual void print(std::ostream& os) const = 0;
            virtual std::shared_ptr<Kifejezes const> simplify() const = 0;

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
            virtual std::shared_ptr<Kifejezes const> substitute(SubstMap const& m) const = 0;

            virtual ~Kifejezes() = default;
        };

    }
}

#endif //MATEK_KIFEJEZES_HPP