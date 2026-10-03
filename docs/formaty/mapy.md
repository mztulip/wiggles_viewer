# Szablony map: `Data/Templates`

Stan: struktura plików ✅, znaczenie części pól 🔶, teren (maska w `.PMP` + `BlockDB`) jeszcze nie zdekodowany. Notatki z badań: [`BAZA_WIEDZY.md`](../BAZA_WIEDZY.md) §9. Implementacja: `src/lib/map_template.cpp`, `src/lib/class_db.cpp`, przeglądarka: zakładka „Mapy” w wigview.

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
u32 blok[w*h]                ID bloku w BlockDB (0..118687)
u32 n
u16 maska[n]                 🔶 dane kodowane przebiegami (teren wysokiej rozdzielczości?)
u8  warstwa2[w*h]            🔶 0..8
```
Brak nadmiarowych bajtów we wszystkich 220 plikach.

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
