# 12. Efekty cząsteczkowe

Ogień w kielichach i lampach, mgiełka przy kryształach, iskry w kuźni, wióry u stolarza, „Z” nad śpiącym krasnalem: to wszystko są cząsteczki. Każda to mały kwadrat zwrócony do kamery z obrazkiem z atlasu tekstur, dodawany do obrazu (mieszanie addytywne). Ten rozdział opisuje, skąd biorą się źródła cząsteczek, czym różnią się typy i jak liczyć ich ruch.

**Źródło i status.** Lista typów, atlasy i model ruchu pochodzą z [Zimennik/Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) (`Docs/PARTICLE_SYSTEM_REWRITE.md` §1, §4; `Runtime/Engine/Particles/ParticleTypeTable.cs`, `CpuParticleEmitter.cs`). Tamci autorzy część parametrów przepisali z gry, a część dobrali na oko, więc całość jest 🔶. ✅ Składnię komend Tcl i parametry źródeł sprawdziliśmy w skryptach gry. ✅ Kształt, wielkość i kolor płomieni typu 2 porównaliśmy ze zrzutem gry (§12.6).

## 12.1 Źródła: komendy Tcl

Źródło cząsteczek jest przypięte do obiektu. Klasa może je zadeklarować w `def_class`, a skrypt może je zmienić w trakcie gry:

```tcl
class_particle         <typ> {rx ry rz} {dx dy dz} <max> <naKlatkę> <widoczneWMgle> <?> <?> [<?>]
change_particlesource  <obiekt> <slot> <typ> {rx ry rz} {dx dy dz} <max> <naKlatkę> <widoczneWMgle> [...]
set_particlesource     <obiekt> <slot> 0|1         ;# wyłącz / włącz
create_particlesource  <typ> <pozycja> {dx dy dz} <ile> ...   ;# jednorazowy wybuch w świecie
blow_particlesource    ...                          ;# podmuch (przesuwa emiter)
free_particlesource    ...
```

| Pole | Znaczenie |
|---|---|
| `typ` | numer typu 0..42 (§12.3) |
| `{rx ry rz}` | położenie emitera względem obiektu: bloki, **y w dół**, z w skali skryptu (jak przy `class_light`: z świata = (z obiektu + rz)·0,5 + 7) |
| `{dx dy dz}` | prędkość początkowa cząsteczki w blokach na klatkę (ujemne dy = w górę) |
| `max` | najwięcej żywych cząsteczek |
| `naKlatkę` | ile nowych cząsteczek na klatkę |
| `widoczneWMgle` | ❓ |
| pozostałe | ❓ (Diggles-Unity: punkt modelu, zasięg, wierzchołki modelu, część siatki) |

Przykłady z danych gry (`Scripts/classes/deco/`):

```tcl
def_class Dummy_Feuerkelch_b ... {           ;# kielich ognia
	class_light {0 -0.5 0.7} 2 {0.7 0.6 0.4}
	class_particle 2 {0 -0.2 0.6} {0 0 0} 64 2 0 0 0
}
def_class Dummy_Haengelicht ... {            ;# lampa wisząca na łańcuchach
	class_particle 3 {0 1.6 0} {0 0 0} 128 4 0 0 0     ;# płomień 1,6 bloku POD punktem zawieszenia
}
class_particle 21 {-0.05 -0.75 0.4} {0 -0.005 0} 60 1 0 0 0.5   ;# kryształ: mgiełka unosząca się
```

Skrypty gry używają 29 klas dekoracji z `class_particle`. Pełną listę użyć każdego typu pokazuje zakładka Cząsteczki w wigview.

## 12.2 Atlasy

Sześć tekstur w `Data/Texture/Misc`, w kolejności numerów:

