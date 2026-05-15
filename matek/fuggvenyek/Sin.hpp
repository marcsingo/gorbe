#ifndef MATEK_FUGGVENYEK_SIN_HPP
#define MATEK_FUGGVENYEK_SIN_HPP

#include <cmath>
#include "Fuggveny.hpp"
#include "../levelek/Konstans.hpp"

namespace Matek {
    namespace Analizis {

        struct Cos;

        // Sin::get_fd() visszaad egy Cos-t, ezért csak a Cos.hpp-ban van definiálva.
        struct Sin : public Fuggveny {
        protected:
            std::shared_ptr<Kifejezes const> get_fd() const override;

            const char * get_name() const override { return "sin"; }
        public:
            explicit Sin(std::shared_ptr<Kifejezes const> kif)
                : Fuggveny([](float x) { return std::sin(x); }, std::move(kif)) {}

            std::shared_ptr<Kifejezes const> simplify() const override {
                auto k = kif->simplify();
                auto kk = std::dynamic_pointer_cast<Konstans const>(k);
                if (kk && kk->get_value() == 0.0f) return std::make_shared<Konstans>(0.0f);
                return std::make_shared<Sin>(k);
            }
        };

        inline std::shared_ptr<Kifejezes const> sin(std::shared_ptr<Kifejezes const> kif) {
            return std::make_shared<Sin>(std::move(kif));
        }

    }
}

#endif //MATEK_FUGGVENYEK_SIN_HPP
