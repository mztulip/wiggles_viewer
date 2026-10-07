# 19. Kopanie terenu

Rozdział opisuje, jak gra zmienia teren przy kopaniu: znaczniki graczy, materiał pól i wycinanie węzłów siatki przez graf przejść bloków BlockDB. Ruch krasnala do miejsca kopania (krawędź, stanowisko) opisuje [rozdział 17.7](17_ruch.md), a bloki i ich rekordy [blockdb.md](../formaty/blockdb.md).

**Źródło i status.** Prawa pochodzą z opracowania [Zimennik/Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) (MIT): `Docs/DIGGING_MECHANIC.md`, `Runtime/World/DiggableTerrain.cs`, `DigglesTclEngine.cs` (komendy `dig_*`); adresy `FUN_…` gry to ich cytowane źródło. ✅ Sprawdzone w działającej grze ([BAZA_WIEDZY §9.12](../BAZA_WIEDZY.md)): materiał pól z typu `.pmp` oraz liczba kroków wycinania jaskini i tunelu. Reszta 🔶.

## 19.1 Przebieg w skryptach

Rozkaz kopania to zdarzenie `evt_task_dig` krasnala (`z_events.tcl`), które wkłada do kolejki zadanie `dig_starttask` (`z_dignwalk.tcl`). Krasnal idzie na stanowisko (17.7), a potem w pętli:

```
dig_continue:  punkt = dig_next $poz this $próba          ;# najbliższy zaznaczony węzeł (−1 −1 −1 = koniec)
               dig_check: co najmniej 2 z 3 próbek get_hmap < 13 od strony krasnala, inaczej próba + 1
               dig_anim punkt                              ;# animacja kopania
dig_execute:   dopóki punkt w zasięgu (3 + 2·exp_Stein):  dig_apply punkt this
               szansa na krok: rand < digcount · get_material · 0,7; nagroda exp_Stein, czasem znalezisko
```

Jedyną komendą zmieniającą świat jest `dig_apply`. Skrypt dodaje tylko nagrody (doświadczenie, kamienie, rudy).

## 19.2 Materiał pola ✅

Każde pole ma materiał (gra: `tile+0x18 & 3`): **0** skała (granit), **1** piasek, **2** ziemia, **3** otwarte (otwór do tła). Gra ustala go przy wczytaniu z bajtu typu pola `.pmp` ([szablony.md](../formaty/szablony.md)) i potem go nie zmienia:

| bajt typu & 0x7F | materiał |
|---|---|
| 2 | 2 ziemia |
| 3 | 1 piasek |
| 4 | 0 skała |
| 5 | 3 otwarte |
| 0, 1 | pierwszy niezerowy bajt pola niżej, z prawej, z prawej niżej; bez niego 2 |

Kopać można tylko materiał 1 i 2. `get_material poz` zwraca tę liczbę, a skrypt mnoży przez nią tempo kopania: ziemia idzie dwa razy szybciej niż piasek. Sprawdzone w grze (`get_material` po `live2.sh show`): szablon `kris_caves_1_b` (`CAVES_1.PMP`, wszystkie pola typu 4) daje same 0, a `urw_gsg_001_a` (`GSG_001.PMP`, typ 2) same 2. Jaskinie ze stempli `*_caves_*` stoją więc w skale nie do kopania.

## 19.3 Znaczniki

Każde pole ma 8 półbajtów znaczników (gra: `tile+0x08`, półbajt gracza 0…7) i 8 półbajtów id znacznika (`tile+0x0c`). `dig_mark gracz x y tryb [głębokość]`:

| tryb | półbajt | cel wycinania |
|---|---|---|
| 1 tunel | 3 | 11 (jeden krok od litej skały) |
| 0 jaskinia | `głębokość | 8` | głębokość 0 → 0 (do dna), 1 → 3, 2 → 7, 3 → 11 |
| 2 | zdjęcie znacznika | |

Cel to poziom krawędzi węzła (19.4), poniżej którego kopanie się kończy. Znacznik powstaje tylko na materiale 1 i 2 i tylko wtedy, gdy krawędź węzła jeszcze sięga celu (`krawędź ≥ cel`): tunel zaznaczony na już wykopanym tunelu przepada. Id znacznika to licznik gracza (`inc_playerdigmarkid`) w chwili malowania, & 15. `is_dig_marked x1 y1 x2 y2` sprawdza prostokąt [x1, x2) × [y1, y2) dla gracza 0.

## 19.4 Węzły i krawędź

Kopie się **węzły siatki**, nie pola. Węzeł (nx, ny) to wspólny narożnik czterech pól: (nx−1, ny−1) narożnik SE, (nx, ny−1) SW, (nx−1, ny) NE, (nx, ny) NW. Rekord bloku ma dla każdego narożnika (NW, NE, SW, SE) blok sąsiedni w grafie przejść (`u32` od bajtu 64, `0xFFFFFFFF` = brak) i bajt bramki (od bajtu 80). Przejście narożnika daje kształt z tym narożnikiem obniżonym o jeden poziom. Przykład: blok 0 (lita skała, narożniki 15) przechodzi narożnikiem NW w 54 (11/15/15/15), dalej 34511, 39259, 47449 (0).

