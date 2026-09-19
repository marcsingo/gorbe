#ifndef MATEK_MUVELETEK_SZORZAT_HPP
#define MATEK_MUVELETEK_SZORZAT_HPP

#include "KetOperandus.hpp"
#include "Osszeg.hpp"

namespace Matek {
    namespace Analizis {

        struct Szorzat : public KetOperandus {
            explicit Szorzat(Tree bal, Tree jobb) : KetOperandus(std::move(bal), std::move(jobb)) {}

            float at(glm::vec3 const v) const override {
                return get_bal()->at(v) * get_jobb()->at(v);
            }

            Tree simplify() const override {
                auto bal = get_bal()->simplify();
                auto jobb = get_jobb()->simplify();
                if (const_of(jobb) && !const_of(bal)) std::swap(bal, jobb);   // konstans balra
                auto a = const_of(bal), b = const_of(jobb);
                if (a && b) return std::make_shared<Konstans>(a->get_value() * b->get_value());
                if (is_const(bal, 0.0f)) return std::make_shared<Konstans>(0.0f);
                if (is_const(bal, 1.0f)) return jobb;
                // c1 * (c2 * u)  ->  (c1*c2) * u   — így lesz a 2*x*3-ból 6*x
                if (a)
                    if (auto in = dynamic_cast<Szorzat const*>(jobb.get()))
                        if (auto c2 = const_of(in->get_bal()))
                            return std::make_shared<Szorzat>(
                                std::make_shared<Konstans>(a->get_value() * c2->get_value()),
                                in->get_jobb());
                return std::make_shared<Szorzat>(bal, jobb);
            }

            Tree derive(Var const& var) const override {
                return std::make_shared<Osszeg>(
                    std::make_shared<Szorzat>(get_bal()->derive(var), get_jobb()),
                    std::make_shared<Szorzat>(get_bal(), get_jobb()->derive(var)));
            }

            Tree with_children(Tree a, Tree b) const override {
                return std::make_shared<Szorzat>(std::move(a), std::move(b));
            }

            int compile(Program& prog) const override {
                int a = get_bal()->compile(prog);
                int b = get_jobb()->compile(prog);
                return prog.emit(Op::Mul, a, b);
            }

        protected:
            char get_operator() const override { return '*'; }
        };

    }
}

#endif //MATEK_MUVELETEK_SZORZAT_HPP
