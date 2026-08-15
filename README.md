# Implicit felület mintavételezés részecskékkel

Witkin–Heckbert *„Using Particles to Sample and Control Implicit Surfaces"* (SIGGRAPH '94)
alapján: egy implicit felületet (`F(x,y,z)=0`) szabadon mozgó részecskékkel mintavételezünk,
amelyek lokális taszítással egyenletesen szétterülnek, és a sűrűségtől függően
osztódnak (fisszió) vagy elhalnak.

## Fordítás és futtatás

CMake + egy C++20 fordító kell. A `libraries/` mappa tartalmazza a GLFW/GLM/GLAD-ot.

> **Windows / MSVC:** a fordítóhoz be kell tölteni a Visual Studio környezetét,
> különben a linker nem találja a Windows SDK `.lib`-jeit
> (`LNK1104: cannot open file 'kernel32.lib'`). A legegyszerűbb a Start menüből az
> **„x64 Native Tools Command Prompt for VS 2022"**-t indítani, és abban futtatni az
> alábbi parancsokat. (A CLion ezt automatikusan beállítja, ott elég a szokásos Build/Run.)

```bash
cmake -S . -B build
cmake --build build --target gorbe
./build/gorbe                      # Windows: .\build\gorbe.exe
```

> **Optimalizált buildet használj.** A szimuláció forró útja sok apró függvényből áll;
> `Debug`-ban mérve **5.8–7.5×** lassabb. Ha nem adsz build típust, a CMake magától
> `RelWithDebInfo`-t választ. **CLion-ban ez nem érvényes** — ott a profil dönt, tehát
> a *Settings → Build → CMake* alatt vegyél fel egy `RelWithDebInfo` profilt, és
> mérésnél/használatnál azt futtasd.

A build a shadereket a bináris mellé másolja, és a program onnan tölti, így
**tetszőleges munkakönyvtárból indítható**.

### Tesztek

A `matek/` réteg (parser, szimbolikus deriválás, kifejezés-fordító) és a részecske-
kényszerek GL nélkül, fejlécből tesztelhetők:

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

| teszt | mit fed le |
|---|---|
| `test_shapes` | a beépített alakzatok képletei: `F = 0` a felületen, gradiens, precedencia |
| `test_csg` | halmazműveletek értéke és deriváltja, szimbolikus vs. numerikus gradiens, hibás hívások |
| `test_domain` | a feltétel-operátorok és a tartomány-kényszer (becsúszás, perem-fal) időléptetéssel |
| `test_program` | a lefordított program **bitre azonos** a fabejárással; a rács szomszédai azonosak a nyers párbejáráséval |

Kikapcsolható: `-DGORBE_BUILD_TESTS=OFF`.

