#ifndef MATEK_KIFEJEZES_HPP
#define MATEK_KIFEJEZES_HPP

#include <memory>
#include <iostream>
#include <glad/glad.h>
#include "vec3.hpp"
#include "Program.hpp"

namespace Matek {
    namespace Analizis {

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

            virtual ~Kifejezes() = default;
        };

    }
}

#endif //MATEK_KIFEJEZES_HPP