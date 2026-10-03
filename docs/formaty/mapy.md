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

## Składanie świata ✅
Prawo nakładania szablonu (`FUN_005992b0` w exe), za [Zimennik/Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) (MIT, `BlitPmpIntoWorld`). Implementacja: `src/lib/map_world.cpp`.
- `map create W H` tworzy świat W×H bloków: blok 0 (lita skała), typ 0 (klasa gruntu jak typ 2).
- `MapTemplateSet x y` nakłada szablon w bloku (x, y). Pole szablonu jest **przezroczyste**, gdy w oknie 2×2 (pole, +x, +y, +x+y; pola poza szablonem pomijane) wszystkie typy & 0x7F == 0, i wtedy świat pod nim zostaje. W przeciwnym razie nadpisywane są ID bloku, typ i warstwa 2. W `.l2m` wartości 0 nie nadpisują świata.
- ⚠ **Korekta wcześniejszego wniosku:** w eksperymencie z korytarzem o samych typach 0 gra pokazała litą skałę, bo **wszystkie pola były przezroczyste** (widać było domyślny świat). Typ 0 nie oznacza „niewykopanego tunelu”. Pojedyncze pola typu 0 obok niezerowych są zapisywane razem z kształtem bloku.
- **Scenariusze** (`Data/Gamesave/*.tcl`): mapy stałe to `map create` i seria `call templates/X.tcl` + `MapTemplateSet x y` (np. `single_Knockersdorf` z 46 szablonami, `oberwelt.tcl` z samouczka, `single_wassermatch`, `single_Test_Lorelei_Brains` z 159). Skrypty mogą wołać inne przez `call data/gamesave/…`. Kampania i sandbox (`StoryMgr load_level` / `load_sandbox`) generują świat proceduralnie (komendy `lg_*`, Diggles-Unity `WORLDGEN_ORIGINAL_RE.md`); tego jeszcze nie obsługujemy.

## Kamera gry (`set_view`) ✅
`set_view x y zoom dx dy` (np. menu: `set_view 32.368 40.858 1.38 -0.355 0.165`). Prawo z exe za [Zimennik/Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) (MIT, `DigglesCameraController.cs`), w układzie gry w blokach (x w prawo, y w dół, z do widza):
```
cel = (x, y, 14)                  R = zoom·16 − 10
oko = cel + R·(cos dx·sin(−dy), sin dx, cos dx·cos dy)
góra = (sin dx·sin(−dy), −cos dx, sin dx·cos dy)
perspektywa: poziomy pół-kąt = 0,3·(szer./wys.) rad  (4:3 → 45,8° w poziomie)
zoom: gra 1,0–2,5, skrypty i przerywniki 0,5–6,0
```
✅ wigview (`--scenario systeminit.tcl` w 1024×768) daje kadr menu zgodny ze zrzutem gry.

**Przycinanie kątów przy dużym oddaleniu** (🔶 ustalone obserwacją, dopasowanie empiryczne). Zrzuty OBERWELT z różnymi `set_view` (`tools/lab/capture.sh scene`) porównane z naszym renderem przy przeszukiwaniu kątów pokazują, że gra ogranicza |dx| i |dy| zależnie od oddalenia (`get_view 1` zwraca przy tym wartości niezmienione, `set_view_begin` nie ma wpływu, a obraz po 60 i 160 s jest taki sam):

| zoom | R (bloki) | zadany kąt → widoczny |
|---|---|---|
| 1,38 | 12,1 | dy 0,57 → 0,57, dx 0,5 → 0,5 (bez przycięcia) |
| 1,7 | 17,2 | dy 0,57 → 0,57 |
| 2,0 | 22 | dy 0,57 → ≈0,35 |
| 2,25 | 26 | dy 0,57 → ≈0,23 |
| 2,5 | 30 | dy 0,3 i 0,57 → ≈0,155; dx 0,22 → ≈0,15; dx −0,4 → ≈−0,15 |

Dopasowanie: `|dx|, |dy| ≤ 2,23 / (R − 15,6)` dla R > 15,6, poniżej bez limitu (`gameViewAngleLimit` w `src/wigview/main.cpp`). Kształt prawa (hiperbola) to hipoteza zgodna z pomiarami, nie stała z gry.

