# 9. Klatka obrazu

Ten rozdział zbiera w jednym miejscu kolejność rysowania i stany renderu. Wszystkie prawa są opisane w poprzednich rozdziałach; tu jest lista kontrolna.

## 9.1 Raz po wczytaniu mapy

1. Złóż świat z szablonów (rozdział 3) i przelicz światło nieba (7.3).
2. Zbuduj siatkę terenu: wierzchołki ze wszystkich bloków (4.1) z atrybutami z 4.6. Kolor oświetlenia liczony per wierzchołek zależy od lamp, więc liczy się go co klatkę (albo gdy lampy się zmienią).
3. Pogrupuj trójkąty terenu według tekstur podkładu i osobno według tekstur detali (4.4).
4. Dla obiektów: klasa → model i animacja (5.2), macierz (5.3), jasność bazowa B w punkcie obiektu (7.2), lista lamp z klas (7.4), lista stref horyzontu (8.1).

## 9.2 W każdej klatce

```
1. kamera: set_view → oko, cel, góra, przycięcie kątów (6.1, 6.3); macierze widoku i projekcji
2. strefa horyzontu: najbliższa w zasięgu od ogniska (8.1)
3. wyczyść ekran kolorem strefy (bez strefy: czarnym) i bufor głębi
4. panorama, jeśli jest niebo (8.4):
     warstwy 0..5: siatka ekranowa, bez głębi, mieszanie alfa (8.2)
     dywan łąki: geometria świata, bez głębi, mieszanie alfa (8.3)
5. teren, podkład: test i zapis głębi, bez cullingu, alpha test > 126 (otwory), bez mieszania (4)
6. teren, detale: te same trójkąty, głębia LEQUAL bez zapisu, mieszanie DST_COLOR·SRC_COLOR (×2) (4.4);
     detale lawy (alfa = maska żyłek): mieszanie alfa, kolor bez oświetlenia (🔶 4.4)
7. modele, faza nieprzezroczysta: pomiń obiekty ukryte przez nieodkryty teren (10.5);
     dla pozostałych klatka animacji z interpolacją (5.4),
     tryb rysowania z flags linku (5.5): pomiń tryby 0, 2, 6, 7, tryb 3 tylko alfa ≥ 0,5,
     test i zapis głębi, bez cullingu, alpha test ≥ 0,5 (1, 3, 4; tryby 8, 9, 11 bez),
     oświetlenie dwustronne (7.5), zanikanie w kolor strefy dla z < 0 (8.1)
   (teren i modele: po oświetleniu szara mgła i czerń z rozdziału 10.5)
8. modele, faza przezroczysta (🔶 nasz podział): obiekty od najdalszego (najmniejsze z),
     miękkie brzegi trybu 3 (alfa < 0,5) i całe linki 6, 7 mieszaniem alfa, blask trybu 2 addytywnie,
     bez zapisu głębi
9. płyny (13), potem cząsteczki: addytywnie, test głębi bez zapisu (12)
10. ramka ekranu (9.4), gdy włączona w ustawieniach
11. interfejs (🔶 kolejność 10 i 11 nie jest sprawdzona)
```

## 9.3 Stany renderu w tabeli

| Przebieg | Test głębi | Zapis głębi | Culling | Mieszanie | Alpha test | Oświetlenie |
|---|---|---|---|---|---|---|
| panorama 0–5 | nie | nie | nie | SRC_ALPHA, 1−SRC_ALPHA | nie | nie |
| dywan łąki | nie | nie | nie | SRC_ALPHA, 1−SRC_ALPHA | nie | nie |
| teren podkład | LESS | tak | nie | nie | alfa otworu > 126/255 | tak (7.1) + nieodkryty teren (10.5) |
| teren detale | LEQUAL | nie | nie | DST_COLOR, SRC_COLOR; lawa: SRC_ALPHA/1−SRC_ALPHA (🔶) | jak podkład | (dziedziczy z podkładu); lawa bez oświetlenia |
| modele, faza nieprzezroczysta | LESS | tak (poza `nozwrite`) | nie | wg `flags` linku: 1 brak, 2 addytywne, 3 SRC_ALPHA/1−SRC_ALPHA ([5.5](05_obiekty.md)) | alfa tekstury ≥ 0,5 (tryby 1, 3, 4; nie 8, 9, 11) | tak, dwustronnie (7.5) + nieodkryty teren (10.5); 8: alfa ≥ 0,5 bez oświetlenia, 11 bez oświetlenia, 5 mapowanie otoczenia |
| modele, faza przezroczysta (🔶) | LESS | nie | nie | tryby 3, 6, 7: SRC_ALPHA/1−SRC_ALPHA, tryb 2: ONE/ONE | tryb 3: tylko alfa < 0,5 | jak wyżej; tryb 2 bez oświetlenia |
| cząsteczki | LESS | nie | nie | ONE/ONE (addytywne) | nie | nie |
| ramka ekranu | nie | nie | nie | SRC_ALPHA/1−SRC_ALPHA, kolor czarny | nie | nie |

## 9.4 Ramka ekranu (`BorderMask`) ✅

