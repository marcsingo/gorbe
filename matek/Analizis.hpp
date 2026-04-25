#ifndef MATEK_FUGGVENY_HPP
#define MATEK_FUGGVENY_HPP

#include <functional>
#include <memory>
#include <glad/glad.h>
#include <iostream>

#include "vec3.hpp"

namespace Matek {
    namespace Analizis {



        struct Kifejezes {

            Kifejezes() = default;
            virtual float at(glm::vec3 const v) const = 0;
            float operator()(glm::vec3 const v) const { return at(v); }
            virtual std::shared_ptr<Kifejezes const> derrive(char var) const = 0;
            virtual std::shared_ptr<Kifejezes const> derrive(std::shared_ptr<Kifejezes const> var) const = 0;
            virtual std::shared_ptr<Kifejezes const> derrive(float const * var) const = 0;
            virtual void print(std::ostream& os) const = 0;
            virtual std::shared_ptr<Kifejezes const> simplify() const = 0;
            // virtual std::shared_ptr<Kifejezes> clone() const = 0;
            virtual ~Kifejezes() = default;
        };



        struct Konstans : public Kifejezes {
        private:
            float value;
        public:
            Konstans(float value) : value(value) {}
            float at(glm::vec3 const v) const override {
                return value;
            }

            float get_value() const { return value; }

            std::shared_ptr<Kifejezes const> derrive(char var) const override {
                return std::make_shared<Konstans>(0);
            }

            std::shared_ptr<Kifejezes const> derrive(std::shared_ptr<Kifejezes const> var) const override {
                return std::make_shared<Konstans>(0);
            }

            std::shared_ptr<Kifejezes const> derrive(float const * var) const override {
                return std::make_shared<Konstans>(0);
            }

            std::shared_ptr<Kifejezes const> simplify() const override {
                return std::make_shared<Konstans const>(value);
            }

            // std::shared_ptr<Kifejezes> clone() const override {
            //     return std::make_shared<Konstans>(value);
            // }

            void print(std::ostream& os) const override {
                os << value;
            }
        };


        struct KetOperandus : public Kifejezes {
        private:
            std::shared_ptr<Kifejezes const> bal;
            std::shared_ptr<Kifejezes const> jobb;
        protected:
            std::shared_ptr<Kifejezes const> get_bal() const { return bal; }
            std::shared_ptr<Kifejezes const> get_jobb() const { return jobb; }
            virtual char const get_operator() const = 0;
        public:
            KetOperandus(std::shared_ptr<Kifejezes const> bal,
                std::shared_ptr<Kifejezes const> jobb):
                bal(std::move(bal)),
                jobb(std::move(jobb)) {}

            void print(std::ostream& os) const override {
                os << '(';
                get_bal()->print(os);
                os << ' ' << get_operator() << ' ';
                get_jobb()->print(os);
                os << ')';
            }


        };


        struct Osszeg : public KetOperandus {

            explicit Osszeg(std::shared_ptr<Kifejezes const> bal, std::shared_ptr<Kifejezes const> jobb)
                : KetOperandus(std::move(bal), std::move(jobb)) {}

            float at(glm::vec3 const v) const override {
                return get_bal()->at(v) + get_jobb()->at(v);
            }



            std::shared_ptr<Kifejezes const> simplify() const override {
                std::shared_ptr<Kifejezes const> bal = get_bal()->simplify();
                std::shared_ptr<Kifejezes const> jobb = get_jobb()->simplify();
                if (auto bal_k = std::dynamic_pointer_cast<Konstans const>(bal)) {
                    if (bal_k->get_value() == 0) {
                        return jobb;
                    }
                }
                if (auto jobb_k = std::dynamic_pointer_cast<Konstans const>(jobb)) {
                    if (jobb_k->get_value() == 0) {
                        return bal;
                    }
                }
                return std::make_shared<Osszeg>(bal, jobb);
            }

            std::shared_ptr<Kifejezes const> derrive(char var) const override {
                return std::make_shared<Osszeg>(
                    get_bal()->derrive(var),
                    get_jobb()->derrive(var)
                    );
            }

