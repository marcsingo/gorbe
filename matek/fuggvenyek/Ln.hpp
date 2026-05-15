#ifndef MATEK_FUGGVENYEK_LN_HPP
#define MATEK_FUGGVENYEK_LN_HPP

#include <cmath>
#include "Fuggveny.hpp"
#include "../muveletek/Hanyados.hpp"
#include "../levelek/Konstans.hpp"

namespace Matek {
    namespace Analizis {

        struct Ln : public Fuggveny {
        protected:
            std::shared_ptr<Kifejezes const> get_fd() const override {
                return std::make_shared<Hanyados>(std::make_shared<Konstans>(1), kif);
            }
            const char * get_name() const override { return "(1/log(2.271))*log"; }
        public:
            explicit Ln(std::shared_ptr<Kifejezes const> kif)
                : Fuggveny([](float x) { return std::log(x); }, std::move(kif)) {}

            std::shared_ptr<Kifejezes const> simplify() const override {
                auto k = kif->simplify();
                auto kk = std::dynamic_pointer_cast<Konstans const>(k);
                if (kk && kk->get_value() == 1.0f) return std::make_shared<Konstans>(0.0f);
                return std::make_shared<Ln>(k);
            }
        };

        inline std::shared_ptr<Kifejezes const> ln(std::shared_ptr<Kifejezes const> kif) {
            return std::make_shared<Ln>(std::move(kif));
        }

    }
}

#endif //MATEK_FUGGVENYEK_LN_HPP
