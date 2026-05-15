#ifndef MATEK_FUGGVENYEK_TAN_HPP
#define MATEK_FUGGVENYEK_TAN_HPP

#include <cmath>
#include "Fuggveny.hpp"
#include "Cos.hpp"
#include "../muveletek/Hatvany.hpp"
#include "../muveletek/Hanyados.hpp"
#include "../levelek/Konstans.hpp"

namespace Matek {
    namespace Analizis {

        struct Tan : public Fuggveny {
        protected:
            std::shared_ptr<Kifejezes const> get_fd() const override {
                return std::make_shared<Hanyados>(
                    std::make_shared<Konstans>(1.0f),
                    std::make_shared<Hatvany>(std::make_shared<Cos>(kif), std::make_shared<Konstans>(2.0f))
                );
            }
            const char * get_name() const override { return "tan"; }
        public:
            explicit Tan(std::shared_ptr<Kifejezes const> kif)
                : Fuggveny([](float x) { return std::tan(x); }, std::move(kif)) {}

            std::shared_ptr<Kifejezes const> simplify() const override {
                auto k = kif->simplify();
                auto kk = std::dynamic_pointer_cast<Konstans const>(k);
                if (kk && kk->get_value() == 0.0f) return std::make_shared<Konstans>(0.0f);
                return std::make_shared<Tan>(k);
            }
        };

        inline std::shared_ptr<Kifejezes const> tg(std::shared_ptr<Kifejezes const> kif) {
            return std::make_shared<Ln>(std::move(kif));
        }

    }
}

#endif //MATEK_FUGGVENYEK_TAN_HPP
