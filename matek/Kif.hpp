#ifndef GORBE_KIF_HPP
#define GORBE_KIF_HPP
#include <memory>
#include <string>
#include <cctype>
#include <stdexcept>
#include <functional>
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

            // String-ből épített kifejezés (NEM explicit, így `Kif f = "x^2 + y^2 - 1";`
            // és a stringliterál közvetlen átadása is működik). A definíció a fájl végén
            // van, mert a parser a lentebb deklarált operátorokra/függvényekre épül.
            // Természetes matematikai precedencia: ^  >  * /  >  + -, így NEM kell
            // zárójelezni a hatványt, mint a C++ DSL-ben. Változók: x, y, z; függvények:
            // sin, cos, tan/tg, ctg/cot, ln, log, sqrt. (Paramétert string nem tud hivatkozni.)
            Kif(char const* s);
            Kif(std::string const& s);

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
                return Kif(ptr->derrivate(var)).simplify();
            }

            Kif derrive(float const * var) const {
                return Kif(ptr->derrivate(var)).simplify();
            }

            Kif derrive(std::shared_ptr<Kifejezes const> var) const {
                return Kif(ptr->derrivate(var)).simplify();
            }

            Kif derrive(Kif const & var) const {
                return Kif(ptr->derrivate(var.get())).simplify();
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

        inline Kif const x = Kif(std::make_shared<Valtozo const>('x'));
        inline Kif const y = Kif(std::make_shared<Valtozo const>('y'));
        inline Kif const z = Kif(std::make_shared<Valtozo const>('z'));

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

        inline Kif operator""_k(unsigned long long value) {
            return Kif(static_cast<float>(value));
        }

        inline Kif operator""_k(long double value) {
            return Kif(static_cast<float>(value));
        }

        inline Kif operator""_v(const char var) {
            return Kif(var);
        }

        // Függvények is sokkal tisztábbak
        inline Kif sin(const Kif& a) {
            return Kif(std::make_shared<Sin>(a.get()));

        }

        // ----------------------------------------------------------------------
        // String -> kifejezésfa (recursive descent parser)
        //
        // Nyelvtan (a precedencia növekvő sorrendben, mind balasszociatív, kivéve a
        // jobbasszociatív hatványt):
        //   expr   := term   (('+' | '-') term)*
        //   term   := factor (('*' | '/') factor)*
        //   factor := ('+' | '-')* power            // előjel
        //   power  := atom ('^' factor)?            // jobbasszociatív, megenged ^-2-t is
        //   atom   := number | valtozo | fuggveny '(' expr ')' | '(' expr ')'
        // ----------------------------------------------------------------------
        namespace detail {
            struct Parser {
                std::string s;
                size_t i = 0;
                // Névfeloldó a felhasználói paraméterekhez: egy névhez (ami nem x/y/z és
                // nem függvény) stabil címet (float const*) ad; üres esetén ismeretlen
                // név hibát dob.
                std::function<float const*(std::string const&)> resolver;

                explicit Parser(std::string str,
                                std::function<float const*(std::string const&)> r = {})
                    : s(std::move(str)), resolver(std::move(r)) {}

                void skip_ws() { while (i < s.size() && std::isspace((unsigned char)s[i])) ++i; }
                char peek()    { skip_ws(); return i < s.size() ? s[i] : '\0'; }
                bool match(char c) { if (peek() == c) { ++i; return true; } return false; }

                [[noreturn]] void error(std::string const& msg) const {
                    throw std::runtime_error(
                        "Kif parse hiba a(z) " + std::to_string(i) + ". pozicionál: " +
                        msg + "  (\"" + s + "\")");
                }

                Kif parse() {
                    Kif e = parse_expr();
                    if (peek() != '\0') error("váratlan karakter a kifejezés végén");
                    return e;
                }

                Kif parse_expr() {
                    Kif left = parse_term();
                    for (;;) {
                        char c = peek();
                        if (c == '+')      { ++i; left = left + parse_term(); }
                        else if (c == '-') { ++i; left = left - parse_term(); }
                        else break;
                    }
                    return left;
                }

                Kif parse_term() {
                    Kif left = parse_factor();
                    for (;;) {
                        char c = peek();
                        if (c == '*')      { ++i; left = left * parse_factor(); }
                        else if (c == '/') { ++i; left = left / parse_factor(); }
                        else break;
                    }
                    return left;
                }

                Kif parse_factor() {
                    char c = peek();
                    if (c == '+') { ++i; return parse_factor(); }
                    if (c == '-') { ++i; return Kif(0.0f) - parse_factor(); }
                    return parse_power();
                }

                Kif parse_power() {
                    Kif base = parse_atom();
                    if (peek() == '^') { ++i; return base ^ parse_factor(); } // jobbasszociatív
                    return base;
                }

                Kif parse_atom() {
                    char c = peek();
                    if (c == '(') {
                        ++i;
                        Kif e = parse_expr();
                        if (!match(')')) error("hiányzó ')'");
                        return e;
                    }
                    if (std::isdigit((unsigned char)c) || c == '.') return parse_number();
                    if (std::isalpha((unsigned char)c))             return parse_ident();
                    error("váratlan karakter");
                }

                Kif parse_number() {
                    skip_ws();
                    size_t start = i;
                    while (i < s.size() && (std::isdigit((unsigned char)s[i]) || s[i] == '.')) ++i;
                    // opcionális tudományos jelölés: 1e-3, 2.5E+4
                    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
                        ++i;
                        if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
                        while (i < s.size() && std::isdigit((unsigned char)s[i])) ++i;
                    }
                    return Kif(std::stof(s.substr(start, i - start)));
                }

                Kif parse_ident() {
                    skip_ws();
                    size_t start = i;
                    while (i < s.size() && (std::isalnum((unsigned char)s[i]) || s[i] == '_')) ++i;
                    std::string name = s.substr(start, i - start);

                    if (peek() == '(') { // függvényhívás
                        ++i;
                        Kif arg = parse_expr();
                        if (!match(')')) error("hiányzó ')' a függvény argumentuma után");
                        return make_func(name, arg);
                    }
                    // változó: egyetlen x / y / z betű
                    if (name.size() == 1 && (name[0] == 'x' || name[0] == 'y' || name[0] == 'z'))
                        return Kif(name[0]);
                    // egyébként felhasználói paraméter: a resolver névről stabil címet ad,
                    // amire a fa Parameter-csomópontja hivatkozik (a deriválás cím szerint megy).
                    if (resolver)
                        if (float const* ref = resolver(name))
                            return Kif(ref);
                    error("ismeretlen azonosító (x/y/z változó vagy felvett paraméter?): " + name);
                }

                Kif make_func(std::string const& name, Kif const& a) const {
                    if (name == "sin")                    return Kif(std::make_shared<Sin>(a.get()));
                    if (name == "cos")                    return Kif(std::make_shared<Cos>(a.get()));
                    if (name == "tan" || name == "tg")    return Kif(std::make_shared<Tan>(a.get()));
                    if (name == "ctg" || name == "cot")   return Kif(std::make_shared<Ctg>(a.get()));
                    if (name == "ln")                     return Kif(std::make_shared<Ln>(a.get()));
                    if (name == "log")                    return Kif(std::make_shared<Log>(a.get()));
                    if (name == "sqrt")                   return a ^ Kif(0.5f);
                    error("ismeretlen függvény: " + name);
                }
            };

            inline Kif parse(std::string const& s,
                             std::function<float const*(std::string const&)> resolver) {
                return Parser(s, std::move(resolver)).parse();
            }
        }

        // Stringből kifejezés, opcionális paraméter-feloldóval. A resolver egy névhez
        // (ami nem x/y/z és nem ismert függvény) STABIL címet (float const*) ad vissza; a
        // fa egy Parameter-csomóponttal hivatkozik rá. Üres resolver esetén az ismeretlen
        // név hibát dob (ez a sima `Kif("...")` viselkedése).
        inline Kif make_kif(std::string const& s,
                            std::function<float const*(std::string const&)> resolver = {}) {
            return detail::parse(s, std::move(resolver));
        }

        inline Kif::Kif(std::string const& s) : ptr(make_kif(s).get()) {}
        inline Kif::Kif(char const* s)        : ptr(make_kif(std::string(s)).get()) {}
    }
}

#endif //GORBE_KIF_HPP