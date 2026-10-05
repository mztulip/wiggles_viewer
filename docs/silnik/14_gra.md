# 14. Gra: pierwsza grywalna wersja

Program `openwiggles` ([`src/apps/game/main.cpp`](../../src/apps/game/main.cpp)) łączy to, co opisują rozdziały 1–13, w działającą grę: wczytuje świat z zapisu albo scenariusza, pokazuje go kamerą gry, a krasnale chodzą i kopią na rozkaz gracza. Zachowania krasnali z Tcl jeszcze nie działają. Ruch i kopanie to celowo proste przybliżenia (🔶), które później zastąpimy prawami gry.

## 14.1 Start

```
openwiggles                       # menu wyboru gry: nasze zapisy, zapisy gry z Data/Gamesave, scenariusze
openwiggles --save save_kampania.sav
openwiggles --scenario single_Knockersdorf
openwiggles --template urw_gng_021_a
```

Menu (🔶, zamiast menu oryginału) otwiera też Esc w trakcie gry; wybrana gra wczytuje się po klatce z napisem „Wczytywanie…”. Bez podanego źródła tryb zrzutu (`--screenshot`) bierze najnowszy zapis z `Data/Gamesave` (podgląd menu: `OW_TEST_MENU=1`).

Krasnale to obiekty klasy `Zwerg` z zapisu albo z `set_pos [qnew Zwerg] {x y z}` w scenariuszu. Model: `mann.3db`, obiekt `mann`. Animacje mają nazwy z `Scripts/classes/zwerg/z_anims.tcl` (`set_class_anim walkloop mann.gehen_loop` itd.). Gdy mapa nie ma krasnali, gra stawia jednego przy środku kamery.

## 14.2 Kamera

Symulacja świata (skrypty obiektów, krasnale, wukery, kopanie) jest w [`lib/world_sim`](../../src/lib/world_sim.h), wspólna dla gry i przeglądarki (wigview: przełącznik „Symulacja (skrypty)”). Wukery chodzą tym samym mechanizmem co krasnale (🔶), z własnymi animacjami (`gehen_loop`, `kletter_hoch`, `kletter_runter`); panel gracza pokazuje tylko krasnale.

Prawo `set_view` z rozdziału 6: cel (x, y) w blokach, `zoom`, kąty. Gracz przesuwa cel (WASD, strzałki, środkowy przycisk myszy) i zmienia zoom (kółko, 1,0–2,5 jak w grze, [6.1](06_kamera.md)). Jak w oryginale kamera jedzie też, gdy kursor dotknie krawędzi okna: pas szerokości `max(4, min(szer, wys)/60)` px, prędkość jak klawiszy (🔶 obie wartości na oko, do porównania z grą).

Wskazywanie pola: promień z kamery przez piksel kursora przecinamy z płaszczyzną głębokości krasnali (z świata 11 bloków). Pole = `floor(x), floor(y)` punktu przecięcia.

## 14.3 Ruch (🔶 uproszczenie)

Siatka pól ([`src/lib/nav.h`](../../src/lib/nav.h)):

```
otwarte(x, y)  = w dolnej połowie bloku (wiersze 4..7 siatki 8×8 BlockDB) ≥ 16 drobnych pól ma wysokość < 13
stoi(x, y)     = otwarte(x, y) i nie otwarte(x, y + 1)
wspina(x, y)   = otwarte(x, y) i (lita skała z lewej albo z prawej)
```

Ruchy (koszt): w bok między polami „stoi” (10), stopień ±1 wiersz po skosie (14), zejście z krawędzi (12), w górę po ścianie (20), w dół (15); w powietrzu tylko spadanie. Droga: A* z heurystyką Manhattan.

Krasnal stoi stopami w `(x + 0,5, y + 1)`, na głębokości skryptu 8 (świat 11). Prędkość: chód 1,6 pola/s, wspinanie 0,9 pola/s. Animacja: `gehen_loop` w bok, `kletter_hoch_loop` / `kletter_runter_loop` w pionie, `standard` w miejscu, `kletterstand` na ścianie. Obrót: −π/2 = twarzą w prawo, +π/2 = w lewo, π = do ściany.

Gra ma tu znacznie bogatsze prawa (drobna siatka 8×8, maska ruchu w `.pmp`, drabiny, windy): Diggles-Unity `ORIGINAL_PATHFINDING_RE.md`, `NAVIGATION_AND_MOVEMENT.md`.

## 14.4 Kopanie (🔶 uproszczenie)