Wokół całego obrazu gra rysuje cienką czarną ramkę z zaokrąglonymi rogami. Daje ją wartość rejestru `Performance\BorderMask=1` (domyślnie włączona) i tekstura `Data/GUI/bordermask.tga`. Poza ramką obraz się nie zmienia: zrzuty z `BorderMask=1` i `0` są identyczne od 11. piksela od krawędzi w głąb. Ciemne brzegi widoczne na zrzutach z gry to więc treść sceny (nieoświetlona skała, rozdział 7), a nie winieta.

**Tekstura.** 128×128, kolor czarny, cała informacja w kanale alfa. Ćwiartki: lewa górna to róg (zaokrąglony gradient), prawa górna to pasek górnej krawędzi, lewa dolna to pasek lewej krawędzi (w obu: alfa 1 przy krawędzi, przejście do 0 między tekselem 4 a 30 od krawędzi), prawa dolna to gradient poziomy o nieustalonej roli (❓). Pozostałe krawędzie i rogi to odbicia tych kawałków.

**Prawo** (pomiar stosunku jasności „z ramką / bez ramki” na zrzutach 1024×768, [BAZA_WIEDZY §8.7](../BAZA_WIEDZY.md)): obraz = scena · (1 − α), czyli zwykłe mieszanie alfa czarnej tekstury. Przy krawędzi:

| piksel od krawędzi | 0–3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | ≥ 11 |
|---|---|---|---|---|---|---|---|---|---|
| przepuszczanie (1 − α) | 0 | 0,04 | 0,17 | 0,31 | 0,50 | 0,72 | 0,86 | 0,92 | 1 |

✅ Dopasowanie: **środek piksela `p` od krawędzi odpowiada tekselowi `3·(p + 0,5 − 2)` paska tekstury, próbkowanemu liniowo**, czyli 64-tekselowy pasek leży na 21⅓ piksela, zaczynając 2 piksele od krawędzi. Dopasowanie przesunięcia i skali (3,0 teksela na piksel, próbkowanie punktowe, liniowe albo uśrednienie dały podobny błąd) do tabeli wyżej: odchyłka ≤ 0,02 dla każdego p. Nasza ramka zmierzona tak samo jak gra (stosunek zrzutów z ramką i bez, 1024×768): 0,02; 0,06; 0,16; 0,32; 0,51; 0,70; 0,85; 0,94; 0,99 dla p = 3…11, na przekątnej 0,53 w pikselu 12 (gra: 0,52). Wcześniejsze przybliżenie `4·(p − 3)` dawało przejście za ostre (0,23 zamiast 0,31 w p = 6).

🔶 Szerokość nie zależy od rozdzielczości: przy `Screen\Width/Height` = 800×600 (gra rysuje wtedy obraz 800×600 w rogu pulpitu laboratorium) przejście też kończy się około 10.–11. piksela od krawędzi, a przy skalowaniu z ekranem (21⅓ piksela = 1/48 szerokości przy 1024) kończyłoby się koło 9. Pomiar zaszumiony (animowane menu, dwa osobne uruchomienia; stosunki 0,0–1,1), stąd 🔶. U nas ramka ma stałą szerokość w pikselach: [`render/border_mask`](../../src/lib/render/border_mask.h), shader [border_mask.frag](../../src/shaders/border_mask.frag); w grze domyślnie włączona (`--no-border-mask`), w wigview przełącznik „Ramka ekranu” w zakładkach map (`--border-mask`).

## 9.5 Weryfikacja

Najlepszy test to porównanie ze zrzutem z oryginału w tej samej rozdzielczości i z tym samym `set_view`. Zrzuty robi [`tools/lab/capture.sh`](../../tools/lab/capture.sh) (gra pod Wine w niewidocznym serwerze X, opis w [BAZA_WIEDZY.md](../BAZA_WIEDZY.md) §8). Sceny wzorcowe:

| Scena | Co sprawdza |
|---|---|
| menu (`systeminit.tcl`) | otwory do tła, strefa horyzontu, zanikanie modeli tła, lampy w jaskini, detale `.l2m` |
| `OBERWELT.TCL` z `set_view 131.78 23.95 2.5 0 0` | panorama, światło nieba, oświetlenie dwustronne |
| `OBERWELT.TCL` z kamerą startową | przycinanie kątów kamery |
| korytarz `urw_gng_021_a` z różnymi typami pól | rodzaje gruntu |

---
**Diggles-Unity** (ich opracowanie, 🔶 do sprawdzenia w laboratorium): [RENDERING_PIPELINE_COMPLETE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/RENDERING_PIPELINE_COMPLETE.md), [RENDERING_LIGHTING_RESEARCH](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/RENDERING_LIGHTING_RESEARCH.md). Porównanie i stan: [mapa Diggles-Unity](../DIGGLES_UNITY.md).

**W kodzie OpenWiggles** (pomocniczo, zasada działania jest opisana wyżej): [`lib/map_scene.cpp`](../../src/lib/map_scene.cpp) (`MapScene::render`), [`lib/render/rhi/rhi.h`](../../src/lib/render/rhi/rhi.h). Wszystkie moduły: [mapa kodu](../KOD.md).
