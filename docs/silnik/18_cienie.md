# 18. Cienie obiektów

Gra nie liczy cieni na żywo. Każdy model `.3db` ma do dwóch gotowych obrazów cienia 32 × 32 (skala szarości) i dla każdej klatki prostokąt `shadowQuad` na ziemi, na którym obraz leży ([3db.md §3.6](../formaty/3db.md)). Silnik kładzie ten obraz na podłodze pod obiektem, gdy w pobliżu stoi lampa, pochyla go w stronę od lampy i przyciemnia nim podłogę.

**Źródło i status.** Diggles-Unity opisuje tylko ogólnie, że cienie to „mapy 32×32 tylko w zasięgu `MaxShadowLightDist` = 8 od światła” (`Docs/RENDERING_LIGHTING_RESEARCH.md`, [Zimennik/Diggles-Unity](https://github.com/Zimennik/Diggles-Unity)). Prawo rzutu ustaliliśmy pomiarem w działającej grze (krasnal i lampa w kilkunastu położeniach, [BAZA_WIEDZY §9.11](../BAZA_WIEDZY.md)), więc całość to 🔶: stałe są dopasowane do zrzutów, a nie odczytane z gry.

## 18.1 Co przedstawia obraz cienia

Obraz to rozmyty rzut bryły na ziemię światłem padającym pod 45° od tyłu ([BAZA_WIEDZY §9.7](../BAZA_WIEDZY.md)). Kolumna obrazu rośnie z x (od x0 do x1 prostokąta), wiersz rośnie z głębokością z (od z0 z tyłu do z1 przy widzu). Wiersze daleko przed obiektem odpowiadają więc wysokim częściom bryły. Przykład `MANN.3DB`, animacja `standard`: prostokąt x od −82 do 95, z od −46 do 275 jednostek (200 jednostek = 1 blok); obraz kończy się około 1,15 bloku przed punktem obiektu.

## 18.2 Które lampy dają cień

- Lampa to każde `class_light` obiektu (lampy `Misc_LightSrc_*`, świecące grzyby itd.).
- ✅ Zasięg `Performance\MaxShadowLightDist` (rejestr, domyślnie 8) mierzony **w płaszczyźnie x/y**: lampa 9 bloków obok nie daje cienia, 7 bloków obok daje. Lampa 6 bloków w bok i 6 w głąb (3D 8,5) cień daje, a z nie zmienia ani długości, ani kierunku.
- 🔶 Najwyżej dwa cienie na obiekt: krasnal stojący przy dwóch lampach z zapisu nie dostał cienia od trzeciej, dostawionej bliżej. Którą parę gra wybiera (najbliższą, pierwszą na liście), nie wiadomo; u nas dwie najbliższe.
- Przy `MaxShadowLightDist` = 0 cieni nie ma wcale. Na powierzchni (słońce, bez lamp) też ich nie widać ([BAZA_WIEDZY §9.8](../BAZA_WIEDZY.md)).

## 18.3 Pochylenie od lampy

Obraz nie jest obracany, tylko **pochylany wzdłuż x**: każdy jego wiersz w głębi z przesuwa się w bok, od lampy, proporcjonalnie do odległości od tylnej krawędzi prostokąta. Głębokość z zostaje, więc koniec smugi leży bliżej widza niż stopy obiektu (na zrzutach smuga opada lekko w dół ekranu).

```
d   = √((x_obj − x_lampy)² + (y_obj − y_lampy)²)      // bloki, bez z
kier = znak(x_obj − x_lampy)                           // cień w stronę od lampy
σ   = max(0,7; 0,285 · d)                              // 🔶 dopasowane do zrzutów
dla narożnika prostokąta (x, z):  x' = x + kier · σ · (z − z0),  z' = z
```

Przykład: lampa 4 bloki w prawo i 1 wyżej (d = 4,12): σ = 1,17, więc wiersz obrazu 1,15 bloku przed obiektem (1,38 od tylnej krawędzi) przesuwa się o 1,6 bloku w lewo. Zmierzony koniec smugi w grze to 1,26 bloku od środka krasnala, a u nas 1,18.

Ciekawostka (sprzeczna z fizyką): **wyższa lampa daje dłuższy cień**, bo liczy się odległość w x/y, a nie kąt padania. Przy 2 blokach w bok i wysokości 0,5 / 1 / 2 / 3 bloki smuga ma 0,75 / 0,77 / 0,99 / 1,10 bloku.

🔶 Lampa dokładnie nad obiektem (x lampy = x obiektu): w grze smuga idzie w obie strony naraz (kierunek nieokreślony). U nas cień zostaje wtedy pod obiektem bez pochylenia (|dx| < 0,05 bloku). Inaczej niż w oryginale, ale ten przypadek trudno odtworzyć dokładnie.

## 18.4 Przyciemnienie

Cień **mnoży barwę podłogi**: `podłoga · (1 − 0,4 · v/255)`, gdzie v to wartość obrazu cienia. W rdzeniu (v ≈ 205) daje to około 0,68, a w grze najciemniejsze miejsca mają 0,68–0,74. Mnożnik nie zależy od odległości lampy ani od innych świateł: przy trzech lampach naraz rdzeń ma 0,77–0,80, tak jak przy jednej. Lampa dostawiona w zapisie kampanii nie rozjaśniła podłogi pod krasnalem (zrzuty bez krasnala z lampą i bez niej są identyczne), więc cień nie odejmuje „swojego” światła, tylko przyciemnia całość.

## 18.5 Implementacja u nas

[`render/shadow_renderer`](../../src/lib/render/shadow_renderer.h) ([.cpp](../../src/lib/render/shadow_renderer.cpp)), wywołany z `MapScene::render` po terenie, a przed modelami (obiekt zasłania wtedy swój cień testem głębi):

```
dla każdego widocznego obiektu z klatką mającą shadowIndex:
    lampy = lampy gry z d ≤ 8, posortowane po d, najwyżej 2
    narożniki = macierz obiektu · (x, −2, z)       // 2 jednostki nad podłogą
    dla każdej lampy: pochyl narożniki (18.3), dorysuj prostokąt z obrazem cienia
mieszanie: cel · źródło · 2, shader zwraca (1 − 0,4·v)/2; test głębi bez zapisu
```

Shadery [`shadow.vert`](../../src/shaders/shadow.vert) i [`shadow.frag`](../../src/shaders/shadow.frag). Lampa podglądu przeglądarki (szablony bez świateł) cieni nie daje, bo w grze jej nie ma.

**Różnice wobec gry (🔶):**
- nasza smuga jest węższa: w grze ma do 2 razy więcej pikseli, ale płytszych (śr. mnożnik 0,82 wobec naszych 0,76). Gra prawdopodobnie mocniej rozmywa obraz albo rozciąga go także w poprzek;
- przy lampie z lewej nasz cień jest o 0,17 bloku krótszy niż w grze;
- obróconych obiektów nie mierzyliśmy: prostokąt obracamy razem z obiektem, a pochylenie liczymy w osiach świata;
- lampy z obiektów tworzonych w trakcie gry jeszcze nie trafiają do sceny (światła zbieramy przy wczytaniu), więc nie dają ani światła, ani cienia.

**Sprawdzenie:** test `shadows` w [`tools/tests/run.sh`](../../tools/tests/run.sh) (krasnal w (249,5; 42,5; 13) zapisu kampanii, lampa 4 bloki w prawo: koniec smugi i mnożnik jak w grze; lampa 9 bloków obok: brak cienia). Ręcznie: `wigview --save save_kampania.sav --fow 0 --view 249.5 42 1.0 -0.2 0.2 --place Misc_LightSrc_a 253.5 41.5 13 --place Zwerg 249.5 42.5 13`.