            std::shared_ptr<Kifejezes const> derrive(float const * var) const override {
                return std::make_shared<Osszeg>(
                    get_bal()->derrive(var),
                    get_jobb()->derrive(var)
                    );
            }

            std::shared_ptr<Kifejezes const> derrive(std::shared_ptr<Kifejezes const> var) const override {
                return std::make_shared<Osszeg>(
                    get_bal()->derrive(var),
                    get_jobb()->derrive(var)
                    );
            }

        protected:
            char const get_operator() const override {
                return '+';
            }

        };

        struct Kulonbseg : public KetOperandus {
            explicit Kulonbseg(std::shared_ptr<Kifejezes const> bal, std::shared_ptr<Kifejezes const> jobb)
                    : KetOperandus(std::move(bal), std::move(jobb)) {}

            float at(glm::vec3 const v) const override {
                return get_bal()->at(v) - get_jobb()->at(v);
            }

            std::shared_ptr<Kifejezes const> simplify() const override {
                auto bal = get_bal()->simplify();
                auto jobb = get_jobb()->simplify();

                if (bal == jobb) return std::make_shared<Konstans>(0.0f);

                auto jobb_k = std::dynamic_pointer_cast<Konstans const>(jobb);
                if (jobb_k && jobb_k->get_value() == 0.0f) return bal; // x - 0 = x

                return std::make_shared<Kulonbseg>(bal, jobb);
            }

            std::shared_ptr<Kifejezes const> derrive(char var) const override {
                return std::make_shared<Kulonbseg>(
                        get_bal()->derrive(var),
                        get_jobb()->derrive(var)
                );
            }

            std::shared_ptr<Kifejezes const> derrive(float const * var) const override {
                return std::make_shared<Kulonbseg>(
                        get_bal()->derrive(var),
                        get_jobb()->derrive(var)
                );
            }

            std::shared_ptr<Kifejezes const> derrive(std::shared_ptr<Kifejezes const> var) const override {
                return std::make_shared<Kulonbseg>(
                        get_bal()->derrive(var),
                        get_jobb()->derrive(var)
                );
            }
        protected:
            char const get_operator() const override {
                return '-';
            }
        };

        struct Szorzat : public KetOperandus {
            explicit Szorzat(std::shared_ptr<Kifejezes const> bal, std::shared_ptr<Kifejezes const> jobb)
                    : KetOperandus(std::move(bal), std::move(jobb)) {}

            float at(glm::vec3 const v) const override {
                return get_bal()->at(v) * get_jobb()->at(v);
            }

            std::shared_ptr<Kifejezes const> simplify() const override {
                auto bal = get_bal()->simplify();
                auto jobb = get_jobb()->simplify();

                auto bal_k = std::dynamic_pointer_cast<Konstans const>(bal);
                auto jobb_k = std::dynamic_pointer_cast<Konstans const>(jobb);

                // x * 0 = 0 vagy 0 * x = 0
                if ((bal_k && bal_k->get_value() == 0.0f) ||
                    (jobb_k && jobb_k->get_value() == 0.0f)) {
                    return std::make_shared<Konstans>(0.0f);
                    }

                if (bal_k && bal_k->get_value() == 1.0f) return jobb; // 1 * x = x
                if (jobb_k && jobb_k->get_value() == 1.0f) return bal; // x * 1 = x

                return std::make_shared<Szorzat>(bal, jobb);
            }

            std::shared_ptr<Kifejezes const> derrive(char var) const override {
                return std::make_shared<Osszeg>(
                        std::make_shared<Szorzat>(get_bal()->derrive(var), get_jobb()),
                        std::make_shared<Szorzat>(get_bal(), get_jobb()->derrive(var))
                );
            }

            std::shared_ptr<Kifejezes const> derrive(float const * var) const override {
                return std::make_shared<Osszeg>(
                        std::make_shared<Szorzat>(get_bal()->derrive(var), get_jobb()),
                        std::make_shared<Szorzat>(get_bal(), get_jobb()->derrive(var))
                );
            }

