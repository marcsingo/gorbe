#ifndef GORBE_MATEK_PARSER_HPP
#define GORBE_MATEK_PARSER_HPP

#include <algorithm>
#include <cctype>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Kif.hpp"

// ===========================================================================
// String <-> kifejezésfa, MINDKÉT irányban.
//
//   make_kif(szöveg, resolver, funcs) : string -> fa. Az ismeretlen neveket
//                                       (paraméter, elnevezett alakzat) a resolver,
//                                       a felhasználói függvényeket a funcs oldja fel.
//   kif_text(fa, namer)               : fa -> string. A paraméterek NEVÉT a namer
//                                       adja vissza (a resolver inverze).
//
// A kettő együtt oda-vissza működik: make_kif(kif_text(f, namer), resolver) UGYANAZT
// a függvényt adja, mint f (lásd tests/test_parser.cpp). A szöveg nem feltétlenül
// ugyanaz, mint amiből f készült: a parser a rövidítéseket kifejti (hypot, unio,
// a > b -> a - b), a kiírás pedig teljesen zárójelez.
//
// A NYELV (a precedencia növekvő sorrendben; mind balasszociatív, kivéve a hatványt):
//
//   or     := and  (('or' | '||') and)*               feltételek: "igaz" = POZITÍV
//   and    := not  (('and' | '&&') not)*
//   not    := ('not' | '!') not | cmp
//   cmp    := expr (('>' | '<' | '>=' | '<=') expr)?
//   expr   := term (('+' | '-') term)*
//   term   := factor (('*' | '/') factor | factor)*   az utolsó: IMPLICIT szorzás (2x, 3(x+1))
//   factor := ('+' | '-')* power                      előjel
//   power  := atom (('^' | '**') factor)?             jobbasszociatív, megengedi a ^-2-t
//   atom   := szám | név | név '(' args ')' | '(' or ')'
//
//   Változók: x y z.  Állandók: pi, e.  Függvények: a Fuggvenyek.hpp táblája.
//   Kényelmi jelek: π -> pi, ² -> ^2, ³ -> ^3, · és × -> *, − -> -.
//
// A jelenet szabályai (hatókörök, alakzatra hivatkozás, a jelenet saját függvényei)
// NEM itt vannak: azokat a hívó adja a resolverrel/funcs-szal (lásd scene/Build.hpp).
// ===========================================================================
namespace Matek {
    namespace Analizis {

        // Névfeloldó: egy azonosítóhoz (ami nem x/y/z, nem állandó és nem beépített
        // függvény) egy részkifejezést ad vissza. Lehet skalár PARAMÉTER (Parameter-
        // csomópont egy float const*-mal) vagy egy elnevezett ALAKZAT teljes fája.
        // nullptr -> ismeretlen név. (Az inverze, a ParamNamer a Kifejezes.hpp-ben van.)
        using NameResolver = std::function<Tree(std::string const&)>;

        // Felhasználói függvény: név + a már beolvasott argumentumok -> a kifejtett fa.
        // nullptr -> nincs ilyen függvény. Rossz argumentumszámnál kivételt dob.
        using FuncResolver = std::function<Tree(std::string const&, std::vector<Kif> const&)>;

        namespace detail {

            // Két szó szerkesztési távolsága — a "erre gondoltál?" javaslathoz.
            inline int edit_distance(std::string const& a, std::string const& b) {
                std::vector<int> d(b.size() + 1);
                for (std::size_t j = 0; j <= b.size(); ++j) d[j] = static_cast<int>(j);
                for (std::size_t i = 1; i <= a.size(); ++i) {
                    int prev = d[0];
                    d[0] = static_cast<int>(i);
                    for (std::size_t j = 1; j <= b.size(); ++j) {
                        int cur = d[j];
                        d[j] = std::min({d[j] + 1, d[j - 1] + 1, prev + (a[i - 1] != b[j - 1])});
                        prev = cur;
                    }
                }
                return d[b.size()];
            }