| nr | plik | rozmiar | zawartość |
|---|---|---|---|
| 0 | `particle_a_128d.tga` | 128² | drobne kule w kolorach (wiersze: żółty, pomarańcz, czerwony, fiolet, niebieski…) |
| 1 | `particle_b_128d.tga` | 128² | wióry, krople, litery (Z, nuta) |
| 2 | `particle_c_256d.tga` | 256² | błyski, kłęby dymu |
| 3 | `particle_e_256.tga` | 256² | iskry, płomienie |
| 4 | `particle_d_256d.tga` | 256² | gwiazdki, płatki, smuga |
| 5 | `particle_a_256d.tga` | 256² | obłoki „puf”, pierścienie energii |

Typ dzieli swój atlas na siatkę `grid × grid` (4, 8 albo 16) i bierze z niej komórkę. Komórka ma numer `v·grid + u` (u w prawo, v w dół od góry obrazka). Tekstury mają kanał alfa, a kolor jest już przemnożony przez alfę (ciemny tam, gdzie przezroczysto).

## 12.3 Typy

Każdy typ to zestaw stałych: atlas i siatka, jasność (alfa materiału 70..255), kształt, w którym rodzą się cząsteczki, rozrzut prędkości, sposób wyboru komórki, kolor, rozmiar i jego zmiana, grawitacja lub wypór oraz czas życia w klatkach. W grze każdy typ ma własną funkcję. My liczymy wszystkie jednym modelem (§12.4), a różnice zapisujemy w tabeli ([`src/lib/particles.cpp`](../../src/lib/particles.cpp), `buildTypes`).

| typ | efekt | atlas, siatka | życie |
|---|---|---|---|
| 0 | żar kowala (kula ognia) | 3, 4 | 32 |
| 1 | iskry i bryzgi przy ziemi | 5, 8 | 100 |
| 2 | ciepły żar (kielichy ognia) | 0, 16 | 32 |
| 3 | ogień kowala, lampy wiszące | 0, 16 | 32 |
| 4 | litera Z nad śpiącym | 1, 8 | 64 |
| 5 | nieużywany (gra nie ma dla niego funkcji) | | |
| 6 | dym z wierzchołków modelu | 0, 16 | 32 |
| 7 | spadające krople (kryształy) | 4, 4 | 32 |
| 8, 10, 15 | krople spadające na ziemię | 1, 16 | 95 |
| 9, 27, 34 | małe kule ognia | | 32..111 |
| 11, 12, 35 | dym | 0, 16 | 40..64 |
| 13, 33 | magiczne iskierki | 2, 4 | 48 |
| 14 | wybuch promienisty | 2, 4 | 64 |
| 16 | ciepły stożek w górę | 2, 4 | 47 |
| 17, 26 | wióry (stolarz, kamieniarz) | 1, 8 | 100 |
| 18 | iskry w kuźni | 1, 8 | 32 |
| 19, 20 | pył, chmura ziemi | 3 / 2, 4 | 128 / 32 |
| 21 | mgiełka i bryzgi (kryształy) | 4, 8 | 32 |
| 22..24 | płatki, bąbelki | 4 | 32 |
| 25 | promień / lina | 1, 8 | 8 |
| 28..30 | fioletowy i szary dym | 3, 4 | 32 |
| 31 | nuta | 1, 8 | 64 |
| 32, 41 | błyski | 2, 4 | 12..16 |
| 36..40 | obłoki „puf”, pierścienie energii (animacja 8–9 klatek) | 5, 8 | 9..10 |
| 42 | rosnąca kula pyłu | 3, 4 | 63 |

## 12.4 Symulacja (wspólny model)

Liczymy w blokach, w układzie z **osią y w górę** (odwrotnie niż w grze). Przy konfiguracji źródła przeliczamy: `rel = (rx, −ry, rz·0,5)`, `dir = (dx, −dy, dz·0,5)`. Emiter stoi w `obiekt + rel` (z świata obiektu = z·0,5 + 7). Losowość: generator gry `x = x·0x343FD + 0x269EC3`, próbki z bitów 16+ (`sym(n) = ((x>>16 & 15) − 8)/n`).