            std::shared_ptr<Kifejezes const> derrive(std::shared_ptr<Kifejezes const> var) const override {
                return std::make_shared<Osszeg>(
                        std::make_shared<Szorzat>(get_bal()->derrive(var), get_jobb()),
                        std::make_shared<Szorzat>(get_bal(), get_jobb()->derrive(var))
                );
            }

        protected:
            char const get_operator() const override {
                return '*';
            }
        };

        struct Hanyados : public KetOperandus {
            explicit Hanyados(std::shared_ptr<Kifejezes const> bal, std::shared_ptr<Kifejezes const> jobb)
                    : KetOperandus(std::move(bal), std::move(jobb)) {}

            float at(glm::vec3 const v) const override {
                return get_bal()->at(v) / get_jobb()->at(v);
            }


            std::shared_ptr<Kifejezes const> simplify() const override {
                auto bal = get_bal()->simplify();
                auto jobb = get_jobb()->simplify();

                if (bal == jobb) return std::make_shared<Konstans>(1.0f); // x / x = 1

                auto bal_k = std::dynamic_pointer_cast<Konstans const>(bal);
                auto jobb_k = std::dynamic_pointer_cast<Konstans const>(jobb);

                if (bal_k && bal_k->get_value() == 0.0f) return std::make_shared<Konstans>(0.0f); // 0 / x = 0
                if (jobb_k && jobb_k->get_value() == 1.0f) return bal; // x / 1 = x

                return std::make_shared<Hanyados>(bal, jobb);
            }

            std::shared_ptr<Kifejezes const> derrive(char var) const override {
                return std::make_shared<Hanyados>(
                    std::make_shared<Kulonbseg>(
                        std::make_shared<Szorzat>(get_bal()->derrive(var), get_jobb()),
                        std::make_shared<Szorzat>(get_bal(), get_jobb()->derrive(var))
                    ),
                    std::make_shared<Szorzat>(get_jobb(), get_jobb())
                );
            }

            std::shared_ptr<Kifejezes const> derrive(float const * var) const override {
                return std::make_shared<Hanyados>(
                    std::make_shared<Kulonbseg>(
                        std::make_shared<Szorzat>(get_bal()->derrive(var), get_jobb()),
                        std::make_shared<Szorzat>(get_bal(), get_jobb()->derrive(var))
                    ),
                    std::make_shared<Szorzat>(get_jobb(), get_jobb())
                );
            }

            std::shared_ptr<Kifejezes const> derrive(std::shared_ptr<Kifejezes const> var) const override {
                return std::make_shared<Hanyados>(
                    std::make_shared<Kulonbseg>(
                        std::make_shared<Szorzat>(get_bal()->derrive(var), get_jobb()),
                        std::make_shared<Szorzat>(get_bal(), get_jobb()->derrive(var))
                    ),
                    std::make_shared<Szorzat>(get_jobb(), get_jobb())
                );
            }

        protected:
            char const get_operator() const override {
                return '/';
            }
        };




        struct Valtozo : public Kifejezes {
        private:
            char const var;
        public:
            Valtozo(char const var) : var(var) {}

            float at(glm::vec3 const v) const override {
                if (var == 'x') return v.x;
                if (var == 'y') return v.y;
                if (var == 'z') return v.z;
                return 0.0f;
            }

            std::shared_ptr<Kifejezes const> derrive(char var) const override {
                return std::make_shared<Konstans>(this->var == var ? 1 : 0);
            }

            std::shared_ptr<Kifejezes const> derrive(float const * var) const override {
                return std::make_shared<Konstans>(0);
            }

            std::shared_ptr<Kifejezes const> derrive(std::shared_ptr<Kifejezes const> var) const override {
                if (this == var.get()) return std::make_shared<Konstans const>(1);
                return std::make_shared<Konstans>(0);
            }

            void print(std::ostream &os) const override {
                os << var;
            }

            std::shared_ptr<Kifejezes const> simplify() const override {
                return std::make_shared<Valtozo>(var);
            }

        };

        struct Fuggveny : public Kifejezes {