            inline std::string suggest(std::string const& name) {
                // "xy" -> valószínűleg szorzatot akart
                if (name.size() > 1 && name.find_first_not_of("xyz") == std::string::npos) {
                    std::string s(1, name[0]);
                    for (std::size_t k = 1; k < name.size(); ++k) (s += '*') += name[k];
                    return s;
                }
                std::string best;
                int best_d = 3;   // legfeljebb 2 eltérés
                for (auto const& n : builtin_names()) {
                    int dist = edit_distance(name, n);
                    if (dist < best_d && n.size() > 1) { best_d = dist; best = n; }
                }
                return best;
            }

            // A kényelmi Unicode-jelek átírása a nyelv ASCII alakjára.
            inline std::string normalize(std::string s) {
                static std::pair<char const*, char const*> const MAP[] = {
                    {"\xCF\x80", "pi"},            // π
                    {"\xC2\xB2", "^2"},            // ²
                    {"\xC2\xB3", "^3"},            // ³
                    {"\xC2\xB7", "*"},             // ·
                    {"\xC3\x97", "*"},             // ×
                    {"\xE2\x88\x92", "-"},         // − (matematikai mínusz)
                };
                for (auto [from, to] : MAP)
                    for (std::size_t p; (p = s.find(from)) != std::string::npos;)
                        s.replace(p, std::strlen(from), to);
                return s;
            }

            struct Parser {
                std::string s;
                size_t i = 0;
                NameResolver resolver;
                FuncResolver funcs;

                Parser(std::string str, NameResolver r = {}, FuncResolver f = {})
                    : s(normalize(std::move(str))), resolver(std::move(r)), funcs(std::move(f)) {}

                void skip_ws() { while (i < s.size() && std::isspace((unsigned char)s[i])) ++i; }
                char peek()    { skip_ws(); return i < s.size() ? s[i] : '\0'; }
                char peek2()   { skip_ws(); return i + 1 < s.size() ? s[i + 1] : '\0'; }
                bool match(char c) { if (peek() == c) { ++i; return true; } return false; }

                // Két karakteres operátor (>=, <=, &&, ||, **).
                bool match2(char a, char b) {
                    skip_ws();
                    if (i + 1 < s.size() && s[i] == a && s[i + 1] == b) { i += 2; return true; }
                    return false;
                }

                // Kulcsszó (and / or / not) a KÖVETKEZŐ pozíción? Csak akkor, ha nem egy
                // hosszabb azonosító eleje — így egy "orso" nevű alakzat nem lesz "or" + "so".
                bool at_kw(char const* kw) {
                    skip_ws();
                    size_t n = std::strlen(kw);
                    if (s.compare(i, n, kw) != 0) return false;
                    size_t after = i + n;
                    return !(after < s.size() &&
                             (std::isalnum((unsigned char)s[after]) || s[after] == '_'));
                }
                bool match_kw(char const* kw) {
                    if (!at_kw(kw)) return false;
                    i += std::strlen(kw);
                    return true;
                }

                // Hibaüzenet a hely megjelölésével:
                //     ismeretlen fuggveny: sni - erre gondoltal: sin?
                //       x^2 + sni(y)
                //             ^
                [[noreturn]] void error(std::string const& msg, size_t at) const {
                    std::string caret(std::min(at, s.size()) + 2, ' ');
                    throw std::runtime_error(msg + "\n  " + s + "\n" + caret + "^");
                }
                [[noreturn]] void error(std::string const& msg) const { error(msg, i); }