**Krawędź węzła** (`rimAtNode`): dla każdego z czterech pól weź blok, do którego prowadzi jego narożnik przy węźle, i jego bramkę dla tego narożnika (gdy przejścia nie ma, własną bramkę); krawędź to największa z nich. Gdy żaden narożnik nie ma przejścia, krawędź = −1 (węzeł wykopany do dna). Dla dziewiczej skały krawędź wynosi 11, po jednym kroku 7, potem 3, potem 0.

## 19.5 Krok wycinania (`dig_apply`)

```
dig_apply punkt obj:
    węzeł = (floor(x + 0,5), floor(y + 0,5)); gracz = właściciel obj
    jeśli węzeł w pasie 1 pola od brzegu świata: 0
    jeśli materiał pola węzła ∉ {1, 2}: zdejmij znacznik, 0
    cel = dekoduj(półbajt gracza), krawędź = rimAtNode
    jeśli krawędź < 0 albo cel > krawędź: zdejmij znacznik, 0      ;# już wykopane do celu
    dla każdego z 4 pól: przejdź narożnikiem, jeśli bramka bloku docelowego == krawędź   ;# szew bez dziur
    nowa krawędź: znaczniki wszystkich graczy z celem > nowa krawędź są wykonane (zdjęte)
    jeśli nic się nie zmieniło i krawędź stoi: zdejmij znacznik, 0  ;# 🔶 za Diggles-Unity (zastój)
    zapamiętaj id znacznika w obj (dig_next woli dokończyć tę samą żyłę); 1
```

Każde wywołanie to jeden krok, a skrypt woła `dig_apply` w pętli. Zmienione pola dostają nowe ID bloków; teren i płaszczyzna ruchu (17.2) przeliczają się dla nich od razu. Wykopane dno to blok 59, a sąsiednie pola stają się blokami krawędzi (ściany, narożniki) z własną geometrią ścian.

✅ Liczba kroków zgodna z grą: na `urw_gsg_001_a` jaskinia do dna (`dig_mark 0 x y 0 0`) w czterech sąsiednich węzłach daje po 4 kroki (15 → 11 → 7 → 3 → 0), a tunel (`dig_mark 0 x y 1`) w dwóch sąsiednich węzłach po 1 kroku.

## 19.6 Następny węzeł (`dig_next`)

`dig_next poz obj [próba] [tylko_id]`: w oknie x ∈ [px − 16, px + 16), y ∈ [py − 17, py + 15) (px, py = część całkowita pozycji, okno przycięte o 2 pola od brzegu) bierze zaznaczone węzły gracza z materiałem 1/2 i krawędzią ≥ 0. Koszt:

```
koszt = (x − px)² + (y − (py − 0,75))² + 0,25 · ((krawędź + 4) − (pz + 6))² + (id ≠ id kopiącego ? 8 : 0)
```

Z argumentem `tylko_id` inne id odpadają. Zwraca węzeł na miejscu `próba` (0 = najtańszy; przy remisie kolejność przeglądania, wiersze, potem kolumny) jako `x y z`, gdzie z = krawędź + 6 przycięte do [1, 14]. Mniej kandydatów niż `próba + 1` daje wartownika `-1 -1 -1`, który kończy pętlę skryptu. Oryginał zwraca wartownika z przypadkowymi znakami za liczbą (w laboratorium `-1.0 -1.0 1.0°w`); skrypt czyta tylko y, więc to bez znaczenia.

## 19.7 Nasza implementacja

[`lib/dig_terrain`](../../src/lib/dig_terrain.h) ([.cpp](../../src/lib/dig_terrain.cpp)): materiał, znaczniki, `rimAtNode`, `carveStep`, `next`. [`WorldSim::carveNode`](../../src/lib/world_sim.h) podmienia bloki w scenie i przelicza ruch zmienionych pól, komendy Tcl są w [`script/cmd_dig.cpp`](../../src/lib/script/cmd_dig.cpp). Kopanie rozkazem gracza (`WorldSim::dig`, gra) używa tego samego wycinania: zaznacza cztery węzły pola jako tunel i wycina je do celu.

**Różnice (🔶):**
- gra blokuje wycinanie pod stojącymi obiektami (`FUN_005976b0`) i usuwa dekoracje, do których dotrze wykop; my jeszcze nie;
- krasnale gracza kopią na razie naszą pętlą (`WorldSim::digAt`), a nie skryptem `z_dignwalk.tcl` przez `evt_task_dig`;
- brak znaczników jaskiń ze stempli (`cave0..3.tga`) i rekordów jaskiń (`cave_skin`).

**Sprawdzenie:** test `http_dig` w [`tools/tests/run.sh`](../../tools/tests/run.sh) (materiał i liczby kroków jak w grze).