        private:
            std::function<float(float)> const f;
        protected:
            std::shared_ptr<Kifejezes const> kif;
            virtual std::shared_ptr<Kifejezes const> get_fd() const = 0;
            virtual char const * get_name() const = 0;
        public:

            Fuggveny(
                std::function<float(float)> const f,
                std::shared_ptr<Kifejezes const> kif
                )
                : f(f), kif(std::move(kif)){}

            float at(glm::vec3 const v) const override {
                return f(kif->at(v));
            }

            std::shared_ptr<Kifejezes const> derrive(char var) const override {
                return std::make_shared<Szorzat>(
                    get_fd(),
                    kif->derrive(var)
                );
            }

            std::shared_ptr<Kifejezes const> derrive(float const * var) const override {
                return std::make_shared<Szorzat>(
                    get_fd(),
                    kif->derrive(var)
                );
            }

            std::shared_ptr<Kifejezes const> derrive(std::shared_ptr<Kifejezes const> var) const override {
                return std::make_shared<Szorzat>(
                    get_fd(),
                    kif->derrive(var)
                );
            }


            void print(std::ostream &os) const override {
                os << get_name() << '(';
                kif->print(os);
                os << ')';
            }

        };

        struct Ln : public Fuggveny {


        protected:
            std::shared_ptr<Kifejezes const> get_fd() const override {
                return std::make_shared<Hanyados>(
                    std::make_shared<Konstans>(1),
                    kif
                );
            }

            const char * get_name() const override {
                return "(1/log(2.271))*log";
            }

        public:
            explicit Ln(std::shared_ptr<Kifejezes const> kif)
                : Fuggveny([](float x) { return std::log(x); }, std::move(kif)) {}
            std::shared_ptr<Kifejezes const> simplify() const override {
                auto k = kif->simplify();
                auto kk = std::dynamic_pointer_cast<Konstans const>(k);

                if (kk && kk->get_value() == 1.0f) return std::make_shared<Konstans>(0.0f); // ln(1) = 0

                return std::make_shared<Ln>(k);
            }
        };

        inline std::shared_ptr<Kifejezes const> ln(std::shared_ptr<Kifejezes const> kif) {
            return std::make_shared<Ln>(std::move(kif));
        }

        struct Log : public Fuggveny {
        protected:
            std::shared_ptr<Kifejezes const> get_fd() const override {
                // log10'(x) = 1 / (x * ln(10))
                return std::make_shared<Hanyados>(
                    std::make_shared<Konstans>(1.0f),
                    std::make_shared<Szorzat>(
                        kif,
                        std::make_shared<Konstans>(std::log(10.0f))
                    )
                );
            }

            const char * get_name() const override {
                return "(1/log(10))*log";
            }

        public:
            explicit Log(std::shared_ptr<Kifejezes const> kif)
                : Fuggveny([](float x) { return std::log10(x); }, std::move(kif)) {}

            std::shared_ptr<Kifejezes const> simplify() const override {
                auto k = kif->simplify();
                auto kk = std::dynamic_pointer_cast<Konstans const>(k);

                if (kk && kk->get_value() == 1.0f) return std::make_shared<Konstans>(0.0f); // log(1) = 0

                return std::make_shared<Log>(k);
            }
        };

        inline std::shared_ptr<Kifejezes const> log(std::shared_ptr<Kifejezes const> kif) {
            return std::make_shared<Log>(std::move(kif));
        }

        struct Cos;

        struct Sin : public Fuggveny {
        protected:
            std::shared_ptr<Kifejezes const> get_fd() const override;

            const char * get_name() const override {
                return "sin";
            }

        public:
            explicit Sin(std::shared_ptr<Kifejezes const> kif)
                : Fuggveny([](float x) { return std::sin(x); }, std::move(kif)) {}

            std::shared_ptr<Kifejezes const> simplify() const override {
                auto k = kif->simplify();
                auto kk = std::dynamic_pointer_cast<Konstans const>(k);

                if (kk && kk->get_value() == 0.0f) return std::make_shared<Konstans>(0.0f); // sin(0) = 0

                return std::make_shared<Sin>(k);
            }
        };

