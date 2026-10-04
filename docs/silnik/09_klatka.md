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
6. teren, detale: te same trójkąty, głębia LEQUAL bez zapisu, mieszanie DST_COLOR·SRC_COLOR (×2) (4.4)
7. modele: pomiń obiekty ukryte przez mgłę wojny (10.5);
     dla pozostałych klatka animacji z interpolacją (5.4),
     test i zapis głębi, bez cullingu, alpha test ≥ 0,5,
     oświetlenie dwustronne (7.5), zanikanie w kolor strefy dla z < 0 (8.1)
   (teren i modele: po oświetleniu szara mgła i czerń z rozdziału 10.5)
8. interfejs
```

## 9.3 Stany renderu w tabeli

| Przebieg | Test głębi | Zapis głębi | Culling | Mieszanie | Alpha test | Oświetlenie |
|---|---|---|---|---|---|---|
| panorama 0–5 | nie | nie | nie | SRC_ALPHA, 1−SRC_ALPHA | nie | nie |
| dywan łąki | nie | nie | nie | SRC_ALPHA, 1−SRC_ALPHA | nie | nie |
| teren podkład | LESS | tak | nie | nie | alfa otworu > 126/255 | tak (7.1) + mgła wojny (10.5) |
| teren detale | LEQUAL | nie | nie | DST_COLOR, SRC_COLOR | jak podkład | (dziedziczy z podkładu) |
| modele | LESS | tak (poza `nozwrite`) | nie | nie | alfa tekstury ≥ 0,5 | tak, dwustronnie (7.5) + mgła wojny (10.5) |

## 9.4 Weryfikacja

Najlepszy test to porównanie ze zrzutem z oryginału w tej samej rozdzielczości i z tym samym `set_view`. Zrzuty robi `tools/lab/capture.sh` (gra pod Wine w niewidocznym serwerze X, opis w [BAZA_WIEDZY.md](../BAZA_WIEDZY.md) §8). Sceny wzorcowe:

| Scena | Co sprawdza |
|---|---|
| menu (`systeminit.tcl`) | otwory do tła, strefa horyzontu, zanikanie modeli tła, lampy w jaskini, detale `.l2m` |
| `OBERWELT.TCL` z `set_view 131.78 23.95 2.5 0 0` | panorama, światło nieba, oświetlenie dwustronne |
| `OBERWELT.TCL` z kamerą startową | przycinanie kątów kamery |
| korytarz `urw_gng_021_a` z różnymi typami pól | rodzaje gruntu |
