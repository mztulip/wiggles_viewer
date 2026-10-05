# 1. Przegląd: jak zbudowana jest gra

## 1.1 Gra w jednym akapicie

Wiggles to gra 2,5D w przekroju „mrowiska”. Patrzymy z boku na pionowy plaster ziemi, w którym krasnale kopią korytarze. Świat jest siatką **bloków** (kwadratów) w płaszczyźnie ekranu: x w prawo, y w dół. Każdy blok ma trójwymiarowy kształt, czyli zagłębienie wykute w głąb ekranu. Lita skała stoi na przedniej płaszczyźnie, a korytarz jest wgłębieniem o kilka metrów w głąb. Na tym terenie stoją modele 3D: dekoracje, budynki i postacie z animacją wierzchołkową. Kamera patrzy na plaster pod niewielkim kątem.

Całą logiką rządzą **skrypty Tcl**. Silnik w C++ (oryginalnie Direct3D 7) dostarcza około 640 komend natywnych (rysowanie, obiekty, mapa, dźwięk), a skrypty z `Data/Scripts` opisują klasy obiektów, menu, kampanię i zachowanie krasnali.

## 1.2 Co robi silnik, od startu do obrazu

```
start programu
 └─ uruchom interpreter Tcl i skrypt Data/systeminit.tcl          (rozdział 2)
     ├─ definicje klas obiektów, animacji, dźwięków, tekstur
     ├─ db_load <model>: wczytanie baz modeli .3db                  (rozdział 5)
     ├─ map create W H: pusty świat                                  (rozdział 3)
     ├─ MapTemplateSet x y: nałożenie szablonów, czyli teren i obiekty (rozdział 3)
     ├─ set_view …: ustawienie kamery                               (rozdział 6)
     └─ gametime start: start pętli gry

pętla, w każdej klatce
 ├─ logika: timery, zdarzenia, zadania postaci (skrypty)            (rozdział 11)
 └─ obraz:                                                           (rozdział 9)
     ├─ czyszczenie kolorem strefy horyzontu                         (rozdział 8)
     ├─ panorama nieba (na powierzchni)                              (rozdział 8)
     ├─ teren: siatki bloków, grunt, detale, otwory                  (rozdział 4)
     └─ modele obiektów z animacją                                   (rozdział 5)
         oświetlenie terenu i modeli: wspólne prawo                  (rozdział 7)
```

## 1.3 Dane gry

Wszystko leży w katalogu `Data/` instalacji. **Nazwy plików mają mieszaną wielkość liter** (`BABY.3DB`, `baby.3db`, `ANIMINIT.TCL`), a skrypty podają je dowolnie, więc wyszukiwanie musi ignorować wielkość liter. Teksty są w Windows-1252 (polskie teksty w CP1250) z końcami linii CRLF.

| Katalog | Zawartość | Opis |
|---|---|---|
| `Data/systeminit.tcl` | punkt wejścia | rozdział 2 |
| `Data/Scripts/` | 834 skrypty Tcl: klasy, menu, kampania, teksty | rozdział 2 |
| `Data/3db/` | 43 bazy modeli `.3db` | [3db.md](../formaty/3db.md), rozdział 5 |
| `Data/Texture/` | tekstury modeli TGA (`m256` … `m032`, `Misc`) | [tekstury.md](../formaty/tekstury.md) |
| `Data/Ground/` | tekstury terenu: grunty, maski, detale | rozdział 4 |
| `Data/Templates/` | szablony map: `.tcl` + `.pmp` + `.l2m` | [szablony.md](../formaty/szablony.md), rozdział 3 |
| `Data/BlockDB0/1/2.BDB` | 118 688 kształtów bloków terenu, 3 poziomy szczegółów | [blockdb.md](../formaty/blockdb.md), rozdział 4 |
| `Data/SPHERE.DAT` | 256 normalnych do kwantyzacji | [sphere_dat.md](../formaty/sphere_dat.md) |
| `Data/Gamesave/` | scenariusze (`.tcl`) i zapisy (`.sav`, gzip) | rozdział 2, [sav.md](../formaty/sav.md) |
| `Data/Sound/`, `Data/Music/` | WAV 8 bit 22 kHz, MP3 | (jeszcze nieopisane) |
| `Data/GUI/` | interfejs, fonty `.kft` | (jeszcze nieopisane) |

## 1.4 Jednostki ✅

Gra używa kilku skal. Wszystkie przeliczają się prosto:

| Jednostka | Ile to | Gdzie występuje |
|---|---|---|
| **blok** (pole mapy) | podstawowa jednostka świata | pozycje obiektów, `set_view`, szablony, `MapTemplateSet` |
| **drobne pole** | 1/8 bloku | rekord wysokości bloku (8×8), `get_map_width` (mapa 64 bloków = 512) |
| **jednostka modelu** | 1/200 bloku | wierzchołki `.3db` po dekwantyzacji, BlockDB po przeskalowaniu |

