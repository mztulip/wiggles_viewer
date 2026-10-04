# 14. Gra: pierwsza grywalna wersja

Program `openwiggles` (`src/game/main.cpp`) łączy to, co opisują rozdziały 1–13, w działającą grę: wczytuje świat z zapisu albo scenariusza, pokazuje go kamerą gry, a krasnale chodzą i kopią na rozkaz gracza. Zachowania krasnali z Tcl jeszcze nie działają. Ruch i kopanie to celowo proste przybliżenia (🔶), które później zastąpimy prawami gry.

## 14.1 Start

```
openwiggles                       # najnowszy zapis z Data/Gamesave (np. początek kampanii), bez zapisów: szablon kris_unq_060
openwiggles --save save_kampania.sav
openwiggles --scenario single_Knockersdorf
openwiggles --template urw_gng_021_a
```

Krasnale to obiekty klasy `Zwerg` z zapisu albo z `set_pos [qnew Zwerg] {x y z}` w scenariuszu. Model: `mann.3db`, obiekt `mann`. Animacje mają nazwy z `Scripts/classes/zwerg/z_anims.tcl` (`set_class_anim walkloop mann.gehen_loop` itd.). Gdy mapa nie ma krasnali, gra stawia jednego przy środku kamery.

## 14.2 Kamera

Prawo `set_view` z rozdziału 6: cel (x, y) w blokach, `zoom`, kąty. Gracz przesuwa cel (WASD, strzałki, środkowy przycisk myszy) i zmienia zoom (kółko, 0,9–3).

Wskazywanie pola: promień z kamery przez piksel kursora przecinamy z płaszczyzną głębokości krasnali (z świata 11 bloków). Pole = `floor(x), floor(y)` punktu przecięcia.

## 14.3 Ruch (🔶 uproszczenie)

Siatka pól (`src/lib/nav.h`):

```
otwarte(x, y)  = w dolnej połowie bloku (wiersze 4..7 siatki 8×8 BlockDB) ≥ 16 drobnych pól ma wysokość < 13
stoi(x, y)     = otwarte(x, y) i nie otwarte(x, y + 1)
wspina(x, y)   = otwarte(x, y) i (lita skała z lewej albo z prawej)
```

Ruchy (koszt): w bok między polami „stoi” (10), stopień ±1 wiersz po skosie (14), zejście z krawędzi (12), w górę po ścianie (20), w dół (15); w powietrzu tylko spadanie. Droga: A* z heurystyką Manhattan.

Krasnal stoi stopami w `(x + 0,5, y + 1)`, na głębokości skryptu 8 (świat 11). Prędkość: chód 1,6 pola/s, wspinanie 0,9 pola/s. Animacja: `gehen_loop` w bok, `kletter_hoch_loop` / `kletter_runter_loop` w pionie, `standard` w miejscu, `kletterstand` na ścianie. Obrót: −π/2 = twarzą w prawo, +π/2 = w lewo, π = do ściany.

Gra ma tu znacznie bogatsze prawa (drobna siatka 8×8, maska ruchu w `.pmp`, drabiny, windy): Diggles-Unity `ORIGINAL_PATHFINDING_RE.md`, `NAVIGATION_AND_MOVEMENT.md`.

## 14.4 Kopanie (🔶 uproszczenie)

PPM na polu skały: krasnal idzie na najbliższe stanowisko obok (z boku, nad albo pod polem), kopie 2 s (`buddeln_vorn`, `buddeln_unten`, `buddeln_oben_normal`), a potem pole dostaje blok 59 (pusty) i typ 3. Teren, wysokości i płyny przebudowujemy w całości.

W grze kopanie wybiera kształty sąsiednich bloków z tablicy sąsiadów BlockDB (rekord §2, przesunięcie 64), tak żeby krawędzie się zszywały, i zależy od twardości materiału: Diggles-Unity `DIGGING_MECHANIC.md`, `ROOFED_TUNNEL_RE.md`.

## 14.5 Krasnale ze skryptami gry

Domyślnie krasnalami sterują ich skrypty (`Scripts/classes/zwerg`, rozdział 11). Gra udostępnia im świat przez interfejs `Host` (`src/game/main.cpp`):

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
