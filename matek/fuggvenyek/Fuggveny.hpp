#ifndef MATEK_FUGGVENYEK_FUGGVENY_HPP
#define MATEK_FUGGVENYEK_FUGGVENY_HPP

#include <functional>
#include "../Kifejezes.hpp"
#include "../muveletek/Szorzat.hpp"

namespace Matek {
    namespace Analizis {

        struct Fuggveny : public Kifejezes {
        private:
            std::function<float(float)> const f;
        protected:
            std::shared_ptr<Kifejezes const> kif;
            virtual std::shared_ptr<Kifejezes const> get_fd() const = 0;
            virtual char const * get_name() const = 0;
            // A laposított program műveletkódja (lásd matek/Program.hpp). Csak ennyiben
            // különböznek a függvények, ezért a compile() itt, közösen van megírva.
            virtual Op get_op() const = 0;
        public:
            int compile(Program& prog) const override {
                return prog.emit(get_op(), kif->compile(prog));
            }

            Fuggveny(std::function<float(float)> const f, std::shared_ptr<Kifejezes const> kif)
                : f(f), kif(std::move(kif)) {}

            float at(glm::vec3 const v) const override { return f(kif->at(v)); }

            std::shared_ptr<Kifejezes const> derrivate(char var) const override {
                return std::make_shared<Szorzat>(get_fd(), kif->derrivate(var));
            }
            std::shared_ptr<Kifejezes const> derrivate(float const * var) const override {
                return std::make_shared<Szorzat>(get_fd(), kif->derrivate(var));
            }
            std::shared_ptr<Kifejezes const> derrivate(std::shared_ptr<Kifejezes const> var) const override {
                return std::make_shared<Szorzat>(get_fd(), kif->derrivate(var));
            }

            void print(std::ostream &os) const override {
                os << get_name() << '(';
                kif->print(os);
                os << ')';
            }
        };

    }
}

#endif //MATEK_FUGGVENYEK_FUGGVENY_HPP
