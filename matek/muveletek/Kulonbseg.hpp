#ifndef MATEK_MUVELETEK_KULONBSEG_HPP
#define MATEK_MUVELETEK_KULONBSEG_HPP

#include "KetOperandus.hpp"
#include "../levelek/Konstans.hpp"

namespace Matek {
    namespace Analizis {

        struct Kulonbseg : public KetOperandus {
            explicit Kulonbseg(std::shared_ptr<Kifejezes const> bal, std::shared_ptr<Kifejezes const> jobb)
                : KetOperandus(std::move(bal), std::move(jobb)) {}

            float at(glm::vec3 const v) const override {
                return get_bal()->at(v) - get_jobb()->at(v);
            }

            std::shared_ptr<Kifejezes const> simplify() const override {
                auto bal = get_bal()->simplify();
                auto jobb = get_jobb()->simplify();
                if (bal == jobb) return std::make_shared<Konstans>(0.0f);
                auto jobb_k = std::dynamic_pointer_cast<Konstans const>(jobb);
                if (jobb_k && jobb_k->get_value() == 0.0f) return bal;
                return std::make_shared<Kulonbseg>(bal, jobb);
            }

            std::shared_ptr<Kifejezes const> derrivate(char var) const override {
                return std::make_shared<Kulonbseg>(get_bal()->derrivate(var), get_jobb()->derrivate(var));
            }
            std::shared_ptr<Kifejezes const> derrivate(float const * var) const override {
                return std::make_shared<Kulonbseg>(get_bal()->derrivate(var), get_jobb()->derrivate(var));
            }
            std::shared_ptr<Kifejezes const> derrivate(std::shared_ptr<Kifejezes const> var) const override {
                return std::make_shared<Kulonbseg>(get_bal()->derrivate(var), get_jobb()->derrivate(var));
            }

        protected:
            char const get_operator() const override { return '-'; }
        };

    }
}

#endif //MATEK_MUVELETEK_KULONBSEG_HPP
