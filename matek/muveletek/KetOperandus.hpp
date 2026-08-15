#ifndef MATEK_MUVELETEK_KETOPERANDUS_HPP
#define MATEK_MUVELETEK_KETOPERANDUS_HPP

#include "../Kifejezes.hpp"

namespace Matek {
    namespace Analizis {

        struct KetOperandus : public Kifejezes {
        private:
            std::shared_ptr<Kifejezes const> bal;
            std::shared_ptr<Kifejezes const> jobb;
        protected:
            std::shared_ptr<Kifejezes const> get_bal() const { return bal; }
            std::shared_ptr<Kifejezes const> get_jobb() const { return jobb; }
            virtual char const get_operator() const = 0;
            // Ugyanaz a muvelet, mas gyerekekkel. Csak ennyiben kulonboznek a
            // ketoperandusu csomopontok, ezert a substitute() itt, kozosen van megirva.
            virtual std::shared_ptr<Kifejezes const> with_children(
                std::shared_ptr<Kifejezes const> a,
                std::shared_ptr<Kifejezes const> b) const = 0;
        public:
            KetOperandus(std::shared_ptr<Kifejezes const> bal,
                std::shared_ptr<Kifejezes const> jobb)
                : bal(std::move(bal)), jobb(std::move(jobb)) {}

            std::shared_ptr<Kifejezes const> substitute(SubstMap const& m) const override {
                return with_children(bal->substitute(m), jobb->substitute(m));
            }

            void print(std::ostream& os) const override {
                os << '(';
                get_bal()->print(os);
                os << ' ' << get_operator() << ' ';
                get_jobb()->print(os);
                os << ')';
            }
        };

    }
}

#endif //MATEK_MUVELETEK_KETOPERANDUS_HPP
