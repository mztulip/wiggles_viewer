# 6. Kamera

Kamera gry krąży po sferze wokół punktu na mapie. Skrypty ustawiają ją komendą `set_view`. Ten rozdział podaje dokładne prawo, liczbowy przykład i przejście do OpenGL.

## 6.1 `set_view x y zoom dx dy` ✅

| Parametr | Znaczenie |
|---|---|
| `x`, `y` | punkt, na który patrzy kamera (ognisko), w blokach |
| `zoom` | oddalenie; w grze gracz zmienia je w zakresie 1,0–2,5, skrypty i przerywniki używają 0,5–6,0 |
| `dx` | pochylenie: kąt w pionie w radianach (ujemny = kamera nad ogniskiem, patrzy w dół) |
| `dy` | odchylenie: kąt w poziomie w radianach (dodatni = kamera z lewej, patrzy w prawo) |

Po kątach mogą stać dodatkowe parametry ruchu (`move freeze speed set`) do płynnych przejazdów w przerywnikach. Do ustawienia kamery wystarczy pięć pierwszych.

Prawo (układ gry, bloki; y w dół, z do widza):

```
cel  = (x, y, 14)
R    = zoom · 16 − 10                                   odległość od celu
oko  = cel + R · ( cos dx · sin(−dy),  sin dx,  cos dx · cos dy )
góra =           ( sin dx · sin(−dy), −cos dx,  sin dx · cos dy )
```

Cel leży na głębokości 14 bloków, tuż za przednią płaszczyzną (14,5). Przy dx = dy = 0 kamera stoi na osi +z i patrzy prosto w ekran.

**Perspektywa.** Kąt widzenia jest zdefiniowany w poziomie: **poziomy pół-kąt = 0,3 · (szerokość / wysokość)** radiana. Dla 4:3 daje to 0,4 rad, czyli 45,8° w poziomie. Pionowy kąt wynika z proporcji:
```
tg(pionowy pół-kąt) = tg(0,3 · a) / a,     a = szerokość / wysokość      (4:3 → 35,2° w pionie)
```
Płaszczyzny przycinania: **bliska 0,5 bloku, daleka 180 bloków** (wartości podane przez Diggles-Unity i potwierdzone przez nich z macierzy projekcji gry). Bliska ma znaczenie dla panoramy (rozdział 8.2), bo od niej zaczynają się promienie.

Prawo pochodzi z [Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) (`DigglesCameraController.cs`). ✅ Kadr menu w 1024 × 768 pokrywa się ze zrzutem gry, a kadr OBERWELT na wprost (`dx = dy = 0`) również.

## 6.2 Przykład: menu

`set_view 32.368 40.858 1.38 -0.355 0.165`:

```
R    = 1,38 · 16 − 10 = 12,08
oko  − cel = 12,08 · (cos(−0,355)·sin(−0,165), sin(−0,355), cos(−0,355)·cos(0,165))
           = (−1,860, −4,199, 11,173)
oko  = (30,508, 36,659, 25,173)          kamera nieco z lewej i 4 bloki wyżej, patrzy w dół
góra = (0,057, −0,938, −0,343)           prawie „−y”, czyli w górę ekranu
```

## 6.3 Przycinanie kątów przy dużym oddaleniu 🔶

Gra nie pokazuje dowolnych kątów, gdy kamera jest daleko. Ustaliliśmy to obserwacją: zrzuty sceny OBERWELT z różnymi `set_view` porównaliśmy z naszym renderem i szukaliśmy kąta, który daje ten sam obraz.

| zoom | R | zadany kąt → widoczny kąt |
|---|---|---|
| 1,38 | 12,1 | dy 0,57 → 0,57; dx 0,5 → 0,5 (bez zmian) |
| 1,7 | 17,2 | dy 0,57 → 0,57 |
| 2,0 | 22 | dy 0,57 → ≈ 0,35 |
| 2,25 | 26 | dy 0,57 → ≈ 0,23 |
| 2,5 | 30 | dy 0,3 i 0,57 → ≈ 0,155; dx 0,22 → ≈ 0,15; dx −0,4 → ≈ −0,15 |

Pomiary dobrze opisuje wzór (to dopasowanie, nie stała z gry):

```
limit(R) = 2,23 / (R − 15,6)    dla R > 15,6, w przeciwnym razie bez limitu
dx_widoczny = clamp(dx, −limit, limit),  tak samo dy
```

Przykładowe wartości: R = 22 → 0,348; R = 26 → 0,214; R = 30 → 0,155. `get_view 1` zwraca przy tym kąty zadane, a nie przycięte, a obraz po 60 i 160 sekundach jest taki sam (to nie jest powolny dojazd kamery). Skąd limit się bierze, nie wiadomo; najpewniej chroni przed pokazaniem krawędzi świata lub tła.

## 6.4 Przejście do OpenGL

Trzymaj świat w układzie gry, a odbicie osi Y dodaj na końcu (rozdział 1.5):

```
oko_gl  = (oko.x,  −oko.y,  oko.z)
cel_gl  = (cel.x,  −cel.y,  cel.z)
góra_gl = (góra.x, −góra.y, góra.z)
widok   = lookAt(oko_gl, cel_gl, góra_gl)
projekcja = perspective(pionowy kąt, a, bliska, daleka)
model   = scale(1, −1, 1) · macierz obiektu w układzie gry
```

Wszystkie wielkości w tej samej skali (bloki albo jednostki modelu, × 200).

## 6.5 Inne komendy kamery

| Komenda | Działanie |
|---|---|
| `get_view` | bieżący stan kamery (zaraz po starcie zwraca jeszcze śmieci) |
| `get_view 1` | stan zadany ostatnim `set_view` |
| `set_viewpos x y z zoom dx dy …` | ❓ wariant z pełną pozycją |
| `set_view_begin N` | „Y-beginning of the view”: ograniczenie przewijania w górę. Na kadr ustawiony przez `set_view` nie wpływa (sprawdzone) |
| `camera_set`, `set_camerafollow` | przejazdy i śledzenie obiektu (przerywniki) ❓ |

---
**Diggles-Unity** (ich opracowanie, 🔶 do sprawdzenia w laboratorium): [COORDINATE_SYSTEM](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/COORDINATE_SYSTEM.md). Porównanie i stan: [mapa Diggles-Unity](../DIGGLES_UNITY.md).

**W kodzie OpenWiggles** (pomocniczo, zasada działania jest opisana wyżej): [`lib/map_world.h`](../../src/lib/map_world.h) (`GameView`), [`apps/wigview/main.cpp`](../../src/apps/wigview/main.cpp) (`viewProj`, `gameViewAngleLimit`), [`apps/game/main.cpp`](../../src/apps/game/main.cpp). Wszystkie moduły: [mapa kodu](../KOD.md).