Krasnal ma około 200 jednostek modelu wysokości, czyli 1 blok.

## 1.5 Układ współrzędnych ✅

Gra używa układu **lewoskrętnego** jak Direct3D:

```
          −z (w głąb ekranu, tylna ściana)
         ↗
  +x ───────→  (w prawo)
  │
  ↓ +y (w dół)
         ↙
          +z (do widza, przednia płaszczyzna; tu stoi kamera)
```

- **x, y** to pozycja w płaszczyźnie ekranu, w blokach. Blok (bx, by) zajmuje kwadrat [bx, bx+1) × [by, by+1). Wiersz 0 jest na górze świata.
- **z** to głębokość. Teren mieści się w przedziale **z ∈ [7; 14,5] bloku**:
  - z = 14,5 to **przednia płaszczyzna**, czyli lico litej skały;
  - z = 7 to dno najgłębszego wykopu;
  - z < 7 leży za terenem (dekoracje tła), a z > 14,5 przed nim.
- Kamera stoi po stronie +z i patrzy w stronę −z.

**Głębokość obiektów w skryptach.** Obiekty tworzy się komendą `new Klasa "" "x y z" "rx ry rz"`. Ich z ze skryptu **nie jest** głębokością świata, tylko przelicza się wzorem:

```
z_świata = z_skryptu · 0,5 + 7      [bloki]
```

Mech z z = 13,54 ląduje więc na 13,77, skóra jaskini z z = 15,01 tuż przed przednią płaszczyzną (14,5), a drzewo z z = −37 daleko w tle (−11,5). Ten sam wzór obowiązuje dla świateł klas (rozdział 7). ✅ Bez tego przeliczenia scena menu nie zgadza się ze zrzutem gry. Stałe 0,5 i 7 podaje [Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) (`Docs/COORDINATE_SYSTEM.md` §3.3).

**Przejście do OpenGL** (prawoskrętny, y w górę): **odbij oś Y**, czyli (x, y, z) → (x, −y, z), tak samo dla normalnych. Nic więcej, w szczególności nie obracaj o 180° wokół X, bo to daje lustrzane napisy. Najprościej trzymać cały świat w układzie gry i dodać odbicie Y w macierzy widoku (`scale(1, −1, 1)`). Odbicie zmienia kierunek nawijania trójkątów, ale gra i tak rysuje wszystko bez odrzucania tylnych ścian.

## 1.6 Słowniczek

| Termin | Znaczenie |
|---|---|
| **szablon** (template) | gotowy kawałek mapy: siatka bloków (`.pmp`), detale (`.l2m`) i skrypt z obiektami (`.tcl`) |
| **scenariusz** | skrypt w `Data/Gamesave/` układający szablony w świat (mapy stałe, samouczek) |
| **typ pola** | bajt w `.pmp`: rodzaj gruntu (piasek, ziemia, skała) albo otwór do tła |
| **warstwa 2** | bajt w `.pmp`: poziom światła bazowego pola |
| **blok BlockDB** | jeden z 118 688 gotowych kształtów 3D wnętrza bloku |
| **przednia płaszczyzna** | z = 14,5 bloku, lico niewykopanej skały |
| **strefa horyzontu** | kolor tła i mgły w głąb, ustawiany przez obiekty `Info_Fog_*` |
| **panorama** | malowane niebo i krajobraz na powierzchni, z paralaksą |
| **klasa** | typ obiektu ze skryptów (np. `Dummy_Moos_um_a`, `Zwerg`); wskazuje model i animację |
| **obiekt `.3db`** | nazwany zestaw animacji w bazie modeli, np. `urw_sumpf` z animacją `standard` |
| **świat** (urw, kris, lava, swf) | motyw graficzny szablonu: dżungla, kryształy, lawa, siarka |

Nazwy w danych są niemieckie: *Zwerg* to krasnal, *Urwald* dżungla, *Höhle* jaskinia, *Gang* korytarz, *Oberwelt* powierzchnia, *Baum* drzewo.

---
**Diggles-Unity** (ich opracowanie, 🔶 do sprawdzenia w laboratorium): [COORDINATE_SYSTEM](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/COORDINATE_SYSTEM.md), [ENGINE_ARCHITECTURE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/ENGINE_ARCHITECTURE.md). Porównanie i stan: [mapa Diggles-Unity](../DIGGLES_UNITY.md).

**W kodzie OpenWiggles** (pomocniczo, zasada działania jest opisana wyżej): [`lib/vfs.h`](../../src/lib/vfs.h), [`lib/text.h`](../../src/lib/text.h). Wszystkie moduły: [mapa kodu](../KOD.md).
