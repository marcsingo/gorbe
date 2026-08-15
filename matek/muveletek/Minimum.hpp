#ifndef MATEK_MUVELETEK_MINIMUM_HPP
#define MATEK_MUVELETEK_MINIMUM_HPP

#include <algorithm>
#include "KetOperandus.hpp"
#include "Osszeg.hpp"
#include "Kulonbseg.hpp"
#include "Szorzat.hpp"
#include "../levelek/Konstans.hpp"
#include "../fuggvenyek/Elojel.hpp"

namespace Matek {
    namespace Analizis {

        // min(a, b) — az implicit felületek ÉLES uniója az F<0 = belül konvencióval.
        // (A BlobTree-cikk max-ot ír, mert ott a potenciál belül NAGY; nálunk fordítva.)
        //
        // Miért külön csomópont, és nem a 0.5*(a+b-|a-b|) azonosság?
        // Mert az azonosság a fában KÉTSZER szerepelteti a-t és b-t, így egymásba
        // ágyazott CSG-nél a kiértékelés költsége exponenciálisan nő. Így viszont az
        // at() egy menetben megy, és csak a DERIVÁLT fája használja az azonosságot:
        //
        //     d/dv min(a,b) = ½·( a' + b' − sign(a−b)·(a' − b') )
        //
        // ami a<b esetén a'-t, a>b esetén b'-t ad — pontosan a "nyertes ág" deriváltját.
        // Ez felel meg a cikk 3.8-ának is (CSG-csomópontnál a normális a megfelelő
        // gyerektől jön). Figyelem: a varraton (a=b) a gradiens ugrik, a második
        // derivált pedig ott értelmetlen — sima átmenethez lásd a sunio/smetszet-et.
        struct Minimum : public KetOperandus {
            explicit Minimum(std::shared_ptr<Kifejezes const> bal, std::shared_ptr<Kifejezes const> jobb)
                : KetOperandus(std::move(bal), std::move(jobb)) {}

            float at(glm::vec3 const v) const override {
                return std::min(get_bal()->at(v), get_jobb()->at(v));
            }

            void print(std::ostream& os) const override {
                os << "min(";
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
                    return std::make_shared<Konstans>(std::min(bal_k->get_value(), jobb_k->get_value()));
                return std::make_shared<Minimum>(bal, jobb);
            }

            int compile(Program& prog) const override {
                int a = get_bal()->compile(prog);
                int b = get_jobb()->compile(prog);
                return prog.emit(Op::Min, a, b);
            }

            template<class Var>
            std::shared_ptr<Kifejezes const> derrivate_impl(Var const& var) const {
                auto da = get_bal()->derrivate(var);
                auto db = get_jobb()->derrivate(var);
                auto sgn = std::make_shared<Elojel>(
                    std::make_shared<Kulonbseg>(get_bal(), get_jobb()));
                return std::make_shared<Szorzat>(
                    std::make_shared<Konstans>(0.5f),
                    std::make_shared<Kulonbseg>(
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
            char const get_operator() const override { return '<'; } // a print felül van írva
        };

    }
}

#endif //MATEK_MUVELETEK_MINIMUM_HPP
