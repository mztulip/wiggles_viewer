# 17. Ruch postaci jak w grze

Rozdział opisuje, jak gra wyznacza, gdzie postać może stać i chodzić, jak szuka drogi i jak ją przechodzi. Prawa pochodzą z opracowania [Zimennik/Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) (MIT): `Docs/ORIGINAL_PATHFINDING_RE.md`, `Docs/re_parts/walk-*.md`, `climb-*.md` i ich implementacji `Runtime/Navigation/` (odczyt działającej gry; adresy `FUN_…` to ich cytowane źródło). Nasz kod: [`lib/nav_world`](../../src/lib/nav_world.h) (dane i droga po polach), [`lib/walk_plan`](../../src/lib/walk_plan.h) (trasa z głębokością), wykonawca chodu w [`lib/world_sim`](../../src/lib/world_sim.h). Wcześniejsze uproszczenie (siatka pól i A*, rozdział [14.3](14_gra.md)) zastępujemy etapami:

| Etap | Treść | Stan |
|---|---|---|
| P0 | dane ruchu: podpola 8×8 (podłoga, ściana), graf pól | ✅ |
| P1 | wyszukiwanie drogi Calc1 | ✅ |
| P2 | chód po podłodze (głębokość, prędkość z animacji, obroty, start i zatrzymanie) | ✅ |
| P3 | kopanie: dojście do krawędzi | |
| P4 | wspinanie po ścianach | |

## 17.1 Układ: widok z boku

Świat to „mrowisko” oglądane z boku: x w prawo, **y w dół** (grawitacja, kierunek wspinania), a wysokość z siatki 8×8 bloku BlockDB (0…15, [rozdział 4](04_teren.md)) to **głębokość w ekran** (oś kopania; z świata = h · 0,5 + 7, 15 = lita skała przy przedniej płaszczyźnie). Podłoga nie jest powierzchnią w (x, y), tylko **linią grubości jednego podpola**: granicą między wolną przestrzenią nad nią a materiałem pod nią. Postać chodzi po takich liniach w bok, a wzdłuż głębokości porusza się swobodnie (krasnale z zapisu stoją na tej samej linii y = 30,5 przy z od 6 do 15).

## 17.2 Podpola (EFFPASSWALK) ✅

Każde pole ma 8 × 8 podpól (podpole = 1/8 bloku). Wartość podpola: **0** zablokowane, **1** ściana (można się wspinać), **3** podłoga (bity: 0x01 można stać, 0x02 podłoga). Gra zapisuje płaszczyznę w `.pmp` ([szablony.md](../formaty/szablony.md)), ale po wczytaniu świata i po każdym kopaniu wylicza ją z wysokości bloków:

```
wartość(sx, sy):
    jeśli typ pola ((sx + 4) >> 3, (sy + 4) >> 3) ma bit 0x80: 0            (wymuszona ściana)
    jeśli którekolwiek z 31 podpól „kapsuły” nad stopą ma wysokość ≥ 14: 0   (brak miejsca na postać)
        kapsuła (dx, dy): (0,−8) (±7,−15) (0,−15) (0,−1) (±7,−8) (±4,−3) (±7,−5) (±7,−12) (±4,−15)
                          (0,−12) (0,−4) (±4,−8) (±3,−13) (±5,−11) (±2,−10) (±2,−6) (±5,−5) (±2,−2)
    h = wys(sx, sy), d = wys(sx, sy + 1)
    jeśli wys(sx, sy − 1) + 2 < h i wys(sx, sy − 11) + 2 < h: 3             (pierwszy rząd materiału pod pustką)
    jeśli h + 2 < d: wys(sx, sy − 10) + 2 < d ? 3 : 1                         (ostatni wolny rząd nad podporą)
    inaczej 1
```

Przykład: korytarz `GNG_004` (tylna ściana na wysokości 11, podłoga w połowie trzeciego wiersza pól) daje jedną linię podłogi w wierszach podpól 19–20, czyli na y = 2,5, gdzie w grze stoją kolumny i krasnal (BAZA_WIEDZY §8.12). Głęboka jaskinia `kris_caves_1_b` (tylna ściana na wysokości 2–3) ma podłogę na dole i całą tylną ścianę jako podpola „1”.

**Sprawdzenie:** na 231 szablonach gry wyliczona płaszczyzna zgadza się z zapisaną w `.pmp` w 99,96% podpól (`test3db`, wnętrza szablonów). Po kopaniu przelicza się prostokąt zmienionych podpól poszerzony o zasięg kapsuły: 8 w bok, 2 w górę, 16 w dół.

🔶 Gra przelicza podpola w całym świecie poza marginesem (3 pola z boków i u dołu, 2 u góry), a w marginesie zostawia płaszczyznę z `.pmp`. Szablon oglądany osobno jest u nas całym światem, więc liczymy wszędzie; poza światem wysokość 15.

## 17.3 Graf pól (EFFBLOCK) ✅

Wyszukiwanie drogi działa na polach, nie na podpolach. Dla każdego pola z jego 64 podpól:

