#ifndef MATEK_LEVELEK_PARAMETER_HPP
#define MATEK_LEVELEK_PARAMETER_HPP

#include <stdexcept>
#include "../Kifejezes.hpp"
#include "Konstans.hpp"

namespace Matek {
    namespace Analizis {

        struct Parameter : public Kifejezes {
        private:
            char const id;
            float const * ertek_ref;
        public:
            Parameter(float const * ref, char const id = 'p')
                : id(id), ertek_ref(std::move(ref)) {
                if (!ertek_ref)
                    throw std::invalid_argument("A parameter referenciaja nem lehet null!");
            }

            float at(glm::vec3 const v) const override { return *ertek_ref; }

            std::shared_ptr<Kifejezes const> derrivate(char var) const override {
                return std::make_shared<Konstans>(this->id == var ? 1.0f : 0.0f);
            }
            std::shared_ptr<Kifejezes const> derrivate(float const * var) const override {
                if (ertek_ref == var) return std::make_shared<Konstans>(1);
                return std::make_shared<Konstans>(0);
            }
            std::shared_ptr<Kifejezes const> derrivate(std::shared_ptr<Kifejezes const> var) const override {
                if (this == var.get()) return std::make_shared<Konstans>(1);
                return std::make_shared<Konstans>(0);
            }

            void print(std::ostream &os) const override { os << "p_" << id; }

            std::shared_ptr<Kifejezes const> simplify() const override {
                return std::make_shared<Parameter>(ertek_ref, id);
            }
        };

    }
}

#endif //MATEK_LEVELEK_PARAMETER_HPP
