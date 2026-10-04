# 10. Czego jeszcze nie wiemy

Lista luk, które dzielą przewodnik od pełnego silnika. Przy każdej: co wiadomo i gdzie szukać. Bieżący plan prac jest w [TODO.md](../../TODO.md).

## 10.1 Obraz

| Temat | Stan | Co wiadomo |
|---|---|---|
| Teksturowanie terenu współrzędnymi u, v palety BlockDB | ❓ | paleta ma u, v w zakresie ok. 0…7,5; reguła nieznana (zamiennik w 4.2) |
| Przejścia gruntów maskami `Msk_*` | ❓ | 14 masek 64 × 64: wszystkie układy liter A/B w narożnikach TL TR BL BR poza AAAA i BBBB; biel = grunt A, czerń = grunt B, poszarpana granica. Która para gruntów i jak gra wybiera narożniki: do ustalenia zrzutami (szablony testowe `ow_m1..3` gotowe w laboratorium). Diggles-Unity też tego nie rozpracowało (ich kod zgaduje) |
| Wybór wariantu tekstury gruntu 01–15 i detalu `_a`…`_f` | ❓ | |
| Bit 0x80 typu pola, młodszy bajt `.l2m` | ❓ | bez widocznej różnicy na zrzutach |
| Ciemnienie w kątach tuneli (światło bazowe pod ziemią) | 🔶 | gra liczy je z wypełnienia drobnych pól ([Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) `TUNNEL_TORCH_LIGHTING_RE.md` §6); szablony mają przybliżone wartości |
| Mgła wojny: sprawdzenie zrzutami | 🔶 | prawo opisane w [rozdziale 10](10_mgla_wojny.md) za Diggles-Unity; brakuje porównania z grą |
| **Cienie** pod obiektami | ❓ | `.3db` ma mapy cieni 32 × 32 przypisane do klatek; tylko w zasięgu `MaxShadowLightDist` = 8 od lampy; sposób rzutowania nieznany (Diggles-Unity też go nie zna); do ustalenia zrzutami z lampą |
| Winieta (przyciemnienie brzegów ekranu w menu) | ❓ | widoczna na zrzutach |
| Linia przycięcia panoramy, położenie panoramy przy kamerze startowej OBERWELT | 🔶 | rozdział 8 |
| Prędkość animacji i interpolacja; pola `move`/`rotate` animacji | 🔶 | 10 kl./s według Diggles-Unity; sprzeczne dane o interpolacji ([11.6](11_obiekty_tcl_i_petla.md)); `move` to najpewniej przesunięcie korzenia na cykl (prędkość chodu) |
| Cząsteczki, woda, animacje tekstur (`txtanim`), materiały `shader.tcl` | ❓ | |

## 10.2 Świat i rozgrywka

| Temat | Stan | Gdzie szukać |
|---|---|---|
| Generator świata kampanii i trybu swobodnego (`lg_*`, `StoryMgr load_level`) | ❓ | `Scripts/init/LGINIT.TCL`, Diggles-Unity `WORLDGEN_ORIGINAL_RE.md`, `LEVEL_GENERATOR.md` |
| Maska ruchu w `.pmp` (przebiegi u16) | 🔶 | płaszczyzna nawigacji; Diggles-Unity `ORIGINAL_PATHFINDING_RE.md` |
| Model obiektów Tcl: szczegóły | 🔶 | opisany w [rozdziale 11](11_obiekty_tcl_i_petla.md) za Diggles-Unity; do sprawdzenia: semantyka licznika stanów, dlaczego timer klasy z `userstartup.tcl` nie zadziałał |
| Kopanie, produkcja, walka, AI, interfejs, dźwięk | ❓ | skrypty w `Data/Scripts`; Diggles-Unity ma dokumenty do każdego tematu (indeks: [BAZA_WIEDZY.md](../BAZA_WIEDZY.md) §10) |
| Zapisy `.sav` | ❓ | gzip, ok. 7 MB binarki zaczynającej się od `V1.0.844` |
| Fonty `.kft`, `ALPHABET.BIN`, `GUI/*.bin` | ❓ | |

## 10.3 Jak badać

Nie analizujemy kodu exe. Zachowanie ustala się obserwacją oryginału: scena w `data/userstartup.tcl`, zrzut ekranu przy zmienianym jednym parametrze, porównanie z własnym renderem. Narzędzia i wnioski z dotychczasowych eksperymentów: [BAZA_WIEDZY.md](../BAZA_WIEDZY.md) §8–9.