Co klatkę symulacji:

```
A) błądzenie emitera: drift = (drift + sym(800)·[wander]) · 0,9
B) nowe cząsteczki: n = naKlatkę (Jitter: ±1; Single: 1; Gated: n tylko z szansą gate)
   dla każdej: pozycja = emiter + przesunięcie wg kształtu
                 Disc/Cone: kąt losowy, promień ≤ R;  Box: sym(8)·półwymiar;  Sphere: kierunek losowy, ≤ R
               prędkość = dir + sym(800)·rozrzut·100
               komórka atlasu: stała / losowa z pasma (tylko niepuste komórki) / pierwsza klatka animacji
               rozmiar = rozmiar typu · (1 ± sizeJitter)
C) ruch żywych: pos += vel
                vel.y += grawitacja·2,5;  przez pierwsze buoyancyFrames klatek vel.y += 0,003
                vel ·= drag;  vel += drift·0,03 [wander];  x += cos(wiek·0,25)·0,03 [sway]
                typy „ziemne”: pod podłożem odbicie vel.y = −0,3·vel.y, vel.xz ·= 0,5
                (podłoże: pierwsza lita próbka mapy wysokości pod emiterem, 🔶; bez terenu 3 bloki niżej)
   wiek t = wiek/życie:  rozmiar · (Grow 0,5+0,8t | Shrink 1−0,9t | Pulse)
                         kolor: biały / stały / szary (narasta 20%, gaśnie) / ciepły (biały, alfa 1−t²)
                         animacja: klatka = wiek, a dla ognia (flipSquare) klatka = floor(t²·liczba klatek)
   wiek ≥ życie: cząsteczka znika
```

Rysowanie: kwadrat o boku `rozmiar · 0,12` bloku, zwrócony do kamery, kolor `tekstura.rgb · tekstura.a · kolor.rgb · kolor.a · alfaTypu`, mieszanie `cel + źródło`, test głębi bez zapisu. Kwadraty budujemy na CPU: kierunki „w prawo” i „w górę” ekranu to znormalizowane wiersze 0 i 1 macierzy rzutowania (dla rzutu perspektywicznego wiersz 0 = P₀₀·oś x kamery).

**Tempo** ❓: gra liczy cząsteczki co klatkę obrazu. Diggles-Unity przyjmuje 12 klatek symulacji na sekundę i my też.

**Nieodkryty teren** (rozdział 10.5): cząsteczki obiektu rysujemy tylko na polu z bajtem mgły ≥ 0x81.

## 12.5 Czego jeszcze brakuje

- 🔶 Typy liczone wspólnym modelem zamiast 42 osobnych funkcji gry. Diggles-Unity ma przepisane z gry funkcje części typów (0–3, 6, 8–23, 26, 28–30, 32, 36–40): warto porównać każdy typ ze zrzutem gry (§12.6).
- 🔶 Podłoże cząsteczek „ziemnych” (1, 8, 10, 15, 17, 18, 26): bierzemy pierwszą litą próbkę mapy wysokości (wartość 15) pod emiterem w tej samej kolumnie (`MapScene::floorDrop`); wióry 17 i 26 oraz plama 8 leżą wtedy na podłodze jak w grze. ❓ Jak dokładnie robi to gra.
- ❓ Typ 6 i inne „z wierzchołków modelu”: na razie kula 0,3 bloku.
- ❓ Lina (25), obrót cząsteczek, smugi wzdłuż prędkości (7, 42).
- ❓ Znaczenie pól `widoczneWMgle` i ostatnich argumentów `class_particle`, tryb 0x0f/0x10.
- Płomienie typu 3 przy lampach wiszących (`Dummy_Haengelicht`) nie są u nas widoczne na zrzucie porównawczym (§12.6); do sprawdzenia przesunięcie {0 1,6 0} przy obróconym obiekcie.

