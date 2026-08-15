#ifndef MATEK_MUVELETEK_OSSZEG_HPP
#define MATEK_MUVELETEK_OSSZEG_HPP

#include "KetOperandus.hpp"
#include "../levelek/Konstans.hpp"

namespace Matek {
    namespace Analizis {

        struct Osszeg : public KetOperandus {
            explicit Osszeg(std::shared_ptr<Kifejezes const> bal, std::shared_ptr<Kifejezes const> jobb)
                : KetOperandus(std::move(bal), std::move(jobb)) {}

            float at(glm::vec3 const v) const override {
                return get_bal()->at(v) + get_jobb()->at(v);
            }

            std::shared_ptr<Kifejezes const> simplify() const override {
                auto bal = get_bal()->simplify();
                auto jobb = get_jobb()->simplify();
                if (auto bal_k = std::dynamic_pointer_cast<Konstans const>(bal))
                    if (bal_k->get_value() == 0) return jobb;
                if (auto jobb_k = std::dynamic_pointer_cast<Konstans const>(jobb))
                    if (jobb_k->get_value() == 0) return bal;
                return std::make_shared<Osszeg>(bal, jobb);
            }

            std::shared_ptr<Kifejezes const> derrivate(char var) const override {
                return std::make_shared<Osszeg>(get_bal()->derrivate(var), get_jobb()->derrivate(var));
            }
            std::shared_ptr<Kifejezes const> derrivate(float const * var) const override {
                return std::make_shared<Osszeg>(get_bal()->derrivate(var), get_jobb()->derrivate(var));
            }
            std::shared_ptr<Kifejezes const> derrivate(std::shared_ptr<Kifejezes const> var) const override {
                return std::make_shared<Osszeg>(get_bal()->derrivate(var), get_jobb()->derrivate(var));
            }

            std::shared_ptr<Kifejezes const> with_children(
                std::shared_ptr<Kifejezes const> a,
                std::shared_ptr<Kifejezes const> b) const override {
                return std::make_shared<Osszeg>(std::move(a), std::move(b));
            }

            int compile(Program& prog) const override {
                int a = get_bal()->compile(prog);
                int b = get_jobb()->compile(prog);
                return prog.emit(Op::Add, a, b);
            }

        protected:
            char const get_operator() const override { return '+'; }
        };

    }
}

#endif //MATEK_MUVELETEK_OSSZEG_HPP