Az irányítás részletei lentebb: [Irányítás](#irányítás).

## Irányítás

A képernyőn háromféle elem látszik: a **szürke** referencia-felület (occluder), a felületet
mintavételező **lebegő részecskék** (kék korongok), és a **piros kontrollpontok**, amelyekkel
a felületet lehet irányítani.

Kétféle dolgot lehet vezérelni: a **nézőpontot** (kamera) és magát a **felületet**
(kontrollpontokon keresztül).

### Kamera (nézőpont)

| Bevitel | Hatás |
|---|---|
| jobb egérgomb + húzás | nézet forgatása |
| `Alt` + bal egérgomb + húzás | nézet forgatása (alternatíva) |
| `W` / `S` | kamera előre / hátra |
| `A` / `D` | kamera balra / jobbra |
| egérgörgő | zoom (látószög 1°–45° között) |
| `Esc` | kilépés |

### Felület irányítása (kontrollpontok)

A kontrollpontok a felületre tett kényszerek: ha egy pontot megmozgatsz, a rendszer úgy
mozgatja/deformálja a felületet (a `q` paramétereit), hogy a pont a felületen maradjon
(ez a cikk szerinti constraint-megoldás). Így a felületet közvetlenül, „kézzel" lehet húzni.

| Bevitel | Hatás |
|---|---|
| `Shift` + bal kattintás | új kontrollpont lerakása a kurzor helyén |
| bal kattintás egy ponton + húzás | a pont mozgatása → a felület követi (deformálódik / mozog) |
| bal gomb elengedése | a pont elengedése |

Megjegyzések:
- A bal egérgomb `Alt` **nélkül** a kontrollpontoké, `Alt`-tal a kameráé — így nem ütköznek.
- A kontrollpont a kamera nézőirányára merőleges síkban mozog (a mélységet a nézet
  forgatásával lehet beállítani).
- Egy ponttól `0.5` egységnél közelebbi kattintás számít megfogásnak.

## Alakzatok szerkesztése (GUI)

A jelenetet futás közben, három ImGui-panelen lehet összerakni. Minden alakzat
**önálló** implicit felület, saját részecskékkel mintavételezve.

| ablak | mi van benne |
|---|---|
| **Alakzatok** | felül `Új alakzat` (üres) és a **sablon-lenyíló** + `Hozzáad`, alatta a jelenet alakzatainak listája: láthatóság-pipa, név (kattintásra kijelöl), `X` a törléshez. Legalul `Indít` / `Töröl`, valamint a közös `d` (méretskála) és görbület-taszítás csúszka. |
| **Tulajdonságok** | a kijelölt alakzat neve, `F(x,y,z) =` képlete, opcionális **tartomány-feltétele** és a **lokális** paraméterei (név = érték). |
| **Globális paraméterek** | minden alakzat által látott paraméterek (név = érték). |
| **Nézet és súgó** | jelmagyarázat (melyik szín mit jelent), rács ki/be, nézet-előbeállítások és a teljes irányítás — a program használata közben végig látható. |

### Tájékozódás a térben

A jelenetben a **z a függőleges** (a sík-sablon `z = 0`, a henger a z tengely mentén
áll), és a kamera is ehhez igazodik: forgatáskor a horizont vízszintes marad.

- **Talajrács** a `z = 0` síkban, 1 egység osztással; minden 5. vonal hangsúlyos.
- **Tengelyek**: piros = x, zöld = y, kék = z. A pozitív fél telített, a negatív
  halványabb, a távoli szakaszok pedig a háttérbe fakulnak — így hosszan is
  mutatják az irányt anélkül, hogy elnyomnák a modellt.
- Nyílhegy és `X` / `Y` / `Z` felirat a pozitív végeken, egész értékeknél osztások.
- A tengelyek vastagsága, a nyílhegyek és a feliratok **állandó képernyő-méretűek**,
  tehát zoomtól függetlenül ugyanúgy néznek ki. (Nem `glLineWidth`-tel: a core
  profil csak az 1.0 vastagságot garantálja, a többit a driver elnyelheti.)
- A **Nézet** szakasz gombjai: felülnézet, 3/4 nézet, oldalról, elölről, alapnézet,
  plusz egy távolság-csúszka. A nézetváltás megtartja az origótól mért távolságot.

A képlet beírása után az **Indít** parseolja az összes alakzatot és újraindítja a
mintavételezést. Változók: `x`, `y`, `z`; a `^` precedenciája a szokásos (nem kell
zárójelezni). Függvények:

| kategória | függvények |
|---|---|
| egyváltozós | `sin cos tan`/`tg` `ctg`/`cot` `ln log sqrt abs sign` |
| éles halmazműveletek | `unio(a,b)` `metszet(a,b)` `kulonbseg(a,b)` `min(a,b)` `max(a,b)` |
| sima halmazműveletek | `sunio(a,b[,k])` `smetszet(a,b[,k])` `skulonbseg(a,b[,k])` `smin` `smax` |

Az angol nevek is működnek: `union`, `intersect`, `subtract`, `sunion`, `sintersect`,
`ssubtract`. A sima műveleteknél a `k` a lekerekítés mértéke (elhagyva `0.5`).

### Tartomány-feltétel — végtelen alakzatok véges darabon

Egy `z` képlet végtelen síkot jelent, egy `x^2+y^2-1` végtelen hengert. Ezeket úgy
lehet véges darabon megmutatni, hogy az alakzathoz megadsz egy **tartomány-feltételt**
a Tulajdonságok panelen — a részecskék csak ott élnek, ahol az teljesül:

```
F(x,y,z) =   z
Tartomany:   x > -3 and x < 3 and y > -3 and y < 3
```

| operátor | jelentés |
|---|---|
| `>` `<` `>=` `<=` | összehasonlítás |
| `and` `or` `not` (`&&` `\|\|` `!`) | logikai műveletek |

**Miért nem `metszet`?** Mert a CSG *testeken* dolgozik: a `metszet(z, 2-x)` nullhalmaza
nem csak a levágott sík, hanem az `x=2` vágólap `z<0`-ba eső darabja is — vagyis egy éket
kapsz, nem egy síkdarabot. A tartomány-feltétel viszont **nyers szélű felületdarabot**
ad, fedőlapok nélkül. Ráadásul jóval olcsóbb: mérve egy téglalapra vágott sík 4 egymásba
ágyazott `metszet`-tel **4.99 µs/részecske**, ugyanaz tartomány-feltétellel **0.149 µs**
(33×) — mert a feltételhez nem kell a teljes derivált-készlet.

Használd a `metszet`-et, ha **zárt testet** modellezel (kell a fedőlap), és a
tartomány-feltételt, ha egy felület **egy darabját** akarod megmutatni.

#### Hogyan viselkednek a részecskék a peremen

A feltétel nem egyszerűen kiöli a rossz helyre került részecskéket — a Witkin-algoritmus
egy **második kényszert** kap, ugyanazzal a szerkezettel, ahogy az első a felületen tartja
a részecskét (lásd `particle_sampling/DomainConstraint.hpp`):

- A rossz térrészbe került részecske **a felület mentén csúszik** a jó térrész felé. Az
  irány a feltétel gradiensének érintőirányú része, `g = ∇dom − (∇dom·∇F/|∇F|²)·∇F`;
  mivel `g ⊥ ∇F`, a csúszás közben a részecske a felületen marad.
- A peremnél **fal** van: a rendszer előre kiszámolja, hova vinné a lépés, és ha átlépné
  a peremet, pontosan annyit vesz ki a kifelé mutató sebességből, hogy a peremen álljon
  meg. A tartomány belsejében semmi nem korlátozza a mozgást.
- Amíg egy részecske kívül van (és csúszik befelé), nem rajzoljuk ki, és nem is
  osztódik/hal meg — különben a peremen folyamatos „churn" alakulna ki.

A küszöbök **geometriai távolságban** vannak (`dom/|g|`, Taubin-közelítés), ezért az
`x > 2` és a `100*x > 200` pontosan ugyanúgy viselkedik.

### Halmazműveletek (CSG)

A [BlobTree-cikk](#hivatkozasok) (Wyvill–Guy–Galin, 1999) alapján. **Figyelem az
előjel-konvencióra:** itt `F < 0` van *belül*, a cikkben viszont a potenciál belül
*nagy* — ezért nálunk az **unió `min`** és a **metszet `max`**, a cikkhez képest
fordítva.

| művelet | képlet | folytonosság |
|---|---|---|
| `unio(a,b)` | `min(a, b)` | C⁰ — a varraton törés |
| `metszet(a,b)` | `max(a, b)` | C⁰ |
| `kulonbseg(a,b)` | `max(a, −b)` | C⁰ |
| `sunio(a,b,k)` | `½(a + b − √((a−b)² + k²))` | C^∞ |
| `smetszet(a,b,k)` | `½(a + b + √((a−b)² + k²))` | C^∞ |
| `skulonbseg(a,b,k)` | `smetszet(a, −b, k)` | C^∞ |

Melyiket használd? A részecske-szimuláció a **gradienst** használja a felületre
vetítéshez, a **második** deriváltakat pedig a görbület-adaptív taszításhoz. Az éles
műveleteknél a varraton a gradiens ugrik, a Hesse-mátrix pedig értelmetlen — ezért ott
a részecskék remegnek. **Alapesetben a sima változatokat érdemes használni**; az éles
műveletek akkor jók, ha tényleg éles élt akarsz, és elfogadod a varrat körüli zajt.

A sima műveletek `k`-ja az `F` *értékének* nagyságrendjében van (nem hosszban), ezért
alakzatonként hangolni kell — például a tórusz kvartikus `F`-je sokkal nagyobb
értékeket vesz fel, mint egy gömb `x²+y²+z²−r²`-e.

> A cikk 3.2-es R-függvény alakja (`a + b ± √(a²+b²)`, `k` nélkül) **nem** használható
> itt: a gyök argumentuma pont a varraton 0, és mivel a `sqrt(u)` a parserben `u^0.5`,
> a deriváltja ott végtelen — a keletkező NaN a taszításon keresztül az összes
> szomszédos részecskére átterjedne. Ugyanígy nem használható a 3.3-as szuperelliptikus
> blend (`(aⁿ + bⁿ)^(1/n)`) sem: előjeles `F`-nél az alap belül negatív, törtkitevővel
> NaN. Mindkettőt mérés igazolta.

### Kész alakzatok (sablonok)

Az `Alakzatok` panel lenyíló listájából egy kattintással felvehető egy kész alakzat
— a képlete és a lokális paraméterei is bemásolódnak, onnantól szabadon szerkeszthető.
A lenyíló fölé húzva a kurzort megjelenik a képlet.

| sablon | képlet | paraméterek |
|---|---|---|
| Gömb | `x^2 + y^2 + z^2 - r^2` | `r` |
| Ellipszoid | `x^2/a^2 + y^2/b^2 + z^2/c^2 - 1` | `a`, `b`, `c` |
| Tórusz | `(x^2+y^2+z^2+R^2-r^2)^2 - 4*R^2*(x^2+y^2)` | `R`, `r` |
| Ellipszis (ell. henger) | `x^2/a^2 + y^2/b^2 - 1` | `a`, `b` |
| Henger | `x^2 + y^2 - r^2` | `r` |
| Kúp | `x^2 + y^2 - a^2*z^2` | `a` |
| Hiperboloid (1 köpeny) | `x^2/a^2 + y^2/b^2 - z^2/c^2 - 1` | `a`, `b`, `c` |
| Lekerekített kocka | `x^4 + y^4 + z^4 - a^4` | `a` |
| Sík (négyzet darab) | `z` + tartomány `-m < x < m`, `-m < y < m` | `m` |
| Henger (véges hosszú) | `x^2 + y^2 - r^2` + tartomány `-h < z < h` | `r`, `h` |
| *Éles:* Unió / Metszet / Különbség | `unio(f1, f2)` stb. | — |
| *Sima:* Unió / Metszet / Különbség | `sunio(f1, f2, k)` stb. | `k` |

Megjegyzések:

- A **tórusz** algebrai (négyzetgyök nélküli) alakban van felírva, hogy a deriváltjai
  mindenhol végesek legyenek. Ugyanezért polinomiálisak a többiek is: a `sqrt(u)` a
  parserben `u^0.5`-tá alakul, aminek a deriváltja `u=0`-ban végtelen — ez pont a
  felületen (F=0) lenne baj. A blendben a gyök alatt mindig ott a `+k²`, ezért az jó.
- Az **ellipszis** implicit felületként valójában elliptikus henger (a képlet nem
  függ `z`-től); egy valódi 2D görbe nem F=0 alakú felület, azt a részecske-sampler
  nem tudja mintavételezni.
- A **halmazművelet-sablonok** a listában **előtte álló** két alakzatra hivatkoznak
  `f1`/`f2` néven — ezeket át kell írni a saját alakzataid nevére.
- A tórusz `R`/`r` és a többiek `a`/`b`/`c` nevei **kis-nagybetű érzékenyek**.

### Névfeloldás és névütközés

Egy azonosítót a parser ebben a sorrendben keres meg:

1. az alakzat saját **lokális** paraméterei,
2. a **globális** paraméterek,
3. egy, a listában **előtte álló** alakzat neve — ekkor annak a teljes képlete
   beépül ide (így lehet alakzatokat egymásból építeni, pl. blendelni).

Mivel ezek egyetlen közös névtérben élnek, a következők **nem** egyezhetnek meg:
lokális ↔ globális paraméter, lokális/globális paraméter ↔ alakzatnév, két globális
paraméter, két alakzat. Két **különböző** alakzat lokális paraméterei viszont
nyugodtan hívhatók ugyanúgy — azok külön scope-ok.

A `x`, `y`, `z` és a beépített függvénynevek foglaltak. A hibás sorok pirosan
jelennek meg, és amíg van ütközés, az `Indít` gomb tiltva marad.

> Megjegyzés: a paraméterek értékét a kifejezésfa **cím szerint** tárolja, ezért egy
> paraméter vagy alakzat törlésekor a program eldobja a beparseolt képleteket és
> leállítja a szimulációt — ilyenkor újra `Indít` kell.

## A `main` felépítése (fordítási idejű alakzatok)

A GUI-s út mellett megmaradt a fordítási idejű változat is: a teljes boilerplate
(ablak, kamera, render loop) az `App` mögött van, és elég a felület típusát megadni:

```cpp
#include "App.hpp"
#include "particle_sampling/Surface.hpp"

int main() {
    App app{800, 800, "Particle sampling"};
    app.show<Torus>();          // Sphere, Torus, Ellipsoid, Ellipse, ...
    app.run();
}
```

Opcionális hangolás (a cikk paraméterei, lásd `SimParams`):

```cpp
app.show<Sphere>({.alpha = 8.0f, .phi = 20.0f});
```

## Felhasználói felület (Dear ImGui)

A [Dear ImGui](https://github.com/ocornut/imgui) be van építve (`libraries/imgui`,
GLFW + OpenGL3 backend), készen a modellező-UI fejlesztéséhez. Alapból egy kis
demo-panel jelenik meg (rajta egy kapcsoló az ImGui demo-ablakhoz).

Saját panelt az `App::set_gui(...)`-val adhatsz, ami minden frame-ben lefut:

```cpp
#include "imgui.h"
// ...
app.set_gui([&] {
    ImGui::Begin("Vezérlőpult");
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    // ImGui::SliderFloat(...), ImGui::Button(...), stb.
    ImGui::End();
});
```

Az UI automatikusan „elnyeli" a bevitelt: amíg az egér/billentyűzet az ImGui fölött van,
a kamera és a kontrollpontok nem reagálnak (a `Window` a `Gui::wants_mouse/keyboard`
alapján szűri az eseményeket). A teljes ImGui API elérhető — gombok, csúszkák, fák,
dokkolható panelek építőkövei stb.

---

## Saját alakzat hozzáadása

Három lépés. Csak a felület-definícióhoz és a referencia-meshhez kell hozzányúlni,
a `main`-ben utána már csak `app.show<SajatAlakzat>()` a dolgod.

### 1. Felület-osztály (`particle_sampling/Surface.hpp`)

Származz le a `Surface<L>`-ből, ahol `L` a felület paramétereinek (`q`) száma.
A konstruktorban:

- állítsd be a `q` kezdőértékét,
- írd fel az `F` implicit függvényt a kifejezés-DSL-lel,
- hívd meg a `calculate()`-et (ez deriválja `F`-et `x,y,z` és minden `q[i]` szerint),
- írd felül a `diameter()`-t (lásd lentebb, miért fontos).

Példa — **forgási ellipszoid** (az `y` a forgástengely), `F = (x²+z²)/a² + y²/b² - 1`,
két paraméterrel (`a`, `b`):

```cpp
// q = {a, b}
struct Spheroid : Surface<2> {
    Spheroid() {
        q = {2.0f, 1.0f};
        F = ((x^2.0f) + (z^2.0f)) / ((&q.x)^2.0_k)
          + (y^2.0f) / ((&q.y)^2.0_k) - 1.0_k;
        calculate();
    }
    // A legkisebb jellemző méret skálája (lásd: „diameter()").
    float diameter() const override { return 2.0f * std::min(q.x, q.y); }
};
```

**Kifejezés-DSL** (a `Matek::Analizis` névtérből, `using namespace` nélkül is elérhető
a `Surface.hpp`-ben):

| elem | jelentés |
|---|---|
| `x`, `y`, `z` | a térbeli változók |
| `&q.x`, `&q.y`, … | a felület paraméterei **cím szerint** (futás közben változhatnak; ettől tudja a kontrollpont mozgatni a felületet) |
| `2.0f` vagy `2.0_k` | konstans |
| `+ - * /` | alapműveletek |
| `^` | hatvány — pl. `(x ^ 2.0f)`. **Zárójelezd**, mert a `^` precedenciája alacsony! |
| `sin(...)` | szinusz (jelenleg ez az egyetlen beépített függvény-wrapper) |

A deriválás szimbolikus és automatikus; nem kell kézzel deriváltat írni. (Konstans
kitevőjű hatványt – pl. `x^2` – a rendszer a stabil `n·aⁿ⁻¹·a'` szabállyal deriválja.)

#### `diameter()` — fontos a stabilitáshoz

A `diameter()` a felület **legkisebb jellemző méretét** adja vissza (nem a befoglaló
átmérőt). Ebből számolódik a részecskék taszítási sugara (`σ̂ = d/4`). Ha `d` nagyobb,
mint a felület legkisebb görbületi sugara, a taszítás „átér" a vékony részeken, és a
szimuláció instabillá válik.

- gömb/ellipszoid: a legvékonyabb tengely → `2·min(a,b,c)`
- tórusz: a **cső** átmérője (`2r`), nem a külső `2(R+r)`

### 2. Referencia-mesh (occluder) + párosítás (`particle_sampling/Occluders.hpp`)

Az occluder a szürke „tömör" felület, ami a részecskék mögött látszik (csak vizuális
segédlet). Minden felülethez kell egy, és a végén egy sorral párosítani kell a felülettel.

Az occluder egy `Model`, ami a felület egy paraméterezéséből háromszögeket tölt a
`vertices`-be. A legegyszerűbb a meglévők egyikét (`SphereOccluder`, `TorusOccluder`)
mintának venni, és a `v(...)` paraméterezést kicserélni. Vázlat:

```cpp
class SpheroidOccluder : public Model {
    Spheroid const& surf;
    glm::vec3 color;
    void render(const Camera&) override {
        vertices.clear();
        // ... a surf.q alapján generálj háromszögeket a vertices-be ...
        update_buffers();
        set_uniform("color", color);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertices.size());
    }
public:
    SpheroidOccluder(Spheroid const& s, glm::vec3 col, Camera const&)
        : surf{s}, color{col} {
        update_buffers_on_draw = false;
        Builder::ShaderBuilder b;
        set_shader(b.add_vertex_shader(SHADER_DIR "/vertex.vert")
                    .add_fragment_shader(SHADER_DIR "/fragment.glsl")
                    .build());
    }
};
```

Majd a fájl alján (a többi `OccluderFor` mellé) egy sor, ami összeköti a kettőt:

```cpp
template<> struct OccluderFor<Spheroid> { using type = SpheroidOccluder; };
```

> Megjegyzés: az occluder konstruktorának kötelezően
> `(SajatAlakzat const&, glm::vec3, Camera const&)` a szignatúrája, és az `OccluderFor`
> párosítás nélkül az `ImplicitSurface<SajatAlakzat>` nem fordul le.

### 3. Megjelenítés (`main.cpp`)

```cpp
app.show<Spheroid>();
```

Ennyi — a részecske-mintavételezés, fisszió/halál és a kontrollpontos vezérlés
automatikusan működik az új felületen.

---

## Hogyan gyors ez (teljesítmény)

A szimuláció részecskénként és lépésenként 10 mennyiséget kér a felülettől: `F`, a
gradiens 3 eleme és a Hesse 6 eleme. Naivan ez 10 fabejárás, és a szimbolikus deriválás
után a fák tele vannak ismételt részkifejezésekkel. Három dolog van ellene:

1. **Lefordított, lapos program** (`matek/Program.hpp`). A fa egy topologikusan
   rendezett utasítástömbbé fordul, és az `emit()` érték-számozásos közös
   részkifejezés-kiemelést végez. Mind a 10 kimenet **egyetlen lineáris menetben**
   áll elő, a közös részszámítások egyszer futnak.

   | képlet | derivált-fák (karakter) | program (utasítás) | gyorsulás |
   |---|---|---|---|
   | henger | 50 | 11 | 2.2× |
   | tórusz | 730 | 45 | 13.8× |
   | `unio(f1,f2)` | 996 | 45 | 20.3× |
   | `sunio(f1,f2,k)` | 5 347 | 106 | 43.8× |
   | kétszintű beágyazott CSG | 26 402 | **156** | **128×** |

2. **A Hesse csak ha kell.** Ha a görbület-taszítás csúszka 0-n áll, a szimuláció a
   4 kimenetes programot futtatja a 10 helyett.

3. **Térbeli rács** (`particle_sampling/SpatialGrid.hpp`). A Gauss-taszítás 3σ-n túl
   elhanyagolható, ezért a párbejárás helyett rácsból jönnek a szomszédok:
   O(n²) → O(n). 2000 részecskénél mérve 3 998 000 helyett 6 520 vizsgált pár.

Az eredmény bitre azonos a fabejárásével — ezt teszt ellenőrzi minden képletre és
minden kimenetre.

---

## Projekt-szerkezet (röviden)

| hely | mi |
|---|---|
| `App.hpp` | ablak + kamera + render loop wrapper |
| `particle_sampling/Surface.hpp` | a `Surface<L>` ős és a konkrét felületek |
| `particle_sampling/Occluders.hpp` | referencia-meshek + `OccluderFor` párosítás |
| `particle_sampling/ImplicitSurface.hpp` | a szimuláció (taszítás, fisszió, halál) és `SimParams` |
| `particle_sampling/Particle.hpp` | részecske + a `Particles`/`Floaters`/`ControlPoints` modellek |
| `matek/` | a szimbolikus kifejezés-/deriválórendszer (Kif DSL) |
| `matek/Kif.hpp` | a string → kifejezésfa parser és a függvénytábla (`make_func`) |
| `matek/Program.hpp` | a kifejezésfa lapos, futtatható alakja (közös részkifejezés-kiemeléssel) |
| `particle_sampling/SpatialGrid.hpp` | egyenletes rács a taszítás szomszédkereséséhez |
| `matek/muveletek/{Minimum,Maximum}.hpp` | éles halmazműveletek (CSG) csomópontjai |
| `matek/fuggvenyek/{Abs,Elojel}.hpp` | `abs` / `sign` — ezekre épül a min/max deriváltja |
| `particle_sampling/DomainConstraint.hpp` | a tartomány-feltétel matematikája (felület menti csúsztatás + perem-fal) |
| `model/`, `utils/` | OpenGL-réteg (kamera, ablak, shader, Model) |
| `model/Gui.{hpp,cpp}` | Dear ImGui wrapper (init/frame/render, input-szűrés) |
| `tests/` | ctest-tesztek (GL nélkül futnak) |
| `libraries/` | GLFW, GLM, GLAD, Dear ImGui (`imgui`) |

Az ablak eseményei (`Window::add_*_event`) azonosítót adnak vissza, és a
`Window::Subscription` RAII-osztállyal automatikusan leiratkoznak — ezért a
kamera, a felületek és a samplerek szabadon létrehozhatók és megszüntethetők
futás közben (nem marad utánuk megszűnt objektumra mutató eseménykezelő).

## Hivatkozások

A gyökérben lévő cikkek:

| fájl | mi |
|---|---|
| `witkin_andrew_1994_1.pdf` | Witkin–Heckbert: *Using Particles to Sample and Control Implicit Surfaces* (SIGGRAPH '94) — a mintavételezés módszere |
| `1999-blobtree-model.pdf` | Wyvill–Guy–Galin: *Extending the CSG Tree* (CGF 18(2), 1999) — a BlobTree, a halmazműveletek és a warpok forrása |
| `[2005, Goldman] Curvature formulas...pdf` | implicit felületek görbületi képletei (`Surface::curvature`) |