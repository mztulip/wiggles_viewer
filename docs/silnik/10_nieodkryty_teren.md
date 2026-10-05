# 10. Nieodkryty teren

Gracz widzi tylko to, co odkryły jego krasnale. Pola nigdy niewidziane są **czarne**, a pola już odkryte, na które teraz nikt nie patrzy, przykrywa **ciepła szara mgła** (170, 170, 155), mocniejsza w głębi wykopów. **Nazwa.** Gra nie nazywa tego mechanizmu w polskich tekstach (samouczek mówi tylko „Pod powierzchnią jest ciemno.”); w skryptach i komendach występuje angielskie `fogofwar` (`set_fogofwar`, `get_max_fow`), którego nie zmieniamy, bo to nazwy komend gry. W dokumentacji i interfejsie mówimy o **nieodkrytym terenie**; „szara mgła” opisuje tylko wygląd terenu zbadanego, którego teraz nikt nie widzi.

Ten rozdział opisuje dane mgły, to, jak obiekty ją odkrywają i jak wpływa na obraz.

**Źródło i status.** Cały mechanizm opisuje [Zimennik/Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) (`Docs/FOG_OF_WAR_RE.md`, implementacja `Runtime/World/FogOfWarState.cs`), ze stałymi odczytanymi z gry. U nas to 🔶: wzory przenosimy dokładnie. ✅ Potwierdzone zrzutami: odkrywanie promieniami z „skorupą” 3 pól w skale (płaska ściana `ow_flat`: lita skała wokół szablonu zostaje czarna poza wąskim pasem przy krawędzi). Pozostałe prawa (szarość, wygasanie): do sprawdzenia.

## 10.1 Dane: jeden bajt na pole i gracza

Każde pole świata ma **8 bajtów mgły, po jednym na gracza** (0–7). Silnik rysuje według bajtu gracza lokalnego. Wartość bajtu `b`:

| b | stan | wygląd |
|---|---|---|
| 0 | nigdy niewidziane | czarne |
| 1 | odkryte, niewidoczne | pełna szara mgła |
| 2 … 0xFE | wygasa: było widoczne, mgła wraca | od czystego do szarego |
| 0xFF | widoczne teraz | czyste |

Nie ma osobnej warstwy „widoczne teraz”: obiekt, który widzi pole, ustawia mu 0xFF, a potem wartość co aktualizację spada o 4, aż do 1. Mgła zapisuje się w stanie gry (zapisy `.sav`).

Do każdego gracza należy też:
- **`fowBegin`**: wiersz, powyżej którego nic nie wygasa (niebo, powierzchnia), ustawiany komendą `set_fow_begin`;
- **lista wygasających pól**, żeby nie przeglądać całej mapy;
- **prostokąt odkrytego obszaru** (dla minimapy).

## 10.2 Kto odkrywa mgłę

Odkrywa każdy obiekt należący do gracza 0–7, który ma ustawiony zasięg widzenia:

```tcl
set_fogofwar <obiekt> <sx> <sy>      ;# zasięg w polach; −1 −1 wyłącza
```

Silnik mnoży zasięg przez 8 (drobne pola). Typowe wartości z danych: krasnale, trolle, wukery 14 × 8; budynki od 2 (drzwi) do 30 (wartownia), ustawiane przy budowie; windy 8 × 8.

**Aktualizacja** odbywa się co 4. krok logiki serwera (❓ częstotliwość kroku; port Unity przyjmuje 10 Hz, czyli mgła co 0,4 s):

```
dla każdego gracza: wygaszanie (10.4)
dla każdego obiektu gracza z zasięgiem:
    jeśli przesunął się o całe pole albo przypada jego kolej (co 6. aktualizację):
        odkryj wokół niego (10.3)
```

Punkt widzenia leży 1,5 pola nad stopami obiektu (`y − 12` drobnych pól), czyli na wysokości oczu.

## 10.3 Odkrywanie

**Wachlarz promieni z kolizją** (zwykły przypadek, `sy ≥ 1`). W jednostkach pól:

```
R  = sx (zasięg poziomy), Ry = sy
N  = floor(max(R, Ry) · 2π)                      liczba promieni
dla i = 0..N:
    kąt = i / N · 2π
    krok = (cos kąt, sin kąt · Ry/R) / 1,1           elipsa, krok nieco krótszy od pola
    p = punkt widzenia, w_skale = 0
    powtórz R razy:
        pole = (floor(p.x + 0,5), floor(p.y + 0,5))
        jeśli to nie pierwszy krok:
            jeśli w_skale == 3: przerwij promień
            jeśli w_skale > 0: w_skale += 1
            inaczej jeśli pole jest lite: w_skale = 1
        odkryj(pole)
        p += krok
```

Promień wchodzi najwyżej 3 pola w litą skałę i staje, więc ściany tunelu są widoczne na głębokość „skorupy”, ale skała nie jest przezroczysta. Pole jest lite, gdy pierwsza wysokość jego rekordu BlockDB (lewe górne drobne pole) wynosi 15.

**Inne kształty:**
- **prostokąt bez kolizji**, gdy oba zasięgi są ujemne: odkrywa ±|sx| × ±|sy| wokół obiektu, przez skałę. `FogRemover` przekazuje swoje argumenty wprost do `set_fogofwar`, więc `fog_remove 0 -50 -50` daje prostokąt, a `fog_remove 0 50 50` (menu) zwykły wachlarz promieni o promieniu 50;
- **linia** dla obiektów na drabinie (w górę) i w windzie (w dół szybu): w każdym wierszu dwa poziome przebiegi w lewo i w prawo, każdy z własnym limitem 3 pól skały.

**Odkrycie pola** (wspólne dla wszystkich kształtów):

```
funkcja odkryj(pole, gracz):
    b = mgła[pole, gracz]
    jeśli b == 0xFF: koniec
    jeśli b < 2 i wiersz pola ≥ fowBegin[gracz]: dopisz pole do listy wygasania
    mgła[pole, gracz] = 0xFF
```

## 10.4 Wygasanie

```
dla każdego pola z listy wygasania gracza:
    b = b − 4
    jeśli b < 2: b = 1, usuń z listy
```

Pełne wygaszenie 0xFF → 1 trwa 64 aktualizacje. Pole jest „czyste” tylko przez pierwsze 7 (b ≥ 0xE3), potem szarzeje przez ok. 25 aktualizacji (0xE2 → 0x80) i dalej stoi w pełnej szarości, choć bajt jeszcze spada. Pola powyżej `fowBegin` nie trafiają na listę, więc raz odkryte zostają czyste na zawsze.

## 10.5 Wpływ na obraz

Dla każdego narożnika bloku (wartość pola w jego lewym górnym narożniku, jak przy świetle bazowym w 7.2) liczy się trzy wielkości, a potem interpoluje je dwuliniowo w miejscu wierzchołka:

| wielkość | b = 0 | b = 1…0x7F | b = 0x80…0xE2 | b ≥ 0xE3 |
|---|---|---|---|---|
| **szarość** g (0…1) | 0 | 1 | (255 − 2·min(128, ⌊(b − 0x80)·1,3⌋)) / 255 | 0 |
| **czerń** k (mnożnik) | 0 | 1 | 1 | 1 |
| **lampy** S (rozdz. 7.1) | 0 | 0 | min(⌊(b − 0x80)·1,3⌋, 128) · 2, najwyżej 255 | 255 |

Kolor wierzchołka (terenu i modeli) po oświetleniu z rozdziału 7:

```
f = clamp((1 − (z − 7) · 0,125 · 1,02) · g, 0, 1)        z = głębokość wierzchołka w blokach
rozproszone = rozproszone · (1 − f)
dodatek     = dodatek + f · (170, 170, 155) / 255
kolor       = (tekstura · rozproszone + dodatek) · k
```

- Szarość zależy od głębokości: na przedniej płaszczyźnie (z = 14,5) to tylko ok. 4%, a na dnie wykopów (z = 7) 100%. Dlatego mgła wygląda jak opar wypełniający korytarze, a lico skały prawie nie szarzeje.
- W szarej mgle lampy nie świecą (S = 0), widać tylko światło bazowe.
- Teren pod szarą mgłą pokazuje stan bieżący (wykopy zrobione w tym czasie przez innych też widać).

**Czerń.** Pola z b = 0 w ogóle nie dostają siatki terenu. W ich miejscu silnik rysuje czarne czworokąty na przedniej płaszczyźnie (z = 14,5) i czarne ścianki wzdłuż granicy z odkrytymi sąsiadami, sięgające w głąb do dna sąsiada. Na granicy mnożnik k łagodzi przejście (dwuliniowo od 0 w czarnym narożniku do 1).

