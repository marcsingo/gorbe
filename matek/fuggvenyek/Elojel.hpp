#ifndef MATEK_FUGGVENYEK_ELOJEL_HPP
#define MATEK_FUGGVENYEK_ELOJEL_HPP

#include "Fuggveny.hpp"
#include "../levelek/Konstans.hpp"

namespace Matek {
    namespace Analizis {

        // sign(a): -1 / 0 / +1.
        //
        // A deriváltja majdnem mindenhol 0 (a 0-ban nem differenciálható, ott is 0-t
        // adunk). Önmagában ritkán kell, viszont ez a láncszem az Abs és a Minimum /
        // Maximum deriválásában: azok "melyik ág nyert" kérdését az előjel dönti el.
        struct Elojel : public Fuggveny {
        protected:
            std::shared_ptr<Kifejezes const> get_fd() const override {
                return std::make_shared<Konstans>(0.0f);
            }

            char const * get_name() const override { return "sign"; }

            Op get_op() const override { return Op::Sign; }

            std::shared_ptr<Kifejezes const> with_arg(
                std::shared_ptr<Kifejezes const> a) const override {
                return std::make_shared<Elojel>(std::move(a));
            }

        public:
            explicit Elojel(std::shared_ptr<Kifejezes const> kif)
                : Fuggveny([](float v) { return v > 0.0f ? 1.0f : (v < 0.0f ? -1.0f : 0.0f); },
                           std::move(kif)) {}

            std::shared_ptr<Kifejezes const> simplify() const override {
                auto k = kif->simplify();
                if (auto kk = std::dynamic_pointer_cast<Konstans const>(k)) {
                    float v = kk->get_value();
                    return std::make_shared<Konstans>(v > 0.0f ? 1.0f : (v < 0.0f ? -1.0f : 0.0f));
                }
                return std::make_shared<Elojel>(k);
            }
        };

        inline std::shared_ptr<Kifejezes const> elojel(std::shared_ptr<Kifejezes const> kif) {
            return std::make_shared<Elojel>(std::move(kif));
        }

    }
}

#endif //MATEK_FUGGVENYEK_ELOJEL_HPP
