#ifndef MATEK_FUGGVENYEK_ABS_HPP
#define MATEK_FUGGVENYEK_ABS_HPP

#include <cmath>
#include "Fuggveny.hpp"
#include "Elojel.hpp"
#include "../levelek/Konstans.hpp"

namespace Matek {
    namespace Analizis {

        // |a|. Deriváltja sign(a)*a' (a 0-ban nem differenciálható, ott 0-t ad).
        struct Abs : public Fuggveny {
        protected:
            std::shared_ptr<Kifejezes const> get_fd() const override {
                return std::make_shared<Elojel>(kif);
            }

            char const * get_name() const override { return "abs"; }

            Op get_op() const override { return Op::Abs; }

            std::shared_ptr<Kifejezes const> with_arg(
                std::shared_ptr<Kifejezes const> a) const override {
                return std::make_shared<Abs>(std::move(a));
            }

        public:
            explicit Abs(std::shared_ptr<Kifejezes const> kif)
                : Fuggveny([](float v) { return std::abs(v); }, std::move(kif)) {}

            std::shared_ptr<Kifejezes const> simplify() const override {
                auto k = kif->simplify();
                if (auto kk = std::dynamic_pointer_cast<Konstans const>(k))
                    return std::make_shared<Konstans>(std::abs(kk->get_value()));
                return std::make_shared<Abs>(k);
            }
        };

        inline std::shared_ptr<Kifejezes const> abs(std::shared_ptr<Kifejezes const> kif) {
            return std::make_shared<Abs>(std::move(kif));
        }

    }
}

#endif //MATEK_FUGGVENYEK_ABS_HPP