        inline std::shared_ptr<Kifejezes const> sin(std::shared_ptr<Kifejezes const> kif) {
            return std::make_shared<Sin>(std::move(kif));
        }

        struct Cos : public Fuggveny {
        protected:
            std::shared_ptr<Kifejezes const> get_fd() const override;

            const char * get_name() const override {
                return "cos";
            }

        public:
            explicit Cos(std::shared_ptr<Kifejezes const> kif)
                : Fuggveny([](float x) { return std::cos(x); }, std::move(kif)) {}

            std::shared_ptr<Kifejezes const> simplify() const override {
                auto k = kif->simplify();
                auto kk = std::dynamic_pointer_cast<Konstans const>(k);

                if (kk && kk->get_value() == 0.0f) return std::make_shared<Konstans>(1.0f); // cos(0) = 1

                return std::make_shared<Cos>(k);
            }
        };

        inline std::shared_ptr<Kifejezes const> Sin::get_fd() const {
            return std::make_shared<Cos>(kif);
        }

        inline std::shared_ptr<Kifejezes const> Cos::get_fd() const {
            // cos'(x) = -1 * sin(x)
            return std::make_shared<Szorzat>(
                std::make_shared<Konstans>(-1.0f),
                std::make_shared<Sin>(kif)
            );
        }

        inline std::shared_ptr<Kifejezes const> cos(std::shared_ptr<Kifejezes const> kif) {
            return std::make_shared<Cos>(std::move(kif));
        }



        struct Hatvany : public KetOperandus {
        protected:
            const char get_operator() const override {
                return '^';
            }

        public:
            explicit Hatvany(std::shared_ptr<Kifejezes const> bal, std::shared_ptr<Kifejezes const> jobb):
                    KetOperandus(std::move(bal), std::move(jobb)) {}

            float at(glm::vec3 const v) const override {
                return powf(get_bal()->at(v), get_jobb()->at(v));
            }

            void print(std::ostream &os) const override {
                os << "pow(";
                get_bal()->print(os);
                os << ", ";
                get_jobb()->print(os);
                os << ')';
            }

            std::shared_ptr<Kifejezes const> derrive(char var) const override {
                return std::make_shared<Szorzat>(
                    std::make_shared<Hatvany>(get_bal(), get_jobb()),
                    std::make_shared<Osszeg>(
                        std::make_shared<Szorzat>(
                            get_jobb()->derrive(var),
                             std::make_shared<Ln>(get_bal())
                        ),
                        std::make_shared< Szorzat> (
                            get_jobb(),
                            std::make_shared<Hanyados>(
                                get_bal()->derrive(var),
                                get_bal()
                            )
                        )
                    )
                );
            }

            std::shared_ptr<Kifejezes const> derrive(std::shared_ptr<Kifejezes const> var) const override {
                return std::make_shared<Szorzat>(
                    std::make_shared<Hatvany>(get_bal(), get_jobb()),
                    std::make_shared<Osszeg>(
                        std::make_shared<Szorzat>(
                            get_jobb()->derrive(var),
                             std::make_shared<Ln>(get_bal())
                        ),
                        std::make_shared< Szorzat> (
                            get_jobb(),
                            std::make_shared<Hanyados>(
                                get_bal()->derrive(var),
                                get_bal()
                            )
                        )
                    )
                );
            }

            std::shared_ptr<Kifejezes const> derrive(float const * var) const override {
                return std::make_shared<Szorzat>(
                    std::make_shared<Hatvany>(get_bal(), get_jobb()),
                    std::make_shared<Osszeg>(
                        std::make_shared<Szorzat>(
                            get_jobb()->derrive(var),
                             std::make_shared<Ln>(get_bal())
                        ),
                        std::make_shared< Szorzat> (
                            get_jobb(),
                            std::make_shared<Hanyados>(
                                get_bal()->derrive(var),
                                get_bal()
                            )
                        )
                    )
                );
            }

