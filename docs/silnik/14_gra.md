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
