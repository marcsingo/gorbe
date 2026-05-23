#ifndef MATEK_LEVELEK_KONSTANS_HPP
#define MATEK_LEVELEK_KONSTANS_HPP

#include "../Kifejezes.hpp"

namespace Matek {
    namespace Analizis {

        struct Konstans : public Kifejezes {
        private:
            float value;
        public:
            Konstans(float value) : value(value) {}

            float at(glm::vec3 const v) const override { return value; }

            float get_value() const { return value; }

            std::shared_ptr<Kifejezes const> derrivate(char var) const override {
                return std::make_shared<Konstans>(0);
            }
            std::shared_ptr<Kifejezes const> derrivate(std::shared_ptr<Kifejezes const> var) const override {
                return std::make_shared<Konstans>(0);
            }
            std::shared_ptr<Kifejezes const> derrivate(float const * var) const override {
                return std::make_shared<Konstans>(0);
            }

            std::shared_ptr<Kifejezes const> simplify() const override {
                return std::make_shared<Konstans const>(value);
            }

            void print(std::ostream& os) const override { os << value; }
        };

    }
}

#endif //MATEK_LEVELEK_KONSTANS_HPP
