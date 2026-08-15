#ifndef MATEK_FUGGVENYEK_CTG_HPP
#define MATEK_FUGGVENYEK_CTG_HPP

#include <cmath>
#include "Fuggveny.hpp"
#include "Cos.hpp"
#include "../muveletek/Hatvany.hpp"
#include "../muveletek/Hanyados.hpp"
#include "../levelek/Konstans.hpp"

namespace Matek {
    namespace Analizis {

        struct Ctg : public Fuggveny {
        protected:
            std::shared_ptr<Kifejezes const> get_fd() const override {
                return std::make_shared<Hanyados>(
                    std::make_shared<Konstans>(-1.0f),
                    std::make_shared<Hatvany>(std::make_shared<Sin>(kif), std::make_shared<Konstans>(2.0f))
                );
            }
            const char * get_name() const override { return "ctg"; }

            Op get_op() const override { return Op::Ctg; }
        public:
            explicit Ctg(std::shared_ptr<Kifejezes const> kif)
                : Fuggveny([](float x) { return 1.0f / std::tan(x); }, std::move(kif)) {}

            std::shared_ptr<Kifejezes const> simplify() const override {
                auto k = kif->simplify();
                return std::make_shared<Ctg>(k);
            }
        };

    }
}

#endif //MATEK_FUGGVENYEK_CTG_HPP
