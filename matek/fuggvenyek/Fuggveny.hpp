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
        public:
            Fuggveny(std::function<float(float)> const f, std::shared_ptr<Kifejezes const> kif)
                : f(f), kif(std::move(kif)) {}

            float at(glm::vec3 const v) const override { return f(kif->at(v)); }

            std::shared_ptr<Kifejezes const> derrive(char var) const override {
                return std::make_shared<Szorzat>(get_fd(), kif->derrive(var));
            }
            std::shared_ptr<Kifejezes const> derrive(float const * var) const override {
                return std::make_shared<Szorzat>(get_fd(), kif->derrive(var));
            }
            std::shared_ptr<Kifejezes const> derrive(std::shared_ptr<Kifejezes const> var) const override {
                return std::make_shared<Szorzat>(get_fd(), kif->derrive(var));
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