**Obiekty:**
- model rysuje się tylko wtedy, gdy największy bajt mgły z 7 punktów wokół obiektu (środek, 4 narożniki, środki boków pionowych prostokąta obiektu) wynosi co najmniej 0x80. W szarej mgle znikają więc postacie i ruchome rzeczy;
- klasy z `class_viewinfog 1` (domyślnie wszystkie dekoracje) pokazują w szarej mgle ostatnio widziany stan („duchy”), żeby krajobraz nie znikał;
- ikony i cząsteczki wymagają bajtu ≥ 0x81.

## 10.6 Komendy Tcl

| Komenda | Działanie |
|---|---|
| `set_fogofwar obj sx sy` | zasięg widzenia obiektu (×8 wewnętrznie); −1 −1 wyłącza; ujemne = prostokąt bez kolizji |
| `set_fow_begin N [gracz]` | `fowBegin = min(N, wysokość − 2)`; wiersze 5…N−1 (cała szerokość bez 2 ostatnich kolumn) stają się na stałe widoczne. Używane tylko w mapach pojedynczych (75–300), kampanie tego nie robią |
| `remove_black_fog gracz l t r b` | w prostokącie pól zamienia **tylko** 0 na 1: odkrywa teren, ale od razu w szarości |
| `get_max_fow obj [gracz]` | największy bajt wokół obiektu (jak przy rysowaniu); bez gracza: maksimum po wszystkich |
| `class_viewinfog b`, `set_viewinfog obj b` | czy obiekt pokazuje się w szarej mgle |
| `show_fogofwar b` | w wydanej grze nic nie robi |
| `obj_next_visible …` | najbliższy obiekt klasy w zasięgu z linią widzenia |

Opcja uruchomienia `-nofow` odkrywa całą mapę na stałe.

## 10.7 Podgląd w przeglądarce

`wigview` nie symuluje jeszcze obiektów odkrywających, ale pokazuje prawo renderu (opcja „Nieodkryty teren” w zakładce Mapy albo `--fow TRYB [PROMIEŃ [BAJT]]`):

| tryb | stan pól |
|---|---|
| 0 | brak mgły (wszystko 0xFF) |
| 1 | całość odkryta, niewidoczna (bajt 1): szara mgła wszędzie |
| 2 | prostokąt ±PROMIEŃ pól wokół ogniska kamery z bajtem BAJT, reszta szara |
| 3 | jak 2, reszta czarna (nieodkryta) |

Pola powyżej `set_fow_begin` scenariusza są zawsze widoczne. Uproszczenie 🔶: pola czarne rysujemy jako teren pomnożony przez zero, a nie jako czarne czworokąty na przedniej płaszczyźnie (różnica widoczna tylko przy otworach w tylnej ścianie).

## 10.8 Przykład: menu

`systeminit.tcl` tworzy obiekt `FogRemover` w (32,37; 39,5) i woła:
```tcl
call_method $FR fog_remove 0 50 50     ;# właściciel 0 (gracz lokalny), set_fogofwar 50 50
call_method $FR timer_delete -1
```
Wachlarz promieni o promieniu 50 pól odkrywa całą jaskinię menu i 3 pola skały wokół niej; dalsza lita skała zostaje czarna, ale w kadrze jej nie widać. ❓ Do sprawdzenia w laboratorium: zgodnie z prawem 10.4 po usunięciu `FogRemover` (`timer_delete`) scena powinna w ciągu kilkudziesięciu sekund zajść szarą mgłą; w naszym `capture.sh` zanotowaliśmy jednak, że obraz robił się czarny.

---
**Diggles-Unity** (ich opracowanie, 🔶 do sprawdzenia w laboratorium): [FOG_OF_WAR_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/FOG_OF_WAR_RE.md). Porównanie i stan: [mapa Diggles-Unity](../DIGGLES_UNITY.md).

**W kodzie OpenWiggles** (pomocniczo, zasada działania jest opisana wyżej): [`lib/map_scene.cpp`](../../src/lib/map_scene.cpp) (`updateFog`, `objectVisibleInFog`), [`shaders/game_lighting.glsl`](../../src/shaders/game_lighting.glsl). Wszystkie moduły: [mapa kodu](../KOD.md).
