# Alakzatok kódja: a `gorbe_api.hpp` használata

Egy alakzathoz C++ osztály írható, amely mozgatja, deformálja vagy animálja. Ez a
leírás a felületet, a szabályokat és a példákat mutatja be. A beépítés részletei
(fordítás, DLL, futtató) a [README](../README.md#alakzatok-kódja-c) megfelelő
fejezetében vannak.

## Tartalom

- [Gyorskezdés](#gyorskezdés)
- [Az osztály felépítése](#az-osztály-felépítése)
- [A két függvény: `start()` és `update()`](#a-két-függvény-start-és-update)
- [Elérhető függvények](#elérhető-függvények)
- [Szabályok](#szabályok)
- [Példák](#példák)
- [Hibák és megoldásuk](#hibák-és-megoldásuk)
- [Korlátok](#korlátok)

## Gyorskezdés

1. **Mentsd a projektet.** A kód a projekt mappájába kerül.
2. Jelöld ki az alakzatot. A **Tulajdonságok** panel **Kód (C++)** részén add meg az
   osztály nevét (pl. `FelLe`), és nyomd meg a **Kód létrehozása** gombot.
3. A program létrehozza a `kod/FelLe.cpp` vázat, és megnyitja a szerkesztőben.
   Írd meg a kódot, és mentsd el.
4. Nyomd meg az **Indít** gombot. A program lefordítja a kódot, és az **Alakzatok**
   panelen kiírja, hogy a build sikeres volt-e. Sikertelennél a hibasorok is
   megjelennek.

```
<projektmappa>/
  projekt.gorbe.json
  gorbe_api.hpp        ← a program írja, NE szerkeszd (minden fordításkor felülíródik)
  kod/FelLe.cpp        ← a te kódod
  build/<jelenet>/     ← a fordítás kimenete; a teljes napló: forditas.log
```

## Az osztály felépítése

```cpp
#include "../gorbe_api.hpp"

struct FelLe : gorbe::ShapeScript {      // 1. a ShapeScript-ből származik
    void start() override { }            // 2. egyszer, az Indításkor
    void update(float t, float dt) override { }   // 3. minden ciklusban
};
GORBE_SHAPE(FelLe)                       // 4. regisztráció, a fájl végén
```

- **A fájl neve és az osztály neve egyezzen** (`FelLe.cpp` → `struct FelLe`), és
  ugyanez a név kerüljön a `GORBE_SHAPE(...)`-be is. A program a fájl nevéből tudja,
  melyik osztályt keresse.
- Az osztálynév C++-azonosító legyen: betű, szám és `_`, nem kezdődhet számmal.
  Ékezetes betű nem lehet benne.
- Mindkét függvény elhagyható: amelyiket nem írod meg, az nem csinál semmit.
- **Minden alakzat saját példányt kap.** A tagváltozók (pl. `float z0`)
  alakzatonként külön léteznek, akkor is, ha több alakzat futtatja ugyanazt a fájlt.

## A két függvény: `start()` és `update()`

| | `start()` | `update(float t, float dt)` |
|---|---|---|
| mikor fut | egyszer, az Indításkor, a képletek felépítése **előtt** | minden ciklusban, a részecskék léptetése előtt |
| mit állíthat | **szerkezetet és értéket**: `add_param`, `add_warp`, `formula`, és minden, amit az `update()` | **csak értéket**: paraméter, pozíció, forgatás, méret |
| mire jó | kezdőértékek megjegyzése, saját paraméter, warp, képlet | animáció |

Az `update()` paraméterei:

- **`t`**: a program ideje másodpercben. Ugyanez a `t` szerepel a képletekben is.
- **`dt`**: az előző ciklus óta eltelt idő másodpercben. Ha valamit egyenletes
  sebességgel akarsz változtatni (pl. forgatni), szorozd vele: `rotation().z += 0.8f * dt;`.
  Így a sebesség nem függ az FPS-től.

## Elérhető függvények

Mindegyik az ősosztály (`gorbe::ShapeScript`) védett tagja, a saját osztályodból
közvetlenül hívható.

### Paraméterek

```cpp
float& param(char const* name);
```

Egy paraméter **változója**, referenciaként. Írható és olvasható, az írás azonnal
látszik a felületen, újraépítés nélkül. A név keresési sorrendje:

1. az alakzat lokális paraméterei (Tulajdonságok panel),
2. a kódból felvett paraméterek (`add_param`),
3. a jelenet paraméterei,
4. a program paraméterei.

A **képletes** paraméter (amelynek képlete van, pl. `2*r + 1`) nem írható, ilyenkor
hiba jelenik meg. A visszakapott cím a futás végéig érvényes, ezért elég egyszer
lekérni a `start()`-ban:

```cpp
float* r = nullptr;
void start() override { r = &param("r"); }
void update(float t, float) override { *r = 2.0f + std::sin(t); }
```

```cpp
float& add_param(char const* name, float value);    // csak start()
```

Új paraméter, **csak erre a futásra**: nem mentődik, és a felületen sem jelenik meg.
A képletben, a warpokban és a `param()`-mal használható. **Ha már van ilyen nevű
paraméter** (pl. a felületen felvetted), azt adja vissza, a felületen beállított
értékével. A `value` ilyenkor csak alapérték.

### Pozíció, forgatás, méret

```cpp
gorbe::vec3& position();          // eltolás
gorbe::vec3& rotation();          // forgatás az x, y, z tengely körül, RADIÁNBAN
gorbe::vec3& scale();             // méret tengelyenként (1 = eredeti)
void position(gorbe::vec3 p);     // rövidítés: position() = p
```

A `gorbe::vec3` egy egyszerű `{x, y, z}` struktúra. Elemenként és egészben is írható:

```cpp
position().z = 3.0f;
position({1.0f, 2.0f, 0.0f});
scale() = {2.0f, 2.0f, 2.0f};
```

Ugyanezek az értékek láthatók a felület **Transzformáció** részén. Az alakzat előbb a
warpokon megy át, és a kész, deformált alakzatot helyezi el a pozíció, a forgatás és
a méret.

### Szerkezet (csak a `start()`-ban)

```cpp
bool add_warp(char const* fx, char const* fy, char const* fz);
bool formula(char const* f);
```

- **`add_warp`**: egy warp a lánc végére, a felületen felvettek **után**. A három
  kifejezés ugyanaz, mint a felület warp-szerkesztőjében: a tér → alakzat (inverz)
  leképezés, tehát *„hol keressük az alakzatot ehhez a ponthoz”*. A kifejezés
  hivatkozhat paraméterekre és a `t` időre is.
- **`formula`**: a felületen megadott képlet **helyett** ez az `F(x, y, z) = 0`.
  A nyelv ugyanaz, mint a Képlet mezőé.

Mindkettő igazat ad, ha elfogadta. A felépítés hibáit (pl. elírt képlet) az Indítás
jelzi, ugyanúgy, mint a felületről beírt képletnél.

### Konzol és hibaüzenet

```cpp
void print(char const* fmt, ...);
void error(char const* msg);
```

- **`print`**: egy sor a **Konzolra** (középen alul, a 3D nézet alatt), printf-formátummal.
  Elé kerül az idő (`t`) és az alakzat neve:

  ```cpp
  print("r = %.2f, z = %.2f", param("r"), position().z);
  // a konzolon:  [   12.34] gomb1: r = 1.50, z = 0.73
  ```

  Ha **egymás után ugyanaz a sor** jön, nem ismétlődik, csak a számlálója nő:
  `... gomb1: itt vagyok (x240)`. Így az `update()`-ből is nyugodtan logolhatsz.
  A konzol legfeljebb 2000 sort tart meg, a régebbiek kiesnek.
  A formátum a `printf`-é: `%d` egész, `%.2f` tört (a `float` is jó), `%s` szöveg.

  > Azért nem `log` a neve, mert az osztályon belül elfedné a `<cmath>` `log()`
  > függvényét, pedig az a képletekhez gyakran kell.

- **`error`**: saját hibaüzenet, az **Alakzatok** panelre narancssárgán, a konzolra
  pirosan. Ugyanaz az üzenet csak egyszer jelenik meg, akkor is, ha minden ciklusban
  hívod.

A konzolra kerül még az Indítás és a fordítás eredménye („Build sikeres” /
„Build sikertelen”), a fordító figyelmeztetései (sárgán) és hibasorai (pirosan),
valamint a futás hibái. A **Törlés** gomb kiüríti, a **Másolás** a vágólapra teszi a
teljes tartalmát. Az **automatikus görgetés** követi az új sorokat; ha feljebb
görgetsz, megáll, hogy a régebbi sorokat el tudd olvasni. A konzol és a 3D nézet
határa húzható.

## Szabályok

- **A felületen beállított érték a kezdőérték.** Amihez a kód hozzányúl (paraméter,
  pozíció, forgatás, méret), az a futás végén visszaáll: a **Töröl** gomb vagy egy
  újabb **Indítás** után. Mentéskor is a kezdőérték kerül a fájlba, nem a futás közbeni
  érték. Ezért érdemes a `start()`-ban megjegyezni a kezdőértéket, és ahhoz képest
  mozgatni (lásd a példákat).
- **Szerkezetet csak a `start()` állíthat.** Az `add_param`, az `add_warp` és a
  `formula` az `update()`-ből hívva nem csinál semmit, és hibát ír ki. Ezekhez a
  képletet újra kell építeni, ami lassú. Az értékek írása viszont olcsó.
- **A kód kiegészítései nem halmozódnak.** A `start()`-ban felvett warp és paraméter
  minden Indításkor újra keletkezik. Nem mentődnek, és nem gyűlnek fel.
- **Az Indítás mindig újrafordít.** A kódot elég a szerkesztőben elmenteni, és
  megnyomni az Indít gombot. Ez néhány másodperc, közben folyamatjelző látszik,
  amely meg is szakítható.
- A futás közben a felületen elmozdított csúszkát a kód a következő ciklusban
  felülírja, ha ő is írja azt az értéket.
- **Kivétel** esetén csak az a kód áll le, a jelenet és a többi alakzat kódja fut
  tovább. A hiba megjelenik a panelen.

## Példák

Mindegyik egy önálló fájl a `kod/` mappában. A fájlnév az osztály neve.

### 1. Fel-le mozgás a z tengelyen

```cpp
#include "../gorbe_api.hpp"

// Fel-le mozgás a z tengelyen, a kezdőhely körül.
struct FelLe : gorbe::ShapeScript {
    float z0 = 0.0f;

    void start() override {
        z0 = position().z;                      // a felületen beállított kezdő z
    }

    void update(float t, float) override {
        position().z = z0 + 2.0f * std::sin(t); // ±2 egység
    }
};
GORBE_SHAPE(FelLe)
```

### 2. Ugyanez csúszkával állítható amplitúdóval és sebességgel

A felületen vegyél fel az alakzatnak `amp` (pl. 2) és `seb` (pl. 1) **lokális
paramétert**. Futás közben a csúszkák élőben hatnak.

```cpp
#include "../gorbe_api.hpp"

// Mint a FelLe, de az amplitúdó és a sebesség csúszkáról jön: vegyél fel az
// alakzatnak "amp" és "seb" lokális paramétert a felületen.
struct Csuszkas : gorbe::ShapeScript {
    float  z0  = 0.0f;
    float* amp = nullptr;
    float* seb = nullptr;

    void start() override {
        z0  = position().z;
        amp = &param("amp");    // a cím a futás végéig érvényes: elég egyszer lekérni
        seb = &param("seb");
    }

    void update(float t, float) override {
        position().z = z0 + *amp * std::sin(*seb * t);
    }
};
GORBE_SHAPE(Csuszkas)
```

### 3. Lüktető gömb (paraméter írása)

A képlet: `x^2 + y^2 + z^2 - r^2`, és az alakzatnak legyen egy `r` lokális paramétere.

```cpp
#include "../gorbe_api.hpp"

// Lüktető gömb: a képlet r paraméterét írja. A képlet: x^2 + y^2 + z^2 - r^2
struct Lukteto : gorbe::ShapeScript {
    float r0 = 1.0f;

    void start() override {
        r0 = param("r");                        // a felületen beállított sugár
    }

    void update(float t, float) override {
        param("r") = r0 * (1.0f + 0.3f * std::sin(3.0f * t));
    }
};
GORBE_SHAPE(Lukteto)
```

### 4. Egyenletes forgás (a `dt` használata)

```cpp
#include "../gorbe_api.hpp"

// Egyenletes forgás a z tengely körül, a dt-vel (így a sebesség nem függ az FPS-től).
struct Forgo : gorbe::ShapeScript {
    void update(float, float dt) override {
        rotation().z += 0.8f * dt;              // radián / másodperc
    }
};
GORBE_SHAPE(Forgo)
```

### 5. Körpálya, billegés és pulzáló méret

```cpp
#include "../gorbe_api.hpp"

// Körpálya az xy síkban, közben billeg és pulzál a mérete.
struct Korpalya : gorbe::ShapeScript {
    gorbe::vec3 kozep;

    void start() override { kozep = position(); }

    void update(float t, float) override {
        position({kozep.x + 4.0f * std::cos(t), kozep.y + 4.0f * std::sin(t), kozep.z});
        rotation().x = 0.3f * std::sin(2.0f * t);
        float const s = 1.0f + 0.2f * std::sin(4.0f * t);
        scale() = {s, s, s};
    }
};
GORBE_SHAPE(Korpalya)
```

### 6. Warp a kódból, élőben változó erősséggel

A `start()` felvesz egy saját paramétert és egy rá hivatkozó warpot, az `update()`
pedig csak a paraméter értékét írja. Ehhez nem kell újraépítés.

```cpp
#include "../gorbe_api.hpp"

// Szerkezet a start()-ban: saját paraméter + warp, amely hivatkozik rá.
struct Hullamos : gorbe::ShapeScript {
    float* a = nullptr;

    void start() override {
        a = &add_param("hullam", 0.0f);          // csak erre a futásra, nem mentődik
        add_warp("x + hullam*sin(3*y)", "y", "z");
    }

    void update(float t, float) override {
        *a = 0.4f * std::sin(t);                 // a warp erőssége élőben változik
    }
};
GORBE_SHAPE(Hullamos)
```

> Ha a mozgás csak az időtől függ, `update()` nélkül is megoldható: a `t` a
> kifejezésekben is használható, pl. `add_warp("x + 0.4*sin(t)*sin(3*y)", "y", "z")`.

### 7. A képletet is a kód adja

```cpp
#include "../gorbe_api.hpp"

// A képletet is a kód adja: tórusz, a felületen megadott képlet helyett.
struct Torusz : gorbe::ShapeScript {
    void start() override {
        add_param("R", 3.0f);
        add_param("rr", 1.0f);
        formula("(sqrt(x^2 + y^2) - R)^2 + z^2 - rr^2");
    }
};
GORBE_SHAPE(Torusz)
```

Ha a felületen már van `R` vagy `rr` nevű lokális paraméter, az `add_param` azt
használja, a felületen beállított értékkel.

### 8. Logolás a konzolra

```cpp
#include "../gorbe_api.hpp"

// Indításkor kiírja a kezdőállapotot, futás közben másodpercenként egyszer az állást.
struct Naplozo : gorbe::ShapeScript {
    float kovetkezo = 0.0f;                     // mikor írjunk legközelebb

    void start() override {
        gorbe::vec3 const p = position();
        print("indul: pozicio = (%.2f, %.2f, %.2f)", p.x, p.y, p.z);
    }

    void update(float t, float dt) override {
        position().z += 0.5f * dt;
        if (t >= kovetkezo) {
            print("t = %.1f s, z = %.2f", t, position().z);
            kovetkezo = t + 1.0f;
        }
    }
};
GORBE_SHAPE(Naplozo)
```

### 9. Saját hibaüzenet

```cpp
#include "../gorbe_api.hpp"

// Saját hibaüzenet: a panelen narancssárgán, a konzolon pirosan (egy üzenet egyszer).
struct Ellenor : gorbe::ShapeScript {
    void update(float, float) override {
        if (param("r") <= 0.0f) error("az r legyen pozitiv");
    }
};
GORBE_SHAPE(Ellenor)
```

## Hibák és megoldásuk

A fordítás hibái az **Alakzatok** panelen jelennek meg, **pirosan** (legfeljebb 10
sor, a teljes napló helyével). A futás hibái **narancssárgán** látszanak. Mindkettő
a **Konzolra** is bekerül, a fordítóhibák mind, nem csak az első 10.

| üzenet | ok | megoldás |
|---|---|---|
| `a kodhoz elobb mentsd a projektet` | a projekt nincs elmentve, ezért nincs mappája | Fájl → Mentés |
| `nincs meg a kodfajl: ...` | a fájlt átnevezték vagy törölték | hozd létre újra, vagy válaszd le a kódot |
| `kod\FelLe.cpp(12): error C2065: ...` | C++-hiba a 12. sorban | javítsd a szerkesztőben, aztán Indít |
| `a kodban nincs GORBE_SHAPE a fajl nevevel egyezo osztalyra` | hiányzik a `GORBE_SHAPE(...)`, vagy más név van benne, mint a fájlé | `GORBE_SHAPE(FelLe)` a `FelLe.cpp` végére |
| `nincs 'r' parameter` | a `param("r")` nem talált ilyen nevű paramétert | vedd fel a felületen, vagy használd az `add_param`-ot |
| `a(z) 'r' parameter kepletes, nem irhato` | az `r`-nek képlete van | töröld a képletét, vagy írj egy másik paramétert |
| `add_warp csak a start()-ban hivhato` | szerkezeti hívás az `update()`-ből | tedd át a `start()`-ba |
| `kivetel: ... (a kod leallt)` | a kód kivételt dobott | javítsd, aztán Indít |
| `nem talalhato a Visual Studio C++ fordito` | nincs telepítve a fordító | Visual Studio Installer → „Desktop development with C++” |

## Korlátok

- **Csak Windowson** működik, és kell hozzá a Visual Studio C++ fordítója (2017 vagy
  újabb, a „Desktop development with C++” csomaggal).
- **A program nem védekezik a hibás memóriakezelés ellen.** Egy rossz pointer, egy
  tömb túlindexelése vagy egy végtelen ciklus a programot is összeomlasztja vagy
  lefagyasztja. A kivételeket viszont elkapja.
- **A `static` változók közösek.** Egy jelenet minden kódja egy DLL-be fordul, így
  egy `static` változón az összes alakzat osztozik, amely ugyanazt a fájlt futtatja.
  Alakzatonkénti állapothoz tagváltozót használj.
- Másik alakzat adatai még nem érhetők el a kódból.
- A kód a szerkesztő fülön (variációs szerkesztő) nem fut, csak a jelenetben.