PPM na polu skały: krasnal idzie na najbliższe stanowisko obok (z boku, nad albo pod polem), kopie 2 s (`buddeln_vorn`, `buddeln_unten`, `buddeln_oben_normal`), a potem pole dostaje blok 4523 (tunel: płaska tylna ściana na wysokości 7, czyli z = 10,5). Rodzaj gruntu pola zostaje, więc ściana tunelu pokazuje materiał, w którym kopano (szara skała A, żółta glina C…); tylko „otwór do tła” (typ 5, np. wiersz powierzchni) zamieniamy na grunt pola niżej, bo inaczej ściana dostaje kolor strefy horyzontu. Warstwę detali `.l2m` pola zerujemy. Teren, wysokości i płyny przebudowujemy w całości.

**Obszar do kopania**: przeciągnięcie PPM zaznacza prostokąt pól skały (żółte znaczniki na przedniej płaszczyźnie). Zaznaczony krasnal dostaje zadanie: sam wybiera najbliższe zaznaczone pole, do którego prowadzi droga, i kopie kolejne, aż skończą się osiągalne. W tym czasie skrypt nie może nim chodzić (`walkTo` odmawia). Pojedyncze kliknięcie PPM zdejmuje zadanie.

**Znaleziska**: jak w `dig_execute` (`z_dignwalk.tcl`): przy wykopaniu pola z szansą 4% powstaje obiekt `Stein`, `Eisenerz`, `Kohle`, `Golderz` albo `Kristallerz` (🔶 rodzaj losowy, bez stref kampanii i doświadczenia krasnala).

**Mgła**: krasnale gracza co 0,4 s odkrywają koło o promieniu 6 pól (🔶 zamiast promieni z rozdziału 10.3). `--reveal` wyłącza mgłę.

W grze kopanie wybiera kształty sąsiednich bloków z tablicy sąsiadów BlockDB (rekord §2, przesunięcie 64), tak żeby krawędzie się zszywały, i zależy od twardości materiału: Diggles-Unity `DIGGING_MECHANIC.md`, `ROOFED_TUNNEL_RE.md`.

## 14.5 Krasnale ze skryptami gry

Domyślnie krasnalami sterują ich skrypty (`Scripts/classes/zwerg`, rozdział 11). Gra udostępnia im świat przez interfejs `Host` ([`src/apps/game/main.cpp`](../../src/apps/game/main.cpp)):

| Komenda | U nas |
|---|---|
| `action <obj> walk "-target {x y z} …" kod` | droga A* po polach do pola stóp `(floor(x), floor(y − 0,6))`, po dojściu kod i `get_walkresult` = 4, bez drogi 3 |
| `action <obj> rotate kąt kod` | obrót od razu, kod po 0,3 s (kąty: 4,71 w prawo, 1,57 w lewo, 3,14 tyłem, 0 przodem) |
| `get_pos`, `set_pos` | pozycja ze stopami na podłożu: y = dół pola + pierwszy wiersz siatki 8×8 bloku pod spodem o wysokości ≥ 13 (np. na łące 30,5) |
| `get_hmap x y` | wysokość siatki 8×8 bloku w punkcie (= z w skali skryptu) |
| `get_place -center p -circle r` | najbliższe pole, na którym da się stać |
| `get_gnomeposition` | 1, gdy krasnal wisi na ścianie |
| `obj_query` | filtry `-class`, `-type`, `-range`, `-limit`, `-owner`, `-pos`, `-boundingbox`; wynik posortowany wg odległości albo 0 |

🔶 `get_remaining_sparetime` zwraca zawsze 1 (krasnale mają tylko czas wolny), właściciel krasnali = gracz 0, `animLength` stałe 1,5 s. Rozkaz gracza (PPM) nadpisuje bieżący chód skryptu. `--player-gnomes` wyłącza skrypty krasnali.

## 14.6 Interfejs

- Lewy panel: wersja, liczba krasnali, zaznaczony krasnal, pole pod kursorem, komunikaty, tempo gry, pomoc (F1).
- Prawy panel: krasnale z imionami (nazwa instancji z zapisu), stanem skryptu i paskami potrzeb z atrybutów (`atr_Hitpoints`, `atr_Nutrition`, `atr_Alertness`, `atr_Mood`; czerwony poniżej 30%). Kliknięcie zaznacza krasnala i przesuwa kamerę.
- Żółty trójkąt nad zaznaczonym krasnalem.
- Spacja: pauza, klawisze 1/2/3: tempo ×1/×2/×4.
- Esc: menu wyboru gry (gra stoi); w menu „Wróć do gry” albo „Wyjście”.

