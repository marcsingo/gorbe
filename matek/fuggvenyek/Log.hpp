#ifndef MATEK_FUGGVENYEK_LOG_HPP
#define MATEK_FUGGVENYEK_LOG_HPP

#include <cmath>
#include "Fuggveny.hpp"
#include "../muveletek/Hanyados.hpp"
#include "../muveletek/Szorzat.hpp"
#include "../levelek/Konstans.hpp"

namespace Matek {
    namespace Analizis {

        struct Log : public Fuggveny {
        protected:
            std::shared_ptr<Kifejezes const> get_fd() const override {
                return std::make_shared<Hanyados>(
                    std::make_shared<Konstans>(1.0f),
                    std::make_shared<Szorzat>(kif, std::make_shared<Konstans>(std::log(10.0f)))
                );
            }
            const char * get_name() const override { return "(1/log(10))*log"; }
        public:
            explicit Log(std::shared_ptr<Kifejezes const> kif)
                : Fuggveny([](float x) { return std::log10(x); }, std::move(kif)) {}

            std::shared_ptr<Kifejezes const> simplify() const override {
                auto k = kif->simplify();
                auto kk = std::dynamic_pointer_cast<Konstans const>(k);
                if (kk && kk->get_value() == 1.0f) return std::make_shared<Konstans>(0.0f);
                return std::make_shared<Log>(k);
            }
        };

        inline std::shared_ptr<Kifejezes const> log(std::shared_ptr<Kifejezes const> kif) {
            return std::make_shared<Log>(std::move(kif));
        }

    }
}

#endif //MATEK_FUGGVENYEK_LOG_HPP