            std::shared_ptr<Kifejezes const> simplify() const override {
                auto bal = get_bal()->simplify();
                auto jobb = get_jobb()->simplify();

                auto bal_k = std::dynamic_pointer_cast<Konstans const>(bal);
                auto jobb_k = std::dynamic_pointer_cast<Konstans const>(jobb);

                if (jobb_k && jobb_k->get_value() == 0.0f) return std::make_shared<Konstans>(1.0f); // x^0 = 1
                if (jobb_k && jobb_k->get_value() == 1.0f) return bal; // x^1 = x
                if (bal_k && bal_k->get_value() == 0.0f) return std::make_shared<Konstans>(0.0f); // 0^x = 0
                if (bal_k && bal_k->get_value() == 1.0f) return std::make_shared<Konstans>(1.0f); // 1^x = 1

                return std::make_shared<Hatvany>(bal, jobb);
            }

        };

        struct Tan : public Fuggveny {
        protected:
            std::shared_ptr<Kifejezes const> get_fd() const override {
                return std::make_shared<Hanyados>(
                    std::make_shared<Konstans>(1.0f),
                    std::make_shared<Hatvany>(
                        std::make_shared<Cos>(kif),
                        std::make_shared<Konstans>(2.0f)
                    )
                );
            }

            const char * get_name() const override {
                return "tan";
            }

        public:
            explicit Tan(std::shared_ptr<Kifejezes const> kif)
                : Fuggveny([](float x) { return std::tan(x); }, std::move(kif)) {}

            std::shared_ptr<Kifejezes const> simplify() const override {
                auto k = kif->simplify();
                auto kk = std::dynamic_pointer_cast<Konstans const>(k);

                if (kk && kk->get_value() == 0.0f) return std::make_shared<Konstans>(0.0f); // tan(0) = 0

                return std::make_shared<Tan>(k);
            }
        };

        inline std::shared_ptr<Kifejezes const> tg(std::shared_ptr<Kifejezes const> kif) {
            return std::make_shared<Ln>(std::move(kif));
        }

        struct Ctg : public Fuggveny {
        protected:
            std::shared_ptr<Kifejezes const> get_fd() const override {
                // ctg'(x) = -1 / sin^2(x)
                return std::make_shared<Hanyados>(
                    std::make_shared<Konstans>(-1.0f),
                    std::make_shared<Hatvany>(
                        std::make_shared<Sin>(kif),
                        std::make_shared<Konstans>(2.0f)
                    )
                );
            }

            const char * get_name() const override {
                return "ctg";
            }

        public:
            explicit Ctg(std::shared_ptr<Kifejezes const> kif)
                : Fuggveny([](float x) { return 1.0f / std::tan(x); }, std::move(kif)) {}

            std::shared_ptr<Kifejezes const> simplify() const override {
                auto k = kif->simplify();
                return std::make_shared<Ctg>(k);
            }
        };

        struct Parameter : public Kifejezes {
            private:
            char const id;
            float const * ertek_ref;

            public:
            Parameter(float const * ref, char const id = 'p')
                : id(id), ertek_ref(std::move(ref)) {
                if (!ertek_ref) {
                    // Biztonsági ellenőrzés
                    throw std::invalid_argument("A parameter referenciaja nem lehet null!");
                }
            }

             float at(glm::vec3 const v) const override {
                return *ertek_ref;
            }

            std::shared_ptr<Kifejezes const> derrive(char var) const override {
                return std::make_shared<Konstans>(this->id == var ? 1.0f : 0.0f);
            }

            std::shared_ptr<Kifejezes const> derrive(float const * var) const override {
                if (ertek_ref == var) return std::make_shared<Konstans>(1);
                return std::make_shared<Konstans>(0);
            }

            std::shared_ptr<Kifejezes const> derrive(std::shared_ptr<Kifejezes const> var) const override {
                if (this == var.get()) return std::make_shared<Konstans>(1);
                return std::make_shared<Konstans>(0);
            }

            void print(std::ostream &os) const override {
                // Érdemes vizuálisan megkülönböztetni a normál változóktól
                os << "p_" << id;
            }

            std::shared_ptr<Kifejezes const> simplify() const override {
                return std::make_shared<Parameter>(ertek_ref, id);
            }


        };