```
najlepsze = podpole do chodzenia z największym wynikiem: (podłoga ? 256 : 0) − |4 − wiersz| − |4 − kolumna| + 8
             (pierwsze ściśle największe)
brak podpól do chodzenia albo cztery pola 2×2 z bitem 0x80 typu: koszt −1 (zablokowane)
koszt = 20, gdy najlepsze jest podłogą, inaczej 50 (sama ściana)
punkt wyjścia = najlepsze podpole (x = bity 0..2, y = bity 3..5) + bit 0x40 „pole z podłogą”;
    gdy wszystkie 64 podpola są do chodzenia: bez podłogi środek (4, 4), z podłogą najlepsze zsunięte w dół
    po kolejnych podpolach podłogi
```

„Dig” pola (do kosztu różnic wysokości w wyszukiwaniu drogi) to wysokość środkowego podpola bloku (bajt 0x24 rekordu).

W kodzie: `NavWorld::build/revalidate`, `deriveSub`, `deriveTile`; podgląd: `wigview --template T --print-nav --screenshot x.png` (`_` podłoga, `|` ściana, `.` zablokowane, koszty pól).

## 17.4 Wyszukiwanie drogi (Calc1) ✅

Punkty początku i celu w podpolach (x · 8, y · 8; głębokość pomijana: szukanie jest dwuwymiarowe w płaszczyźnie widoku):

```
do_chodzenia(s) = koszt pola s ≠ −1 i podpole s ma bit 0x01
początek nie do chodzenia: pierścienie r = 1..32, próby wzdłuż osi w kolejności +y, +x, −x, −y; bez trafienia zostaje
cel nie do chodzenia: r = 1..12, osiem prób (0, r) (0, −r/4) (±r/2, 0) (±r/3, r/2) (±r/3, −r/5); bez trafienia: brak drogi
oba punkty: dopóki podpole niżej jest podłogą, y += 1 (dolny rząd półki)
szukanie po polach: kolejka FIFO (Bellman-Ford), sąsiedzi +y, −y, +x, −x, start: dist = koszt + 1
    sąsiad: koszt ≠ −1; postać bez wspinania tylko na pola z bitem 0x40 (podłoga)
    krok: podłoga ↔ ściana: koszt + 276 − 16 · min(dig, dig′)
          ściana → ściana:   koszt + 1 + 16 · |dig − dig′|
          podłoga → podłoga: koszt + 1
    po pierwszym dojściu do celu limit = min(limit, round(dodane · 1,5)) (jeszcze trochę szukania)
wynik: początek, punkt wyjścia każdego kolejnego pola na drodze, cel
```

Przykłady (`wigview --template … --nav-path X0 Y0 X1 Y1 [1]`): w korytarzu lawy droga biegnie po y = 2,5 przez środki pól; w jaskini `kris_caves_1_b` z (2,5; 6) do (6; 3,5) na tylnej ścianie bez wspinania nie ma drogi, a ze wspinaniem prowadzi po podłodze do x = 6,5 i dalej w górę po ścianie. 🔶 Gra odrzuca punkty bliżej niż 40 podpól od brzegu świata; u nas szablon jest całym światem, więc tego warunku nie ma. Warianty z bramką terytorium gracza (koszt przejścia 266) i drabiny/windy (wpływy obiektów) przyjdą z P2–P4.

## 17.5 Trasa z głębokością (FindPath, FloorPathMap) ✅

Droga po polach (17.4) to rzadkie punkty w płaszczyźnie widoku. Trasę, po której postać naprawdę idzie, gra składa z niej odcinkami (FUN_004163a0, za Diggles-Unity `NavWorldService.FindPath`):

```
cel przesunięty przez wyszukiwanie na samą ścianę, a kliknięty nie na ścianie: w dół (do 16 podpól) na pierwszą podłogę
dwa kolejne punkty podłogi z uskokiem ≥ 3 podpola (|dx| ≤ 2): wstawić podpola ściany z kolumny jednego z nich
pierwszy punkt trasy = pozycja postaci; kursor = pierwszy punkt drogi na głębokości postaci
bieg punktów ściany (bez bitu podłogi): każdy na głębokości podparcia
    podparcie(sx, sy) = wys(sx, sy − 1); gdy 15: min z wys(sx ± 1, sy − 1), wys(sx, sy − 2), wys(sx, sy)
    bieg nie na końcu: zajmuje pierwsze podpole podłogi dalej (na głębokości ściany, nie w skale),
    a następny bieg podłogi startuje z głębokości stania min(podparcie + 1, 14,5)
bieg punktów podłogi: głębokość z FloorPathMap od kursora do celu biegu
    cel ostatniego biegu: podpole celu na głębokości celu (0…15,75)
    cel biegu przed ścianą: min(15, min(wys dwóch pierwszych podpól ściany) + 1), czyli tuż przed nią
każdy punkt podłogi: z ≥ min(wys(sx, sy), wys(sx, sy − 1))      (nigdy w skale)
```

