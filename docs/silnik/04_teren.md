# 4. Teren

Teren to siatka bloków. Wnętrze każdego bloku ma gotowy kształt 3D z bazy `BlockDB`, a wygląd ustalają trzy rzeczy: **typ pola** (rodzaj gruntu), **detale** z `.l2m` i **otwory do tła**. Rozdział opisuje, jak z danych świata (rozdział 3) zbudować trójkąty i jak je pokolorować.

## 4.1 Kształt bloku z BlockDB ✅

`Data/BlockDB2.BDB` zawiera 118 688 kształtów. Pliki `BlockDB0/1/2` mają te same kształty w trzech poziomach szczegółów (opcja `LandscapeQuality`); najlepiej brać `BlockDB2`. Format: [blockdb.md](../formaty/blockdb.md).

Każdy kształt składa się z **trzech podsiatek** (górnej, środkowej i dolnej części bloku; każda może być pusta). Wierzchołek podsiatki to indeks do wspólnej **palety** punktów (x, y, z, u, v) i indeks normalnej w `SPHERE.DAT`. Paleta trzyma współrzędne **w obrębie bloku**, więc ten sam kształt można postawić w dowolnym miejscu:

```
dla każdego bloku (bx, by) świata:
    id = świat.blok[bx, by]
    dla podsiatki s w 0..2:
        dla wierzchołka i podsiatki:
            p = paleta[s.indeks_palety[i]]
            pozycja = (bx + p.x, by + p.y, p.z)          [bloki; × 200 = jednostki modelu]
            normalna = sphere[s.indeks_normalnej[i]]      [układ gry]
        trójkąty = s.indeksy (po 3, indeksy wierzchołków podsiatki)
```

- `p.x`, `p.y` leżą zwykle w [0; 1] (y w dół), a `p.z` w [7; 14,5]: 14,5 to przednia płaszczyzna (lita skała), 7 najgłębsze dno.
- Sąsiednie bloki pasują do siebie krawędziami, bo kształty w bazie są dobrane do sąsiadów (rekord bloku ma listę zgodnych sąsiadów). Silnik nie musi niczego zszywać: szablon zawiera już pasujące ID.
- Teren rysuje się z buforem głębi i **bez odrzucania tylnych ścian** (🔶 tak robimy my, obraz jest zgodny; kolejność wierzchołków w BlockDB nie była sprawdzana).

**Mapa wysokości.** Rekord bloku zawiera też uproszczony kształt: siatkę 8 × 8 wysokości `v` ∈ 0…15, gdzie głębokość to `z = 7 + 0,5·v` bloku (15 = lita skała na przedniej płaszczyźnie). Do rysowania nie jest potrzebna. Przydaje się do logiki (np. światło nieba w rozdziale 7 i kopanie) oraz do podglądu 2D. ✅ Zgadza się z eksportem `save_map` z gry.

## 4.2 Rodzaj gruntu: typ pola ✅

Młodszy półbajt typu pola wybiera teksturę podkładu całego bloku:

| typ & 0x0F | grunt | tekstury |
|---|---|---|
| 2, a także 0 | **C**: piasek | `Data/Ground/Gnd_C01..15.tga` |
| 3 | **B**: ziemia | `Gnd_B01..15.tga` |
| 4 (i pozostałe) | **A**: szara skała | `Gnd_A01..15.tga` |
| 5 | otwór do tła (4.5); grunt jak A | |

Bit 0x80 nie zmienia wyglądu (0x04 i 0x84 wyglądają tak samo; ❓ znaczenie). ✅ Wszystko to ustalone eksperymentem: ten sam korytarz z różnymi typami pól wyrenderowany w grze (`../lab/zrzuty/gra/typ_*.png`).

Tekstury gruntu mają 64 × 64 px i pustą alfę. Każdy rodzaj ma 15 wariantów, żeby powierzchnia się nie powtarzała.

**Jak dokładnie gra nakłada te tekstury, nie wiemy** (🔶/❓):
- paleta BlockDB ma współrzędne `u, v` (zakres ok. 0…7,5), których gra używa do teksturowania; reguła nie jest rozpracowana;
- przejścia między gruntami sąsiednich bloków gra robi maskami `Data/Ground/Msk_<ABCD>01.tga` (splatting: na granicy trójkąt dostaje oba grunty, a maska jest alfą);
- nie wiadomo, który z 15 wariantów dostaje dany blok.

**Zamiennik, który daje obraz bliski grze** (tak robi nasza przeglądarka):
1. Wariant 01–15 wybierz pseudolosowo z (bx, by), stałym hashem.
2. Teksturę nakładaj **trójpłaszczyznowo** w skali „1 tekstura na blok”: próbki z płaszczyzn (x, y), (x, z), (y, z) pozycji w blokach, z wagami `|n|⁴` znormalizowanymi do sumy 1. Pionowe ściany nie są wtedy rozciągnięte.
3. Wagi gruntów A/B/C licz dla wierzchołka dwuliniowo między **środkami** czterech najbliższych bloków (punkt (bx + 0,5, by + 0,5) ma wagę 1 dla gruntu swojego bloku). Daje to miękkie przejścia zamiast masek.

## 4.3 Kolor wierzchołków

Każdy wierzchołek terenu jest oświetlany jak w grze (rozdział 7): jasność bazowa z warstwy 2 interpolowana dwuliniowo z czterech narożników bloku w punkcie (p.x, p.y), stałe „słońce” zależne od normalnej i lampy w pobliżu. Wynik mnoży teksturę.

## 4.4 Detale z `.l2m` ✅