## Układ współrzędnych ✅
- Pozycja obiektu `x y z`: x i y w **blokach** (x w prawo, y w dół). **z ze skryptu przelicza się na głębokość świata wzorem z·0,5 + 7** bloków (stałe `DAT_0073708c` = 0,5 i `DAT_00737520` = 7,0 w exe, za Diggles-Unity `COORDINATE_SYSTEM.md` §3.3). Skóra jaskini z z = 15,01 trafia więc na przednią płaszczyznę 14,5, a mech z z = 13,54 na 13,77. ✅ Po tej poprawce scena menu zgadza się ze zrzutem gry (jezioro i drzewa w otworze, skrzynki, mech).
- 1 blok = **200 jednostek modelu `.3db`** (wzorzec: OBJ z `save_map`). Krasnal ma ok. 1 blok wysokości.
- Rotacja `rx ry rz` w radianach. Gra składa ją jako **M = Ry(−ry)·Rx(−rx)·Rz(−rz)** (macierze są transpozycją standardowych; Diggles-Unity `COORDINATE_SYSTEM.md` §3.4, `FUN_004d2420`, `FUN_005cbf70`). Najczęściej używana jest tylko `ry`.
- Kamera gry patrzy z +z w stronę −z (VRML z `save_scene`: kamera w z = 4000). Układ jest lewoskrętny jak w modelach. W OpenGL odbijamy oś Y (patrz `3db.md` §4).
- ⚠ Błąd w danych gry: `kris_caves_2_a.tcl` stawia skórę jaskini w x ≈ 0, a warianty `_b`, `_c` i inne światy w x ≈ 8, czyli w środku wydrążenia. Szablony `kris_caves_*` gra odrzuca zresztą jako „unknown class”, więc pewnie nie są używane.

## `.PMP` ✅ struktura
```
u32 w, u32 h                 rozmiar w blokach
u8  typ[w*h]                 młodszy półbajt ✅ (patrz „Typ pola” niżej), bit 0x80 🔶 (bez widocznej różnicy)
u32 blok[w*h]                ID bloku w BlockDB (0..118687) -> kształt terenu (patrz niżej)
u32 n
u16 maska[n]                 🔶 przebiegi; wiersze po 32·w+1 próbek, 8 wierszy na blok. W szybie GNG_007
                             to pasek szer. 9 pośrodku szybu, więc najpewniej ścieżka ruchu postaci, a nie kształt terenu
u8  warstwa2[w*h]            🔶 0..8
```
Brak nadmiarowych bajtów we wszystkich 220 plikach.

## Teren: `BlockDB0/1/2.BDB` ✅
Pełny układ pliku za [Zimennik/Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) (MIT, `Docs/RENDERING_PIPELINE_COMPLETE.md` §2.1). Zweryfikowaliśmy go: suma sekcji = rozmiar pliku dla BDB0 i BDB2.
```
u32 version = 11, u32 0, u32 nBlocks = 118688, u32 0
u32 nIndex, u32 nDescriptors, u32 nPalette             BDB0: 561168, 17564, 1411; BDB2: 4195746, 104056, 3323
komórki: nBlocks × 3 × (u16 vertCount, u32 descriptor) 3 podsiatki na blok (góra, środek, dół)
rekordy bloków: nBlocks × 89 B
  u8  wysokość[8][8]       wiersze od góry; 15 = lita skała, mniej = wydrążenie
  u32 sąsiedzi[4]          NW, NE, SW, SE do zszywania (0xFFFFFFFF = brak)
  u8  narożniki[4]         wysokości narożników NW, NE, SW, SE
  u8  wariant              młodszy półbajt = wariant shadera (🔶)
  u8  flagi[4]             🔶
indeksy: nIndex × u16       trójkąty, indeksy wierzchołków w obrębie podsiatki
deskryptory: nDescriptors × (u32 offset, u32 liczba)   zakres w puli indeksów
paleta: nPalette × (f32 x, y, z, u, v)                x, y w bloku (0..1, y w dół), z = wysokość w blokach (7..14,5)
dane podsiatek, kolejno: vertCount × u16 indeks palety, potem vertCount × u8 indeks normalnej (SPHERE.DAT)
```
- **Siatka bloku (bx, by):** dla każdej podsiatki wierzchołek i ma pozycję `((bx + p.x)·200, (by + p.y)·200, p.z·200)`, gdzie p = paleta[indeks i], a normalną `sphere[normalna i]`. Trójkąty biorą indeksy z puli w zakresie deskryptora. Implementacja: `src/lib/blockdb.cpp`, `src/lib/terrain_mesh.cpp`.
- **Pliki BDB0/1/2:** rekordy 89 B są w nich identyczne, a różnią się szczegółowością siatek (`LandScapeQuality` 0–2). Przeglądarka używa BDB2.
- **Wysokość:** z = (7 + 0,5·v) bloku, więc przednia płaszczyzna (v = 15) leży przy **z = 14,5 bloku = 2900 jednostek** (zgodnie z VRML gry), a głębokość wydrążenia to (15 − v)·100 jednostek.
- **Mapa wysokości szablonu (8 próbek na blok):** blok (bx, by) z `.PMP` daje próbki (bx·8+sx, by·8+sy) = `wysokość[sy][sx]`. ✅ Wierzchołki OBJ z `save_map` gry zgadzają się z nią (253 dokładnie, 20 na krawędziach pionowych ścian, 12 to punkty pośrednie ścian). Służy do podglądu 2D.
- Bloki niewykopane (typ pola 0) gra rysuje jako litą skałę. Przeglądarka używa wtedy bloku 0.
- 🔶 Współrzędne u, v w palecie (zakres ok. 0..7,5) gra wykorzystuje do teksturowania. Przeglądarka na razie stosuje mapowanie trójpłaszczyznowe.

