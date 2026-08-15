#ifndef MATEK_PROGRAM_HPP
#define MATEK_PROGRAM_HPP

#include <cmath>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

#include "vec3.hpp"

namespace Matek {
    namespace Analizis {

        // A kifejezésfa LAPOS, futtatható alakja.
        //
        // Miért kell? A fa kiértékelése csomópontonként virtuális hívás + pointer-ugrás,
        // és — ami sokkal rosszabb — a szimbolikus deriválás után a fák tele vannak
        // ISMÉTELT részkifejezésekkel. (Mérve: egy kétszintű CSG Hesse-mátrixának fái
        // összesen 21 477 karakter, a hengeré 6.) A simplify() ráadásul minden hívásnál
        // új csomópontokat épít, tehát a shared_ptr-es megosztás is elvész.
        //
        // A program topologikusan rendezett utasítások tömbje: minden utasítás egy
        // slotba ír, és korábbi slotokra hivatkozik. Az emit() ÉRTÉK-SZÁMOZÁSOS közös
        // részkifejezés-kiemelést (CSE) végez: ha ugyanaz a művelet ugyanazokkal az
        // operandus-slotokkal már szerepel, nem keletkezik új utasítás, hanem a meglévő
        // slot indexe jön vissza. Így a strukturálisan azonos részfák egyszer futnak le.
        //
        // Egy program több KIMENETET is adhat (F, a gradiens 3 eleme, a Hesse 6 eleme),
        // amik így mind osztoznak a közös részszámításokon — egyetlen lineáris menetben.
        //
        // FIGYELEM: a run() egy belső munkaterületre ír, ezért ugyanaz a Program
        // példány nem futtatható párhuzamosan több szálon.
        enum class Op : std::uint8_t {
            Const, VarX, VarY, VarZ, Param,
            Add, Sub, Mul, Div, Pow,
            Sin, Cos, Tan, Ctg, Ln, Log, Abs, Sign, Min, Max
        };

        struct Instr {
            Op           op;
            int          a = -1;
            int          b = -1;
            float        k = 0.0f;
            float const* p = nullptr;
        };

        class Program {
            struct Key {
                Op           op;
                int          a, b;
                float        k;
                float const* p;
                bool operator==(Key const& o) const {
                    return op == o.op && a == o.a && b == o.b && p == o.p && k == o.k;
                }
            };
            struct KeyHash {
                std::size_t operator()(Key const& x) const {
                    std::size_t h = static_cast<std::size_t>(x.op);
                    h = h * 1000003u + static_cast<std::size_t>(x.a + 1);
                    h = h * 1000003u + static_cast<std::size_t>(x.b + 1);
                    h = h * 1000003u + std::hash<float>{}(x.k);
                    h = h * 1000003u + std::hash<void const*>{}(x.p);
                    return h;
                }
            };

            std::vector<Instr>                        code;
            std::unordered_map<Key, int, KeyHash>     seen;   // csak fordítás közben
            mutable std::vector<float>                slots;

        public:
            // Egy utasítás kibocsátása. Ha ugyanez az utasítás már szerepel, a meglévő
            // slot indexét adja vissza (CSE) — ettől zsugorodnak össze a derivált-fák.
            int emit(Op op, int a = -1, int b = -1, float k = 0.0f, float const* p = nullptr) {
                Key key{op, a, b, k, p};
                auto it = seen.find(key);
                if (it != seen.end()) return it->second;
                code.push_back(Instr{op, a, b, k, p});
                int idx = static_cast<int>(code.size()) - 1;
                seen.emplace(key, idx);
                return idx;
            }

            // Fordítás vége: a CSE-tábla eldobható, a munkaterület megkapja a méretét.
            void finish() {
                seen.clear();
                seen.rehash(0);
                slots.assign(code.size(), 0.0f);
            }

            std::size_t size() const { return code.size(); }
            float slot(int i) const { return slots[static_cast<std::size_t>(i)]; }

            void run(glm::vec3 v) const {
                std::size_t const n = code.size();
                for (std::size_t i = 0; i < n; ++i) {
                    Instr const& c = code[i];
                    float r;
                    switch (c.op) {
                        case Op::Const: r = c.k;  break;
                        case Op::VarX:  r = v.x;  break;
                        case Op::VarY:  r = v.y;  break;
                        case Op::VarZ:  r = v.z;  break;
                        case Op::Param: r = *c.p; break;
                        case Op::Add:   r = slots[c.a] + slots[c.b]; break;
                        case Op::Sub:   r = slots[c.a] - slots[c.b]; break;
                        case Op::Mul:   r = slots[c.a] * slots[c.b]; break;
                        case Op::Div:   r = slots[c.a] / slots[c.b]; break;
                        case Op::Pow:   r = std::pow(slots[c.a], slots[c.b]); break;
                        case Op::Sin:   r = std::sin(slots[c.a]); break;
                        case Op::Cos:   r = std::cos(slots[c.a]); break;
                        case Op::Tan:   r = std::tan(slots[c.a]); break;
                        case Op::Ctg:   r = 1.0f / std::tan(slots[c.a]); break;
                        case Op::Ln:    r = std::log(slots[c.a]); break;
                        case Op::Log:   r = std::log10(slots[c.a]); break;
                        case Op::Abs:   r = std::abs(slots[c.a]); break;
                        case Op::Sign: {
                            float x = slots[c.a];
                            r = x > 0.0f ? 1.0f : (x < 0.0f ? -1.0f : 0.0f);
                            break;
                        }
                        case Op::Min:   r = std::min(slots[c.a], slots[c.b]); break;
                        case Op::Max:   r = std::max(slots[c.a], slots[c.b]); break;
                        default:        r = 0.0f; break;
                    }
                    slots[i] = r;
                }
            }
        };

    }
}

#endif //MATEK_PROGRAM_HPP