                Kif parse() {
                    Kif e = parse_or();
                    if (peek() != '\0') error("varatlan karakter a kifejezes vegen");
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
                //  mezőnél a szigorúságnak nincs értelme.) A tartomány-feltételek használják.
                Kif parse_or() {
                    Kif left = parse_and();
                    while (match_kw("or") || match2('|', '|')) left = max(left, parse_and());
                    return left;
                }

                Kif parse_and() {
                    Kif left = parse_not();
                    while (match_kw("and") || match2('&', '&')) left = min(left, parse_not());
                    return left;
                }

                Kif parse_not() {
                    if (match_kw("not") || match('!')) return -parse_not();
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

                // Kezdődhet-e itt egy újabb tényező (implicit szorzáshoz: 2x, 3(x+1), 2 pi)?
                // A logikai kulcsszavak nem: "x > 1 and y" nem "1 * and".
                bool starts_factor() {
                    char c = peek();
                    if (c == '(' || c == '.' || std::isdigit((unsigned char)c)) return true;
                    return std::isalpha((unsigned char)c) &&
                           !at_kw("and") && !at_kw("or") && !at_kw("not");
                }

                Kif parse_term() {
                    Kif left = parse_factor();
                    for (;;) {
                        char c = peek();
                        if (c == '*' && peek2() != '*') { ++i; left = left * parse_factor(); }
                        else if (c == '/')              { ++i; left = left / parse_factor(); }
                        else if (starts_factor())       { left = left * parse_power(); }
                        else break;
                    }
                    return left;
                }

                Kif parse_factor() {
                    char c = peek();
                    if (c == '+') { ++i; return parse_factor(); }
                    if (c == '-') { ++i; return -parse_factor(); }
                    return parse_power();
                }

                Kif parse_power() {
                    Kif base = parse_atom();
                    if (match('^') || match2('*', '*')) return base ^ parse_factor(); // jobbasszociatív
                    return base;
                }

                Kif parse_atom() {
                    char c = peek();
                    if (c == '(') {
                        ++i;
                        // Zárójelen belül a LEGFELSŐ szintről indulunk, hogy a
                        // feltételek is működjenek: pl. "not (x > 2)".
                        Kif e = parse_or();
                        if (!match(')')) error("hianyzo ')'");
                        return e;
                    }
                    if (std::isdigit((unsigned char)c) || c == '.') return parse_number();
                    if (std::isalpha((unsigned char)c))             return parse_ident();
                    if (c == '\0') error("hianyzik egy kifejezes");
                    error(std::string("varatlan karakter: '") + c + "'");
                }

                Kif parse_number() {
                    skip_ws();
                    size_t start = i;
                    while (i < s.size() && (std::isdigit((unsigned char)s[i]) || s[i] == '.')) ++i;
                    // tudományos jelölés (1e-3, 2.5E+4) — de csak ha tényleg szám jön
                    // utána: a "2e" a 2·e szorzat, a "2ex" pedig 2·ex.
                    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
                        size_t k = i + 1;
                        if (k < s.size() && (s[k] == '+' || s[k] == '-')) ++k;
                        if (k < s.size() && std::isdigit((unsigned char)s[k])) {
                            i = k;
                            while (i < s.size() && std::isdigit((unsigned char)s[i])) ++i;
                        }
                    }
                    try {
                        return Kif(std::stof(s.substr(start, i - start)));
                    } catch (std::exception const&) {
                        error("hibas szam", start);
                    }
                }

                // Egy név értéke (nem függvényhívás): változó, állandó, vagy a resolver.
                Tree value_of(std::string const& name) {
                    if (name.size() == 1 && (name[0] == 'x' || name[0] == 'y' || name[0] == 'z'))
                        return Kif(name[0]).get();
                    if (auto c = find_const(name)) return Kif(c->value).get();
                    if (resolver) return resolver(name);
                    return nullptr;
                }

                [[noreturn]] void unknown(std::string const& what, std::string const& name, size_t at) {
                    std::string hint = suggest(name);
                    error(what + ": " + name + (hint.empty() ? "" : " - erre gondoltal: " + hint + "?"), at);
                }

                Kif parse_ident() {
                    skip_ws();
                    size_t start = i;
                    while (i < s.size() && (std::isalnum((unsigned char)s[i]) || s[i] == '_')) ++i;
                    std::string name = s.substr(start, i - start);

                    if (peek() != '(') {
                        if (auto v = value_of(name)) return Kif(v);
                        if (find_func(name) || find_macro(name))
                            error("a(z) '" + name + "' fuggveny: zarojelben kell az argumentuma", start);
                        unknown("ismeretlen nev (x/y/z, parameter vagy alakzat?)", name, start);
                    }

                    // Hívás, vesszővel elválasztott argumentumokkal. Az argumentumok is a
                    // legfelső szintről indulnak, hogy feltétel is átadható legyen.
                    ++i;
                    std::vector<Kif> args;
                    if (peek() != ')') {
                        args.push_back(parse_or());
                        while (match(',')) args.push_back(parse_or());
                    }
                    if (!match(')')) error("hianyzo ')' a fuggveny argumentumai utan");
                    return make_func(name, args, start);
                }

                void arity(std::string const& name, std::size_t lo, std::size_t hi,
                           std::size_t got, size_t at) const {
                    if (got >= lo && got <= hi) return;
                    error("a(z) '" + name + "' " +
                          (lo == hi ? std::to_string(lo) : std::to_string(lo) + "-" + std::to_string(hi)) +
                          " argumentumot var, kapott " + std::to_string(got), at);
                }

                Kif make_func(std::string const& name, std::vector<Kif> const& a, size_t at) {
                    if (auto f = find_func(name)) {
                        arity(name, f->arity(), f->arity(), a.size(), at);
                        return Kif(std::make_shared<Hivas>(f, a[0].get(), a.size() > 1 ? a[1].get() : nullptr));
                    }
                    if (auto m = find_macro(name)) return expand(*m, a, at);
                    if (funcs)
                        if (auto t = funcs(name, a)) return Kif(t);
                    // Nem függvény, de ÉRTÉK: akkor ez implicit szorzás, pl. r(x + 1).
                    if (a.size() == 1)
                        if (auto v = value_of(name)) return Kif(v) * a[0];
                    unknown("ismeretlen fuggveny", name, at);
                }

                // Makró kifejtése: a törzset újra beolvassuk, a paraméterneveket az
                // argumentumokra (vagy az alapértékre) oldva fel.
                Kif expand(MacroDef const& m, std::vector<Kif> const& a, size_t at) {
                    auto params = parse_params(m.params);
                    std::size_t required = 0;
                    for (auto const& p : params) if (p.second.empty()) ++required;
                    arity(m.name, required, params.size(), a.size(), at);
                    return make_kif_with(m.body, params, a, funcs);
                }

                // "a, b, k = 0.5" -> {{"a",""}, {"b",""}, {"k","0.5"}}
                static std::vector<std::pair<std::string, std::string>> parse_params(std::string const& ps) {
                    std::vector<std::pair<std::string, std::string>> out;
                    std::stringstream ss(ps);
                    for (std::string item; std::getline(ss, item, ',');) {
                        auto trim = [](std::string t) {
                            t.erase(0, t.find_first_not_of(" \t"));
                            t.erase(t.find_last_not_of(" \t") + 1);
                            return t;
                        };
                        auto eq = item.find('=');
                        if (eq == std::string::npos) out.emplace_back(trim(item), "");
                        else out.emplace_back(trim(item.substr(0, eq)), trim(item.substr(eq + 1)));
                        if (out.back().first.empty()) out.pop_back();
                    }
                    return out;
                }

                // Egy törzs beolvasása úgy, hogy a paraméternevek az argumentumokat jelentik.
                static Kif make_kif_with(std::string const& body,
                                         std::vector<std::pair<std::string, std::string>> const& params,
                                         std::vector<Kif> const& a, FuncResolver const& funcs) {
                    NameResolver r = [&](std::string const& n) -> Tree {
                        for (std::size_t k = 0; k < params.size(); ++k)
                            if (params[k].first == n)
                                return k < a.size() ? a[k].get() : Parser(params[k].second).parse().get();
                        return nullptr;
                    };
                    return Parser(body, r, funcs).parse();
                }
            };

