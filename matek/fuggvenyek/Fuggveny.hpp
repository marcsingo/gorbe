#ifndef MATEK_FUGGVENYEK_FUGGVENY_HPP
#define MATEK_FUGGVENYEK_FUGGVENY_HPP

#include "../Kifejezes.hpp"
#include "../levelek/Konstans.hpp"

namespace Matek {
    namespace Analizis {

        // Egy beépített függvény leírása — a függvénytábla egy sora (Fuggvenyek.hpp).
        // Ebből dolgozik a kiértékelés, a deriválás, a lapos program, a parser és a
        // kiírás is; új függvényhez ezért elég a táblába egy új sort írni.
        struct FuncDef {
            char const* name;
            char const* aliases = "";   // szóközzel elválasztva, pl. "arctg arctan"
            Fn1         f1 = nullptr;   // egyargumentumú: f(u)
            Fn2         f2 = nullptr;   // kétargumentumú: f(u, v)
            // A derivált u (és v) szerint, a parser nyelvén. A láncszabályt a Hivas
            // csomópont végzi el: d f(a, b) = du(a, b)·a' + dv(a, b)·b'.
            char const* du = "0";
            char const* dv = "0";
            char const* help = "";

            int arity() const { return f2 ? 2 : 1; }
        };

        // Egy beépített függvény hívása: f(a) vagy f(a, b).
        struct Hivas : public Kifejezes {
        private:
            FuncDef const* def;
            Tree a, b;   // b csak kétargumentumúnál
        public:
            Hivas(FuncDef const* def, Tree a, Tree b = nullptr)
                : def(def), a(std::move(a)), b(std::move(b)) {}

            FuncDef const& get_def() const { return *def; }

            float at(glm::vec3 const v) const override {
                return b ? def->f2(a->at(v), b->at(v)) : def->f1(a->at(v));
            }

            // A deriváltakat a parser olvassa be a táblából, ezért a definíció a
            // matek/Parser.hpp végén van (a Kif.hpp mindig behúzza).
            Tree derive(Var const& var) const override;

            Tree simplify() const override {
                auto sa = a->simplify();
                auto sb = b ? b->simplify() : nullptr;
                auto ca = const_of(sa);
                auto cb = sb ? const_of(sb) : nullptr;
                if (ca && (!sb || cb))
                    return std::make_shared<Konstans>(sb ? def->f2(ca->get_value(), cb->get_value())
                                                         : def->f1(ca->get_value()));
                return std::make_shared<Hivas>(def, sa, sb);
            }

            bool same(Kifejezes const& o) const override {
                auto h = dynamic_cast<Hivas const*>(&o);
                return h && h->def == def && a->same(*h->a) &&
                       (b ? (h->b && b->same(*h->b)) : !h->b);
            }

            void print(std::ostream& os, ParamNamer const& namer) const override {
                os << def->name << '(';
                a->print(os, namer);
                if (b) { os << ", "; b->print(os, namer); }
                os << ')';
            }

            Tree substitute(SubstMap const& m) const override {
                return std::make_shared<Hivas>(def, a->substitute(m), b ? b->substitute(m) : nullptr);
            }

            int compile(Program& prog) const override {
                int sa = a->compile(prog);
                if (!b) return prog.emit(Op::Call1, sa, -1, 0.0f, nullptr, def->f1);
                int sb = b->compile(prog);
                return prog.emit(Op::Call2, sa, sb, 0.0f, nullptr, nullptr, def->f2);
            }
        };

    }
}

#endif //MATEK_FUGGVENYEK_FUGGVENY_HPP
