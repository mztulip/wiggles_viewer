# 7. Oświetlenie

Gra nie ma dynamicznych cieni ani światła kierunkowego w zwykłym sensie. Każdy wierzchołek terenu i modelu dostaje kolor z jednego wzoru: **jasność bazowa pola × stałe „słońce” zależne od normalnej + suma lamp**, z twardym nasyceniem. Teren i modele używają dokładnie tego samego prawa.

Prawo podaje [Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) (`Docs/RENDERING_LIGHTING_RESEARCH.md` §2a, `TUNNEL_TORCH_LIGHTING_RE.md` §6). ✅ Sprawdziliśmy je na zrzutach menu (jaskinia z lampami) i OBERWELT (powierzchnia w dzień).

## 7.1 Wzór

Dla wierzchołka o pozycji `p` (bloki, układ gry) i jednostkowej normalnej `N` (układ gry, po obrocie obiektu), osobno dla kanałów R, G, B:

```
I = ( B · słońce(N)  +  S · Σ_lampy  c · exp(−2 · d² / R²) · max(N · L, 0,1) ) · 0,1

słońce(N) = 3,6 − 2,546 · N.x − 2,546 · N.y
d         = |pozycja_lampy − p|,  L = (pozycja_lampy − p) / d
lampa pomijana, gdy d² ≥ R²

rozproszone = min(I, 255) / 255
dodatek     = clamp(I − 255, 0, 64) / 255
kolor       = tekstura · rozproszone + dodatek
```

- **B**: jasność bazowa w miejscu wierzchołka (7.2).
- **słońce(N)**: stała funkcja normalnej. Najjaśniej świecą powierzchnie zwrócone w lewo i w górę (N.x < 0, N.y < 0; pamiętaj, że y rośnie w dół). Składowa z nie ma wpływu.
- **S**: współczynnik mgły wojny. 255 dla pola widocznego, mniejszy dla odkrytego, ale niewidocznego (rozdział 10). Bez mgły wojny przyjmij 255.
- **c, R**: kolor i promień lampy (7.4).
- **Nasycenie**: rozproszone nie przekracza 1, a nadmiar (najwyżej 64/255) dodaje się jako jasny „połysk” niezależny od tekstury (w Direct3D 7 to kolor specular). Dzięki temu kilka lamp obok siebie nie prześwietla obrazu: pierwsza nasyca, kolejne już nic nie dodają.

Gra liczy to per wierzchołek, a kolor interpoluje po trójkącie (Gouraud).

## 7.2 Jasność bazowa B ✅

Każde pole świata ma bajt światła bazowego (`warstwa2`, rozdział 3.1). Wartości:

| wartość | znaczenie |
|---|---|
| 0 | ciemny kąt, nisza |
| 4 | zwykły tunel lub skała |
| 8 | najjaśniejsze wartości w szablonach |
| 20 | pełny dzień (pole pod gołym niebem, 7.3) |

Bajt zamienia się na B tak:

```
B = 16 · bajt + 64          (64 to minimalna jasność, nawet przy bajcie 0)
```

Wartość pola leży w jego **lewym górnym narożniku**, czyli w punkcie (bx, by). B w dowolnym punkcie (x, y) to interpolacja dwuliniowa z czterech pól:

```
x0 = floor(x), y0 = floor(y), tx = x − x0, ty = y − y0
b  = (1−tx)(1−ty)·pole[x0, y0] + tx(1−ty)·pole[x0+1, y0] + (1−tx)ty·pole[x0, y0+1] + tx·ty·pole[x0+1, y0+1]
B  = 16 · b + 64
```

- **Teren**: (x, y) to pozycja wierzchołka (bx + p.x, by + p.y).
- **Modele**: (x, y) to pozycja **obiektu** (jedna wartość B dla całego modelu).

## 7.3 Niebo: `set_light_begin` ✅

Scenariusze z powierzchnią wołają `set_light_begin N` (OBERWELT: 33). Oznacza to, że **powyżej wiersza N jest niebo**. Bez tej komendy cały świat jest „pod ziemią” (domyślnie N = −10 000).