### Tekstury gruntu (`Data/Ground`)
`Gnd_A01..15` (szara skała), `Gnd_B01..15` (ziemia), `Gnd_C01..15` (piasek): 64×64, alfa pusta. `Msk_<ABCD>01` to maski przejść między rodzajami. `detail/<świat>[2]_<TL TR BL BR>` to tekstury detali wybierane przez `.l2m` (patrz niżej). Rodzaj gruntu bloku wynika z typu pola `.PMP` (patrz wyżej).

## Typ pola `.PMP` ✅
Ustalony eksperymentem: ten sam korytarz `GNG_021` z różnymi typami pól i zrzuty z gry (`tools/lab/capture.sh`, zestawienie w `../lab/zrzuty/porownania/typy_pol.png`).

| młodszy półbajt | znaczenie |
|---|---|
| 0 | blok **niewykopany**: gra pokazuje litą skałę, choć `BlockDB` ma kształt wydrążenia (tunel do odkopania). Przeglądarka rysuje wtedy blok 0 |
| 2 | grunt **C** (piasek) |
| 3 | grunt **B** (ziemia) |
| 4 | grunt **A** (szara skała) |
| 5 | **otwór do tła** w tylnej ścianie (niżej) |

- Grunt dotyczy całego bloku, także przedniej powierzchni skały. Bit 0x80 nie zmieniał wyglądu (0x04 i 0x84, 0x02 i 0x82 wyglądają tak samo) 🔶. Przejścia między gruntami sąsiednich bloków gra robi maskami `Msk_*` 🔶. Przeglądarka tymczasowo interpoluje wagi gruntów dwuliniowo między środkami bloków.
- Diggles-Unity opisuje typ jako „klasę kamienia” (twardość przy kopaniu): plik 4 → granit (niekopalny), 3 → klasa 1, 2 → klasa 2, 5 → dziura do tła. Ich mapowanie na tekstury było zgadywane, a nasze pochodzi ze zrzutów gry.
- **Warstwa 2 `.PMP`** (0–8) to **poziom światła bazowego** pola (w exe `tile+0x07`, u Diggles-Unity „zone”/BASELIGHT): 4 to zwykła skała, 0 to ciemne wnętrze jaskini, wyższe wartości są jaśniejsze. Używa go oświetlenie (niżej).
- Przykład wysokości z korytarza `GNG_001`: bloki 76–79 to sufit (f, f, e/d/c, b…), blok 80 jest cały b (głębokość 400), blok 81 ma b u góry i f na dole.

