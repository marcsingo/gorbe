#ifndef GORBE_KIF_HPP
#define GORBE_KIF_HPP
#include <memory>
#include "Analizis.hpp"
namespace Matek {
    namespace Analizis {
        class Kif {
        private:
            std::shared_ptr<Kifejezes const> ptr;

        public:
            Kif() : ptr(std::make_shared<Konstans>(0)) {}

            Kif(std::shared_ptr<Kifejezes const> p) : ptr(std::move(p)) {}

            Kif(float v) : ptr(std::make_shared<Konstans>(v)) {}

            Kif(float const * v) : ptr(std::make_shared<Parameter>(v)) {}

            Kif(char v) : ptr(std::make_shared<Valtozo>(v)) {}

            // Getter a belső fához
            [[nodiscard]]
            std::shared_ptr<Kifejezes const> get() const { return ptr; }

            float operator()(glm::vec3 const v) const {
                return ptr->at(v);
            }
            float at(glm::vec3 const v) const {
                return ptr->at(v);
            }

            // 3. Deriválás delegálása (Visszatérési érték automatikusan Kif-be csomagolva!)
            [[nodiscard]] Kif derrive(char var) const {
                return Kif(ptr->derrive(var));
            }

            Kif derrive(float const * var) const {
                return Kif(ptr->derrive(var));
            }

            Kif derrive(std::shared_ptr<Kifejezes const> var) const {
                return Kif(ptr->derrive(var));
            }

            // 4. Egyszerűsítés delegálása
            Kif simplify() const {
                return Kif(ptr->simplify());
            }

            // 5. Kiíratás a szabványos C++ stream operátorral
            friend std::ostream& operator<<(std::ostream& os, const Kif& k) {
                k.ptr->print(os);
                return os;
            }
        };

        // 2. Innentől minden operátort CSAK EGYSZER kell megírni!
        inline Kif operator+(const Kif& a, const Kif& b) {
            return Kif(std::make_shared<Osszeg>(a.get(), b.get()));
        }

        inline Kif operator-(const Kif& a, const Kif& b) {
            return Kif(std::make_shared<Kulonbseg>(a.get(), b.get()));
        }

        inline Kif operator*(const Kif& a, const Kif& b) {
            return Kif(std::make_shared<Szorzat>(a.get(), b.get()));
        }

        inline Kif operator/(const Kif& a, const Kif& b) {
            return Kif(std::make_shared<Hanyados>(a.get(), b.get()));
        }

        inline Kif operator^(const Kif& a, const Kif& b) {
            return Kif(std::make_shared<Hatvany>(a.get(), b.get()));
        }

        // Függvények is sokkal tisztábbak
        inline Kif sin(const Kif& a) {
            return Kif(std::make_shared<Sin>(a.get()));

        }
    }
}

#endif //GORBE_KIF_HPP