## 14.7 Zapis i wczytanie gry 🔶

**Zapis jak w oryginale (`.sav`).** Oryginał zapisuje szybki zapis do `Data/Gamesave/quick_Quicksave_<data>.sav`; katalogu gry nie zmieniamy, więc nasze zapisy leżą w katalogu użytkownika. Gra wczytana z zapisu `.sav` zapisuje się do `.sav`: F5 tworzy `~/.local/share/openwiggles/saves/quick_Quicksave_<RRRRMMDD_GGMM>.sav` (nazwa jak w grze). Nie budujemy pliku od zera, tylko **łatamy** wczytany zapis: kopiujemy jego bajty i nadpisujemy czas, kamerę, pola mapy (bloki, materiał, płyny, nieodkryty teren) oraz pozycje i obroty obiektów ([formaty/sav.md, „Zapis łatą”](../formaty/sav.md)). Dlaczego tak: reszty pliku (sekcje po znaczniku 7) jeszcze nie rozumiemy, a łata zostawia ją nietkniętą, więc plik dalej wczytuje oryginalna gra. Taki zapis otwiera wigview i openwiggles (`--save plik.sav`) oraz oryginalna gra.

Czego łata nie przenosi (🔶, TODO): obiektów utworzonych i usuniętych w naszej grze, timerów, zmiennych Tcl i stanów skryptów (po wczytaniu biorą się z oryginalnego zapisu). Wykopane pola się zapisują.

**Format tymczasowy (`.ows`).** Gra uruchomiona ze scenariusza `.tcl` albo szablonu nie ma zapisu-bazy, więc F5 zapisuje `quick.ows`, a F9 wczytuje najnowszy zapis (`--load plik.ows`; w testach `--save-to plik.ows|plik.sav` po `--warmup`). `.ows` zapisuje **źródło mapy i zmiany**:

```
"OWSAVE" u32 wersja=1
str rodzaj źródła (scenario | savepath | template), str nazwa
f64 czas gry skryptów, 5 × f32 kamera (x y zoom dx dy)
u32 n, n × (u32 indeks pola, u32 blok, u32 typ)          pola zmienione względem źródła (kopanie)
u32 n, n × (u32 indeks pola, u32 typ płynu, f32 wypełnienie)
u32 liczba obiektów, u32 liczba obiektów źródła
każdy obiekt: str klasa, i32 ID skryptu, 3 × f32 pozycja, 3 × f32 obrót, u32 ma skrypt,
              [str nazwa, i32 właściciel, str stan, i32 licznik stanu,
               str skrypt Tcl zmiennych (set/array set; tylko różne od startowych),
               u32 n, n × (str atrybut, f64 wartość)]
u32 n, n × (i32 x, i32 y)                                 pola do kopania
```

Napisy: u32 długość i bajty, liczby little-endian. Wczytanie: źródło jak przy starcie (obiekty i ich `obj_init` w tej samej kolejności, więc te same indeksy i ID), potem bloki, płyny, pozycje obiektów, obiekty utworzone w grze (dopisywane w kolejności), atrybuty, a na końcu zmienne Tcl i stany (zmienne mogą wskazywać obiekty utworzone później). 🔶 Timery skryptów nie są zapisywane: `obj_init` ustawia je na nowo.

---
**Diggles-Unity** (ich opracowanie, 🔶 do sprawdzenia w laboratorium): [GNOME_BEHAVIOR_COMPLETE_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/GNOME_BEHAVIOR_COMPLETE_RE.md), [re_parts/](https://github.com/Zimennik/Diggles-Unity/tree/main/Docs/re_parts/), [ORIGINAL_PATHFINDING_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/ORIGINAL_PATHFINDING_RE.md), [NAVIGATION_AND_MOVEMENT](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/NAVIGATION_AND_MOVEMENT.md), [DIGGING_MECHANIC](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/DIGGING_MECHANIC.md), [WORK_DISPATCHER_NATIVE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/WORK_DISPATCHER_NATIVE.md). Porównanie i stan: [mapa Diggles-Unity](../DIGGLES_UNITY.md).

**W kodzie OpenWiggles** (pomocniczo, zasada działania jest opisana wyżej): [`apps/game/main.cpp`](../../src/apps/game/main.cpp), [`lib/nav.h`](../../src/lib/nav.h), [`lib/map_scene.h`](../../src/lib/map_scene.h). Wszystkie moduły: [mapa kodu](../KOD.md).