### Otwory do tła (typ 5) ✅
Prawa za [Zimennik/Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) (MIT), `Docs/RIM_CUTOUT_RENDER_RE.md` i `BACKDROP_PARALLAX_RE.md`. Wynik zgodny ze zrzutem menu gry (`../lab/zrzuty/porownania/typ5_menu_blisko.png`).
- Węzeł mapy (= blok) jest **otwarty**, gdy pierwszy niezerowy typ (7 młodszych bitów) spośród [węzeł, sąsiad S, sąsiad E, sąsiad SE] to 5.
- Blok (bx, by) ma narożniki w węzłach (bx, by), (bx+1, by), (bx, by+1), (bx+1, by+1). **Alfa wierzchołka** to dwuliniowa interpolacja wartości narożników (0 dla otwartego, 255 dla pozostałych) we współrzędnych palety (x, y). Teren rysowany jest z **alpha testem > 126** (bez mieszania), więc krawędź otworu jest ostra i przebiega w połowie bloku. Bloki z 4 otwartymi narożnikami nie są rysowane.
- **Wypełnienie otworu** to kolor strefy horyzontu: obiekty `Info_Fog_*` w szablonie, klasa wywołuje `horizon_set [get_pos this] r g b głębokość zasięg` (np. `Info_Fog_Urw_d`: 0,305 0,375 0,429, głębokość 32, zasięg 15). Gra czyści tym kolorem ekran. Strefa jest wybierana co klatkę: najbliższy obiekt `Info_Fog_*`, którego odległość 2D od ogniska kamery jest **mniejsza niż zasięg**. Bez takiego obiektu strefy nie ma (✅ zrzut OBERWELT: przy kamerze daleko od stref drzewa w tle nie przechodzą w niebieskoszary).
- **Modele tła** za skałą (z < 0 bloków, czyli ponad 7 bloków za najgłębszym dnem) przechodzą w kolor strefy proporcjonalnie do |z| / głębokość. Tak powstaje widok w głąb (jezioro w menu).
- Panorama powierzchni: osobna sekcja niżej.
- Klasy pokazujące model tylko w edytorze map (`if {[get_mapedit]} {class_defaultanim …}`: światła `Misc_LightSrc_*`, część `Info_*`) są w przeglądarce domyślnie ukryte.

### Panorama powierzchni ✅
Prawo paralaksy za [Zimennik/Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) (MIT, `Docs/BACKDROP_PARALLAX_RE.md` §2), opis i stałe w `src/lib/gfx/panorama_renderer.h`: 6 warstw `Texture/Misc/Panorama_layer_0..5` na siatce 9×9 punktów ekranu, rysowanych przed terenem bez bufora głębi, potem dywan łąki `Panorama_layer_6` w przestrzeni świata (kawałki po 20 pól, z = −60…+7,5, y = wiersz gruntu − 2,5). Kadr na wprost (`set_view 131.78 23.95 2.5 0 0` w OBERWELT) zgadza się ze zrzutem gry.

Ustalone obserwacją (`../lab/zrzuty/gra/oberwelt_*.png`):
- **Bez nieba nie ma panoramy.** Ten sam kadr z `set_light_begin -10000` daje w grze czarne tło (a obiekty są ok. 4× ciemniejsze).
- **`set_ground_begin N`** („Y-beginning of the ground”, w danych gry nieużywane) przesuwa dywan łąki i linię przycięcia panoramy: przy 15 tło jest czarne, przy 24 częściowo czarne, przy 30 dywan widać nad terenem, a 36 i 45 dają obraz jak bez tej komendy. Bez niej przyjmujemy więc wiersz gruntu = `set_light_begin` (33 w OBERWELT, dywan schowany za terenem).
- 🔶 Linia przycięcia panoramy (rysowane tylko komórki nad rzutem płaszczyzny dywanu) nie jest odtworzona. Widać ją tylko przy wierszu gruntu nad kamerą.

### Światło nieba (`set_light_begin`) ✅
`set_light_begin N` (OBERWELT, `single_Minimal`: 33) wyznacza wiersz, powyżej którego jest niebo. Za [Zimennik/Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) (MIT, `Docs/TUNNEL_TORCH_LIGHTING_RE.md` §6): pole w wierszu < N dostaje bazowe światło **20** (zamiast warstwy 2), gdy w jego kolumnie, w nim albo do 6 pól wyżej, jest otwarte drobne pole (wysokość BlockDB < 13) albo skan wychodzi ponad świat. Bez komendy cały świat jest „pod ziemią”. Implementacja: `applySkyLight` w `src/lib/map_world.cpp`. ✅ Zrzut OBERWELT: namioty i drzewa mają jasność jak w grze. 🔶 Gra liczy też ciemnienie w kątach tuneli (tu warstwa 2 szablonu).

