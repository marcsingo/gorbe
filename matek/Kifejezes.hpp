#ifndef MATEK_KIFEJEZES_HPP
#define MATEK_KIFEJEZES_HPP

#include <memory>
#include <iostream>
#include <glad/glad.h>
#include "vec3.hpp"

namespace Matek {
    namespace Analizis {

        struct Kifejezes {
            Kifejezes() = default;
            virtual float at(glm::vec3 const v) const = 0;
            float operator()(glm::vec3 const v) const { return at(v); }
            virtual std::shared_ptr<Kifejezes const> derrive(char var) const = 0;
            virtual std::shared_ptr<Kifejezes const> derrive(std::shared_ptr<Kifejezes const> var) const = 0;
            virtual std::shared_ptr<Kifejezes const> derrive(float const * var) const = 0;
            virtual void print(std::ostream& os) const = 0;
            virtual std::shared_ptr<Kifejezes const> simplify() const = 0;
            virtual ~Kifejezes() = default;
        };

    }
}

#endif //MATEK_KIFEJEZES_HPP