            inline Kif parse(std::string const& s, NameResolver resolver, FuncResolver funcs) {
                return Parser(s, std::move(resolver), std::move(funcs)).parse();
            }

            // A táblabeli derivált (du/dv) beolvasva, u és v helyőrzőkkel. Egyszer
            // olvassuk be, utána a gyorsítótárból jön (a zár a többszálú sugárkövető miatt).
            inline Tree deriv_template(FuncDef const& f, int which) {
                static std::mutex m;
                static std::map<std::pair<FuncDef const*, int>, Tree> cache;
                std::lock_guard<std::mutex> lock(m);
                auto& t = cache[{&f, which}];
                if (!t) {
                    NameResolver uv = [](std::string const& n) -> Tree {
                        if (n == "u" || n == "v") return Kif(n[0]).get();
                        return nullptr;
                    };
                    t = Parser(which == 0 ? f.du : f.dv, uv).parse().get();
                }
                return t;
            }
        }

        // Stringből kifejezés, opcionális név- és függvényfeloldóval. Üres resolver
        // esetén az ismeretlen név hibát dob (ez a sima `Kif("...")` viselkedése).
        inline Kif make_kif(std::string const& s, NameResolver resolver = {}, FuncResolver funcs = {}) {
            return detail::parse(s, std::move(resolver), std::move(funcs));
        }

