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

### Linux

A kód fordul Linuxon (ellenőrizve: g++ 13.3 / libstdc++, kis-nagybetű-érzékeny
fájlrendszeren minden fordítási egység és mind a négy teszt lefordul és lefut).
Az ablakkezeléshez viszont kellenek a GLFW szokásos rendszer-függőségei — Ubuntu/Debian:

```bash
sudo apt install build-essential cmake ninja-build pkg-config libgl1-mesa-dev \
     libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
     libwayland-dev wayland-protocols libxkbcommon-dev extra-cmake-modules
```

A beépített GLFW 3.4 alapból **X11 és Wayland háttérrel is** épül. Ha csak az egyik
kell, a másik kikapcsolható és a hozzá tartozó csomagok elhagyhatók:

```bash
cmake -S . -B build -DGLFW_BUILD_WAYLAND=OFF   # csak X11
cmake -S . -B build -DGLFW_BUILD_X11=OFF       # csak Wayland
```

> A fájlnevek kis-nagybetűje számít: a `CMakeLists.txt` forrásfájl-listája és minden
> `#include` pontosan egyezik a lemezen lévő nevekkel. Windowson egy elírás elmenne,
> Linuxon nem — ezért ez ellenőrzött.

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
| `test_raytrace` | a sugárkövetés: találat/háttér, irányfény (nincs távolság-csökkenés), tartomány-vágás, takarás, BMP-fejléc, többszálú == egyszálú; és az **anyagok**: matt vs. fényes, a csúcsfény fehér (műanyag) vagy színezett (fém), üvegen **átlátszik** a mögötte lévő alakzat (kontrollal: átlátszatlannal nem), a króm visszatükrözi a környezetét, a fa/márvány mintázata megjelenik |
| `test_build` | a jelenet-modell: a névfeloldás sorrendje (lokális → jelenet → program → korábbi alakzat), hivatkozás elhelyezett alakzatra, tartományok ÉS-kapcsolata, hibánál nem marad félkész fa, névellenőrzés, sablonok |
| `test_parser` | a nyelv: szöveg → fa → szöveg → fa oda-vissza ugyanazt adja; a **függvénytábla minden sorára** a szimbolikus derivált egyezik a numerikussal; a rövidítések (`2x`, `x**2`, `π`, `x²`, álnevek) ugyanazt jelentik; a hibaüzenet jelöl és javasol; az egyszerűsítő |
| `test_particles` | a részecske-szimuláció **GL nélkül**: a részecskék a gömbre kerülnek és egyenletesen szétterülnek, a tartományban maradnak, a részecske-plafon tart, a kontrollpont húzható |
| `test_project` | a projektfájl: mentés → betöltés minden mezőt visszaad (a második mentés betűre azonos), a betöltött projekt ugyanúgy felépül; hibás, idegen, újabb verziójú fájl érthető hibát ad; ismeretlen szín, rossz típus, túl hosszú szöveg, Unicode |
| `test_ui` | a 3D nézet koordináta-átváltása, a három szintű hatókör-feloldás/elfedés, a `t` idő (élő követés + `∂F/∂t`), és a részecske-korongok hézag-csúszkája (a rés tényleg `σ·hezag`, a szélek levágva) |
| `test_camera` | a kamera Z-up bázisa és az **egérkezelés előjelei**: jobbra húzva jobbra, felfelé húzva felfelé fordul a nézet |
| `test_transform` | eltolás/forgatás/méret és összetételük, **warpok és warp-láncok** (sorrend-függés), a gradiens szimbolikus vs. numerikus egyezése, az élő paraméterek |

Kikapcsolható: `-DGORBE_BUILD_TESTS=OFF`.

