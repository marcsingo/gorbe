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

A `gorbe::vec3` egy egyszerű `{x, y, z}` struktúra. Elemenként és egészben is írható,
és a szokásos műveletek működnek rajta:

```cpp
position().z = 3.0f;
position({1.0f, 2.0f, 0.0f});
scale() = {2.0f, 2.0f, 2.0f};

gorbe::vec3 const d = masik.position() - position();   // + - (két vektor), * / (számmal)
position() += d * 0.5f;                                 // += -= *=
float const tav = gorbe::length(d);                     // hossz
gorbe::vec3 const irany = gorbe::normalize(d);          // egységvektor (0 hosszra 0)
float const cos_szog = gorbe::dot(irany, {0, 0, 1});    // skaláris szorzat
```

Ugyanezek az értékek láthatók a felület **Transzformáció** részén. Az alakzat előbb a
warpokon megy át, és a kész, deformált alakzatot helyezi el a pozíció, a forgatás és
a méret.

Amit a kód itt átír, azzal az alakzat **részecskéi is vele mennek**, ugyanazzal a
merev leképezéssel. Egy nagy ugrás után sem kell megvárni, amíg a részecskék
átrepülnek az új helyre. Ilyen ugrás például a `position().x = 6 * cos(t)` első
ciklusa, amikor az alakzat még a kezdőhelyén áll. A paraméterek (pl. `r`) változását
viszont a részecskék a szimulációval követik, tehát egy nagy ugrás ott lassabb.

### Más alakzatok

```cpp
gorbe::ShapeRef shape(char const* name);   // a jelenet egy alakzata, név szerint
int             shape_count();             // hány alakzat van a jelenetben
gorbe::ShapeRef shape(int i);              // az i-edik (0-tól; a saját is köztük van)
gorbe::ShapeRef& self();                   // a saját alakzat
```

A `gorbe::ShapeRef` egy alakzatra mutat. Kis érték, tagváltozóba tehető. Ugyanaz
érhető el rajta, mint a saját alakzaton, **írni is lehet**:

```cpp
gorbe::ShapeRef b = shape("Gomb2");
b.name();                  // "Gomb2"
b.param("r") = 2.0f;       // a Gomb2 paramétere (az ő hatókörében keres)
b.position().x += 1.0f;    // a Gomb2 mozgatása
b.rotation(); b.scale();
```

- **A `start()`-ban kérd le**, és tedd el egy tagváltozóba. Így a másik alakzat
  pozíciója, forgatása és mérete írható is lesz, és a keresés sem fut minden
  ciklusban. Ha az `update()`-ben kérsz le először egy alakzatot, és annak
  egységnyi a transzformációja, akkor az írás nem látszik. Erről a konzol szól.
- Ha nincs ilyen nevű alakzat, hiba jelenik meg, és a `ShapeRef` **érvénytelen**:
  `if (b)` hamis, a hívásai nem csinálnak semmit (a paraméter és a pozíció 0-t, a
  `value` NaN-t ad, a `collide` nem jelez ütközést).
- Amit egy másik alakzaton írsz, az is visszaáll a kezdőértékére a futás végén.
- Ha két kód ugyanazt az értéket írja, az utolsó nyer. Az `update()`-ek a lista
  sorrendjében futnak.

### Az implicit függvény: pont-lekérdezések

Minden alakzat egy `F(x, y, z) = 0` felület: **`F < 0` belül**, `F = 0` a felületen,
`F > 0` kívül. Ezt bármelyik alakzaton (a sajátodon is: `self()`) lekérdezheted, a
világ egy pontjában, a pillanatnyi pozícióval, warpokkal és paraméterekkel:

```cpp
float       value(vec3 p);      // F(p)
gorbe::vec3 gradient(vec3 p);   // ∇F(p): a felület normálisa (kifelé), ha p a felületen van
bool        contains(vec3 p);   // F(p) < 0, és a tartomány-feltételen belül
float       distance(vec3 p);   // F / |∇F|: előjeles távolság-becslés (negatív = belül)
```

- A **`distance` becslés**, amely a felület közelében pontos. Távolabb az alakzat
  képletén múlik: az `sqrt(x^2 + y^2 + z^2) - r` gömbnél mindenhol pontos, az
  `x^2 + y^2 + z^2 - r^2` gömbnél csak a felület közelében.
- **Nyitott felületnél** (pl. sík: `z`) a „belül” az `F < 0` félteret jelenti.
- A `start()`-ban ezek (és a `collide`) még nem érhetők el, mert a képletek ott
  épülnek fel. Ilyenkor a `value` NaN-t ad, és hiba jelenik meg.

### Ütközés

```cpp
gorbe::Hit collide(gorbe::ShapeRef masik);         // a saját alakzat és a másik
gorbe::Hit a.collide(gorbe::ShapeRef b);           // bármely két alakzat
```

