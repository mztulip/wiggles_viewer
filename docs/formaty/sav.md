# Zapis gry `.sav`

Pliki `Data/Gamesave/*.sav`: zapis ręczny (`save_<nazwa>.sav`) i szybki zapis (`quick_Quicksave_<data>.sav`). Ten opis obejmuje część potrzebną, żeby **odtworzyć mapę z zapisu**: kamerę, teren, mgłę wojny i obiekty (klasa, pozycja, obrót). Logikę obiektów (zmienne Tcl, akcje, trasy) czytamy tylko po to, żeby ją przeskoczyć.

**Źródło i status.** Układ opracował [Zimennik/Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) (MIT): `Docs/ORIGINAL_SAVE_FORMAT_RE.md`, `Docs/save-re-reports/B4-fixed-engine-state.md`, `Runtime/Save/OriginalSaveObjectDocument.cs`. ✅ Wszystko poniżej sprawdziliśmy na trzech zapisach z laboratorium (polska wersja 1.0 build 844, wersja formatu 0xA5). Nasz czytnik przechodzi przez cały plik i trafia w każdy znacznik kontrolny. Sprawdzenie: `build/release/test3db` (funkcja `testSaves`). Implementacja: `src/lib/save_game.cpp`, budowa świata: `buildWorldFromSave` w `src/lib/map_world.cpp`.

## Kodowanie

- Cały plik to **strumień gzip** (zlib 1.1.3 w grze). Po rozpakowaniu: od ok. 100 KB (mapa 64×64) do ok. 7 MB (500×200).
- Liczby little-endian **bez wyrównania**: `i32`, `f32`, `f64`, `u8`.
- **Napis** = `i32 n` + `n` bajtów, ostatni bajt to zero (długość obejmuje zero). Pusty napis ma `n = 1`.
- **Znacznik** = `i32` o znanej wartości (1, 2, …, 6). Gdy wartość się nie zgadza, czytnik zgubił synchronizację.
- **Bajt obecności** = `u8` 0 albo 1. Przy 1 następuje opcjonalny blok.

## Układ

```
i32   wersja                0xA5
str   nagłówek              "V1.0.844r"
f64   czas gry w sekundach
i32   znacznik 1
205 B sekcja 1 (stan silnika); kamera na przesunięciach 45..64:
        +45 f32 ognisko X (bloki)   +49 f32 ognisko Y
        +53 f32 zoom                +57 f32 kąt dx    +61 f32 kąt dy
      (te same liczby co argumenty set_view x y zoom dx dy, rozdz. 6)
i32   znacznik 2
i32   znacznik 3
— świat —
i32   szerokość W, i32 wysokość H (w blokach)
4 × i32  (❓)
W·H × rekord pola (53 B), wiersz po wierszu od góry
8 × i32  fowBegin dla graczy 0..7 (set_fow_begin)
i32   lightBegin (set_light_begin; −10000 = brak nieba)
i32   groundBegin (set_ground_begin; −10000 = brak)
i32 n  + n × 37 B   źródła wody
i32 n  + n × 41 B   tamy wody
8 B
4 × (i32 n + n × 8 B)   listy wody
1 B
8 × (i32 n + n × 8 B)   listy wygasającej mgły (po jednej na gracza)
i32   znacznik 4, znacznik 5, znacznik 6
— obiekty —
deklaracje: powtarzaj { i32 id; jeśli id == −1 koniec; str klasa }
str   pusty napis (separator)
i32 n + n × rekord pomocniczy (28 B + ładunek, niżej)
ciała: powtarzaj { i32 id; jeśli id == −1 koniec; ciało obiektu }
(dalej: stan rozgrywki, którego nie czytamy)
```

### Rekord pola (53 B)