Na podkład nakłada się drugą warstwę: mech, porosty, kryształy. Wartość detalu bloku (u16, [szablony.md](../formaty/szablony.md) §2) ma w starszym bajcie **cztery narożniki po 2 bity**:

```
bity 15–14  narożnik lewy górny (TL)
bity 13–12  narożnik prawy górny (TR)
bity 11–10  narożnik lewy dolny (BL)
bity  9–8   narożnik prawy dolny (BR)
wartość narożnika: 0 = brak, 1 = zestaw <świat>, 2 = zestaw <świat>2
```

Dla każdego zestawu (1 i 2) zbuduj 4-znakowy kod z narożników, które mają ten zestaw (kolejność TL TR BL BR, `1` = jest). Kod `0000` oznacza brak nakładki. Inaczej tekstura to:

```
Data/Ground/detail/<świat><przyrostek>_<kod>.tga
    świat: urw, kris, lava, swf (z nazwy szablonu); przyrostek: "" dla zestawu 1, "2" dla zestawu 2
    pełne pokrycie 1111 ma 6 wariantów: <świat>_1111_a … _f
```

Przykłady (świat urw):

| wartość | bity 15…8 | TL TR BL BR | tekstury |
|---|---|---|---|
| `0x5000` | `01 01 00 00` | 1 1 0 0 | `urw_1100.tga`: mech wzdłuż górnej krawędzi |
| `0x1100` | `00 01 00 01` | 0 1 0 1 | `urw_0101.tga`: mech po prawej stronie |
| `0x5500` | `01 01 01 01` | 1 1 1 1 | `urw_1111_a.tga` … `_f`: cały blok |
| `0x6400` | `01 10 01 00` | 1 2 1 0 | `urw_1010.tga` (zestaw 1) **i** `urw2_0100.tga` (zestaw 2) |

Tekstury detalu mają gładkie plamy w narożnikach, więc sąsiednie bloki z pasującymi narożnikami łączą się w ciągłe połacie (jak w marching squares). ✅ Kolejność bitów odczytana z krawędzi plam w `unq_menue.l2m`.

Rysowanie:
- detal pokrywa **jeden blok**: UV = (x − bx, y − by), czyli położenie wierzchołka w obrębie bloku, bez zawijania;
- rysuje się go **drugim przebiegiem** po tych samych trójkątach bloku, z testem głębi `LEQUAL` i bez zapisu głębi;
- mieszanie **modulate ×2**: `wynik = cel · detal · 2`, czyli w OpenGL `glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR)`. Szary 128 nic nie zmienia (`detail/50PGray.tga` to neutralna tekstura), jaśniejsze piksele rozjaśniają, ciemniejsze przyciemniają. 🔶 Wniosek z porównania ze zrzutami;
- który wariant `_a`…`_f` dla `1111` wybiera gra, ❓. Wybieraj pseudolosowo z (bx, by).

## 4.5 Otwory do tła (typ 5) ✅

Pola typu 5 to **dziury w tylnej ścianie**, przez które widać tło: jezioro w menu, przepaść, niebo (rozdział 8). Krawędź dziury jest ostra i biegnie przez środki bloków.

**Krok 1: węzły otwarte.** Narożnik siatki (węzeł) (x, y) jest otwarty, jeśli pierwszy niezerowy typ (& 0x7F) w kolejności [pole (x, y), pole (x, y+1), pole (x+1, y), pole (x+1, y+1)] to 5:

```
funkcja otwarty(x, y):
    dla (px, py) w [(x, y), (x, y+1), (x+1, y), (x+1, y+1)]:
        jeśli (px, py) poza światem: kontynuuj
        t = świat.typ[px, py] & 0x7F
        jeśli t ≠ 0: zwróć t == 5
    zwróć FAŁSZ
```

**Krok 2: alfa wierzchołka.** Blok (bx, by) ma narożniki w węzłach (bx, by), (bx+1, by), (bx, by+1), (bx+1, by+1). Każdy narożnik ma wartość 0 (otwarty) albo 255 (zamknięty). Alfa wierzchołka to dwuliniowa interpolacja tych czterech wartości w punkcie (p.x, p.y) palety (obciętym do [0; 1]).

**Krok 3: rysowanie.** Teren rysuje się z **alpha testem: piksel zostaje, gdy alfa > 126** (bez mieszania). Dzięki interpolacji krawędź wypada w połowie odległości między otwartym a zamkniętym narożnikiem. Bloki z czterema otwartymi narożnikami pomija się w całości.

To, co widać przez otwór, to kolor czyszczenia ekranu (kolor strefy horyzontu) i modele tła stojące za terenem (rozdział 8).

Prawo pochodzi z [Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) (`Docs/RIM_CUTOUT_RENDER_RE.md`). ✅ Kształt i położenie otworów w menu zgadzają się ze zrzutem gry.

## 4.6 Podsumowanie: wierzchołek terenu

| Atrybut | Skąd |
|---|---|
| pozycja | (bx + p.x, by + p.y, p.z) z palety BlockDB |
| normalna | `SPHERE.DAT[indeks]` |
| wagi gruntów A/B/C | typ pola, dwuliniowo między środkami bloków (zamiennik) |
| alfa otworu | węzły otwarte, dwuliniowo w (p.x, p.y) |
| jasność bazowa B | warstwa 2, dwuliniowo w narożnikach bloku (rozdział 7) |
| kolor oświetlenia | prawo z rozdziału 7, liczone per wierzchołek |

Stany renderu: test głębi, zapis głębi, bez cullingu, bez mieszania, alpha test > 126; potem przebieg detali (4.4).
