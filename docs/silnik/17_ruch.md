# 17. Ruch postaci jak w grze

Rozdział opisuje, jak gra wyznacza, gdzie postać może stać i chodzić, jak szuka drogi i jak ją przechodzi. Prawa pochodzą z opracowania [Zimennik/Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) (MIT): `Docs/ORIGINAL_PATHFINDING_RE.md`, `Docs/re_parts/walk-*.md`, `climb-*.md` i ich implementacji `Runtime/Navigation/` (odczyt działającej gry; adresy `FUN_…` to ich cytowane źródło). Nasz kod: [`lib/nav_world`](../../src/lib/nav_world.h). Wcześniejsze uproszczenie (siatka pól i A*, rozdział [14.3](14_gra.md)) zastępujemy etapami:

| Etap | Treść | Stan |
|---|---|---|
| P0 | dane ruchu: podpola 8×8 (podłoga, ściana), graf pól | ✅ |
| P1 | wyszukiwanie drogi Calc1 | |
| P2 | chód po podłodze (głębokość, prędkość z animacji, obroty, start i zatrzymanie) | |
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
