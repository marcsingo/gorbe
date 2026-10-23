#ifndef MATEK_LEVELEK_KONSTANS_HPP
#define MATEK_LEVELEK_KONSTANS_HPP

#include <charconv>
#include <cmath>
#include <string>
#include <string_view>
#include "../Kifejezes.hpp"

namespace Matek {
    namespace Analizis {

        struct Konstans : public Kifejezes {
        private:
            float value;
        public:
            Konstans(float value) : value(value) {}

            float at(glm::vec3 const) const override { return value; }

            float get_value() const { return value; }

            Tree derive(Var const&) const override { return std::make_shared<Konstans>(0.0f); }

            Tree simplify() const override { return std::make_shared<Konstans const>(value); }

            bool same(Kifejezes const& o) const override {
                auto k = dynamic_cast<Konstans const*>(&o);
                return k && k->value == value;
            }

            // A legrövidebb alak, ami VISSZAOLVASVA bitre ugyanez a float (to_chars).
            // A negatív szám zárójelbe kerül: "(-3) ^ 2" nem azonos a "-3 ^ 2"-vel.
            // ponytail: inf/nan kiírva nem olvasható vissza — véges képletben nem fordul elő.
            void print(std::ostream& os, ParamNamer const&) const override {
                if (print_style == PrintStyle::Glsl && !std::isfinite(value)) {
                    // A GLSL-ben nincs inf/nan literál.
                    os << (std::isnan(value) ? "(0.0/0.0)" : value > 0.0f ? "(1.0/0.0)" : "(-1.0/0.0)");
                    return;
                }
                char buf[32];
                auto res = std::to_chars(buf, buf + sizeof(buf), value);
                std::string txt(buf, static_cast<std::size_t>(res.ptr - buf));
                // GLSL-ben a "2" EGÉSZ literál (az 1/3 egész osztás volna): mindig lebegőpontos.
                if (print_style == PrintStyle::Glsl && txt.find_first_of(".e") == std::string::npos)
                    txt += ".0";
                if (value < 0.0f) os << '(' << txt << ')';
                else              os << txt;
            }

            Tree substitute(SubstMap const&) const override { return std::make_shared<Konstans>(value); }

            int compile(Program& prog) const override {
                return prog.emit(Op::Const, -1, -1, value);
            }
        };

        // Konstans-e a fa, és ha igen, mennyi (az egyszerűsítők közös segédje).
        inline Konstans const* const_of(Tree const& t) {
            return dynamic_cast<Konstans const*>(t.get());
        }
        inline bool is_const(Tree const& t, float v) {
            auto k = const_of(t);
            return k && k->get_value() == v;
        }

    }
}

#endif //MATEK_LEVELEK_KONSTANS_HPP