**FloorPathMap** (FUN_0041aac0/0041d8a0) rozwiązuje głębokość wzdłuż jednego biegu podłogi na siatce: kolumna = punkt łamanej co 1/8 bloku, wiersz = z · 4 (64 wiersze). Każda kolumna ma **pas** dozwolonych wierszy `[wys(sx, sy − 1) · 4 + 1, wys(sx, sy + 1) · 4 − 1)`: od wolnej przestrzeni nad podłogą (z > tylna ściana) do przedniej krawędzi podłogi. Punkt łamanej najpierw przesuwa się na najbliższą krawędź podłogi (wiersz, w którym wysokość rośnie o więcej niż 2). Start i cel trafiają do najbliższej wolnej komórki w oknie ±16 (cel poza oknem: najbliższy dozwolony wiersz swojej kolumny). Gdy linia prosta start–cel na siatce nie przecina maski, wynik to start, punkt w kolumnie celu na głębokości celu i cel. Inaczej zalewanie od celu (koszt maleje o 1280, 512, 256, 1280, 1024 w pięciu przebiegach sąsiadów: skoczek, przekątne, osie, skoczek, dwa po osi) i powrót od startu zawsze do sąsiada o największym koszcie. Punkty wewnętrzne zsuwają się w dół na ostatni wiersz podłogi. Między punktami głębokości wstawia się wierzchołki łamanej drogi (z liniowo), żeby ruch nie szedł cięciwą przez załamania.

Przykład (`wigview --template lava_unq_vampy_tuer --nav-path 1.5 2.5 14.5 2.5 0 14 5`): korytarz z tylną ścianą 11, postać na z = 14 idzie do celu na z = 5. Pas ma wiersze 45…58 (z 11,25…14,5), więc cel trafia na z = 11,25, a trasa schodzi liniowo z 14 do 11,25 wzdłuż x. Z celem z = 8 trasa idzie od razu na z = 11 (granica skały), a kończy na 11,25.

🔶 Gra dzieli długie biegi (≥ 96 podpól) na odcinki po ok. 32 podpola i przy każdym odczytuje teren na nowo; u nas, jak w Diggles-Unity, jeden odcinek na bieg. Drabiny i windy (wpływy obiektów) jeszcze nie.

## 17.6 Wykonawca chodu (MetaWalk) ✅

Za Diggles-Unity `MetaWalkAction` (odczyt gry FUN_00418280, 00418850, 004185a0):

- **Prędkość z animacji**: |move| animacji pętli chodu · 0,05 bloku na sekundę (gra trzyma tę wartość w tablicy animacji postaci, Diggles-Unity odczytało ją z pamięci). Krasnal: `gehen_loop` 16,22 → 0,811, `laufen_schnell_loop` 34,29 → 1,714; wspinanie ¼ sumy czterech animacji wspinania · 0,05 = 0,416. Wuker: 0,837 i 0,314. Start i stop idą z prędkością pętli.
- **Ruch**: w każdym kroku postać zużywa prędkość · dt drogi wzdłuż punktów trasy, odległość w świecie (dx, dy, dz · 0,5). Na miejscu, gdy punkty się skończą.
- **Fazy**: start (`gehen_start` raz, ruch od razu z pełną prędkością), pętla (`gehen_loop`), stop (`gehen_end` raz, w miejscu), potem `standard`. Animacje krasnala z `set_class_anim` w `ACTORS.TCL` (walkstart, walkloop, walkstop, turnleft…).
- **Bieg**: zwykły chód (`-speedtype 0`) po 2 s marszu przechodzi w bieg (`laufen_schnell_start`, potem pętla). `-speedtype 2` od razu bieg, `1` bez biegu, `-animsets {… −1}` zakazuje biegu.
- **Obrót w miejscu**: gdy kierunek do następnego punktu różni się od patrzenia o ≥ π/4, postać stoi i obraca się animacją `drehen_links/rechts` (> 3π/4: `drehen_ganz_links/drehen_ganz`) z prędkością |rotate| animacji na 0,1 s (krasnal 0,527 i 0,785 rad, wuker 0,157 i 0,195). Po obrocie znowu faza startu.
- **Dryf w marszu**: mniejsze różnice kierunku poprawia o 0,1 rad na 0,1 s; poniżej 0,051 rad od razu.
- **Pierwszy punkt** to pierwszy dalej niż 0,2 (w bok i w głąb); gdy żaden, chód kończy się od razu, bez animacji.

🔶 Kierunek obrotu (lewo/prawo) dobrany z układu modelu, niesprawdzony w grze. Wspinanie na odcinkach ściany na razie uproszczone (animacja w górę albo w dół, twarzą do ściany, prędkość z animacji); wejście na ścianę i zejście to P4. Spadanie (brak podłogi pod stopami i nie na ścianie): 🔶 w pionie 3 bloki/s do pierwszej podłogi, animacja `fallen_loop`.

**Ręczne sterowanie w wigview** (dodatek do testów, nie z gry, [14.8](14_gra.md)) używa tych samych danych: w bok po linii podłogi (w sąsiedniej kolumnie podpól podłoga najwyżej 3 podpola wyżej albo niżej, inaczej ściana zatrzymuje), w głąb w pasie podłogi, z prędkością marszu postaci. PPM wysyła trafiony punkt terenu (x, y, głębokość) jako cel chodu jak w grze.
