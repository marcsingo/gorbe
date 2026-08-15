#ifndef MATEK_LEVELEK_VALTOZO_HPP
#define MATEK_LEVELEK_VALTOZO_HPP

#include "../Kifejezes.hpp"
#include "Konstans.hpp"

namespace Matek {
    namespace Analizis {

        struct Valtozo : public Kifejezes {
        private:
            char const var;
        public:
            Valtozo(char const var) : var(var) {}

            float at(glm::vec3 const v) const override {
                if (var == 'x') return v.x;
                if (var == 'y') return v.y;
                if (var == 'z') return v.z;
                return 0.0f;
            }

            std::shared_ptr<Kifejezes const> derrivate(char var) const override {
                return std::make_shared<Konstans>(this->var == var ? 1 : 0);
            }
            std::shared_ptr<Kifejezes const> derrivate(float const * var) const override {
                return std::make_shared<Konstans>(0);
            }
            std::shared_ptr<Kifejezes const> derrivate(std::shared_ptr<Kifejezes const> var) const override {
                if (this == var.get()) return std::make_shared<Konstans const>(1);
                return std::make_shared<Konstans>(0);
            }

            void print(std::ostream &os) const override { os << var; }

            std::shared_ptr<Kifejezes const> substitute(SubstMap const& m) const override {
                auto it = m.find(var);
                if (it != m.end()) return it->second;
                return std::make_shared<Valtozo>(var);
            }

            int compile(Program& prog) const override {
                if (var == 'x') return prog.emit(Op::VarX);
                if (var == 'y') return prog.emit(Op::VarY);
                if (var == 'z') return prog.emit(Op::VarZ);
                return prog.emit(Op::Const, -1, -1, 0.0f);   // at() is 0-t ad
            }


            std::shared_ptr<Kifejezes const> simplify() const override {
                return std::make_shared<Valtozo>(var);
            }
        };

    }
}

#endif //MATEK_LEVELEK_VALTOZO_HPP
