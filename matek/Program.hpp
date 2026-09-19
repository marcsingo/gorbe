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
        // A függvénytábla (Fuggvenyek.hpp) függvényei ezeken keresztül futnak: egy
        // új függvényhez így nem kell új műveletkód.
        using Fn1 = float (*)(float);
        using Fn2 = float (*)(float, float);

        enum class Op : std::uint8_t {
            Const, VarX, VarY, VarZ, Param,
            Add, Sub, Mul, Div, Pow,
            Call1, Call2
        };

        // Kicsi (24 bájt) marad: a műveletkódtól függően csak EGY adat kell hozzá,
        // ezért union. (Két külön függvénymutatóval 40 bájt lett, és mérve ~13%-kal
        // lassabb a futtatás a rosszabb gyorsítótár-kihasználás miatt.)
        struct Instr {
            Op  op;
            int a = -1;
            int b = -1;
            union {
                float        k;    // Const
                float const* p;    // Param
                Fn1          f1;   // Call1
                Fn2          f2;   // Call2
            };
        };

        static_assert(sizeof(Instr) <= 24, "az utasitas maradjon kicsi");

        class Program {
            struct Key {
                Op           op;
                int          a, b;
                float        k;
                float const* p;
                Fn1          f1;
                Fn2          f2;
                bool operator==(Key const& o) const {
                    return op == o.op && a == o.a && b == o.b && p == o.p && k == o.k &&
                           f1 == o.f1 && f2 == o.f2;
                }
            };
            struct KeyHash {
                std::size_t operator()(Key const& x) const {
                    std::size_t h = static_cast<std::size_t>(x.op);
                    h = h * 1000003u + static_cast<std::size_t>(x.a + 1);
                    h = h * 1000003u + static_cast<std::size_t>(x.b + 1);
                    h = h * 1000003u + std::hash<float>{}(x.k);
                    h = h * 1000003u + std::hash<void const*>{}(x.p);
                    h = h * 1000003u + reinterpret_cast<std::uintptr_t>(x.f1);
                    h = h * 1000003u + reinterpret_cast<std::uintptr_t>(x.f2);
                    return h;
                }
            };

            std::vector<Instr>                        code;
            std::unordered_map<Key, int, KeyHash>     seen;   // csak fordítás közben
            mutable std::vector<float>                slots;

            // Egy művelet az operandusaira (a levelek kivételével).
            static float apply(Instr const& c, float x, float y) {
                switch (c.op) {
                    case Op::Add:   return x + y;
                    case Op::Sub:   return x - y;
                    case Op::Mul:   return x * y;
                    case Op::Div:   return x / y;
                    case Op::Pow:   return std::pow(x, y);
                    case Op::Call1: return c.f1(x);
                    case Op::Call2: return c.f2(x, y);
                    default:        return 0.0f;
                }
            }

        public:
            // Egy utasítás kibocsátása. Ha ugyanez az utasítás már szerepel, a meglévő
            // slot indexét adja vissza (CSE) — ettől zsugorodnak össze a derivált-fák.
            // Ha minden operandusa konstans, helyben kiszámolja (konstans-összevonás).
            int emit(Op op, int a = -1, int b = -1, float k = 0.0f, float const* p = nullptr,
                     Fn1 f1 = nullptr, Fn2 f2 = nullptr) {
                Instr in{op, a, b};
                if      (op == Op::Param) in.p  = p;
                else if (op == Op::Call1) in.f1 = f1;
                else if (op == Op::Call2) in.f2 = f2;
                else                      in.k  = k;
                bool const leaf = op == Op::Const || op == Op::VarX || op == Op::VarY ||
                                  op == Op::VarZ  || op == Op::Param;
                auto is_const = [&](int i) { return i < 0 || code[i].op == Op::Const; };
                if (!leaf && is_const(a) && is_const(b)) {
                    float x = a >= 0 ? code[a].k : 0.0f;
                    float y = b >= 0 ? code[b].k : 0.0f;
                    return emit(Op::Const, -1, -1, apply(in, x, y));
                }

                Key key{op, a, b, k, p, f1, f2};
                auto it = seen.find(key);
                if (it != seen.end()) return it->second;
                code.push_back(in);
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
                // Nyers mutatók helyi változóban: a Call1/Call2 ismeretlen függvényt hív,
                // ezért a fordító a tagváltozókat minden hívás után újraolvasná (mérve:
                // ~12%-kal lassabb volt a tórusz programja, amiben nincs is hívás).
                Instr const* const code_ = code.data();
                float* const s = slots.data();
                std::size_t const n = code.size();
                for (std::size_t i = 0; i < n; ++i) {
                    Instr const& c = code_[i];
                    float r;
                    switch (c.op) {
                        case Op::Const: r = c.k;  break;
                        case Op::VarX:  r = v.x;  break;
                        case Op::VarY:  r = v.y;  break;
                        case Op::VarZ:  r = v.z;  break;
                        case Op::Param: r = *c.p; break;
                        case Op::Add:   r = s[c.a] + s[c.b]; break;
                        case Op::Sub:   r = s[c.a] - s[c.b]; break;
                        case Op::Mul:   r = s[c.a] * s[c.b]; break;
                        case Op::Div:   r = s[c.a] / s[c.b]; break;
                        case Op::Pow:   r = std::pow(s[c.a], s[c.b]); break;
                        case Op::Call1: r = c.f1(s[c.a]); break;
                        case Op::Call2: r = c.f2(s[c.a], s[c.b]); break;
                        default:        r = 0.0f; break;
                    }
                    s[i] = r;
                }
            }
        };

    }
}

#endif //MATEK_PROGRAM_HPP
