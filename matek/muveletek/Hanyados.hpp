#ifndef MATEK_MUVELETEK_HANYADOS_HPP
#define MATEK_MUVELETEK_HANYADOS_HPP

#include "KetOperandus.hpp"
#include "Kulonbseg.hpp"
#include "Szorzat.hpp"

namespace Matek {
    namespace Analizis {

        struct Hanyados : public KetOperandus {
            explicit Hanyados(Tree bal, Tree jobb) : KetOperandus(std::move(bal), std::move(jobb)) {}

            float at(glm::vec3 const v) const override {
                return get_bal()->at(v) / get_jobb()->at(v);
            }

            Tree simplify() const override {
                auto bal = get_bal()->simplify();
                auto jobb = get_jobb()->simplify();
                auto a = const_of(bal), b = const_of(jobb);
                if (a && b) return std::make_shared<Konstans>(a->get_value() / b->get_value());
                if (is_const(bal, 0.0f)) return std::make_shared<Konstans>(0.0f);
                if (is_const(jobb, 1.0f)) return bal;
                if (bal->same(*jobb)) return std::make_shared<Konstans>(1.0f);
                return std::make_shared<Hanyados>(bal, jobb);
            }

            // (a/b)' = (a'b - ab') / b²
            Tree derive(Var const& var) const override {
                return std::make_shared<Hanyados>(
                    std::make_shared<Kulonbseg>(
                        std::make_shared<Szorzat>(get_bal()->derive(var), get_jobb()),
                        std::make_shared<Szorzat>(get_bal(), get_jobb()->derive(var))),
                    std::make_shared<Szorzat>(get_jobb(), get_jobb()));
            }

            Tree with_children(Tree a, Tree b) const override {
                return std::make_shared<Hanyados>(std::move(a), std::move(b));
            }

            int compile(Program& prog) const override {
                int a = get_bal()->compile(prog);
                int b = get_jobb()->compile(prog);
                return prog.emit(Op::Div, a, b);
            }

        protected:
            char get_operator() const override { return '/'; }
        };

    }
}

#endif //MATEK_MUVELETEK_HANYADOS_HPP
