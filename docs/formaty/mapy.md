# Szablony map: `Data/Templates`

Stan: struktura plików ✅, **teren (mapa wysokości z BlockDB) ✅**, znaczenie części pól 🔶 (rodzaj gruntu, maska w `.PMP`, warstwy). Notatki z badań: [`BAZA_WIEDZY.md`](../BAZA_WIEDZY.md) §9. Implementacja: `src/lib/map_template.cpp`, `src/lib/class_db.cpp`, przeglądarka: zakładka „Mapy” w wigview.

## Jak gra buduje mapę ✅
1. `map create 64 64` tworzy pustą mapę 64×64 bloków (512×512 jednostek mapy, 8 na blok).
2. Skrypt szablonu `Data/Templates/<nazwa>.tcl` definiuje `proc MapTemplateSet {xoffset yoffset}`. Wywołanie `MapTemplateSet x y` nakłada szablon z lewym górnym rogiem w bloku (x, y):
   ```tcl
   map_template gng_021.pmp $xoffset $yoffset          ;# teren (czasem pełna ścieżka data/templates/...)
   map_setlayer2 $xoffset $yoffset urw_gng_021_a.l2m   ;# druga warstwa
   loading_objdetail high                              ;# poziom szczegółów kolejnych obiektów
   sel /obj
   set objhandle [new Dummy_Moos_um_a "" "[expr 2.53 + $xoffset] [expr 2.5 + $yoffset] 13.54" "0 6.12 0"]
   set_owner $objhandle -1 ; set_physic $objhandle true ; set_autolight $objhandle false ; set_boxed $objhandle false
   ```
3. Nazwy: `<świat>_<rodzaj>_<nr>_<wariant>`. Światy: `urw` (dżungla), `kris` (kryształy), `lava`, `swf` (siarka), `unq_*` (unikalne), `multi_*`. Rodzaje: `gng` (korytarz), `hol` (jaskinia), `gsg`, `caves`, `start`.

## Układ współrzędnych ✅
- Pozycja obiektu `x y z` jest w **blokach**. x rośnie w prawo, y w dół, z w stronę widza. Przednia płaszczyzna skały leży przy z ≈ 15, a obiekty w korytarzach mają z ≈ 13–15.
- 1 blok = **200 jednostek modelu `.3db`** (wzorzec: OBJ z `save_map`). Krasnal ma ok. 1 blok wysokości.
- Rotacja `rx ry rz` w radianach. W praktyce używana jest tylko `ry`.
- Kamera gry patrzy z +z w stronę −z (VRML z `save_scene`: kamera w z = 4000).

## `.PMP` ✅ struktura
```
u32 w, u32 h                 rozmiar w blokach
u8  typ[w*h]                 🔶 np. 02, 04; 0x84 we wnętrzu jaskini unq_menue (bit 0x80 = wydrążenie?)
u32 blok[w*h]                ID bloku w BlockDB (0..118687) -> kształt terenu (patrz niżej)
u32 n
u16 maska[n]                 🔶 przebiegi; wiersze po 32·w+1 próbek, 8 wierszy na blok. W szybie GNG_007
                             to pasek szer. 9 pośrodku szybu, więc najpewniej ścieżka ruchu postaci, a nie kształt terenu
u8  warstwa2[w*h]            🔶 0..8
```
Brak nadmiarowych bajtów we wszystkich 220 plikach.