        // inline std::shared_ptr<Kifejezes const> ctg(std::shared_ptr<Kifejezes const> kif) {
        //     return std::make_shared<Ctg>(std::move(kif));
        // }
        //
        // inline std::shared_ptr<Kifejezes const> operator^(std::shared_ptr<Kifejezes const> const & a, std::shared_ptr<Kifejezes const> const & b) {
        //     return std::make_shared<Hatvany>(a, b);
        // }
        //
        // inline std::shared_ptr<Kifejezes const> operator""_k(unsigned long long value) {
        //     return std::make_shared<Konstans>(static_cast<float>(value));
        // }
        //
        // inline std::shared_ptr<Kifejezes const> operator""_k(long double value) {
        //     return std::make_shared<Konstans>(static_cast<float>(value));
        // }
        //
        // inline std::shared_ptr<Kifejezes const> operator""_v(const char var) {
        //     return std::make_shared<Valtozo>(var);
        // }
        //
        // inline std::shared_ptr<Kifejezes const> operator+(std::shared_ptr<Kifejezes const> const & a, std::shared_ptr<Kifejezes const> const & b) {
        //     return std::make_shared<Osszeg>(a, b);
        // }
        //
        // inline std::shared_ptr<Kifejezes const> operator-(std::shared_ptr<Kifejezes const> const & a, std::shared_ptr<Kifejezes const> const & b) {
        //     return std::make_shared<Kulonbseg>(a, b);
        // }
        //
        // inline std::shared_ptr<Kifejezes const> operator*(std::shared_ptr<Kifejezes const> const & a, std::shared_ptr<Kifejezes const> const & b) {
        //     return std::make_shared<Szorzat>(a, b);
        // }
        //
        // inline std::shared_ptr<Kifejezes const> operator/(std::shared_ptr<Kifejezes const> const & a, std::shared_ptr<Kifejezes const> const & b) {
        //     return std::make_shared<Hanyados>(a, b);
        // }
        //
        // inline std::shared_ptr<Kifejezes const> konst(float v)
        // {
        //     return std::make_shared<Konstans>(v);
        // }
        //
        // inline std::shared_ptr<Kifejezes const>
        // operator+(const std::shared_ptr<Kifejezes const>& a, float b)
        // {
        //     return a + konst(b);
        // }
        //
        // inline std::shared_ptr<Kifejezes const>
        // operator-(const std::shared_ptr<Kifejezes const>& a, float b)
        // {
        //     return a - konst(b);
        // }
        //
        // inline std::shared_ptr<Kifejezes const>
        // operator*(const std::shared_ptr<Kifejezes const>& a, float b)
        // {
        //     return a * konst(b);
        // }
        //
        // inline std::shared_ptr<Kifejezes const>
        // operator/(const std::shared_ptr<Kifejezes const>& a, float b)
        // {
        //     return a / konst(b);
        // }
        //
        // inline std::shared_ptr<Kifejezes const>
        // operator+(float a, const std::shared_ptr<Kifejezes const>& b)
        // {
        //     return konst(a) + b;
        // }
        //
        // inline std::shared_ptr<Kifejezes const>
        // operator-(float a, const std::shared_ptr<Kifejezes const>& b)
        // {
        //     return konst(a) - b;
        // }
        //
        // inline std::shared_ptr<Kifejezes const>
        // operator*(float a, const std::shared_ptr<Kifejezes const>& b)
        // {
        //     return konst(a) * b;
        // }
        //
        // inline std::shared_ptr<Kifejezes const>
        // operator/(float a, const std::shared_ptr<Kifejezes const>& b)
        // {
        //     return konst(a) / b;
        // }


    }

    // namespace Valtozok {
    //     using namespace Matek::Analizis;
    //     typedef std::shared_ptr<Kifejezes const> Egyenlet;
    //     inline std::shared_ptr<Kifejezes const> const x = 'x'_v;
    //     inline std::shared_ptr<Kifejezes const> const y = 'y'_v;
    //     inline std::shared_ptr<Kifejezes const> const z = 'z'_v;
    // }
}

#endif //MATEK_FUGGVENY_HPP