## Oświetlenie ✅
Prawo z exe (teren `FUN_004cc040`, modele `FUN_004856e0`), za [Zimennik/Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) (MIT), `Docs/RENDERING_LIGHTING_RESEARCH.md` §2a. Implementacja: `src/lib/gfx/game_lighting.cpp`.
```
I_c = (B · słońce(N) + S · Σ c_i · exp(−2·d²/R²) · max(N·L, 0,1)) · 0,1     per wierzchołek, per kanał
B = 16 · warstwa2 (dwuliniowo z narożników bloku albo w miejscu obiektu) + 64
słońce(N) = 3,6 − 2,546·nx − 2,546·ny                          stałe, z lewej góry (y w dół)
S = 255 dla odkrytego pola (mgła wojny zmniejsza)
lampa: class_light {ox oy oz} R {r g b}: pozycja = obiekt + przesunięcie, z·0,5 + 7; c = int(kolor·31)·0,5
kolor = tekstura · min(I, 255)/255 + clamp(I − 255, 0, 64)/255
```
Bez lamp zwykła skała ma I ≈ 46/255, czyli jest ciemna jak w grze. Jedna lampa nasyca swoje otoczenie (obcięcie do 255), więc nakładające się lampy nie prześwietlają obrazu. Ciepła szara „mgła” z zrzutów (170, 170, 155) to mgła wojny pól odkrytych, ale niewidocznych (silnik, TODO).

🔶 **Modele: oświetlenie dwustronne.** Drzewa `Obw_baum_*` mają normalne skierowane do środka. Z wzorem wprost wychodzą ciemne, a w grze są jasne. Namioty mają normalne na zewnątrz i są jasne w obu przypadkach. Wybór jaśniejszego wyniku z N i −N daje obraz zgodny ze zrzutami OBERWELT i menu.

## `.l2m`: warstwa detali terenu ✅
```
char[4] "M2L\0"
u32 w, u32 h                 jak w .PMP
u16 wartość[w*h]
  bity 15-14  narożnik lewy górny     0 = brak, 1 = zestaw <świat>, 2 = zestaw <świat>2
  bity 13-12  narożnik prawy górny
  bity 11-10  narożnik lewy dolny
  bity  9-8   narożnik prawy dolny
  bity  7-0   🔶 (0x00, 0x20, 0x40, 0x60, 0xC0, rzadko 1..4)
```
- Dla każdego zestawu (1 i 2) z bitów narożników powstaje kod `TL TR BL BR`, np. 0x11 → narożniki prawe → `0101`. Tekstura to `Data/Ground/detail/<świat>[2]_<kod>.tga`, np. `URW_0101.TGA` z mchem po prawej stronie. Pełne pokrycie `1111` ma warianty `_a` … `_f` (✅ pliki istnieją; który wariant wybiera gra, 🔶).
- Światy: `urw`, `kris`, `lava`, `swf` (prefiks nazwy szablonu).
- Plamy w danych mają gładkie krawędzie, typowe dla marching squares. Kolejność bitów ✅ odczytaliśmy z krawędzi plam w `unq_menue.l2m`.
- Mieszanie: **modulate ×2** (wynik = podkład × detal × 2), więc szary 128 nie zmienia podkładu (`detail/50PGray.tga` to neutralna tekstura). 🔶 Wniosek z wyglądu, do potwierdzenia.
- Tekstura detalu obejmuje jeden blok.
- Plik może mieć 0 B (8 przypadków), co oznacza brak warstwy.

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
`save_map {x y w h} plik` (OBJ z mapą wysokości szablonów), `save_scene` (VRML), `data/userstartup.tcl` (własny układ szablonów), **zrzuty obrazu**: `tools/lab/capture.sh` (gra w niewidocznym Xvfb). Patrz `tools/lab/`.

Ze zrzutu sceny menu (świat urw): podkład to szary grunt A (typ pola 4) na dnie i na ścianach, a mech pochodzi z nakładek `.l2m`. Zrzuty i zestawienia: `../lab/zrzuty/` (poza repo).
