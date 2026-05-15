#ifndef MATEK_MUVELETEK_HANYADOS_HPP
#define MATEK_MUVELETEK_HANYADOS_HPP

#include "KetOperandus.hpp"
#include "Kulonbseg.hpp"
#include "Szorzat.hpp"
#include "../levelek/Konstans.hpp"

namespace Matek {
    namespace Analizis {

        struct Hanyados : public KetOperandus {
            explicit Hanyados(std::shared_ptr<Kifejezes const> bal, std::shared_ptr<Kifejezes const> jobb)
                : KetOperandus(std::move(bal), std::move(jobb)) {}

            float at(glm::vec3 const v) const override {
                return get_bal()->at(v) / get_jobb()->at(v);
            }

            std::shared_ptr<Kifejezes const> simplify() const override {
                auto bal = get_bal()->simplify();
                auto jobb = get_jobb()->simplify();
                if (bal == jobb) return std::make_shared<Konstans>(1.0f);
                auto bal_k = std::dynamic_pointer_cast<Konstans const>(bal);
                auto jobb_k = std::dynamic_pointer_cast<Konstans const>(jobb);
                if (bal_k && bal_k->get_value() == 0.0f) return std::make_shared<Konstans>(0.0f);
                if (jobb_k && jobb_k->get_value() == 1.0f) return bal;
                return std::make_shared<Hanyados>(bal, jobb);
            }

            std::shared_ptr<Kifejezes const> derrive(char var) const override {
                return std::make_shared<Hanyados>(
                    std::make_shared<Kulonbseg>(
                        std::make_shared<Szorzat>(get_bal()->derrive(var), get_jobb()),
                        std::make_shared<Szorzat>(get_bal(), get_jobb()->derrive(var))
                    ),
                    std::make_shared<Szorzat>(get_jobb(), get_jobb())
                );
            }
            std::shared_ptr<Kifejezes const> derrive(float const * var) const override {
                return std::make_shared<Hanyados>(
                    std::make_shared<Kulonbseg>(
                        std::make_shared<Szorzat>(get_bal()->derrive(var), get_jobb()),
                        std::make_shared<Szorzat>(get_bal(), get_jobb()->derrive(var))
                    ),
                    std::make_shared<Szorzat>(get_jobb(), get_jobb())
                );
            }
            std::shared_ptr<Kifejezes const> derrive(std::shared_ptr<Kifejezes const> var) const override {
                return std::make_shared<Hanyados>(
                    std::make_shared<Kulonbseg>(
                        std::make_shared<Szorzat>(get_bal()->derrive(var), get_jobb()),
                        std::make_shared<Szorzat>(get_bal(), get_jobb()->derrive(var))
                    ),
                    std::make_shared<Szorzat>(get_jobb(), get_jobb())
                );
            }

        protected:
            char const get_operator() const override { return '/'; }
        };

    }
}

#endif //MATEK_MUVELETEK_HANYADOS_HPP
