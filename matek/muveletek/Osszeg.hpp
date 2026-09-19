#ifndef MATEK_MUVELETEK_OSSZEG_HPP
#define MATEK_MUVELETEK_OSSZEG_HPP

#include "KetOperandus.hpp"

namespace Matek {
    namespace Analizis {

        struct Osszeg : public KetOperandus {
            explicit Osszeg(Tree bal, Tree jobb) : KetOperandus(std::move(bal), std::move(jobb)) {}

            float at(glm::vec3 const v) const override {
                return get_bal()->at(v) + get_jobb()->at(v);
            }

            Tree simplify() const override {
                auto bal = get_bal()->simplify();
                auto jobb = get_jobb()->simplify();
                auto a = const_of(bal), b = const_of(jobb);
                if (a && b) return std::make_shared<Konstans>(a->get_value() + b->get_value());
                if (is_const(bal, 0.0f)) return jobb;
                if (is_const(jobb, 0.0f)) return bal;
                return std::make_shared<Osszeg>(bal, jobb);
            }

            Tree derive(Var const& var) const override {
                return std::make_shared<Osszeg>(get_bal()->derive(var), get_jobb()->derive(var));
            }

            Tree with_children(Tree a, Tree b) const override {
                return std::make_shared<Osszeg>(std::move(a), std::move(b));
            }

            int compile(Program& prog) const override {
                int a = get_bal()->compile(prog);
                int b = get_jobb()->compile(prog);
                return prog.emit(Op::Add, a, b);
            }

        protected:
            char get_operator() const override { return '+'; }
        };

    }
}

#endif //MATEK_MUVELETEK_OSSZEG_HPP
