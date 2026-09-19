#ifndef MATEK_LEVELEK_VALTOZO_HPP
#define MATEK_LEVELEK_VALTOZO_HPP

#include <stdexcept>
#include <string>
#include "../Kifejezes.hpp"
#include "Konstans.hpp"

namespace Matek {
    namespace Analizis {

        // Térbeli változó: x, y vagy z. (A függvénytábla deriváltjaiban `u` és `v` is
        // előfordul, de csak helyőrzőként: a láncszabály még kiértékelés ELŐTT
        // kicseréli őket a tényleges argumentumokra — lásd Fuggvenyek.hpp.)
        struct Valtozo : public Kifejezes {
        private:
            char const var;

            [[noreturn]] void unknown() const {
                throw std::logic_error(std::string("ismeretlen valtozo: ") + var);
            }
        public:
            Valtozo(char const var) : var(var) {}

            float at(glm::vec3 const v) const override {
                if (var == 'x') return v.x;
                if (var == 'y') return v.y;
                if (var == 'z') return v.z;
                unknown();
            }

            Tree derive(Var const& v) const override {
                bool hit = (std::holds_alternative<char>(v) && std::get<char>(v) == var) ||
                           (std::holds_alternative<Kifejezes const*>(v) &&
                            std::get<Kifejezes const*>(v) == this);
                return std::make_shared<Konstans>(hit ? 1.0f : 0.0f);
            }

            bool same(Kifejezes const& o) const override {
                auto p = dynamic_cast<Valtozo const*>(&o);
                return p && p->var == var;
            }

            void print(std::ostream &os, ParamNamer const&) const override { os << var; }

            Tree substitute(SubstMap const& m) const override {
                auto it = m.find(var);
                if (it != m.end()) return it->second;
                return std::make_shared<Valtozo>(var);
            }

            int compile(Program& prog) const override {
                if (var == 'x') return prog.emit(Op::VarX);
                if (var == 'y') return prog.emit(Op::VarY);
                if (var == 'z') return prog.emit(Op::VarZ);
                unknown();
            }

            Tree simplify() const override { return std::make_shared<Valtozo>(var); }
        };

    }
}

#endif //MATEK_LEVELEK_VALTOZO_HPP
