#ifndef GORBE_KIF_HPP
#define GORBE_KIF_HPP
#include <memory>
#include <string>
#include <vector>
#include <cctype>
#include <cstring>
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
            // zárójelezni a hatványt, mint a C++ DSL-ben. Változók: x, y, z.
            // Függvények (lásd make_func):
            //   egyváltozós: sin cos tan/tg ctg/cot ln log sqrt abs sign
            //   éles:        min(a,b) max(a,b) unio metszet kulonbseg
            //   sima:        smin(a,b[,k]) smax sunio smetszet skulonbseg
            // (Paramétert a sima `Kif("...")` nem tud hivatkozni, ahhoz make_kif + resolver kell.)
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

        // Függvény-burkolók a C++ oldali DSL-hez. (A string-parser saját táblát használ,
        // lásd make_func; ezek a kódból épített kifejezésekhez kellenek.)
        inline Kif sin(const Kif& a)  { return Kif(std::make_shared<Sin>(a.get())); }
        inline Kif cos(const Kif& a)  { return Kif(std::make_shared<Cos>(a.get())); }
        inline Kif abs(const Kif& a)  { return Kif(std::make_shared<Abs>(a.get())); }
        inline Kif sign(const Kif& a) { return Kif(std::make_shared<Elojel>(a.get())); }

        inline Kif min(const Kif& a, const Kif& b) {
            return Kif(std::make_shared<Minimum>(a.get(), b.get()));
        }
        inline Kif max(const Kif& a, const Kif& b) {
            return Kif(std::make_shared<Maximum>(a.get(), b.get()));
        }

        // A feltételek "igaz = pozitív" ábrázolásában az ÉS a minimum (lásd parse_and).
        inline Kif kif_and(const Kif& a, const Kif& b) { return min(a, b); }

        // Változó-behelyettesítés a Kif szintjén (tér-transzformációkhoz).
        inline Kif substitute(Kif const& e, SubstMap const& m) {
            return Kif(e.get()->substitute(m));
        }

        // Névfeloldó: egy azonosítóhoz (ami nem x/y/z és nem ismert függvény) egy
        // részkifejezést ad vissza. Lehet skalár PARAMÉTER (Parameter-csomópont egy
        // float const*-mal) vagy egy elnevezett ALAKZAT teljes fája — így a képletben
        // hivatkozhatunk rájuk (pl. f1, f2, ... uniózásához). nullptr -> ismeretlen név.
        using NameResolver = std::function<std::shared_ptr<Kifejezes const>(std::string const&)>;

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
                // Névfeloldó (paraméter vagy elnevezett alakzat); üres esetén az
                // ismeretlen név hibát dob.
                NameResolver resolver;

                explicit Parser(std::string str, NameResolver r = {})
                    : s(std::move(str)), resolver(std::move(r)) {}

                void skip_ws() { while (i < s.size() && std::isspace((unsigned char)s[i])) ++i; }
                char peek()    { skip_ws(); return i < s.size() ? s[i] : '\0'; }
                bool match(char c) { if (peek() == c) { ++i; return true; } return false; }

                // Két karakteres operátor (>=, <=, &&, ||).
                bool match2(char a, char b) {
                    skip_ws();
                    if (i + 1 < s.size() && s[i] == a && s[i + 1] == b) { i += 2; return true; }
                    return false;
                }

                // Kulcsszó (and / or / not). Csak akkor illeszkedik, ha nem egy hosszabb
                // azonosító eleje — így egy "orso" nevű alakzat nem lesz "or" + "so".
                bool match_kw(char const* kw) {
                    skip_ws();
                    size_t n = std::strlen(kw);
                    if (s.compare(i, n, kw) != 0) return false;
                    size_t after = i + n;
                    if (after < s.size() &&
                        (std::isalnum((unsigned char)s[after]) || s[after] == '_')) return false;
                    i = after;
                    return true;
                }

                [[noreturn]] void error(std::string const& msg) const {
                    throw std::runtime_error(
                        "Kif parse hiba a(z) " + std::to_string(i) + ". pozicionál: " +
                        msg + "  (\"" + s + "\")");
                }

                Kif parse() {
                    Kif e = parse_or();
                    if (peek() != '\0') error("váratlan karakter a kifejezés végén");
                    return e;
                }

                // --- Logikai szint ---------------------------------------------------
                // A feltételeket ugyanabban a kifejezésfában ábrázoljuk, mint minden mást:
                // "igaz" azt jelenti, hogy az érték POZITÍV. Ezzel minden logikai művelet
                // leképződik a már meglévő csomópontokra, új osztály nélkül:
                //
                //     a > b   ->  a - b          a and b  ->  min(a, b)
                //     a < b   ->  b - a          a or  b  ->  max(a, b)
                //     a >= b  ->  a - b          not a    ->  -a
                //     a <= b  ->  b - a
                //
                // (Ez az R-függvények szokásos trükkje. A >= és a > ugyanaz: lebegőpontos
                //  mezőnél a szigorúságnak nincs értelme.)
                Kif parse_or() {
                    Kif left = parse_and();
                    while (match_kw("or") || match2('|', '|')) {
                        Kif right = parse_and();
                        left = Kif(std::make_shared<Maximum>(left.get(), right.get()));
                    }
                    return left;
                }

                Kif parse_and() {
                    Kif left = parse_not();
                    while (match_kw("and") || match2('&', '&')) {
                        Kif right = parse_not();
                        left = Kif(std::make_shared<Minimum>(left.get(), right.get()));
                    }
                    return left;
                }

                Kif parse_not() {
                    if (match_kw("not") || match('!')) return Kif(0.0f) - parse_not();
                    return parse_cmp();
                }

                Kif parse_cmp() {
                    Kif left = parse_expr();
                    if (match2('>', '=')) return left - parse_expr();
                    if (match2('<', '=')) { Kif r = parse_expr(); return r - left; }
                    if (match('>'))       return left - parse_expr();
                    if (match('<'))       { Kif r = parse_expr(); return r - left; }
                    return left;
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
                        // Zárójelen belül a LEGFELSŐ szintről indulunk, hogy a
                        // feltételek is működjenek: pl. "not (x > 2)".
                        Kif e = parse_or();
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

                    if (peek() == '(') { // függvényhívás, vesszővel elválasztott argumentumokkal
                        ++i;
                        // Az argumentumok is a legfelső szintről indulnak, hogy
                        // feltétel is átadható legyen: pl. min(x > 1, y > 1).
                        std::vector<Kif> args;
                        if (peek() != ')') {
                            args.push_back(parse_or());
                            while (match(',')) args.push_back(parse_or());
                        }
                        if (!match(')')) error("hiányzó ')' a függvény argumentumai után");
                        return make_func(name, args);
                    }
                    // változó: egyetlen x / y / z betű
                    if (name.size() == 1 && (name[0] == 'x' || name[0] == 'y' || name[0] == 'z'))
                        return Kif(name[0]);
                    // egyébként a resolver oldja fel: paraméter (skalár) vagy elnevezett
                    // alakzat (részkifejezés). A visszakapott fát beágyazzuk ide.
                    if (resolver)
                        if (auto sub = resolver(name))
                            return Kif(sub);
                    error("ismeretlen azonosító (x/y/z változó, paraméter vagy alakzat?): " + name);
                }

                // A halmazműveletek alapértelmezett lekerekítési sugara, ha a hívó nem ad
                // harmadik argumentumot. Az F értékének nagyságrendjéhez kell igazítani,
                // ezért érdemes explicit megadni.
                static constexpr float DEFAULT_K = 0.5f;

                Kif make_func(std::string const& name, std::vector<Kif> const& a) const {
                    auto arity = [&](size_t lo, size_t hi) {
                        if (a.size() < lo || a.size() > hi)
                            error("a(z) '" + name + "' " +
                                  (lo == hi ? std::to_string(lo)
                                            : std::to_string(lo) + "-" + std::to_string(hi)) +
                                  " argumentumot var, kapott " + std::to_string(a.size()));
                    };

                    // --- egyváltozós függvények ---
                    if (name == "sin")  { arity(1, 1); return Kif(std::make_shared<Sin>(a[0].get())); }
                    if (name == "cos")  { arity(1, 1); return Kif(std::make_shared<Cos>(a[0].get())); }
                    if (name == "tan" || name == "tg")  { arity(1, 1); return Kif(std::make_shared<Tan>(a[0].get())); }
                    if (name == "ctg" || name == "cot") { arity(1, 1); return Kif(std::make_shared<Ctg>(a[0].get())); }
                    if (name == "ln")   { arity(1, 1); return Kif(std::make_shared<Ln>(a[0].get())); }
                    if (name == "log")  { arity(1, 1); return Kif(std::make_shared<Log>(a[0].get())); }
                    if (name == "sqrt") { arity(1, 1); return a[0] ^ Kif(0.5f); }
                    if (name == "abs")  { arity(1, 1); return Kif(std::make_shared<Abs>(a[0].get())); }
                    if (name == "sign") { arity(1, 1); return Kif(std::make_shared<Elojel>(a[0].get())); }

                    // --- éles min / max ---
                    auto MIN = [](Kif const& p, Kif const& q) {
                        return Kif(std::make_shared<Minimum>(p.get(), q.get()));
                    };
                    auto MAX = [](Kif const& p, Kif const& q) {
                        return Kif(std::make_shared<Maximum>(p.get(), q.get()));
                    };
                    if (name == "min") { arity(2, 2); return MIN(a[0], a[1]); }
                    if (name == "max") { arity(2, 2); return MAX(a[0], a[1]); }

                    // --- éles halmazműveletek (F < 0 = belül konvencióval) ---
                    // A BlobTree-cikk max-ot ír unióra, mert ott a potenciál BELÜL nagy;
                    // nálunk F belül negatív, ezért unió = min és metszet = max.
                    if (name == "unio"      || name == "union")     { arity(2, 2); return MIN(a[0], a[1]); }
                    if (name == "metszet"   || name == "intersect") { arity(2, 2); return MAX(a[0], a[1]); }
                    if (name == "kulonbseg" || name == "subtract")  { arity(2, 2); return MAX(a[0], Kif(0.0f) - a[1]); }

                    // --- sima (C^inf) halmazműveletek ---
                    // smin(p,q,k) = ½·( p + q − sqrt((p−q)² + k²) ).  A gyök alatti +k²
                    // miatt az argumentum sosem 0, így a derivált a varraton is véges —
                    // ellentétben a cikk R-függvényével, ami ott NaN-t adna.
                    auto k_of = [&](size_t idx) { return a.size() > idx ? a[idx] : Kif(DEFAULT_K); };
                    auto SMIN = [&](Kif const& p, Kif const& q, Kif const& k) {
                        return Kif(0.5f) * (p + q - ((((p - q) ^ Kif(2.0f)) + (k ^ Kif(2.0f))) ^ Kif(0.5f)));
                    };
                    auto SMAX = [&](Kif const& p, Kif const& q, Kif const& k) {
                        return Kif(0.5f) * (p + q + ((((p - q) ^ Kif(2.0f)) + (k ^ Kif(2.0f))) ^ Kif(0.5f)));
                    };
                    if (name == "smin") { arity(2, 3); return SMIN(a[0], a[1], k_of(2)); }
                    if (name == "smax") { arity(2, 3); return SMAX(a[0], a[1], k_of(2)); }
                    if (name == "sunio"      || name == "sunion")     { arity(2, 3); return SMIN(a[0], a[1], k_of(2)); }
                    if (name == "smetszet"   || name == "sintersect") { arity(2, 3); return SMAX(a[0], a[1], k_of(2)); }
                    if (name == "skulonbseg" || name == "ssubtract")  { arity(2, 3); return SMAX(a[0], Kif(0.0f) - a[1], k_of(2)); }

                    error("ismeretlen fuggveny: " + name);
                }
            };

            inline Kif parse(std::string const& s, NameResolver resolver) {
                return Parser(s, std::move(resolver)).parse();
            }
        }

        // Stringből kifejezés, opcionális névfeloldóval (paraméterek / elnevezett alakzatok).
        // Üres resolver esetén az ismeretlen név hibát dob (ez a sima `Kif("...")` viselkedése).
        inline Kif make_kif(std::string const& s, NameResolver resolver = {}) {
            return detail::parse(s, std::move(resolver));
        }

        inline Kif::Kif(std::string const& s) : ptr(make_kif(s).get()) {}
        inline Kif::Kif(char const* s)        : ptr(make_kif(std::string(s)).get()) {}
    }
}

#endif //GORBE_KIF_HPP