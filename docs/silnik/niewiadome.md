# 10. Czego jeszcze nie wiemy

Lista luk, które dzielą przewodnik od pełnego silnika. Przy każdej: co wiadomo i gdzie szukać. Bieżący plan prac jest w [TODO.md](../../TODO.md).

## 10.1 Obraz

| Temat | Stan | Co wiadomo |
|---|---|---|
| Wybór wariantu tekstury gruntu 01–15 i detalu `_a`…`_f` | ❓ | |
| Przejścia gruntów: który grunt jest „A” w masce, trzy grunty w bloku | 🔶 | prawo ogólne ustalone (4.2) |
| Detale `.l2m` zestawu 2 (`<świat>2_*`) | ❓ | zestaw 1 potwierdzony, zestaw 2 zachowuje się inaczej (tabela w 4.4); w menu mech na filarze leży w grze tylko na części powierzchni, u nas na całej |
| Bit 0x80 typu pola, młodszy bajt `.l2m` | ❓ | bez widocznej różnicy na zrzutach |
| Ciemnienie w kątach tuneli (światło bazowe pod ziemią) | 🔶 | gra liczy je z wypełnienia drobnych pól ([Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) `TUNNEL_TORCH_LIGHTING_RE.md` §6); szablony mają przybliżone wartości |
| Nieodkryty teren: sprawdzenie zrzutami | 🔶 | prawo opisane w [rozdziale 10](10_nieodkryty_teren.md) za Diggles-Unity; brakuje porównania z grą |
| **Cienie** pod obiektami | 🔶 | `.3db` ma mapy cieni 32 × 32 (rozmyty rzut bryły pod 45°, [3db.md §3.6](../formaty/3db.md)). W jaskini z lampą przy `MaxShadowLightDist` = 8 na podłodze są długie ciemne smugi od stworzeń, skierowane od lampy; przy 0 ich nie ma. Na powierzchni (samo słońce) cieni nie widać przy żadnej wartości ([BAZA_WIEDZY §9.8](../BAZA_WIEDZY.md)). ❓ Jak obraz jest rozciągany w stronę lampy, próg odległości, czy cieni jest kilka od kilku lamp |
| Linia przycięcia panoramy, położenie panoramy przy kamerze startowej OBERWELT | 🔶 | rozdział 8 |
| Prędkość animacji; pola `move`/`rotate` animacji | 🔶 | 10 kl./s według Diggles-Unity; `move`/`rotate` ✅ na klatkę ([3db.md](../formaty/3db.md) §3.5). Interpolacja: nie badamy, u nas zawsze włączona (decyzja, [11.6](11_obiekty_tcl_i_petla.md)) |
| Cząsteczki: dokładne prawa typów | 🔶 | wspólny model skalibrowany do zrzutów wszystkich 43 typów (rozmiar, emisja, podłoże; [rozdział 12](12_czasteczki.md) §12.5–12.6); barwy części typów i znaczenie pól `class_particle` ❓ |
| Kamera w przerywnikach | ❓ | w rozgrywce `zoom` przycięty do [1,0; 2,5] ✅ ([6.1](06_kamera.md)); skrypty przerywników używają 0,5–6,0 |
| Tryby rysowania linków 6, 7 (i maska trybu 8) | 🔶 | tabela w [3db.md](../formaty/3db.md) §3.2: 4 = 1, 9 i 11 nieprzezroczyste ✅; 6 i 7 półprzezroczyste z mapowaniem otoczenia (dokładny wzór ❓) |
| Płyny: symulacja przepływu | ❓ | dane i rysowanie: [rozdział 13](13_plyny.md); prawa przepływu: Diggles-Unity `FLUID_SYSTEM_RE.md` |
| Animacje tekstur (`txtanim`), materiały `shader.tcl` | ❓ | |

## 10.2 Świat i rozgrywka

| Temat | Stan | Gdzie szukać |
|---|---|---|
| Generator świata kampanii i trybu swobodnego (`lg_*`, `StoryMgr load_level`) | ❓ | `Scripts/init/LGINIT.TCL`, Diggles-Unity `WORLDGEN_ORIGINAL_RE.md`, `LEVEL_GENERATOR.md` |
| Maska ruchu w `.pmp` (przebiegi u16) | 🔶 | płaszczyzna nawigacji; Diggles-Unity `ORIGINAL_PATHFINDING_RE.md` |
| Model obiektów Tcl: szczegóły | 🔶 | opisany w [rozdziale 11](11_obiekty_tcl_i_petla.md) za Diggles-Unity; do sprawdzenia: semantyka licznika stanów, dlaczego timer klasy z `userstartup.tcl` nie zadziałał |
| Dźwięk: przełączanie motywów `adaptive_sound`, próg ciszy daleko od kamery (głośność od odległości i stereo zmierzone: [15.4a](15_dzwiek.md); materiały i pomieszczenia w danych zawsze `any`) | 🔶 | składnia i powiązania: [rozdział 15](15_dzwiek.md); prawa mieszania: Diggles-Unity `SOUND_SYSTEM_RE.md` |
| Ramka ekranu: rola prawej dolnej ćwiartki `bordermask.tga` | ❓ | prawo ✅ i szerokość stała w pikselach (🔶 800×600): [rozdział 9.4](09_klatka.md); dawna „winieta” to ramka plus ciemna treść sceny |
| Rejestr `TCLDEFINES`: składnia symboli preprocesora | ❓ | sama nazwa nie działa: [rozdział 2.2](02_start_i_skrypty.md) |
| Kopanie, produkcja, walka, AI, interfejs | ❓ (zasady gracza: [rozdział 16](16_rozgrywka.md) 🔶) | skrypty w `Data/Scripts`; Diggles-Unity ma dokumenty do każdego tematu ([mapa Diggles-Unity](../DIGGLES_UNITY.md)) |
| Zapisy `.sav`: stan rozgrywki | 🔶 | mapę, kamerę, mgłę i obiekty czytamy ✅ ([formaty/sav.md](../formaty/sav.md)); logika obiektów, gracze, ekonomia i fabuła wg Diggles-Unity `ORIGINAL_SAVE_FORMAT_RE.md`, niesprawdzone |
| Fonty `.kft`, `ALPHABET.BIN`, `GUI/*.bin` | ❓ | |

## 10.3 Jak badać

Nie analizujemy kodu exe. Zachowanie ustala się obserwacją oryginału: scena w `data/userstartup.tcl`, zrzut ekranu przy zmienianym jednym parametrze, porównanie z własnym renderem. Stan działającej gry (obiekty, klasy, zmienne, wynik dowolnego Tcl) najprościej odczytać przez wbudowany serwer WebTraq ([`tools/lab/live.sh`](../../tools/lab/live.sh)). Narzędzia i wnioski z dotychczasowych eksperymentów: [BAZA_WIEDZY.md](../BAZA_WIEDZY.md) §8–9.
