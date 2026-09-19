#ifndef GORBE_KIF_HPP
#define GORBE_KIF_HPP
#include <memory>
#include <string>
#include <vector>
#include "Analizis.hpp"

namespace Matek {
    namespace Analizis {

        // Érték-szemantikájú burkoló a kifejezésfa köré, és a C++ oldali DSL alapja:
        //     Kif f = (x^2) + (y^2) - 1.0_k;      // C++ operátorokkal
        //     Kif g = "x^2 + y^2 - 1";            // vagy szövegből (matek/Parser.hpp)
        class Kif {
        private:
            Tree ptr;

        public:
            Kif() : ptr(std::make_shared<Konstans>(0.0f)) {}
            Kif(Tree p) : ptr(std::move(p)) {}
            Kif(float v) : ptr(std::make_shared<Konstans>(v)) {}
            Kif(float const * v) : ptr(std::make_shared<Parameter>(v)) {}
            Kif(char v) : ptr(std::make_shared<Valtozo>(v)) {}

            // Szövegből épített kifejezés (NEM explicit, így `Kif f = "x^2 + y^2 - 1";`
            // is működik). A nyelv leírása és a definíció a matek/Parser.hpp-ben van.
            // (Paraméterre a sima `Kif("...")` nem hivatkozhat, ahhoz make_kif + resolver kell.)
            Kif(char const* s);
            Kif(std::string const& s);

            [[nodiscard]] Tree get() const { return ptr; }

            float operator()(glm::vec3 const v) const { return ptr->at(v); }
            float at(glm::vec3 const v) const { return ptr->at(v); }

            // Deriválás: változó ('x'), paraméter (float cím) vagy részfa szerint.
            // Az eredmény egyszerűsítve jön vissza.
            [[nodiscard]] Kif derive(char var) const          { return Kif(ptr->derive(Var{var})).simplify(); }
            [[nodiscard]] Kif derive(float const* var) const  { return Kif(ptr->derive(Var{var})).simplify(); }
            [[nodiscard]] Kif derive(Tree const& var) const   { return Kif(ptr->derive(Var{var.get()})).simplify(); }
            [[nodiscard]] Kif derive(Kif const& var) const    { return derive(var.get()); }

            Kif simplify() const { return Kif(ptr->simplify()); }

            // Kiírás (a paraméterek az értékükkel; nevekkel lásd kif_text).
            friend std::ostream& operator<<(std::ostream& os, const Kif& k) {
                k.ptr->print(os, {});
                return os;
            }
        };

        inline Kif const x = Kif(std::make_shared<Valtozo const>('x'));
        inline Kif const y = Kif(std::make_shared<Valtozo const>('y'));
        inline Kif const z = Kif(std::make_shared<Valtozo const>('z'));

        inline Kif operator+(const Kif& a, const Kif& b) { return Kif(std::make_shared<Osszeg>(a.get(), b.get())); }
        inline Kif operator-(const Kif& a, const Kif& b) { return Kif(std::make_shared<Kulonbseg>(a.get(), b.get())); }
        inline Kif operator*(const Kif& a, const Kif& b) { return Kif(std::make_shared<Szorzat>(a.get(), b.get())); }
        inline Kif operator/(const Kif& a, const Kif& b) { return Kif(std::make_shared<Hanyados>(a.get(), b.get())); }
        inline Kif operator^(const Kif& a, const Kif& b) { return Kif(std::make_shared<Hatvany>(a.get(), b.get())); }
        inline Kif operator-(const Kif& a) { return Kif(0.0f) - a; }

        inline Kif operator""_k(unsigned long long value) { return Kif(static_cast<float>(value)); }
        inline Kif operator""_k(long double value)        { return Kif(static_cast<float>(value)); }
        inline Kif operator""_v(const char var)           { return Kif(var); }

        // Bármelyik táblabeli függvény C++-ból: fn("atan", x), fn("atan2", y, x).
        inline Kif fn(char const* name, Kif const& a) { return Kif(call(name, a.get())); }
        inline Kif fn(char const* name, Kif const& a, Kif const& b) { return Kif(call(name, a.get(), b.get())); }

        // A leggyakoribbak rövid néven is (a kódból épített kifejezésekhez).
        inline Kif sin(const Kif& a)  { return fn("sin", a); }
        inline Kif cos(const Kif& a)  { return fn("cos", a); }
        inline Kif abs(const Kif& a)  { return fn("abs", a); }
        inline Kif sign(const Kif& a) { return fn("sign", a); }
        inline Kif sqrt(const Kif& a) { return fn("sqrt", a); }
        inline Kif min(const Kif& a, const Kif& b) { return fn("min", a, b); }
        inline Kif max(const Kif& a, const Kif& b) { return fn("max", a, b); }

        // A feltételek "igaz = pozitív" ábrázolásában az ÉS a minimum (lásd Parser.hpp).
        inline Kif kif_and(const Kif& a, const Kif& b) { return min(a, b); }

        // Változó-behelyettesítés a Kif szintjén (tér-transzformációkhoz).
        inline Kif substitute(Kif const& e, SubstMap const& m) {
            return Kif(e.get()->substitute(m));
        }

    }
}

// A string <-> kifejezésfa átalakítás (a Kif(char const*) konstruktor is ott van
// definiálva, mert a parser a fenti operátorokra épül).
#include "Parser.hpp"

#endif //GORBE_KIF_HPP