Po złożeniu świata pole (bx, by) z wierszem by < N dostaje bajt **20** (pełny dzień), jeśli w jego kolumnie, w nim samym albo w polach do **6 wyżej**, jest otwarta przestrzeń:

```
dla każdego pola (bx, by), by < N:
    dla k = 0..6:
        jeśli by − k < 0:                          skan wyszedł ponad świat: niebo
            pole[bx, by] = 20; przerwij
        jeśli otwarte(świat.blok[bx, by − k]):
            pole[bx, by] = 20; przerwij

otwarte(blok) = któraś z 64 wysokości rekordu BlockDB jest < 13   (wnęka, powietrze; blok 59 to czyste powietrze)
```

Dzięki skanowi w dzień świeci nie tylko powietrze, ale też powierzchnia gruntu i pas około 6 bloków pod nią. Pola niżej zachowują wartość z szablonu.

🔶 Gra robi to dokładniej: skanuje drobne pola (8 na blok) kolumna po kolumnie, a pod ziemią liczy ciemnienie w kątach tuneli z różnic wypełnienia sąsiednich drobnych pól. Wartości z szablonu są dobrym przybliżeniem tego drugiego.

✅ Obserwacja: ten sam kadr OBERWELT z `set_light_begin -10000` jest w grze około 4 razy ciemniejszy (i nie ma nieba, rozdział 8).

## 7.4 Lampy ✅

Lampy to obiekty klas z komendą:

```tcl
class_light {ox oy oz} R {r g b}
```

- pozycja lampy = pozycja obiektu + (ox, oy, oz), a potem głębokość przelicza się jak dla obiektów: `z = (z_obiektu + oz) · 0,5 + 7`;
- R to promień w blokach (zasięg i szerokość spadku);
- kolor c = `int(kolor · 31) · 0,5` dla każdego kanału, np. pochodnia {0,9 0,9 0,8} daje c = (13,5; 13,5; 12).

Klasa może mieć kilka `class_light`. Typowe źródła: niewidoczne w grze `Misc_LightSrc_*` w szablonach, ogniska, pochodnie. Gra składa listę lamp dla każdego pola z emiterów w pobliżu. Nasza przeglądarka bierze do 32 lamp najbliższych środkowi kadru (🔶 uproszczenie).

**Pochodnie krasnali** (❓ nie wdrożone): krasnale z flagą autolight co jakiś czas stawiają na ścianach obiekty `Misc_Light` (pochodnie), więc przebyte tunele są oświetlone. Opis zachowania: [Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) `Docs/TUNNEL_TORCH_LIGHTING_RE.md` §1–5.

## 7.5 Modele: oświetlenie dwustronne 🔶

Część modeli ma normalne skierowane do środka (np. drzewa `Obw_baum_*` na powierzchni). Liczone wprost wychodzą ciemne, a w grze są jasne. Namioty z normalnymi na zewnątrz są jasne w obu wariantach. Zgodny z grą obraz daje reguła:

```
oblicz (rozproszone, dodatek) dla N i dla −N
weź wariant o większej sumie rozproszone + dodatek
```

Stosujemy ją tylko do modeli, nie do terenu. ✅ Sprawdzone na zrzutach OBERWELT i menu.

## 7.6 Przykłady liczbowe

| sytuacja | B | słońce | lampy | I | wynik |
|---|---|---|---|---|---|
| tunel (bajt 4), ściana na wprost (N = (0,0,1)) | 128 | 3,6 | brak | 46 | 18% jasności tekstury: ciemno |
| to samo pod niebem (bajt 20) | 384 | 3,6 | brak | 138 | 54% |
| dach pod niebem, N = (0,−1,0) | 384 | 6,146 | brak | 236 | 93% |
| tunel, pochodnia c = 13,5 tuż obok (exp ≈ 1, N·L = 1) | 128 | 3,6 | 255·13,5 | 46 + 344 = 390 | 100% + dodatek 64/255 |

Pod ziemią bez lamp jest więc ciemno, jak w grze, a jedna pochodnia wystarcza do pełnego oświetlenia swojego otoczenia (ok. 3 bloków).