```cpp
struct Hit {
    bool  hit;      // ütköznek-e (a Hit feltételként is használható: if (h) ...)
    int   count;    // hány mintapont van a másikban
    vec3  point;    // a legmélyebb pont
    vec3  normal;   // amerre a SAJÁT (első) alakzatot tolni kell, hogy kiszabaduljon
    float depth;    // mennyit
};
```

A legegyszerűbb ütközéskezelés: tolja ki magát, épp annyit, hogy érintkezzenek.

```cpp
if (gorbe::Hit h = collide(fal))
    position() += h.normal * h.depth;
```

**Hogyan működik?** Az implicit egyenlet tulajdonságát használja. Az alakzatok
részecskéi a saját felületükön ülnek, ezért egy részecske akkor van a másik
alakzatban, ha a másik `F`-je ott negatív. A vizsgálat **mindkét irányban** lefut:
A részecskéi B-ben, és B részecskéi A-ban. Így akkor is ütközést jelez, ha az egyik
alakzat teljesen a másikon belül van. A legmélyebb pontot a program Newton-lépésekkel
a másik felületére vetíti, ebből adódik a `depth` és a `normal`.

- A pontossága a **mintavétel sűrűségén** múlik (a `d` beállításon). Kisebb `d` több
  részecskét és pontosabb ütközést ad. Ami kisebb két részecske távolságánál, azt
  átfedésként sem veszi észre.
- **Az Indítás után** a részecskék néhány másodperc alatt terítik be a felületet. Addig
  az ütközés hiányos lehet.
- Gyors mozgásnál egy vékony alakzaton „át lehet ugrani” egy ciklus alatt.
- Költsége ciklusonként a két alakzat részecskeszámával arányos. Néhány alakzatpárra
  gond nélkül hívható minden ciklusban.
- Önmagával sosem ütközik, ezért az összes alakzaton végig lehet menni (lásd a 12.
  példát).

### Futás, a jelenet kamerája és a bemenet

Az **Indít** után a jelenet **fut**, egészen a **Töröl**-ig. Az Alakzatok panelen ilyenkor
zöld „FUT” felirat látszik.

- **Futás közben** a 3D nézet a **jelenet kamerájából** lát. A billentyűzet és az egér a
  **kódé**, a szerkesztő kamera áll, és a kontrollpontok húzása szünetel. A tengelyek és a
  rács nem rajzolódnak ki, mert ez a „végleges” kép.
- A nézet **bal felső sarkában** egy gomb vált a jelenet kamerája és a szerkesztő kamera
  között. A szerkesztő kamerából nézve a billentyűk újra a szerkesztő kamerát mozgatják,
  és a kód nem kap bemenetet.
- **Nem futó jelenetben** a megszokott szerkesztő kamera dolgozik. A jelenet kameráját egy
  **narancs gúla** mutatja: a csúcsa a kamera helye, a háromszög a kép „fel” iránya.

A jelenet kamerája az Alakzatok lista **első sora („Kamera”)**. Erre kattintva a
Tulajdonságok panelen állítható a pozíció, az irány (yaw), az emelkedés (pitch), a dőlés
(roll) és a látószög. A **Ránéz** gombbal egy pont felé fordul, a **Szerkesztő nézetből**
gombbal oda áll, ahonnan most a szerkesztő kamera néz. A beállítás a projektfájlba is
mentődik.

#### A kamera a kódból

```cpp
gorbe::CameraRef& camera();

camera().position()    // vec3&: a helye
camera().rotation()    // vec3&: x = yaw (0 = +x felé), y = pitch (negatív = lefelé, ±89°),
                       //        z = roll (dőlés) — RADIÁNBAN
camera().fov()         // float&: függőleges látószög, FOKBAN (5..120)
camera().look_at(p)    // a p pont felé fordul (a helye és a dőlése marad)
camera().forward()     // a nézőirány a világban
camera().right()       // a kép jobbra iránya (a dőléssel együtt)
camera().up()          // a kép felfelé iránya
```

Ugyanúgy viselkedik, mint egy alakzat pozíciója: amit a kód futás közben ír, az a
**Töröl után visszaáll** a panelen beállított értékre.

#### Bemenet

```cpp
bool key_down(int k);        // le van nyomva (folyamatos mozgáshoz)
bool key_pressed(int k);     // ebben a ciklusban nyomták le (egyszeri akcióhoz: ugrás, lövés)
bool key_released(int k);    // ebben a ciklusban engedték fel
bool mouse_down(int b);      // MOUSE_LEFT, MOUSE_RIGHT, MOUSE_MIDDLE
bool mouse_pressed(int b);
bool mouse_released(int b);
gorbe::vec2 mouse_position();   // képpont a 3D nézetben (bal felső sarok = 0, 0)
gorbe::vec2 mouse_delta();      // elmozdulás az előző ciklus óta
float       scroll();           // görgetés ebben a ciklusban
gorbe::vec2 view_size();        // a 3D nézet mérete képpontban
gorbe::Ray  mouse_ray();        // sugár a kamerából az egér alá: origin, dir (egységnyi)
bool        input_active();     // kap-e most bemenetet a kód
```