| Przesunięcie | Typ | Znaczenie |
|---|---|---|
| 0 | u32 | ID bloku w BlockDB (kształt) |
| 4 | u8 | koszt nawigacji |
| 5 | 4 B | kopanie (postęp) |
| 9 | 4 B | ❓ |
| 13 | u8 | stan podłogi |
| 14 | u8 | flagi szablonu |
| 15 | u16 | wartość detali `.l2m` (jak warstwa 2 szablonu) |
| 17 | u8 | stan terenu, młodszy bajt: `& 3` = rodzaj gruntu, bit 0x80 jak w typie pola `.pmp` |
| 18 | u8 | stan terenu, starszy bajt (🔶 losowy wariant tekstury) |
| 19 | u8 | typ płynu, bufor A: 0 brak, 1 woda, 2 siarka, 3 lawa, 4 lawa z wodą ✅ |
| 20 | u8 | typ płynu, bufor B (gra trzyma dwa bufory symulacji) |
| 21 | f32 | wypełnienie, bufor A: 0..1 = część pola; powyżej 1 ciśnienie słupa (0,85 przy powierzchni, każdy wiersz niżej +1) ✅ |
| 25 | f32 | wypełnienie, bufor B (prawie równe A) |
| 29, 33 | 2 × f32 | przepływ poziomy (A, B) 🔶 |
| 37, 41 | 2 × f32 | przepływ pionowy (A, B) 🔶 |
| 45 | 8 × u8 | bajt mgły wojny graczy 0..7 (rozdz. 10.1) |

**Rodzaj gruntu w zapisie a typ pola `.pmp`** ✅ (porównanie pól zapisu z szablonami, z których powstała mapa):

| zapis (`& 3`) | grunt | typ `.pmp` |
|---|---|---|
| 0 | A, skała | 4 |
| 1 | B | 3 |
| 2 | C | 2 (i 0) |
| 3 | otwór do tła | 5 |

Przykład: pole z bajtem 17 = 0x81 to grunt B (`0x81 & 3 = 1`) z bitem 0x80, czyli typ `.pmp` = 3 | 0x80 = 0x83.

### Ciało obiektu

Pozycja i obrót leżą na początku, ale żeby dojść do następnego obiektu, trzeba przeczytać całe ciało. Pola pomijane opisujemy liczbą słów `i32`:

```
str   klasa (ta sama co w deklaracji)
i32   typ obiektu silnika (−1..12; 0 dekoracja, 1 postać, 5 budynek …)
3 × f32  pozycja x, y, z
3 × i32  pozycja przejściowa
3 × i32  czas utworzenia, początek i długość pracy
13 × i32 rdzeń; słowa 7..9 to obrót rx, ry, rz (f32, radiany)
wektor i32 (n + n słów)   obiekty potomne
wektor napisów (n + n napisów)
16 × i32
i32 obecność zmiennych Tcl (0/1); przy 1: i32 n + n × (str nazwa, str wartość)
1 × i32 ID obiektu stanu, str nazwa instancji (imię krasnala), 8 × i32
trasa: wektor i32 + 2 × i32
i32; jeśli ≠ 0: 512 × i32 (siatka)
19 × i32
bajt obecności cząsteczek; przy 1: 160 × i32
3 B, str napis wyświetlania, 17 × i32, 1 B, 11 × i32
typ 1 lub 11: 9 × i32, wektor napisów, bajt obecności
typ 1: 1 × i32, potem powtarzaj { i32 s; −1 = koniec; s == 1: str + 1 × i32 }
typ 5, 6, 7, 10: i32 n + n × (2 × i32, str, str, 2 × i32, 2 B),
                 2 × i32, 7 B, 3 × i32, 8 B, 4 × i32, 1 B, 53 × i32, 2 B
typ 1: 4 × i32
bajt obecności historii; przy 1: i32 n + n × trasa
16 × i32, 16 B, 16 × i32, 1 B, 1 × i32, 1 B
bajt obecności akcji; przy 1: wariant akcji (niżej)
2 × i32, potem 64 × (bajt obecności; przy 1: ładunek pomocniczy)
i32 n + n × ładunek pomocniczy
1 × i32, 1 B
```