        // Felhasználói függvény kifejtése a makrókéval azonos módon: a `params` a
        // paraméterlista ("u, v" vagy "u, k = 1"), a `body` a törzs. A törzsben a
        // paraméternevek az argumentumokat jelentik, minden mást a `resolver` old fel.
        inline Kif expand_user_function(std::string const& name, std::string const& params,
                                        std::string const& body, std::vector<Kif> const& args,
                                        NameResolver resolver, FuncResolver funcs) {
            auto ps = detail::Parser::parse_params(params);
            std::size_t required = 0;
            for (auto const& p : ps) if (p.second.empty()) ++required;
            if (args.size() < required || args.size() > ps.size())
                throw std::runtime_error("a(z) '" + name + "' " + std::to_string(ps.size()) +
                                         " argumentumot var, kapott " + std::to_string(args.size()));
            NameResolver r = [&](std::string const& n) -> Tree {
                for (std::size_t k = 0; k < ps.size(); ++k)
                    if (ps[k].first == n)
                        return k < args.size() ? args[k].get() : make_kif(ps[k].second, resolver).get();
                return resolver ? resolver(n) : nullptr;
            };
            try {
                return make_kif(body, r, std::move(funcs));
            } catch (std::exception const& e) {
                throw std::runtime_error("a(z) '" + name + "' fuggveny torzse: " + e.what());
            }
        }

        // Kifejezésfa -> újra beolvasható szöveg. A paraméterek nevét a namer adja;
        // enélkül (vagy ismeretlen címnél) a paraméter PILLANATNYI értéke íródik ki.
        inline std::string kif_text(Kif const& f, ParamNamer const& namer = {}) {
            std::ostringstream os;
            f.get()->print(os, namer);
            return os.str();
        }

        inline Kif::Kif(std::string const& s) : ptr(make_kif(s).get()) {}
        inline Kif::Kif(char const* s)        : ptr(make_kif(std::string(s)).get()) {}

        // Láncszabály a táblabeli függvényekre: d f(a, b) = du(a, b)·a' + dv(a, b)·b'.
        inline Tree Hivas::derive(Var const& var) const {
            SubstMap m{{'u', a}};
            if (b) m['v'] = b;
            Tree r = std::make_shared<Szorzat>(detail::deriv_template(*def, 0)->substitute(m),
                                               a->derive(var));
            if (b)
                r = std::make_shared<Osszeg>(r, std::make_shared<Szorzat>(
                        detail::deriv_template(*def, 1)->substitute(m), b->derive(var)));
            return r;
        }
    }
}

#endif //GORBE_MATEK_PARSER_HPP
