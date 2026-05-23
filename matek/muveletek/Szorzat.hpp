#ifndef MATEK_MUVELETEK_SZORZAT_HPP
#define MATEK_MUVELETEK_SZORZAT_HPP

#include "KetOperandus.hpp"
#include "Osszeg.hpp"
#include "../levelek/Konstans.hpp"

namespace Matek {
    namespace Analizis {

        struct Szorzat : public KetOperandus {
            explicit Szorzat(std::shared_ptr<Kifejezes const> bal, std::shared_ptr<Kifejezes const> jobb)
                : KetOperandus(std::move(bal), std::move(jobb)) {}

            float at(glm::vec3 const v) const override {
                return get_bal()->at(v) * get_jobb()->at(v);
            }

            std::shared_ptr<Kifejezes const> simplify() const override {
                auto bal = get_bal()->simplify();
                auto jobb = get_jobb()->simplify();
                auto bal_k = std::dynamic_pointer_cast<Konstans const>(bal);
                auto jobb_k = std::dynamic_pointer_cast<Konstans const>(jobb);
                if ((bal_k && bal_k->get_value() == 0.0f) || (jobb_k && jobb_k->get_value() == 0.0f))
                    return std::make_shared<Konstans>(0.0f);
                if (bal_k && bal_k->get_value() == 1.0f) return jobb;
                if (jobb_k && jobb_k->get_value() == 1.0f) return bal;
                return std::make_shared<Szorzat>(bal, jobb);
            }

            std::shared_ptr<Kifejezes const> derrivate(char var) const override {
                return std::make_shared<Osszeg>(
                    std::make_shared<Szorzat>(get_bal()->derrivate(var), get_jobb()),
                    std::make_shared<Szorzat>(get_bal(), get_jobb()->derrivate(var))
                );
            }
            std::shared_ptr<Kifejezes const> derrivate(float const * var) const override {
                return std::make_shared<Osszeg>(
                    std::make_shared<Szorzat>(get_bal()->derrivate(var), get_jobb()),
                    std::make_shared<Szorzat>(get_bal(), get_jobb()->derrivate(var))
                );
            }
            std::shared_ptr<Kifejezes const> derrivate(std::shared_ptr<Kifejezes const> var) const override {
                return std::make_shared<Osszeg>(
                    std::make_shared<Szorzat>(get_bal()->derrivate(var), get_jobb()),
                    std::make_shared<Szorzat>(get_bal(), get_jobb()->derrivate(var))
                );
            }

        protected:
            char const get_operator() const override { return '*'; }
        };

    }
}

#endif //MATEK_MUVELETEK_SZORZAT_HPP