**Wariant akcji:** `i32 v`, a potem:
- v = 0: 8 × i32, `i32 n` + n × 10 × i32, 3 × i32, 48 × i32, 15 × i32, bajt obecności;
- v = 1: 7 × i32, bajt obecności, 2 × i32;
- v = 2: 7 × i32, bajt obecności, 1 × i32, wektor i32, wektor napisów, 1 × i32.

**Ładunek pomocniczy:** 32 B nazwy, 4 × i32 odwołań, 9 × i32, str, str, 3 × i32, 8 B. Rekord pomocniczy z listy przed ciałami ma przed ładunkiem jeszcze 28 B nagłówka.

**Odczytane pola ciała** (oprócz klasy, typu, pozycji, obrotu) ✅ na zapisie kampanii:
- grupa 10 × i32 zaczynająca się od `+0x26C` (po `2 × i32` i `4 × i32` za siatką): element [3] = **rodzic przypięcia** (ID obiektu, `+0x278`), [4] = **punkt zaczepienia** (`+0x27C`). Przykład: 5 czapek `Dummy_Muetze_*` przypiętych do 5 krasnali w punkcie 4 (głowa);
- trzecie i32 po napisie wyświetlania = **właściciel** (`+0x36C`): −1 świat, 0..7 gracze.
- **zmienne Tcl obiektu** (i32 obecność, i32 n, n × (str nazwa, str wartość)): gra po wczytaniu przywraca je zamiast `obj_init`. My wykonujemy `obj_init` (ustawia timery, których kolejki jeszcze nie czytamy) i nakładamy na nie zapisane zmienne; dlatego obiekty z zapisu mają w skryptach te same ID co w zapisie (zmienne odwołują się do ID, np. `current_muetze_ref`).

**Pozycja z** jest w skali skryptu, czyli tak jak argument `new` w szablonie: świat z = z · 0,5 + 7 (rozdz. 5). Przykład z zapisu OBERWELT: obiekt 89 `Dummy_Moos_um_a` ma (23,531, 42,5, 13,54) i obrót ry = 6,123, dokładnie jak w szablonie, z którego pochodzi.

## Sekcje po obiektach

Po znaczniku −1 kończącym ciała obiektów, aż do znacznika 7 (kolejność i rozmiary za Diggles-Unity: `Tools/SaveFormatResearch/parse_a5_object_bodies.py`, `Docs/save-re-reports/B3-support-class-spatial.md` §2–4; ✅ nasz czytnik dochodzi do znacznika 7 we wszystkich naszych zapisach):

```
18 × 512 × i32    tablice atrybutów graczy (owner −1..7, po 2 × 512)
2 × i32
3 × { tablica, tablica, wektor i32, 2 B }   tablica = i32 n + n × (str klasa, 7 × i32, 3 B, str, i32)
i32 liczba rekordów akcji menedżera (zawsze 0)
— tabela klas: lekkie obiekty zastępcze —
i32, 0x3000 × i32 (stos wolnych miejsc), i32, 1 B, wektor i32, wektor i32
powtarzaj { i32 indeks; −1 = koniec;
            16 × f32 macierz 4×4, 3 × f32 pozycja, i32, wektor i32, i32 własny indeks, i32 ID obiektu-wzoru,
            str klasa, 1 B, 4 × i32, 2 B, 10 × i32, str nazwa instancji, 3 B, i32, 6 × i32, 1 B }
— kubełki przestrzenne: ceil(W/8) · ceil(H/8) —
każdy: i32 n + n × { 3 × f32 pozycja, 3 × f32 obrót, str klasa, 4 × i32, 2 B }
i32 znacznik 7
```