## 12.6 Porównanie ze zrzutem gry

**Wszystkie typy** ([BAZA_WIEDZY §9.10](../BAZA_WIEDZY.md)): w działającej grze każdy typ 0–42 podpięty do krasnala (`change_particlesource $G 1 <typ> {0 0 0} {0 0 0} 64 2 0 10`). Poza płomieniami (2, 27) i typami 24, 25 nasze typy są za małe albo niewidoczne; obłoki i wybuchy 33–42 są w grze wielokrotnie większe od płomienia (39, 40, 42 na cały kadr przy `zoom` 0,8), u nas porównywalne z nim. Typy 7, 8, 30–32, 37–41 u nas nic nie pokazują. Pierwsza kalibracja rozmiaru i emisji w tej samej scenie (`wigview --emit`, [BAZA_WIEDZY §9.10](../BAZA_WIEDZY.md)) zbliżyła zasięg i jasność typów 7, 12–14, 17–19, 22, 24, 26, 28–30, 32–42 do gry (stosunek jasności 0,8–2,0 zamiast do 3000). 🔶 Obłoki i wybuchy (36–40) i błyski 41 gra przy źródle ciągłym emituje w każdej klatce, a nie z losową bramką Diggles-Unity. Typy „ziemne” 1, 8, 15, 17, 26 po dodaniu podłoża z terenu i powiększeniu leżą na podłodze z jasnością gry co do czynnika 3 (10 w grze jest na poziomie szumu). Typy 0, 3, 6 poprawione kształtem (zwarta kolumna, szerszy ogień, szary dym). Do zrobienia: barwy (8 bardziej różowa w grze, 12, 42).

**Płomienie** (pierwsza kalibracja):

Szablon `kris_unq_060` (dwa kielichy ognia `Dummy_Feuerkelch_b`, typ 2, i dwie lampy `Dummy_Haengelicht`, typ 3), kamera `set_view 24,5 21 1,0 0 0` (szablon w bloku 16,16). Zrzut gry: `../lab/zrzuty/czasteczki/kris_unq_060_gra.png` (poza repozytorium), nasz obraz: `wigview --template kris_unq_060 --view 8.5 5 1.0 0 0`.

W grze płomień kielicha ma ok. 0,27 bloku szerokości i 0,6 bloku wysokości, jest żółty z jasnym środkiem, zwarty. Parametry Diggles-Unity dawały płomień ok. 0,9 bloku szerokości, pomarańczowo-czerwony i rozproszony. Po trzech poprawkach obraz jest zgodny:
1. skala rozmiaru 0,25 → **0,12**;
2. kolor ognia daje tekstura (wiersze atlasu przechodzą z żółtego w czerwony), więc kolor cząsteczki zostaje biały, a gaśnie tylko alfa (1 − t²). Klatki animacji rozłożone na całe życie wg t², a nie jedna na klatkę;
3. rozrzut prędkości 0,02 → 0,004–0,008 i promień kuli 0,18 → 0,06 (typ 2) / 0,25 → 0,12 (typ 3).

Przy okazji widać, że gra rysuje w tym szablonie wodę (rozdział o płynach: do napisania).

---
**Diggles-Unity** (ich opracowanie, 🔶 do sprawdzenia w laboratorium): [PARTICLE_SYSTEM_REWRITE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/PARTICLE_SYSTEM_REWRITE.md). Porównanie i stan: [mapa Diggles-Unity](../DIGGLES_UNITY.md).

**W kodzie OpenWiggles** (pomocniczo, zasada działania jest opisana wyżej): [`lib/particles.h`](../../src/lib/particles.h), [`lib/render/particle_renderer.h`](../../src/lib/render/particle_renderer.h), [`apps/wigview/particle_view.h`](../../src/apps/wigview/particle_view.h). Wszystkie moduły: [mapa kodu](../KOD.md).