- **Billentyűkódok:** a betű és a szám a saját karaktere: `'W'`, `'a'` (a kisbetűt is
  elfogadja), `'1'`. A többi névvel érhető el: `gorbe::KEY_SPACE`, `KEY_ENTER`,
  `KEY_ESCAPE`, `KEY_TAB`, `KEY_BACKSPACE`, `KEY_UP` / `DOWN` / `LEFT` / `RIGHT`,
  `KEY_LEFT_SHIFT`, `KEY_LEFT_CONTROL`, `KEY_LEFT_ALT`, `KEY_RIGHT_SHIFT`,
  `KEY_RIGHT_CONTROL`.
- **A kód csak futás közben**, a jelenet kamerájának nézetében kap bemenetet. A billentyűt
  csak akkor kapja, ha épp nem egy mezőbe gépelsz, az egeret csak a 3D nézet fölött. Máskor
  minden hamis, illetve nulla, így egy billentyű sem „ragad be”.
- A `key_pressed` egy lenyomásra **pontosan egyszer** igaz. Az operációs rendszer
  billentyű-ismétlése nem számít új lenyomásnak.
- Az **Esc** továbbra is kilép a programból.

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

### 10. Egy másik alakzat követése

```cpp
#include "../gorbe_api.hpp"

// Követi a "Cel" nevű alakzatot: minden ciklusban egy kicsit közelebb lép hozzá.
struct Koveto : gorbe::ShapeScript {
    gorbe::ShapeRef cel;

    void start() override {
        cel = shape("Cel");                 // elég egyszer lekérni
    }

    void update(float, float dt) override {
        if (!cel) return;                    // nincs ilyen alakzat (a hiba már kiíródott)
        gorbe::vec3 const hova = cel.position() + gorbe::vec3{0, 0, 3};   // 3 egységgel fölötte
        position() += (hova - position()) * (2.0f * dt);                   // simán közelít
    }
};
GORBE_SHAPE(Koveto)
```

### 11. Ütközés: megáll a falnál

A gömb a fal felé halad. Ha beleér, épp annyit lép vissza, hogy érintkezzenek.

```cpp
#include "../gorbe_api.hpp"

// Ütközéskor kitolja magát a "Fal"-ból, és a konzolra írja.
struct Kitero : gorbe::ShapeScript {
    gorbe::ShapeRef fal;

    void start() override { fal = shape("Fal"); }

    void update(float t, float dt) override {
        position().x += 1.0f * dt;                          // a fal felé halad
        if (gorbe::Hit h = collide(fal)) {
            position() += h.normal * h.depth;               // épp annyit vissza, hogy érintsen
            print("utkozes t = %.1f, melyseg %.3f", t, h.depth);
        }
    }
};
GORBE_SHAPE(Kitero)
```

### 12. Ütközés a jelenet összes alakzatával

```cpp
#include "../gorbe_api.hpp"

// A jelenet minden alakzatával ütközést vizsgál, és kiírja, kivel ütközik.
// (Önmagával sosem ütközik, ezért nem kell kihagyni.)
struct Mindenki : gorbe::ShapeScript {
    void update(float, float) override {
        for (int i = 0; i < shape_count(); ++i) {
            gorbe::ShapeRef masik = shape(i);
            if (gorbe::Hit h = collide(masik))
                print("utkozes: %s (%d pont, melyseg %.2f)", masik.name(), h.count, h.depth);
        }
    }
};
GORBE_SHAPE(Mindenki)
```

### 13. Pont-lekérdezés: belép-e egy zónába

A "Zona" alakzatnak legyen `r` lokális paramétere (pl. a képlete
`x^2 + y^2 + z^2 - r^2`, r = 2).

```cpp
#include "../gorbe_api.hpp"

// Pont-lekérdezés: a saját középpontja benne van-e a "Zona" alakzatban, és milyen messze
// van a felületétől. Ha belép, a "Zona" sugarát (r paraméter) megnöveli.
struct Erzekelo : gorbe::ShapeScript {
    gorbe::ShapeRef zona;
    bool bent = false;

    void start() override { zona = shape("Zona"); }

    void update(float t, float) override {
        position().x = 4.0f * std::sin(0.5f * t);           // ide-oda jár
        bool const most = zona.contains(position());
        if (most != bent) {
            print(most ? "belepett a zonaba" : "kilepett a zonabol");
            zona.param("r") = most ? 2.5f : 2.0f;
            bent = most;
        }
        if (!most) print("tavolsag a zonatol: %.1f", zona.distance(position()));
    }
};
GORBE_SHAPE(Erzekelo)
```

