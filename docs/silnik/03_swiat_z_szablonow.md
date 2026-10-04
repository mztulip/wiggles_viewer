# 3. Świat z szablonów

Świat gry to prostokątna siatka bloków. Każdą mapę stałą (i menu) skleja się z gotowych kawałków, czyli **szablonów**, tak jak z klocków. Ten rozdział opisuje dane świata i to, jak szablon zmienia je przy nakładaniu.

## 3.1 Dane świata

Dla każdego bloku (bx, by) świat przechowuje:

| Pole | Typ | Znaczenie | Rozdział |
|---|---|---|---|
| `blok` | u32 | ID kształtu w BlockDB (0 … 118 687). Kształt wnętrza bloku: lita skała, korytarz, podłoga… | 4 |
| `typ` | u8 | **typ pola**: rodzaj gruntu (tekstura) albo otwór do tła | 4 |
| `warstwa2` | u8 | **światło bazowe** pola (0 = ciemny kąt, 4 = zwykła skała, 20 = dzień) | 7 |
| `detal` | u16 | nakładki detali (mech, kryształy) w narożnikach bloku | 4 |

oraz listę **obiektów** (klasa, pozycja, obrót, poziom szczegółów).

`map create W H` tworzy świat W × H bloków wypełniony wartościami domyślnymi:
- `blok = 0` (lita skała: wszystkie drobne pola mają wysokość 15),
- `typ = 0` (wygląda jak typ 2, piasek; rozdział 4),
- `warstwa2 = 4` 🔶 (wartość zwykłej skały; dla litej skały i tak nie widać różnicy),
- `detal = 0`, brak obiektów.

Mapa menu ma 64 × 64 bloki, a OBERWELT 500 × 200.

## 3.2 Szablon

Szablon to trzy pliki w `Data/Templates/` (gra czyta z tego katalogu; podkatalogi `pmp/`, `l2m/`, `tcl/`, `ent/` z plikami `ADD.BAT` to robocze kopie edytora, różne od wersji używanych przez grę):

| Plik | Zawartość | Format |
|---|---|---|
| `<nazwa>.tcl` | skrypt: wskazuje pliki terenu i tworzy obiekty | [szablony.md](../formaty/szablony.md) §3 |
| `<kształt>.pmp` | siatka bloków: typ, ID bloku, warstwa 2 (+ maska ruchu) | [szablony.md](../formaty/szablony.md) §1 |
| `<nazwa>.l2m` | detale w narożnikach bloków | [szablony.md](../formaty/szablony.md) §2 |

Jeden kształt `.pmp` służy wielu szablonom: `urw_gng_021_a.tcl` i `kris_gng_021_c.tcl` używają tego samego `gng_021.pmp`, a różnią się detalami i dekoracjami. Nazwy mają postać `<świat>_<rodzaj>_<nr>_<wariant>`:
- świat: `urw` (dżungla), `kris` (kryształy), `lava`, `swf` (siarka), `obw` (powierzchnia), `unq_*` (pojedyncze, np. menu), `multi_*`;
- rodzaj: `gng` (korytarz), `hol` (jaskinia), `gsg`, `caves`, `start`.

Skrypt szablonu definiuje jedną procedurę:

```tcl
proc MapTemplateSet {xoffset yoffset} {
    loading_objdetail high
    map_template data/templates/gng_021.pmp $xoffset $yoffset
    map_setlayer2 $xoffset $yoffset data/templates/urw_gng_021_a.l2m
    loading_objdetail medium
    sel /obj
    set objhandle [new Dummy_Moos_um_a "" "[expr 2.53098 + $xoffset] [expr 2.5 + $yoffset] 13.54" "0 6.12319 0"]
    set_owner $objhandle -1
    set_physic $objhandle true
    ...
}
```

Scenariusz wywołuje `call data/templates/urw_gng_021_a.tcl`, co tylko definiuje procedurę, a potem `MapTemplateSet 21 40`, co nakłada szablon z lewym górnym rogiem w bloku (21, 40). Przesunięcia są w blokach i mogą być ułamkowe tylko dla obiektów: teren nakłada się zawsze na całe bloki.

## 3.3 Nakładanie terenu: `map_template` ✅

Najważniejsza reguła: **pole szablonu jest przezroczyste, jeśli on sam i jego sąsiedzi z prawej, z dołu i po przekątnej mają typ 0** (z pominięciem najstarszego bitu). Przezroczyste pole nie zmienia świata. Dzięki temu szablon o nieregularnym kształcie, wpisany w prostokąt `.pmp`, nie wycina prostokątnej dziury w sąsiednich szablonach.

```
funkcja przezroczyste(szablon, x, y):
    dla (dx, dy) w {(0,0), (1,0), (0,1), (1,1)}:
        jeśli (x+dx, y+dy) leży w szablonie i (szablon.typ[x+dx, y+dy] & 0x7F) ≠ 0:
            zwróć FAŁSZ
    zwróć PRAWDA

funkcja map_template(świat, szablon, ox, oy):
    dla każdego pola (x, y) szablonu:
        (wx, wy) = (ox + x, oy + y)
        jeśli (wx, wy) poza światem: pomiń
        jeśli przezroczyste(szablon, x, y): pomiń
        świat.blok    [wx, wy] = szablon.blok    [x, y]
        świat.typ     [wx, wy] = szablon.typ     [x, y]
        świat.warstwa2[wx, wy] = szablon.warstwa2[x, y]
```

Okno 2×2 sięga w prawo i w dół, bo kształt bloku zależy od jego narożników: pole typu 0 tuż obok niezerowego niesie dolną lub prawą krawędź kształtu, więc musi zostać zapisane.

