#ifndef MATEK_LEVELEK_PARAMETER_HPP
#define MATEK_LEVELEK_PARAMETER_HPP

#include <stdexcept>
#include "../Kifejezes.hpp"
#include "Konstans.hpp"

namespace Matek {
    namespace Analizis {

        // Egy külső float CÍMÉRE hivatkozó levél: a kiértékelés mindig a pillanatnyi
        // értékét olvassa, ezért egy GUI-csúszka újraépítés nélkül mozgatja az alakzatot.
        struct Parameter : public Kifejezes {
        private:
            float const * ertek_ref;
        public:
            explicit Parameter(float const * ref) : ertek_ref(ref) {
                if (!ertek_ref)
                    throw std::invalid_argument("A parameter referenciaja nem lehet null!");
            }

            float at(glm::vec3 const) const override { return *ertek_ref; }

            Tree derive(Var const& v) const override {
                bool hit = (std::holds_alternative<float const*>(v) &&
                            std::get<float const*>(v) == ertek_ref) ||
                           (std::holds_alternative<Kifejezes const*>(v) &&
                            std::get<Kifejezes const*>(v) == this);
                return std::make_shared<Konstans>(hit ? 1.0f : 0.0f);
            }

            bool same(Kifejezes const& o) const override {
                auto p = dynamic_cast<Parameter const*>(&o);
                return p && p->ertek_ref == ertek_ref;
            }

            // A nevet a namer adja; enélkül a PILLANATNYI érték íródik ki (az így
            // visszaolvasott kifejezés már nem követi a paramétert).
            void print(std::ostream &os, ParamNamer const& namer) const override {
                std::string name = namer ? namer(ertek_ref) : std::string();
                if (!name.empty()) os << name;
                else               Konstans(*ertek_ref).print(os, namer);
            }

            Tree substitute(SubstMap const&) const override {
                return std::make_shared<Parameter>(ertek_ref);
            }

            int compile(Program& prog) const override {
                return prog.emit(Op::Param, -1, -1, 0.0f, ertek_ref);
            }

            Tree simplify() const override { return std::make_shared<Parameter>(ertek_ref); }
        };

    }
}

#endif //MATEK_LEVELEK_PARAMETER_HPP
