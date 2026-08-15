#ifndef MATEK_FUGGVENYEK_COS_HPP
#define MATEK_FUGGVENYEK_COS_HPP

#include <cmath>
#include "Sin.hpp"
#include "../muveletek/Szorzat.hpp"
#include "../levelek/Konstans.hpp"

namespace Matek {
    namespace Analizis {

        struct Cos : public Fuggveny {
        protected:
            std::shared_ptr<Kifejezes const> get_fd() const override;

            const char * get_name() const override { return "cos"; }

            Op get_op() const override { return Op::Cos; }
        public:
            explicit Cos(std::shared_ptr<Kifejezes const> kif)
                : Fuggveny([](float x) { return std::cos(x); }, std::move(kif)) {}

            std::shared_ptr<Kifejezes const> simplify() const override {
                auto k = kif->simplify();
                auto kk = std::dynamic_pointer_cast<Konstans const>(k);
                if (kk && kk->get_value() == 0.0f) return std::make_shared<Konstans>(1.0f);
                return std::make_shared<Cos>(k);
            }
        };

        inline std::shared_ptr<Kifejezes const> Sin::get_fd() const {
            return std::make_shared<Cos>(kif);
        }

        inline std::shared_ptr<Kifejezes const> Cos::get_fd() const {
            return std::make_shared<Szorzat>(
                std::make_shared<Konstans>(-1.0f),
                std::make_shared<Sin>(kif)
            );
        }

        inline std::shared_ptr<Kifejezes const> cos(std::shared_ptr<Kifejezes const> kif) {
            return std::make_shared<Cos>(std::move(kif));
        }

    }
}

#endif //MATEK_FUGGVENYEK_COS_HPP
