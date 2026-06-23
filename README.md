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
cmake -S . -B cmake-build-debug
cmake --build cmake-build-debug --target gorbe
./cmake-build-debug/gorbe          # Windows: .\cmake-build-debug\gorbe.exe
```

A build a shadereket a bináris mellé másolja, és a program onnan tölti, így
**tetszőleges munkakönyvtárból indítható**.

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

## A `main` felépítése

A teljes boilerplate (ablak, kamera, render loop) az `App` mögött van. A `main`-ben
elég a felület típusát megadni:

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

## Projekt-szerkezet (röviden)

| hely | mi |
|---|---|
| `App.hpp` | ablak + kamera + render loop wrapper |
| `particle_sampling/Surface.hpp` | a `Surface<L>` ős és a konkrét felületek |
| `particle_sampling/Occluders.hpp` | referencia-meshek + `OccluderFor` párosítás |
| `particle_sampling/ImplicitSurface.hpp` | a szimuláció (taszítás, fisszió, halál) és `SimParams` |
| `particle_sampling/Particle.hpp` | részecske + a `Particles`/`Floaters`/`ControlPoints` modellek |
| `matek/` | a szimbolikus kifejezés-/deriválórendszer (Kif DSL) |
| `model/`, `utils/` | OpenGL-réteg (kamera, ablak, shader, Model) |
| `model/Gui.{hpp,cpp}` | Dear ImGui wrapper (init/frame/render, input-szűrés) |
| `libraries/` | GLFW, GLM, GLAD, Dear ImGui (`imgui`) |

A módszer részletei: `witkin_andrew_1994_1.pdf` (a gyökérben).