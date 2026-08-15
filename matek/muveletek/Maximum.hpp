#ifndef MATEK_MUVELETEK_MAXIMUM_HPP
#define MATEK_MUVELETEK_MAXIMUM_HPP

#include <algorithm>
#include "KetOperandus.hpp"
#include "Osszeg.hpp"
#include "Kulonbseg.hpp"
#include "Szorzat.hpp"
#include "../levelek/Konstans.hpp"
#include "../fuggvenyek/Elojel.hpp"

namespace Matek {
    namespace Analizis {

        // max(a, b) — az implicit felületek ÉLES metszete az F<0 = belül konvencióval
        // (a különbség A−B = max(a, −b)). Részletek: lásd Minimum.hpp.
        //
        //     d/dv max(a,b) = ½·( a' + b' + sign(a−b)·(a' − b') )
        struct Maximum : public KetOperandus {
            explicit Maximum(std::shared_ptr<Kifejezes const> bal, std::shared_ptr<Kifejezes const> jobb)
                : KetOperandus(std::move(bal), std::move(jobb)) {}

            float at(glm::vec3 const v) const override {
                return std::max(get_bal()->at(v), get_jobb()->at(v));
            }

            void print(std::ostream& os) const override {
                os << "max(";
                get_bal()->print(os);
                os << ", ";
                get_jobb()->print(os);
                os << ')';
            }

            std::shared_ptr<Kifejezes const> simplify() const override {
                auto bal = get_bal()->simplify();
                auto jobb = get_jobb()->simplify();
                if (bal == jobb) return bal;
                auto bal_k = std::dynamic_pointer_cast<Konstans const>(bal);
                auto jobb_k = std::dynamic_pointer_cast<Konstans const>(jobb);
                if (bal_k && jobb_k)
                    return std::make_shared<Konstans>(std::max(bal_k->get_value(), jobb_k->get_value()));
                return std::make_shared<Maximum>(bal, jobb);
            }

            std::shared_ptr<Kifejezes const> with_children(
                std::shared_ptr<Kifejezes const> a,
                std::shared_ptr<Kifejezes const> b) const override {
                return std::make_shared<Maximum>(std::move(a), std::move(b));
            }

            int compile(Program& prog) const override {
                int a = get_bal()->compile(prog);
                int b = get_jobb()->compile(prog);
                return prog.emit(Op::Max, a, b);
            }

            template<class Var>
            std::shared_ptr<Kifejezes const> derrivate_impl(Var const& var) const {
                auto da = get_bal()->derrivate(var);
                auto db = get_jobb()->derrivate(var);
                auto sgn = std::make_shared<Elojel>(
                    std::make_shared<Kulonbseg>(get_bal(), get_jobb()));
                return std::make_shared<Szorzat>(
                    std::make_shared<Konstans>(0.5f),
                    std::make_shared<Osszeg>(
                        std::make_shared<Osszeg>(da, db),
                        std::make_shared<Szorzat>(sgn, std::make_shared<Kulonbseg>(da, db))
                    )
                );
            }

            std::shared_ptr<Kifejezes const> derrivate(char var) const override {
                return derrivate_impl(var);
            }
            std::shared_ptr<Kifejezes const> derrivate(float const * var) const override {
                return derrivate_impl(var);
            }
            std::shared_ptr<Kifejezes const> derrivate(std::shared_ptr<Kifejezes const> var) const override {
                return derrivate_impl(var);
            }

        protected:
            char const get_operator() const override { return '>'; } // a print felül van írva
        };

    }
}

#endif //MATEK_MUVELETEK_MAXIMUM_HPP