## Teren: `BlockDB0/1/2.BDB` ✅
```
u32 11, u32 0, u32 118688 (liczba bloków), u32 0
u32 ×4                                   🔶 (BDB0: 561168, 17564, 1411, 4)
ILT: 118688 × 3 × (u16 a, u32 b)         odwołania do szczegółowych siatek 3D bloku (🔶)
rekordy bloków: 118688 × 89 B
  u8  głębokość[8][8]                    wiersze od góry; 15 = lita skała, mniej = wydrążenie
  u8  extra[16]                          🔶 4 × u32
  u8  tail[9]                            🔶 4 B (w ~47% równe narożnikom siatki) + 5 B flag
dalej: szczegółowe siatki 3D (listy indeksów u16, wierzchołki) 🔶
```
- Rekordy bloków są **identyczne w trzech plikach**. Pliki różnią się tylko siatkami 3D, a gra wybiera plik ustawieniem `LandScapeQuality` (0–2).
- **Mapa wysokości szablonu:** blok (bx, by) z `.PMP` daje próbki (bx·8+sx, by·8+sy) = `głębokość[sy][sx]`. Próbka ma 25 jednostek (1/8 bloku).
- **Głębokość wydrążenia = (15 − v) · 100 jednostek**. W układzie świata przednia płaszczyzna leży przy z = 3000 (15 bloków), a próbka v ma z = 3000 − (15 − v)·100. Przykład z korytarza `GNG_001`: bloki 76–79 (sufit: f, f, e/d/c, b…), blok 80 (cały b = głębokość 400), blok 81 (b u góry, f na dole).
- ✅ Weryfikacja: wierzchołki OBJ z `save_map` gry dla trzech szablonów zgadzają się z naszą mapą (253 dokładnie, 20 na krawędziach pionowych ścian, 12 to punkty pośrednie ścian).

### Tekstury gruntu (`Data/Ground`)
`Gnd_A01..15` (szara skała), `Gnd_B01..15` (ziemia), `Gnd_C01..15` (piasek): 64×64, alfa pusta. `Msk_<ABCD>01` to maski przejść między rodzajami. `detail/<świat>_<bity>` to nakładki z alfą (np. mech), gdzie 4 bity oznaczają sąsiedztwo. 🔶 Który rodzaj gruntu ma blok, jeszcze nie wiemy. Przeglądarka tymczasowo używa A dla przedniej płaszczyzny i B dla wydrążeń.

## `.l2m` ✅ struktura
```
char[4] "M2L\0"
u32 w, u32 h
u16 wartość[w*h]             🔶 (np. 0x0500, 0xC0xx)
```
Plik może mieć 0 B (8 przypadków), co oznacza brak warstwy.

## `.ENT`
Zwykle puste (0 B). 🔶

## Klasy obiektów → modele `.3db` ✅
- `Data/Scripts/classes/deco/dummy_<plik>.tcl` zawiera listy `SetDummyClassesAll`, `SetDummyClassesNoZ`, `SetFrontDummyClassesNoPhys`, `SetCaveSkinClasses`:
  ```tcl
  SetDummyClassesAll {} {
  { Lava_seule_a lava_seule_a }                          ;# klasa Dummy_Lava_seule_a -> obiekt lava_seule_a
  { Gras_03 {gras_03_a gras_03_b gras_03_c gras_03_d}}   ;# warianty (gra losuje)
  }
  ```
  Klasa to `Dummy_<pierwsze słowo>`, a animacja to `<obiekt>.standard`. Plik `dummy_lavawelt.tcl` wskazuje model `Data/3db/LAVAWELT.3DB`.
- `SetWeaponClasses { {Axt_1 axt_01} … }` w `items/waffen.tcl` tworzy klasy bez prefiksu.
- Pozostałe klasy: `def_class <Klasa> … { … class_defaultanim <obiekt>.<animacja> … }`.
- Te reguły przypisują model do 86,9% z 33 905 obiektów we wszystkich szablonach. Bez modelu zostają głównie znaczniki (`Info_*`, punkty trybów multiplayer `CS_*`, `CF_*`) i klasy ustawiające animację dopiero w kodzie.
- Szablony jaskiń (`*_caves_*`) mają „skórę” w postaci dużych obiektów `Dummy_<świat>_caves_*`, czyli gotowych siatek ścian jaskini w trzech poziomach szczegółów.

## Wzorce z oryginału (laboratorium)
`save_map {x y w h} plik` (OBJ z mapą wysokości szablonów), `save_scene` (VRML), `data/userstartup.tcl` (własny układ szablonów). Patrz `tools/lab/`.