- **Obiekty zastępcze** to tanie kopie żywych obiektów do rysowania z daleka. W naszych zapisach wszystkie wskazują istniejący obiekt (OBERWELT: 893 z 893), więc pomijamy te, których obiekt-wzór jest na liście.
- **Nasiona** w kubełkach to dekoracje czekające na utworzenie, gdy pozwoli na to poziom szczegółów. Pozycja jak w szablonie (z w skali skryptu), obrót w radianach. Kubełek = `ceil(W/8)·floor(y/8) + floor(x/8)`. Rysujemy je jak zwykłe obiekty.

## Odtworzenie mapy z zapisu

Zapis zawiera **gotowy świat**: nie trzeba wykonywać skryptu scenariusza ani składać szablonów. Wystarczy:

1. Siatka bloków W×H: `blok` z rekordu, typ pola z tabeli rodzajów gruntu (z bitem 0x80), detal `.l2m` z przesunięcia 15. Dalej jak w rozdz. 4.
2. Linia nieba `lightBegin` i światło nieba (rozdz. 7.3), wiersz gruntu panoramy `groundBegin` (rozdz. 8).
3. Obiekty: klasa, pozycja, obrót jak obiekty szablonu (rozdz. 5), razem z nasionami dekoracji (§ „Sekcje po obiektach”). Strefy horyzontu `Info_Fog_*` są zwykłymi obiektami zapisu, więc tło działa samo (rozdz. 8). Motyw świata (zestaw tekstur detali `urw`/`kris`/`lava`/`swf`) bierzemy z nazw tych klas, bo zapis nie trzyma go jawnie (🔶).
4. Kamera z sekcji 1.
5. Mgła wojny: bajt gracza 0 z przesunięcia 45 (rozdz. 10).
6. Płyny: typ (19) i wypełnienie (21) każdego pola, rysowane jak w [rozdziale 13](../silnik/13_plyny.md).

**Układ bajtów płynu** ustaliliśmy na zapisie z wodą zrobionym w laboratorium: szablon `kris_unq_060` z obiektem `Wasser` (`{volume 1000} {vpf 5}`), po 40 s komenda Tcl `gamesave ow_woda` (gra zapisuje wtedy plik `ow_woda` w swoim katalogu, bez rozszerzenia). Basen 9 × 8 pól: w wierszu powierzchni wypełnienie 0,85, w każdym niższym o 1 więcej (1,85, 2,85…), typ 1. Pola obok basenu zachowują typ 1 przy wypełnieniu 0.

**Dekoracje poza listą obiektów** ✅: część dekoracji nie leży na liście obiektów, tylko w sekcjach po ciałach obiektów (§ „Sekcje po obiektach”). W zapisie z laboratorium lista ma 18 obiektów, a ściany, podłogi i lampy szablonu to 94 „nasiona” w kubełkach; w zapisie OBERWELT jest ich 361 (łąki, ścieżki, trawa).

W wigview: zakładka Zapisy obok Mapy (lista z `Data/Gamesave`, przycisk odświeżania, pole na ścieżkę do dowolnego pliku `.sav`) albo `wigview --save save_SaveGame.sav` (także `--save /ścieżka/plik.sav`). Mgła z zapisu to tryb 4 (`--fow 4`, domyślny dla zapisów); niezbadana część mapy jest czarna jak w grze. Pole „Odkryj mapę” (albo `--fow 0`) wyłącza mgłę i pokazuje cały teren i wszystkie obiekty; wybór zostaje przy przełączaniu zapisów.

## Czego nie czytamy

❓ Pozostałe pola sekcji 1 (stan silnika poza kamerą), cztery słowa po wymiarach świata, woda, zmienne Tcl i stan pracy obiektów, wszystko po ciałach obiektów (gracze, ekonomia, fabuła). Pełny opis w dokumentach Diggles-Unity wymienionych wyżej. Potrzebne będą przy wczytywaniu zapisu do działającej gry (faza 6).