Az irányítás részletei lentebb: [Irányítás](#irányítás).

## Irányítás

A képernyőn kétféle elem látszik: a felületet mintavételező **részecskék** (kék korongok)
és a **piros kontrollpontok**.

### Kamera (nézőpont)

| Bevitel | Hatás |
|---|---|
| jobb egérgomb + húzás | nézet forgatása |
| `Alt` + bal egérgomb + húzás | nézet forgatása (alternatíva) |
| `W` / `S` | kamera előre / hátra |
| `A` / `D` | kamera balra / jobbra |
| egérgörgő | zoom (látószög 1°–45° között) |
| `Esc` | kilépés |

### Kontrollpontok

> **Jelenleg csak jelölők**: letehetők és húzhatók, de a felületre **nincsenek
> hatással**. A cikk szerinti megoldó a felület `q` paramétereit mozgatta; a képletből
> épülő felületnek ilyen paramétere nincs, ezért ott hatástalan volt, és kikerült
> (`regi-feluletek` tag). Visszakötni pl. az alakzat transzformációjára lehetne.

| Bevitel | Hatás |
|---|---|
| `Shift` + bal kattintás | új kontrollpont lerakása a kurzor helyén |
| bal kattintás egy ponton + húzás | a pont mozgatása |
| bal gomb elengedése | a pont elengedése |

Megjegyzések:
- A bal egérgomb `Alt` **nélkül** a kontrollpontoké, `Alt`-tal a kameráé — így nem ütköznek.
- A kontrollpont a kamera nézőirányára merőleges síkban mozog (a mélységet a nézet
  forgatásával lehet beállítani).
- Egy ponttól `0.5` egységnél közelebbi kattintás számít megfogásnak.

## Alakzatok szerkesztése (GUI)

A jelenetet futás közben, három ImGui-panelen lehet összerakni. Minden alakzat
**önálló** implicit felület, saját részecskékkel mintavételezve.

A panelek **fixek** (nem lebegnek): a főablak méretéhez igazodnak, és a sávok
szélessége, valamint a bal oldali vízszintes osztások **egérrel húzhatók**.

```
┌──────────────┬─────────────────────────────┬──────────────┐
│ Alakzatok    │ [Jelenet 1][Jelenet 2][ + ] │              │
├──────────────┤                             │              │
│ Nezet es     │      3D kép (textúra)       │ Tulajdon-    │
│ sugo         │                             │ sagok        │
├──────────────┤                             │              │
│ Parameterek  │                             │              │
└──────────────┴─────────────────────────────┴──────────────┘
```

A **Tulajdonságok** a jobb oldalon, **teljes magasságban** — ez a panel tartja a
képletet, a tartományt, a transzformációt és a warp-láncot, tehát ennek kell a hely.

A 3D nézet a **középső ablakban**, **fülekre** bontva: minden fül egy önálló jelenet.

| ablak | mi van benne |
|---|---|
| **Alakzatok** | felül `Új alakzat` (üres) és a **sablon-lenyíló** + `Hozzáad`, alatta a jelenet alakzatainak listája: láthatóság-pipa, név (kattintásra kijelöl), `X` a törléshez. Legalul `Indít` / `Töröl`, a **`Fenykep keszitese`** gomb, valamint a közös `d` (méretskála) és görbület-taszítás csúszka. |
| **Tulajdonságok** | a kijelölt alakzat neve, a **`szin`** és **`anyag`** lenyíló (a fényképhez), és összecsukható szekciókban: `F(x,y,z) =` képlet, **tartomány-feltétel**, **transzformáció**, **warp-lánc**, **lokális paraméterek**. |
| **Parameterek** | a **jelenet** paraméterei, a **program-szintű** paraméterek, és a jelenet **munkatere**. |
| **(középen)** | a 3D nézet, fülekre bontva — `+` gombbal új jelenet, `X`-szel bezárható. |
| **Nézet és súgó** | jelmagyarázat (melyik szín mit jelent), rács ki/be, nézet-előbeállítások, a részecske-korongok **hézag**-csúszkája és a teljes irányítás — a program használata közben végig látható. |

### Fénykép (sugárkövetés)

Az `Alakzatok` panelen a **`Fenykep keszitese`** gomb a **kamera aktuális állásából**
kirenderel egy képet, elmenti, és megnyitja a rendszer képnézegetőjében (ez az „új
ablak"). Mellette a felbontás és az árnyék kapcsolható.

A képek a **bináris melletti `kepek/` mappába** kerülnek (nem a munkakönyvtárba, mert
az indítástól függően bárhol lehet), **időbélyeges** néven — `kep_20260817_161517.bmp` —,
tehát semmi nem íródik felül. A pontos útvonalat a gomb alatt is kiírja a program.

> A `kepek/` a build könyvtárban van, amit egy `rebuild --clean-first` vagy a
> könyvtár törlése elvisz. Ha egy kép hosszabb távon kell, mentsd el máshova.

- **Metszés közelítéssel**, ahogy egy implicit felületnél kell: a sugár mentén
  lépkedünk, amíg elég közel nem kerülünk a felülethez. A lépés nem fix, hanem a
  felülettől mért becsült geometriai távolságból (`|F|/|∇F|`) adódik — üres térben
  nagyot lép, a felület közelében aprót. Előjelváltásnál 24 felezéssel finomítunk.
- **Irányfény** (vektorszerű, mint a napfény): párhuzamos sugarak, nincs helye és
  nincs távolság-csökkenés.
- **Szín és anyag** külön választható (Tulajdonságok → `szin`, `anyag`) — lásd lentebb.
  Új alakzat automatikusan a következő palettaszínt kapja. Mindkettő **csak a képre**
  hat: a valós idejű nézetben a részecskék a szokásos árnyalásukat kapják.
- A tartomány-feltétel a képen is érvényes, tehát a levágott rész ott sem látszik.
- A render több szálon fut (minden szál saját másolatot kap a lefordított
  programokból, mert egy `Program` közös munkaterületre ír). 900×600 + árnyék,
  3 alakzat: műanyaggal ~3 s, tükröző fémmel ~3,8 s, üveggel ~6,5 s.

#### Anyagok

Az anyag a **szín mellé** jön, nem helyette: ugyanaz a piros lehet matt gumi,
csillogó műanyag vagy áttetsző üveg.

| anyag | mitől ismerhető fel |
|---|---|
| **Műanyag** (alapértelmezett) | színes diffúz + **fehér** csúcsfény |
| **Gumi (matt)** | gyakorlatilag nincs csúcsfény |
| **Kerámia** | erős, kicsi, fehér csúcsfény |
| **Fém** | sötét diffúz + **színezett** csúcsfény és tükörkép |
| **Króm (tükör)** | szinte teljes tükrözés |
| **Üveg** | átlátszó, törésmutató 1.5, Beer-féle elnyeléssel |
| **Fa** | évgyűrűk a **z tengely körül** + rostozat a szál mentén |
| **Márvány** | zajjal torzított erezet |

Amit ez a rendszer megkövetel a rendertől:

- **A csúcsfény színe választja el a fémet a nemfémtől.** Műanyagnál/kerámiánál a
  csúcsfény fehér marad, fémnél felveszi az anyag színét — ez a legerősebb vizuális
  jelzés arról, hogy mit lát az ember.
- **Tükrözéshez és átlátszósághoz tovább kell követni a sugarat**, ezért a
  színszámítás rekurzív (`radiance`). Üvegnél Fresnel-arányban (Schlick) ágazik
  visszavert és megtört sugárra, a törésmutató iránya a be-/kilépéstől függ, teljes
  visszaverődésnél pedig csak a tükrözés marad.
- **Az elágazást súly szerint vágjuk**, nem mélység szerint: minden sugár viszi
  magával, hogy mekkora résszel számít bele a pixelbe, és a `min_weight` (0.05) alatti
  ág elhal. Egy üveg homlokfelületén a visszavert ág súlya ~0.04, tehát azonnal
  megáll, míg az átmenő ág ~0.96-tal megy tovább — így a 2^mélység szétágazás helyett
  a gyakorlatban egyetlen fő út marad. Ezért engedhető meg a `max_depth = 8`, és kell
  is: egy tömör üveggömbben a teljes visszaverődés több oda-vissza utat okoz, és ha
  ezek elfogynak, sötét foltok maradnak a helyükön. (Mérve, 900×600, 3 alakzat:
  a súlyvágás 0.02 → 0.05 az üveget 10,8 s-ról 6,5 s-ra viszi, a képen látható
  különbség nélkül; a mélység 8 → 3 levágása ennél kevesebbet spórol *és* rontja a képet.)
- **Az átlátszó test nem vet koromfekete árnyékot**: az árnyéksugár nem „talált/nem
  talált", hanem fényláthatóságot ad vissza, amit az áttetsző takaró csak tompít.
- A **mintázatok** (fa, márvány) eljárásosak, textúrafájl nélkül: a térbeli pontból
  számol egy hash-alapú value-noise, ezért minden alakzatra torzulás nélkül feszül rá,
  és két render között sem sercen. A fa évgyűrűi `|sin(π·r)|`-ből jönnek és nem a
  törtrészből: utóbbinak ugrása van minden egésznél, ami tördelt, aliasos élt adna.
- A műanyag is tükröz egy keveset (0.04, a dielektrikumok Fresnel-értéke), ami
  **súrló szögben felerősödik** — ettől lesz a padlónak/nagy lapos alakzatoknak
  tükröződése. Ez a rész a `min_weight` fölött marad, a szemből alig látszó
  tükörképet viszont elhagyjuk.

**Leválasztás:** töröld a `raytrace/` mappát, a `CMakeLists.txt`-ből a `raytrace/*`
sorokat és a `test_raytrace`-t, a `main.cpp`-ből a három `#include "raytrace/..."`
sort, a `Fenykep` gombot kezelő blokkot, a `Shape::color_idx`/`material_idx`/`dom_tree`
mezőket és a `szin`/`anyag` lenyílót. Semmi más nem hivatkozik rá — a komponens nem függ
sem OpenGL-től, sem az ablaktól, sem a jelenet-modelltől, csak a `matek/`
kifejezésrendszertől.

### `t` — az idő, animációhoz

A `t` beépített név: **paraméterként nem vehető fel** (foglalt), viszont bármelyik
képletben használható, és a program indulása óta eltelt időt adja másodpercben.

```
x^2 + y^2 + (z - 2*sin(t))^2 - 4          fel-le mozgó gömb
unio(x^2+y^2+z^2-1, (x-3*cos(t))^2+y^2+z^2-1)   keringő második gömb
```

- A kifejezésfa a `t` **címét** tárolja, ezért az idő múlása magától hat: nincs
  újraparseolás, újraderiválás vagy újraindítás.
- A részecskék **együtt mozognak** a felülettel. Ehhez a Witkin-lépés számlálójában
  szerepel a `∂F/∂t` tag is (`Surface::F_dt`) — enélkül csak a `PHI*F` visszacsatolás
  húzná vissza őket, ami láthatóan lemaradna a mozgó felület mögött. Ha a képlet nem
  hivatkozik `t`-re, a szimbolikus deriválás konstans 0-t ad, tehát ez ingyen van.
- Egyetlen, **program-szintű** óra van (nem fülönkénti). Ebből következik, hogy egy
  háttérben álló fül alakzata visszaváltáskor a megváltozott `t`-hez ugrik.

### Jelenetek (fülek)

Minden fül egy **önálló jelenet**: saját alakzatok, saját paraméterek, saját munkatér,
**saját kamera** (a nézet fülváltáskor megmarad) és saját mintavételezők.

A **háttérben lévő fülek szimulációja áll** — a részecskék állapota megmarad, tehát
visszaváltáskor onnan folytatódik. Így az FPS nem függ attól, hány fül van nyitva. Az
inaktív fülek a bevitelt sem kapják meg; enélkül minden fül kamerája együtt mozogna.

### Paraméterek — három hatókör

```
alakzat lokálisai  →  jelenet paraméterei  →  program-szintű paraméterek
```

Az **első találat nyer**, tehát a belső **elfedi** a külsőt, ugyanúgy, mint C++-ban.
Ez nem hiba: a panel halványan kiírja a paraméter mellé, hogy `elfedi: jelenet` vagy
`elfedi: program`.

Ami **hiba** marad: két azonos nevű paraméter **ugyanazon a szinten**, foglalt név
(`x`, `pi`, `sin`…), és két azonos nevű alakzat egy jeleneten belül.

> A paraméterek **előbb** oldódnak fel, mint az alakzatnevek, tehát egy paraméter egy
> azonos nevű alakzatot is elfed — ezt `elfedi: alakzat` jelzi.

Minden paraméter két sorban jelenik meg:

```
[név      ] [pontos érték] [X]
[min] [=======o========] [max]
```

A **csúszka** élőben hat (újraindítás nélkül), a **pontos érték** mezőbe bármi
beírható: ha kilóg a tartományból, a `min`/`max` magától kitágul hozzá. A `min` és
`max` átírásakor a fordított határokat a program megcseréli, és az értéket a
tartományba húzza. Alapból −10…10; a forgatás-warpok fokban −180…180, a skálázás
0.1…5 tartománnyal indul.

### Árnyalás

A részecske-korongok árnyalva rajzolódnak, hogy a felület formája térben olvasható
legyen. A normálishoz nem kell külön számítás: az a felület gradiense (`∇F`), amit a
szimuláció amúgy is kiszámol, és a korong forgatásához már eddig is használt.

- `Model` kapott egy **opcionális** normál-attribútumot (1-es hely). Ha egy modell nem
  tölt fel normálisokat, az attribútum **letiltva** marad — a letiltott attribútum
  konstans `(0,0,0,1)`, és a fragment shader ebből tudja, hogy árnyalás nélkül,
  egyszínűen kell rajzolnia. Így a tengelyek, a rács és a feliratok ugyanazt a
  shadert használhatják, mint a részecskék.
- A megvilágítás „half-Lambert": a `dot` `[0,1]`-re képződik le a szokásos
  `max(0,dot)` helyett, így a fénytől elforduló korongok sem esnek egyetlen
  egyenletes sötét foltba, és a forma végig olvasható marad.

### A korongok mérete — `hezag` csúszka

A korong sugara alapból `σ/2`. Ez nem véletlen: a Witkin-féle taszítás a szomszédokat
nagyjából `σ` távolságra állítja be, tehát `σ/2` sugárnál a korongok **éppen összeérnek**,
és a felület folytonos hártyának látszik. Ez viszont el is fedi a mintavételezést: nem
látszik, hol vannak valójában a pontok és mennyire egyenletes az eloszlásuk.

A `Nézet és súgó → Nézet → hezag` csúszka ezt állítja: a szomszédos korongok szélei közt
látszó rés `σ·hezag` lesz. `0` = éppen összeérnek, negatív = átfedők (tömörebb felület),
`0.9` = apró pontok. A logika `particle_sampling/ParticleView.hpp`-ben van, GL nélkül
tesztelve (`test_ui` 7. szakasz).

> Ez **csak a rajzolás**. A részecskék helyét, a `σ`-t és a taszítást nem érinti, tehát a
> csúszka mozgatása nem indítja újra és nem billenti ki a mintavételezést — a **tényleges**
> részecsketávolságot a `d` csúszka állítja. A kontrollpontokra szándékosan nem hat: azok
> mérete fix, mert az egérrel való elkapásuk sugara is fix.

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
zárójelezni). Függvények (a teljes, mindig naprakész lista a `matek/Fuggvenyek.hpp`
táblája; a Tulajdonságok panelen a függvénylista fölé víve az egeret is megjelenik):

| kategória | függvények |
|---|---|
| beépített nevek | `x` `y` `z` (hely), **`t`** (idő, másodperc), `pi` `e` |
| trigonometria | `sin cos tan`/`tg` `ctg`/`cot` `asin`/`arcsin` `acos`/`arccos` `atan`/`arctg`/`arctan` `atan2(y, x)` |
| hiperbolikus | `sinh cosh tanh` |
| hatvány, gyök, log | `exp ln log`/`lg` `sqrt cbrt` `pow(a, b)` `hypot(a, b)` `length(a, b[, c])` |
| előjel, kerekítés | `abs sign`/`sgn` `floor ceil round fract mod(a, b)` |
| átmenetek | `clamp(u, lo, hi)` `step(e, u)` `smoothstep(e0, e1, u)` `mix`/`lerp(a, b, s)` |
| éles halmazműveletek | `unio(a,b)` `metszet(a,b)` `kulonbseg(a,b)` `min(a,b)` `max(a,b)` |
| sima halmazműveletek | `sunio(a,b[,k])` `smetszet(a,b[,k])` `skulonbseg(a,b[,k])` `smin` `smax` |

Az angol nevek is működnek: `union`, `intersect`, `subtract`, `sunion`, `sintersect`,
`ssubtract`. A sima műveleteknél a `k` a lekerekítés mértéke (elhagyva `0.5`).

**Kényelmi írásmód** — ugyanazt jelenti, mint a hosszú alak:

| így is írható | jelentése |
|---|---|
| `2x`, `3(x+1)`, `(x+1)(x-1)`, `2 pi x`, `r(x+1)` | szorzás (implicit) |
| `x**2` | `x^2` |
| `-8`, `x > -8` | előjel bárhol (nem kell `0 - 8`) |
| `π`, `x²`, `y³`, `x · y` | `pi`, `x^2`, `y^3`, `x*y` |

**Hibaüzenet**: megmutatja a hiba helyét, és ha elgépelésnek tűnik, javasol:

```
ismeretlen fuggveny: sni - erre gondoltal: sin?
  x^2 + sni(y)
        ^
```

**Saját függvények** (Paraméterek panel → *Jelenet függvényei*): `név(paraméterek) =
törzs`, például `g(u, k = 1) = u^2 + k`, és utána a képletekben `g(x) + g(y, 2)`. Az
alapértékes paraméter elhagyható. A törzs a jelenet- és program-szintű paramétereket
és a `t`-t látja, és a listában **nála korábbi** függvényeket hívhatja (így rekurzió
nem lehet). A képletbe a kifejtett törzs épül be.

### Tér-transzformáció (eltolás / forgatás / méret)

Alakzatonként megadható pozíció, forgatás és méret a Tulajdonságok panelen — nem kell
a képletbe írni, hogy `(x-3)^2 + ...`.

Az alakzatot nem „mozgatjuk": a **teret** transzformáljuk. Ha a lokálisból a világba a
`p_világ = T + R·(S·p_lok)` leképezés visz, akkor a világbeli egyenlet

```
F_világ(p) = F_lok( w(p) ),    w(p) = S⁻¹ · Rᵀ · (p − T)
```

vagyis az **inverz** leképezést helyettesítjük be `F`-be
(`Kifejezes::substitute`, lásd `particle_sampling/Transform.hpp`). A deriváltakkal nem
kell külön foglalkozni: a szimbolikus deriválás a láncszabályt magától elvégzi — a
BlobTree-cikk 3.4-e ehhez explicit Jacobi-mátrixot számol, itt ez ingyen van.

Néhány következmény:

- A transzformáció paraméterei **cím szerint** épülnek be, ezért a csúszkák **élőben**
  mozgatják az alakzatot: nincs újraparseolás és újraderiválás.
- Egységtranszformációnál a warp **nem épül be**, hogy az egyszerű alakzatok olcsók
  maradjanak (a gömb programja 15 utasítás a warpos 153 helyett). Ezért amikor először
  nyúlsz a vezérlőkhöz, a program egyszer újraépíti az alakzatot — utána élő.
- Az alakzat **saját tartomány-feltétele** vele együtt mozog (a „véges hosszú henger"
  végei a hengerrel), a **globális tartomány** viszont nem — az a világ munkatere.
- A rá **hivatkozó** későbbi alakzatok már az elhelyezett formát látják, tehát két
  elhelyezett gömb uniója a helyükön lesz.

| képlet | program (utasítás) | transzformálva |
|---|---|---|
| gömb | 15 | 153 |
| tórusz | 45 | 203 |
| sima unió | 106 | 271 |

### Warpok (tér-deformációk, láncban)

A fenti affin transzformáció mellé alakzatonként megadható egy **warp-lánc**. Egy warp
**három kifejezés** — `x'`, `y'`, `z'` —, amiket `x`, `y`, `z` helyére helyettesítünk:

```
F_warpolt(p) = F( x'(p), y'(p), z'(p) )
```

Semmi több: ugyanaz a `substitute` gépezet, mint a transzformációnál, tehát a
deriváltakat itt sem kell külön kezelni.

**A jelentés fontos:** a három kifejezés a **visszafelé** (tér → alakzat) leképezés —
„hol keressük ki az alakzatot ehhez a térbeli ponthoz". A tér warpolásához mindig az
inverz leképezés kell (Barr 1984; BlobTree 3.4: *„we wish to warp space, thus we use the
inverse warp function"*). Ezért a sablonok a deformáció **inverzét** tartalmazzák.

Sablonok a `Warpok (lancban)` szekció lenyílójából — a paramétereik automatikusan
bekerülnek az alakzat lokálisai közé, ütközésmentes néven, tehát csúszkával
állíthatók és **élőben** hatnak:

| sablon | `x'` | `y'` | `z'` |
|---|---|---|---|
| **Eltolás** | `x − tx` | `y − ty` | `z − tz` |
| **Forgatás z körül** | `x·cos(r) + y·sin(r)` | `−x·sin(r) + y·cos(r)` | `z` |
| **Forgatás x körül** | `x` | `y·cos(r) + z·sin(r)` | `−y·sin(r) + z·cos(r)` |
| **Forgatás y körül** | `x·cos(r) − z·sin(r)` | `y` | `x·sin(r) + z·cos(r)` |
| **Skálázás** | `x/sx` | `y/sy` | `z/sz` |
| Csavarás (twist) z körül | `x·cos(a·z) + y·sin(a·z)` | `−x·sin(a·z) + y·cos(a·z)` | `z` |
| Kúposítás (taper) z mentén | `x/(1+k·z)` | `y/(1+k·z)` | `z` |
| Nyírás (shear) x-ben | `x − k·z` | `y` | `z` |
| Hullám (wave) z-ben | `x` | `y` | `z − a·sin(w·x)` |
| Egyedi (üres) | `x` | `y` | `z` |

Az **eltolás / forgatás / skálázás** ugyanaz, mint a `Transzformacio` szekció — de a
**láncba illeszthető**, tehát tetszőleges sorrendben keverhető a deformációkkal
(pl. csavarás → eltolás → újabb csavarás). Amelyiket mikor érdemes:

| | `Transzformacio` szekció | warp-sablonként |
|---|---|---|
| élő (nincs újraparseolás) | ✔ | ✔ |
| kényelmes vezérlők (fokos csúszka, `DragFloat3`) | ✔ | — |
| sorrend a deformációk közé | — | ✔ |

A forgatás-sablonok **fokban** várják a szöget (a képletben `pi/180` váltja radiánra;
a `pi` a parser beépített állandója). A skálázás alapértéke `1` — **`0` nem lehet**,
mert osztás van benne.

Megjegyzések:

- A lánc **első** eleme hat először az alakzatra; a `^` / `v` gombokkal átrendezhető,
  a pipával egyenként ki-be kapcsolható. A sorrend számít.
- A lánc **után** jön az affin transzformáció, tehát a warpok az alakzat **saját**
  terében dolgoznak, és a kész, deformált alakzatot mozgatja a pozíció/forgatás/méret.
- Az alakzat saját tartomány-feltétele ugyanezt a láncot kapja, tehát a levágott rész
  együtt deformálódik.
- A kúposításnál `1 + k·z = 0` helyen a kifejezés szinguláris — ezt a csúszka
  tartományával kerüld el.
- A **bend** (hajlítás) szándékosan nincs a sablonok között: az inverze `atan2`-t
  igényelne, ami a kifejezésrendszerben még nincs meg.

A lánc a fát gyorsan növeli, a **lefordított program viszont csak lineárisan** — a
közös részkifejezés-kiemelés visszaszedi a nagy részét (mérve, gömbre):

| lánc hossza | fa (karakter) | program (utasítás) |
|---|---|---|
| 0 | 43 | 15 |
| 1 | 137 | 94 |
| 2 | 325 | 181 |
| 3 | 701 | 274 |

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

#### Globális tartomány (munkatér)

A `Globális paraméterek` panelen megadható egy **globális tartomány** is: az a térrész,
amiben egyáltalán értelmezzük az alakzatokat. Ez **minden** alakzatra érvényes, a saját
tartomány-feltételével `and` kapcsolatban — a program a két feltételt
`(globális) and (saját)` alakban fűzi össze. Üresen hagyva korlátlan.

Két gyorsgomb tölti ki a szokásos munkatereket (a `±8`-as talajrácshoz igazítva):

| gomb | feltétel |
|---|---|
| Doboz | `x > -8 and x < 8 and y > -8 and y < 8 and z > -8 and z < 8` |
| Gömb | `x^2 + y^2 + z^2 < 64` |

Ez a legegyszerűbb védelem a végtelen alakzatok ellen: a sík vagy a henger a
munkatér határáig mintavételeződik, és nem termel korlátlanul részecskét. A globális
tartomány csak **globális paramétert** használhat (alakzatnevet nem — az körkörös lenne).

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
> itt: a gyök argumentuma pont a varraton 0, és a `sqrt(u)`
> deriváltja ott végtelen — a keletkező NaN a taszításon keresztül az összes
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
  mindenhol végesek legyenek. Ugyanezért polinomiálisak a többiek is: a `sqrt(u)`
  deriváltja `u=0`-ban végtelen — ez pont a felületen (F=0) lenne baj. A blendben a gyök alatt mindig ott a `+k²`, ezért az jó.
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

## Projekt mentése és betöltése

**Fájl** menü: *Új projekt*, *Megnyitás…* (`Ctrl+O`), *Mentés* (`Ctrl+S`), *Mentés
másként…*. A projekt egy olvasható JSON-fájl (`.gorbe.json`); az első mentést a
program a bináris melletti `projektek/` mappába javasolja. A menüsoron látszik az
aktuális fájl neve.

Ami bekerül: a program-szintű paraméterek, és jelenetenként a név, a munkatér, a `d`
és a görbület, a kamera állása, a paraméterek (tartománnyal), a saját függvények, és
az objektumok minden beállítása (képlet, tartomány, láthatóság, szín, anyag, saját
`d`, transzformáció, warpok, lokális paraméterek). Ami nem: a részecskék és a
felépített képletek — betöltéskor a program minden jelenetet magától felépít.

```json
{
  "format": "gorbe-projekt",
  "version": 1,
  "program_params": [ { "name": "g1", "value": 1.0, "min": -10, "max": 10 } ],
  "scenes": [ {
      "name": "Jelenet 1", "domain": "", "d": 2.0, "curvature": 1.0,
      "camera": { "eye": [-14, -14, 16], "target": [0, 0, 0], "fov": 45 },
      "params": [], "functions": [ { "name": "g", "params": "u", "body": "u^2" } ],
      "objects": [ {
          "name": "gomb1", "formula": "x^2 + y^2 + z^2 - r^2", "domain": "",
          "visible": true, "color": "Kek", "material": "Uveg", "own_d": false, "d": 2.0,
          "transform": { "pos": [0, 0, 0], "rot_deg": [0, 0, 0], "scale": [1, 1, 1] },
          "warps": [], "params": [ { "name": "r", "value": 1.0, "min": -10, "max": 10 } ]
      } ]
  } ]
}
```

- A **szín és az anyag név szerint** kerül a fájlba, így a paletta bővítése nem
  színezi át a régi projekteket. A forgatás fokban van (olvashatóbb).
- A betöltés **hibatűrő**: a hiányzó mező alapértéket kap, az ismeretlen szín/anyag
  az elsőt, a túl hosszú szöveg levágódik — ezekről figyelmeztetés jelenik meg az
  Alakzatok panelen, de a projekt betöltődik. Nem-JSON, más formátumú vagy újabb
  verziójú fájlnál hibaüzenet jön, és a régi projekt érintetlen marad.
- Az **Esc** kilép a programból, de csak ha épp nem egy ablakot (pl. a mentés
  ablakát) vagy egy szövegmezőt zár be.

## Régi, fordítási idejű felületek

Korábban a felületeket C++ osztályként is meg lehetett adni (`Sphere`, `Torus`,
`Ellipsoid`, `Ellipse`, `Circle`, `Cylinder`, `Teszt`), saját referencia-meshsel
(`particle_sampling/Occluders.hpp`) és `q`-paraméteres kontrollpont-vezérléssel. A
program már csak a képletből épülő felületet használja, ezért ezek kikerültek. Az
utolsó állapotuk a **`regi-feluletek`** git tagen érhető el:

```bash
git show regi-feluletek:particle_sampling/Surface.hpp
git show regi-feluletek:particle_sampling/Occluders.hpp
```

---

## Új beépített függvény felvétele

Egyetlen sor a `matek/Fuggvenyek.hpp` táblájában — a parser, a kiírás, a deriválás, a
lapos program, a foglalt nevek és a súgó mind innen dolgozik, a `test_parser` pedig
magától ellenőrzi a deriváltját:

```cpp
// FUNCS: saját csomópont, a derivált a parser nyelvén (u = az argumentum)
{.name = "atan", .aliases = "arctg arctan", .f1 = [](float u) { return std::atan(u); },
 .du = "1/(1 + u^2)", .help = "arkusz tangens"},

// MACROS: ha más függvényekből felírható, elég a törzse (a derivált magától adódik)
{"hypot", "", "a, b", "sqrt(a^2 + b^2)", "atfogo"},
```

Kétargumentumúnál `.f2` és `.dv` is kell (`u`, `v` a két argumentum).

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
| `particle_sampling/Surface.hpp` | a felület: F, deriváltak, lefordított programok, tartomány |
| `particle_sampling/ParticleSystem.hpp` | a szimuláció (taszítás, fisszió, halál) és `SimParams` — **GL nélkül** |
| `particle_sampling/Particle.hpp` | egy részecske állapota (tiszta adat) |
| `object/` | **a térbeli objektum, MVC szerint** (lásd lent) |
| `object/SpaceObject.hpp` | egy alakzat a térben: `model()` + `view()` + `controller()` |
| `object/ObjectView.hpp` | NÉZET: a részecskék korongokként (`ParticleDisks : Model`) |
| `object/ObjectController.hpp` | VEZÉRLŐ: az óra (mikor lép a szimuláció) és a kontrollpont-bevitel |
| `matek/` | a szimbolikus kifejezés-/deriválórendszer (Kif DSL) |
| `matek/Kif.hpp` | a `Kif` burkoló és a C++ DSL (operátorok, `sin`, `min`, …) |
| `matek/Parser.hpp` | string ↔ kifejezésfa, **mindkét irányban**: `make_kif(szöveg, resolver, funcs)` és `kif_text(fa, namer)`; implicit szorzás, hibaüzenet javaslattal |
| `matek/Fuggvenyek.hpp` | **a függvénytábla**: minden beépített függvény egy sor (név, álnevek, kiértékelés, derivált, súgó) — lásd lent |
| `matek/Program.hpp` | a kifejezésfa lapos, futtatható alakja (közös részkifejezés-kiemeléssel) |
| `particle_sampling/SpatialGrid.hpp` | egyenletes rács a taszítás szomszédkereséséhez |
| `matek/muveletek/{Minimum,Maximum}.hpp` | éles halmazműveletek (CSG) csomópontjai |
| `matek/fuggvenyek/{Abs,Elojel}.hpp` | `abs` / `sign` — ezekre épül a min/max deriváltja |
| `particle_sampling/DomainConstraint.hpp` | a tartomány-feltétel matematikája (felület menti csúsztatás + perem-fal) |
| `particle_sampling/Transform.hpp` | tér-transzformáció: a világ→lokális leképezés behelyettesítése |
| `particle_sampling/WarpPresets.hpp` | a warp-sablonok (a teszt pontosan ezt az adatot ellenőrzi) |
| `model/`, `utils/` | OpenGL-réteg (kamera, ablak, shader, Model) |
| `model/Gui.{hpp,cpp}` | Dear ImGui wrapper (init/frame/render, input-szűrés) |
| `model/CameraBasis.hpp` | a kamera Z-up bázisa és az egérkezelés előjel-konvenciója (GL nélkül, tesztelhetően) |
| `model/Viewport.hpp` | a 3D nézet téglalapja és a koordináta-átváltás; a bemenet-kapu |
| `model/Framebuffer.hpp` | képernyőn kívüli rajzolási cél (a nézet textúrája) |
| `main.cpp` | csak összeköti az MVC három rétegét (lásd lent) |
| `scene/` | **MODEL**: GL és ImGui nélkül, tesztelhető |
| `scene/Document.hpp` | a jelenet adatai: `Warp`, `Shape`, `SceneDoc` |
| `scene/Build.hpp` | a képletek felépítése: névfeloldás, warp-lánc + transzformáció, tartományok |
| `scene/Validate.hpp` | névellenőrzés és elfedés-jelzés |
| `scene/Presets.hpp`, `scene/Names.hpp` | alakzat- és warp-sablonok felvétele; szabad nevek, foglalt nevek |
| `scene/ProjectFile.hpp` | a projektfájl (JSON) írása és olvasása, GL nélkül |
| `ui/MenuBar.hpp` | a Fájl menü és az útvonal-ablak |
| `scene/Scope.hpp` | a három szintű hatókör-feloldás (elfedéssel) |
| `scene/Time.hpp` | a képletekben használható `t` idő |
| `app/` | **CONTROLLER**: `Controller` (a fülek gazdája, a kérések végrehajtása), `Scene` (kamera + mintavételezők), `Photo` |
| `ui/` | **VIEW**: egy fájl panelenként; `Requests.hpp` a nézet → vezérlő kérések, `Layout.hpp` az elrendezés |
| `raytrace/` | **leválasztható** sugárkövető komponens (renderer, paletta, anyagok, BMP-mentés) |
| `tests/` | ctest-tesztek (GL nélkül futnak) |
| `libraries/` | GLFW, GLM, GLAD, Dear ImGui (`imgui`), nlohmann/json 3.11.3 (`nlohmann/json.hpp`, MIT) |

**A térbeli objektum** (`object/SpaceObject.hpp`) szintén MVC: a modell
(`ParticleSystem`) a felület és a részecskék, a szimulációval — nem tud a GL-ről és az
óráról. A nézet (`ObjectView`) csak olvassa, és a meglévő `Model` ősosztállyal rajzol; a
vezérlő (`ObjectController`) lépteti a modellt és kezeli a kontrollpontok bevitelét.

A frontend szabálya: a nézet (`ui/`) élőben írhatja az **értékeket**, és a listák végére
is szúrhat, de amire egy kifejezésfa címmel mutathat (paraméter, alakzat, jelenet), azt
sosem törli — csak kéri (`Ui::Requests`). A törlést a `Controller::apply` végzi a frame
végén, előtte eldobja a fákat. Így a „törlés előtt kötelező a drop” szabály egy helyen él.

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