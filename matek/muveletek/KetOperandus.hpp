#ifndef MATEK_MUVELETEK_KETOPERANDUS_HPP
#define MATEK_MUVELETEK_KETOPERANDUS_HPP

#include <typeinfo>
#include "../Kifejezes.hpp"
#include "../levelek/Konstans.hpp"

namespace Matek {
    namespace Analizis {

        struct KetOperandus : public Kifejezes {
        private:
            Tree bal;
            Tree jobb;
        protected:
            Tree get_bal() const { return bal; }
            Tree get_jobb() const { return jobb; }
            virtual char get_operator() const = 0;
            // Ugyanaz a muvelet, mas gyerekekkel. Csak ennyiben kulonboznek a
            // ketoperandusu csomopontok, ezert a substitute() itt, kozosen van megirva.
            virtual Tree with_children(Tree a, Tree b) const = 0;
        public:
            KetOperandus(Tree bal, Tree jobb) : bal(std::move(bal)), jobb(std::move(jobb)) {}

            Tree substitute(SubstMap const& m) const override {
                return with_children(bal->substitute(m), jobb->substitute(m));
            }

            bool same(Kifejezes const& o) const override {
                if (typeid(o) != typeid(*this)) return false;
                auto const& k = static_cast<KetOperandus const&>(o);
                return bal->same(*k.bal) && jobb->same(*k.jobb);
            }

            void print(std::ostream& os, ParamNamer const& namer) const override {
                os << '(';
                bal->print(os, namer);
                os << ' ' << get_operator() << ' ';
                jobb->print(os, namer);
                os << ')';
            }
        };

    }
}

#endif //MATEK_MUVELETEK_KETOPERANDUS_HPP
