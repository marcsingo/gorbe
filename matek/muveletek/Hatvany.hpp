#ifndef MATEK_MUVELETEK_HATVANY_HPP
#define MATEK_MUVELETEK_HATVANY_HPP

#include "KetOperandus.hpp"
#include "Szorzat.hpp"
#include "Osszeg.hpp"
#include "Hanyados.hpp"
#include "../fuggvenyek/Ln.hpp"
#include "../levelek/Konstans.hpp"

namespace Matek {
    namespace Analizis {

        struct Hatvany : public KetOperandus {
        protected:
            const char get_operator() const override { return '^'; }
        public:
            explicit Hatvany(std::shared_ptr<Kifejezes const> bal, std::shared_ptr<Kifejezes const> jobb)
                : KetOperandus(std::move(bal), std::move(jobb)) {}

            float at(glm::vec3 const v) const override {
                return powf(get_bal()->at(v), get_jobb()->at(v));
            }

            void print(std::ostream &os) const override {
                os << "pow(";
                get_bal()->print(os);
                os << ", ";
                get_jobb()->print(os);
                os << ')';
            }

            // Konstans kitevő esetén (a^n, n állandó) a STABIL hatványszabály:
            //   d/dx a^n = n * a^(n-1) * a'
            // A lenti általános (logaritmikus) képletet csak akkor használjuk, ha a
            // kitevő is függ a változótól. Az általános képletben szereplő ln(a) és
            // a'/a ugyanis NaN/Inf lesz a≤0, illetve a=0 esetén (pl. x^2 deriváltja
            // y=0-nál 0/0), ami a kvartikus felületeknél (tórusz) robbanáshoz vezet.
            template<class Var>
            std::shared_ptr<Kifejezes const> derrivate_impl(Var const& var) const {
                if (auto n = std::dynamic_pointer_cast<Konstans const>(get_jobb())) {
                    return std::make_shared<Szorzat>(
                        std::make_shared<Szorzat>(
                            std::make_shared<Konstans>(n->get_value()),
                            std::make_shared<Hatvany>(get_bal(),
                                std::make_shared<Konstans>(n->get_value() - 1.0f))
                        ),
                        get_bal()->derrivate(var)
                    );
                }
                return std::make_shared<Szorzat>(
                    std::make_shared<Hatvany>(get_bal(), get_jobb()),
                    std::make_shared<Osszeg>(
                        std::make_shared<Szorzat>(get_jobb()->derrivate(var), std::make_shared<Ln>(get_bal())),
                        std::make_shared<Szorzat>(get_jobb(), std::make_shared<Hanyados>(get_bal()->derrivate(var), get_bal()))
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

            std::shared_ptr<Kifejezes const> simplify() const override {
                auto bal = get_bal()->simplify();
                auto jobb = get_jobb()->simplify();
                auto bal_k = std::dynamic_pointer_cast<Konstans const>(bal);
                auto jobb_k = std::dynamic_pointer_cast<Konstans const>(jobb);
                if (jobb_k && jobb_k->get_value() == 0.0f) return std::make_shared<Konstans>(1.0f);
                if (jobb_k && jobb_k->get_value() == 1.0f) return bal;
                if (bal_k && bal_k->get_value() == 0.0f) return std::make_shared<Konstans>(0.0f);
                if (bal_k && bal_k->get_value() == 1.0f) return std::make_shared<Konstans>(1.0f);
                return std::make_shared<Hatvany>(bal, jobb);
            }
        };

    }
}

#endif //MATEK_MUVELETEK_HATVANY_HPP
