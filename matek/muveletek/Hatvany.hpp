#ifndef MATEK_MUVELETEK_HATVANY_HPP
#define MATEK_MUVELETEK_HATVANY_HPP

#include <cmath>
#include "KetOperandus.hpp"
#include "Szorzat.hpp"
#include "Osszeg.hpp"
#include "Hanyados.hpp"
#include "../Fuggvenyek.hpp"

namespace Matek {
    namespace Analizis {

        struct Hatvany : public KetOperandus {
        protected:
            char get_operator() const override { return '^'; }
        public:
            explicit Hatvany(Tree bal, Tree jobb) : KetOperandus(std::move(bal), std::move(jobb)) {}

            float at(glm::vec3 const v) const override {
                return std::pow(get_bal()->at(v), get_jobb()->at(v));
            }

            // Konstans kitevő esetén (a^n, n állandó) a STABIL hatványszabály:
            //   d/dx a^n = n * a^(n-1) * a'
            // Az általános képletet csak akkor használjuk, ha a kitevő is függ a
            // változótól:
            //   d/dx a^b = a^b * ( b' * ln|a| + b * a'/a )
            // Az ln|a| (nem ln a) miatt negatív alapnál is helyes, ahol a^b értelmes
            // (egész kitevő); a = 0-ban viszont ez is végtelen — ott a^b maga sem sima.
            Tree derive(Var const& var) const override {
                auto a = get_bal(), b = get_jobb();
                if (auto n = const_of(b)) {
                    return std::make_shared<Szorzat>(
                        std::make_shared<Szorzat>(
                            std::make_shared<Konstans>(n->get_value()),
                            std::make_shared<Hatvany>(a, std::make_shared<Konstans>(n->get_value() - 1.0f))),
                        a->derive(var));
                }
                return std::make_shared<Szorzat>(
                    std::make_shared<Hatvany>(a, b),
                    std::make_shared<Osszeg>(
                        std::make_shared<Szorzat>(b->derive(var), call("ln", call("abs", a))),
                        std::make_shared<Szorzat>(b, std::make_shared<Hanyados>(a->derive(var), a))));
            }

            Tree with_children(Tree a, Tree b) const override {
                return std::make_shared<Hatvany>(std::move(a), std::move(b));
            }

            int compile(Program& prog) const override {
                int a = get_bal()->compile(prog);
                int b = get_jobb()->compile(prog);
                return prog.emit(Op::Pow, a, b);
            }

            Tree simplify() const override {
                auto bal = get_bal()->simplify();
                auto jobb = get_jobb()->simplify();
                auto a = const_of(bal), b = const_of(jobb);
                if (a && b) return std::make_shared<Konstans>(std::pow(a->get_value(), b->get_value()));
                if (is_const(jobb, 0.0f)) return std::make_shared<Konstans>(1.0f);
                if (is_const(jobb, 1.0f)) return bal;
                if (is_const(bal, 0.0f)) return std::make_shared<Konstans>(0.0f);
                if (is_const(bal, 1.0f)) return std::make_shared<Konstans>(1.0f);
                return std::make_shared<Hatvany>(bal, jobb);
            }
        };

    }
}

#endif //MATEK_MUVELETEK_HATVANY_HPP
