#ifndef MATEK_MUVELETEK_KULONBSEG_HPP
#define MATEK_MUVELETEK_KULONBSEG_HPP

#include "KetOperandus.hpp"

namespace Matek {
    namespace Analizis {

        // Az előjelváltás is ez: -a = 0 - a (külön csomópont nélkül).
        struct Kulonbseg : public KetOperandus {
            explicit Kulonbseg(Tree bal, Tree jobb) : KetOperandus(std::move(bal), std::move(jobb)) {}

            float at(glm::vec3 const v) const override {
                return get_bal()->at(v) - get_jobb()->at(v);
            }

            Tree simplify() const override {
                auto bal = get_bal()->simplify();
                auto jobb = get_jobb()->simplify();
                auto a = const_of(bal), b = const_of(jobb);
                if (a && b) return std::make_shared<Konstans>(a->get_value() - b->get_value());
                if (is_const(jobb, 0.0f)) return bal;
                if (bal->same(*jobb)) return std::make_shared<Konstans>(0.0f);
                return std::make_shared<Kulonbseg>(bal, jobb);
            }

            Tree derive(Var const& var) const override {
                return std::make_shared<Kulonbseg>(get_bal()->derive(var), get_jobb()->derive(var));
            }

            Tree with_children(Tree a, Tree b) const override {
                return std::make_shared<Kulonbseg>(std::move(a), std::move(b));
            }

            int compile(Program& prog) const override {
                int a = get_bal()->compile(prog);
                int b = get_jobb()->compile(prog);
                return prog.emit(Op::Sub, a, b);
            }

            // 0 - a  ->  "(-a)": így olvasható, és a parser ugyanezt a fát építi belőle.
            void print(std::ostream& os, ParamNamer const& namer) const override {
                if (!is_const(get_bal(), 0.0f)) return KetOperandus::print(os, namer);
                os << "(-";
                get_jobb()->print(os, namer);
                os << ')';
            }

        protected:
            char get_operator() const override { return '-'; }
        };

    }
}

#endif //MATEK_MUVELETEK_KULONBSEG_HPP