Przykład: typy w szablonie 4 × 4 (`.` = 0) i wynik (Z = pole zapisane, P = przezroczyste):
```
typy        wynik
. . . .     Z Z Z P      (0,0): okno zawiera (1,1) = 4, więc Z
. 4 4 .     Z Z Z P      (3,1): okno to (3,1) i (3,2), same zera, więc P
. 4 4 .     Z Z Z P      (0,2): okno zawiera (1,2) = 4, więc Z
. . . .     P P P P      ostatni wiersz: okno poza dolną krawędzią, zostają same zera
```

⚠ Typ 0 **nie** oznacza „niewykopanego tunelu”. W eksperymencie szablon z samymi zerami był w całości przezroczysty, więc widać było domyślną litą skałę świata pod nim.

Prawo przezroczystości podaje [Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) (`BdbPmpWorldBuilder.cs`, `BlitPmpIntoWorld`). ✅ Złożone przez nas sceny menu i OBERWELT zgadzają się ze zrzutami gry.

## 3.4 Nakładanie detali: `map_setlayer2` ✅

```
funkcja map_setlayer2(świat, l2m, ox, oy):
    dla każdego pola (x, y) w l2m:
        jeśli l2m[x, y] ≠ 0: świat.detal[ox + x, oy + y] = l2m[x, y]
```

Zero nie nadpisuje świata. Plik `.l2m` może mieć 0 bajtów, co oznacza brak detali. Znaczenie wartości: rozdział 4.4.

## 3.5 Obiekty szablonu ✅

Każde `new` w procedurze tworzy obiekt:

```tcl
new <Klasa> "<nazwa>" "<x> <y> <z>" "<rx> <ry> <rz>"
```

- `x`, `y` to pozycja w blokach. W szablonie jest zapisana jako `[expr 2.53098 + $xoffset]`, czyli względem lewego górnego rogu szablonu.
- `z` to głębokość w skali skryptu: `z_świata = z · 0,5 + 7` (rozdział 1.5).
- `rx ry rz` to obrót w radianach (rozdział 5.3). Najczęściej używane jest tylko `ry`.
- Klasa wskazuje model (rozdział 5).

**Poziom szczegółów.** `loading_objdetail high|medium|low|off` ustawia poziom dla kolejnych obiektów. Pozwala to nie tworzyć drobnych dekoracji przy niskiej jakości grafiki (🔶 dokładna reguła wyboru nie jest znana; przy najwyższej jakości tworzone są wszystkie). Obiekty bez tej komendy przed sobą traktuj jak `high`.

Komendy `set_owner`, `set_physic`, `set_autolight`, `set_boxed` po każdym `new` ustawiają właściwości logiki (właściciel −1 = nikt, fizyka, automatyczne pochodnie, pakowanie). Na sam obraz nie wpływają.

**Skóra jaskiń.** Szablony `*_caves_*` stawiają duże obiekty `Dummy_<świat>_caves_*`: gotowe siatki ścian jaskini w trzech poziomach szczegółów. Teren BlockDB jest pod nimi tylko szkieletem.

⚠ Błąd w danych: `kris_caves_2_a.tcl` stawia skórę jaskini w x ≈ 0, a pozostałe warianty w x ≈ 8. Gra i tak odrzuca szablony `kris_caves_*` jako „unknown class”.

## 3.6 Przykład: menu

```
map create 64 64
unq_menue       w (25, 28)   jaskinia z jeziorem w tle
urw_gng_021_a   w (21, 40)   korytarz pod jaskinią
urw_gng_022_a   w (45, 40)   drugi korytarz
set_view 32.368 40.858 1.38 -0.355 0.165
```

Po złożeniu świat ma 64 × 64 bloków, z czego zmienione są tylko pola trzech szablonów, a reszta to lita skała. Kamera patrzy na blok (32,4; 40,9), czyli na dno jaskini. Porównanie z grą: `../lab/zrzuty/porownania/menu_kamera_gry.png` (poza repozytorium).

## 3.7 Warstwy dodatkowe

- **Maska w `.pmp`** (przebiegi u16) to 🔶 płaszczyzna ruchu postaci (wartości 0/1/3), nie kształt terenu. Do obrazu niepotrzebna. Opis formatu: [szablony.md](../formaty/szablony.md) §1.
- **`.ENT`** to zwykle puste pliki (0 B), ❓.
- Gra po złożeniu przelicza jeszcze **światło bazowe nieba** (rozdział 7.2). Zmienia ono warstwę 2 pól pod gołym niebem.

## 3.8 Świat z zapisu gry

Zapis `.sav` przechowuje **gotowy, złożony świat**: dla każdego pola blok, rodzaj gruntu i detal `.l2m`, a do tego wszystkie obiekty z pozycją i obrotem, linie nieba i gruntu, kamerę i mgłę wojny. Wczytanie zapisu pomija więc cały ten rozdział: nie wykonujemy skryptu scenariusza ani nie nakładamy szablonów. Budujemy siatkę wprost z rekordów pól (rodzaj gruntu przeliczamy na typ pola `.pmp`), obiekty traktujemy jak obiekty szablonu i przeliczamy światło nieba (rozdział 7) tak samo jak po `map create`. Układ bajtów i tabela rodzajów: [formaty/sav.md](../formaty/sav.md). ✅ Odczyt sprawdzony na zapisie OBERWELT (500 × 200, 1381 obiektów) i szybkich zapisach menu; obraz porównany z widokiem scenariusza tylko wzrokowo (🔶).