### 14. WASD-irányítás, ugrás és követő kamera

Indíts el egy jelenetet, amelyben a gömb (vagy bármilyen alakzat) ezt a kódot futtatja.
Utána a 3D nézet fölött a **W A S D** mozgatja, a **Space** ugrat.

```cpp
#include "../gorbe_api.hpp"

// WASD-vel irányítható alakzat, a kamerához képest (W = amerre a kamera néz), a
// talajon maradva; Space-re ugrik. A kamera mögötte, fölötte követi.
struct Jatekos : gorbe::ShapeScript {
    static constexpr float SEBESSEG = 4.0f;              // egység / s
    float z0 = 0.0f, vz = 0.0f;                          // a talaj magassága, függőleges sebesség

    void start() override { z0 = position().z; }

    void update(float, float dt) override {
        gorbe::vec3 v;
        if (key_down('W')) v += camera().forward();
        if (key_down('S')) v -= camera().forward();
        if (key_down('D')) v += camera().right();
        if (key_down('A')) v -= camera().right();
        v.z = 0;                                         // csak vízszintesen
        position() += gorbe::normalize(v) * (SEBESSEG * dt);

        // ugrás: egyszeri lenyomásra (key_pressed), utána gravitáció
        if (key_pressed(gorbe::KEY_SPACE) && position().z <= z0) vz = 6.0f;
        vz -= 15.0f * dt;
        position().z += vz * dt;
        if (position().z < z0) { position().z = z0; vz = 0; }

        // követő kamera: mögötte (a nézőirány vízszintes vetületével szemben) és fölötte
        gorbe::vec3 hatra = camera().forward();
        hatra.z = 0;
        camera().position() = position() - gorbe::normalize(hatra) * 8.0f + gorbe::vec3{0, 0, 5};
        camera().look_at(position());
    }
};
GORBE_SHAPE(Jatekos)
```

### 15. Kattintás a talajra (egér-sugár)

```cpp
#include "../gorbe_api.hpp"

// Bal kattintásra az alakzat oda ugrik, ahová a talajon (z = 0 sík) kattintottunk:
// a kamerából az egér alá mutató sugár metszése a síkkal.
struct Kattintas : gorbe::ShapeScript {
    void update(float, float) override {
        if (!mouse_pressed(gorbe::MOUSE_LEFT)) return;
        gorbe::Ray const r = mouse_ray();
        if (r.dir.z >= 0) return;                        // nem a talaj felé néz
        float const t = -r.origin.z / r.dir.z;           // origin.z + t * dir.z = 0
        gorbe::vec3 const p = r.origin + r.dir * t;
        position() = {p.x, p.y, position().z};
        print("ide: (%.1f, %.1f)", p.x, p.y);
    }
};
GORBE_SHAPE(Kattintas)
```

### 16. Keringő kamera, görgővel közelítve

```cpp
#include "../gorbe_api.hpp"

// A kamera körpályán kering az origó körül, és mindig oda néz; a görgővel közelít.
struct Keringo : gorbe::ShapeScript {
    float tav = 15.0f;

    void update(float t, float) override {
        tav -= scroll();                                 // görgő: közelebb / távolabb
        if (tav < 3.0f) tav = 3.0f;
        camera().position() = {tav * std::cos(0.3f * t), tav * std::sin(0.3f * t), 0.6f * tav};
        camera().look_at({0, 0, 0});
    }
};
GORBE_SHAPE(Keringo)
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
| `nincs 'Fal' nevu alakzat a jelenetben` | a `shape("Fal")` nem talált ilyen nevű alakzatot (a kis- és nagybetű számít) | nevezd át az alakzatot, vagy javítsd a nevet a kódban |
| `value/gradient/contains/distance csak az update()-ben hivhato` (vagy `collide ...`) | a `start()`-ban még nincsenek felépítve a képletek | tedd át az `update()`-be |
| a `key_down` mindig hamis | a jelenet nem fut, a szerkesztő kamerából nézünk (a nézet bal felső gombja), vagy az egér nincs a 3D nézet fölött, vagy egy mezőbe gépelsz | Indít, „Jelenet kamera” nézet, az egér a nézet fölött |
| (konzol, sárga) `a(z) 'Fal' pozicioja/forgatasa/merete csak akkor mozgatja, ...` | a másik alakzatot az `update()`-ben kérted le először, így a mozgatása nem látszik | kérd le a `start()`-ban |
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
- Csak a **saját jelenet** alakzatai érhetők el, a többi fülé nem.
- A kód a szerkesztő fülön (variációs szerkesztő) nem fut, csak a jelenetben.
