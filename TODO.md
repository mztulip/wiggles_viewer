# TODO: OpenWiggles

Żywy plan. Odhaczamy `[x]`, dopisujemy nowe punkty, gdy coś odkryjemy.
Oznaczenia: 🔬 = najpierw trzeba zbadać format lub zachowanie (wyniki do `docs/BAZA_WIEDZY.md`).

## Faza 0: rozpoznanie
- [x] Analiza repozytoriów społeczności w `../ref/` (w tym wng-game i wng-server: puste)
- [x] Przegląd zainstalowanej gry i struktury `Data/`
- [x] Potwierdzenie formatu `.3db` na wszystkich 43 plikach i ustalenie dekwantyzacji pozycji
- [x] Skaner natywnych komend Tcl (`tools/scan_tcl_natives.py`)
- [x] Uruchomić oryginał pod Wine w osobnym prefiksie (`tools/lab/setup_lab.sh`, `run_lab.sh`)
- [x] Sonda Tcl: dokładna lista 639 komend natywnych (`docs/natywne_komendy_gry.txt`), trace startu
- [ ] Śledzenie wewnątrz interpreterów obiektów/klas (ciała `def_class`, `method`, `obj_init`) i podczas rozgrywki, nie tylko startu
- [ ] 🔬 `tclhelp`, `web_shell`, rejestr `WebServer=1`, `TCLDEFINES`: czy gra ma wbudowaną dokumentację lub konsolę?
- [ ] Sonda: zrzut `ClassInfo`/`ClassList` dla wszystkich klas (hierarchia, metody, atrybuty)
- [ ] Sonda: zrzut wyników `db_*` dla wszystkich modeli (nazwy animacji, długości): wzorzec do testów naszego parsera `.3db`
- [ ] Opcjonalnie: zbudować barnacle (SDK) pod Wine, gdy czysty Tcl nie wystarczy
- [x] Zrzuty obrazu z gry: `tools/lab/capture.sh menu|template|scene` (Xvfb, wirtualny pulpit Wine, pełny ekran 1024×768, pełna ścieżka C:\\Wiggles\\Wiggles.exe)
- [x] Kamera wigview odpowiadająca `set_view x y zoom dx dy` z gry (prawo z exe za Diggles-Unity); menu ze `systeminit.tcl` zgodne 1:1 ze zrzutem gry
- [ ] Winieta / przyciemnienie brzegów ekranu jak w grze (🔬 skąd się bierze)
- [ ] Przejrzeć `Instrukcja.pdf` pod kątem mechanik gry

## Renderowanie: Vulkan i OpenGL
- [x] Warstwa `rhi` (tekstury, bufory, potoki, uniformy w buforze pierścieniowym, odczyt klatki, ImGui)
- [x] Backend OpenGL 4.5 (render menu identyczny co do piksela z wersją sprzed przebudowy)
- [x] Backend Vulkan 1.2 (swapchain, dwie klatki w locie, deskryptory z pamięcią podręczną, usuwanie zasobów z opóźnieniem, tryb pozaekranowy do zrzutów); obraz zgodny z OpenGL, warstwa walidacji bez błędów
- [x] Shadery z jednego źródła GLSL 450 (`glslc` → SPIR-V, tekst dla OpenGL); `build.sh vulkan|opengl|both`; `--renderer`, domyślnie Vulkan z przełączeniem na OpenGL
- [ ] Sprawdzić Vulkana w oknie na prawdziwym ekranie (zmiana rozmiaru okna, minimalizacja, siatka)
- [ ] Ewentualnie: bufory w pamięci karty (staging) dla kart dyskretnych; dynamic rendering

## Faza 1: szkielet projektu i build
- [x] Struktura katalogów: `src/lib` (libwiggles), `src/wigview`, `src/tools`, `third_party/` (`src/openwiggles` później)
- [x] `build.sh` (bez CMake): kompilacja przyrostowa (zależności z `.d`), równoległa, debug/release, cele `lib`, `imgui`, `wigview`, `test3db`, `clean`
- [x] Zależności: SDL2 i glm z systemu, Dear ImGui 1.92.9 w `third_party/`, własny loader GL zamiast glad (Tcl później)
- [x] Ścieżka do gry: `--game`, zmienna `OW_GAME`, domyślnie instalacja w `~/.wine`
- [x] `git init` (repo `openwiggles_git`; `ref/` i zipy zostają poza repo)
- [x] `.gitignore` (`build/`, pliki wynikowe)

## Faza 2: libwiggles, czyli formaty plików
- [x] Wirtualny system plików: wyszukiwanie bez rozróżniania wielkości liter, wiele korzeni
- [ ] VFS: nakładanie `Data_HD`, wariantów językowych i modów
- [x] Loader TGA (typy 2/3/10/11, 8–32 bit); gra używa wyłącznie typu 2, 32 bit
- [x] Wyszukiwanie tekstury po nazwie materiału (`m256` → `m128` → `m064` → `m032` → `Gray` → `Misc` → `ClassIcons`)
- [x] Parser `.3db` w C++ z walidacją odwołań; `test3db` przechodzi wszystkie 43 modele i 2052 tekstury
- [x] `.3db`: pola mesha `shadowIndex` i `cmapIndex` rozpoznane ✅
- [ ] 🔬 Nieznane pola `.3db`: blok 0x80, blok 0x30, flagi linku, rekordy 20 B, znaczenie `cmap`, pola animacji (ruch korzenia, flagi)
- [x] Specyfikacje formatów do samodzielnej implementacji: `docs/formaty/` (3db, tekstury, SPHERE.DAT)
- [x] `SPHERE.DAT`: tablica normalnych, używana przez `.3db` ✅
- [ ] Dekoder tekstów: CP1250 i CP1252 → UTF-8, `MESSAGES.TXT` (`lmsg`), `QUOTES.TXT`
- [ ] 🔬 `texlevels.txt`, `TEXMAPS.BIN` (prawdopodobnie do pominięcia)

## Faza 3: wigview, czyli przeglądarka assetów
- [x] Okno SDL2 + OpenGL 3.3 + ImGui, lista modeli `Data/3db`, polski font
- [ ] Drzewo całego `Data/` (inne typy plików)
- [x] Podgląd tekstur TGA (lista katalogów i plików, filtr, rozmiar)
- [ ] Tekstury: zoom, podgląd samego kanału alfa, tło szachownicy
- [x] Podgląd modelu `.3db`: kamera orbitalna, lista obiektów i animacji z filtrami
- [x] Odtwarzanie animacji: play, pauza, krok po klatce, prędkość, pętla
- [ ] 🔬 Prawdziwa prędkość animacji w grze (fps) i znaczenie pól animacji (flagi, f32, ruch/obrót korzenia)
- [x] Interpolacja między klatkami (oryginał też interpoluje: `AnimIpol`, domyślnie włączona); przełącznik `I`
- [x] Esc zamyka wigview
- [x] Shader: tekstura × oświetlenie z normalnych `SPHERE.DAT`, alpha test, tryby porównawcze, siatka
- [x] Orientacja osi: Y w dół, układ lewoskrętny; w OpenGL odbicie osi Y (poprawione: obrót odwracał napisy) ✅
- [ ] 🔬 Skala modeli względem świata gry (jednostki mapy)
- [ ] Podgląd cieni 32×32 i `cmap` z `.3db`
- [x] Zrzut PNG z linii poleceń (`wigview --screenshot`)
- [x] Eksport do glTF (`.glb`, morph targets, osadzone tekstury) w wigview i narzędziu `export_gltf`; walidator Khronos bez uwag
- [x] Nawigacja klawiaturą: ↑/↓ modele lub tekstury, PgUp/PgDn animacje
- [x] Wyszukiwarka Ctrl+F: modele, obiekty w modelach, tekstury, szablony
- [x] Języki interfejsu: polski (źródłowy) i angielski, pliki gettext `lang/*.po` osadzane przy budowaniu, wybór w oknie i `--lang`, zapamiętany w `wigview.ini` ([lang/README.md](lang/README.md))
- [ ] Tłumaczenia: komunikaty błędów z `src/lib` i konsoli (dziś tylko po polsku); font dla pism spoza DejaVu Sans
- [ ] Lista modeli do zrzutu wsadowego (miniatury wszystkich obiektów)
- [ ] Odtwarzacz dźwięków WAV i MP3 (opcjonalnie)

## Dokumentacja
- [x] Przewodnik po silniku `docs/silnik/` (rozdziały 1–10: start i skrypty, świat z szablonów, teren, obiekty, kamera, oświetlenie, tło, klatka, niewiadome) z algorytmami i przykładami liczbowymi
- [x] Formaty rozdzielone: `blockdb.md`, `szablony.md` (zamiast `mapy.md`); `BAZA_WIEDZY.md` jako dziennik badań
- [ ] Przy każdym nowym ustaleniu: dopisać zasadę do rozdziału przewodnika, a nie tylko do bazy wiedzy
- [ ] Rozdziały do napisania, gdy będzie wiedza: mgła wojny, cienie, generator świata, logika obiektów Tcl (osobne interpretery, zdarzenia, timery), dźwięk, interfejs

## Faza 4: mapy i teren
- [x] Teren to mapa wysokości (głębokość w ekran); gra eksportuje wzorce: `save_map` → OBJ, `save_scene` → VRML
- [x] `.PMP`: struktura (typ, ID bloku, maska RLE, warstwa2)
- [ ] 🔬 `.PMP`: dekodowanie maski RLE i znaczenie typów oraz warstwy2 (wzorzec: OBJ z `save_map`)
- [x] `BlockDB`: nagłówek i tabela ILT (3 odwołania na blok do puli elementów)
- [x] `BlockDB`: rekordy bloków (głębokość 8×8) ✅
- [x] `BlockDB`: sekcja szczegółowych siatek 3D
- [ ] 🔬 `.l2m` (warstwa 2) i `.ENT`
- [ ] 🔬 Renderowanie gruntu: grunty A/B/C, maski przejść, detale per świat (`Ground/`)
- [x] Podgląd szablonu mapy w wigview: siatka PMP 2D (typ, ID bloku, warstwy), obiekty 3D z modelami (86,9% klas rozpoznanych)
- [x] Teren: mapa wysokości z BlockDB (8×8 na blok, zweryfikowana z OBJ gry), renderer z tekstur gruntu
- [x] Nakładki detali z `.l2m` (narożniki po 2 bity, tekstury `detail/<świat>[2]_<kod>`, modulate×2)
- [x] Grunt podkładu z typu pola `.PMP` (2 = C, 3 = B, 4 = A) i bloki niewykopane (typ 0) ✅ z eksperymentu na zrzutach gry
- [x] Typ 5: otwory do tła (węzły otwarte, alpha test, kolor strefy `Info_Fog_*`, zanikanie modeli za skałą)
- [x] Panorama powierzchni z paralaksą (`Panorama_layer_0..5`) i dywan łąki (`Panorama_layer_6`); kadr na wprost zgodny z grą
- [x] 🔬 Bramka panoramy i wiersz gruntu ustalone obserwacją: bez `set_light_begin` brak panoramy; `set_ground_begin` przesuwa dywan, domyślnie wiersz = `set_light_begin`
- [ ] Linia przycięcia panoramy (komórki tylko nad rzutem płaszczyzny dywanu), widoczna przy wierszu gruntu nad kamerą
- [x] Światło nieba `set_light_begin` (bazowe 20 nad linią nieba, skan 6 pól w górę po drobnych polach BlockDB)
- [x] Wybór strefy horyzontu jak w grze: najbliższy `Info_Fog_*` w zasięgu `size` od ogniska kamery, inaczej brak strefy
- [x] 🔬 Przycinanie kątów `set_view` przy dużym zoomie (pomiary 1,38–2,5; dopasowanie |kąt| ≤ 2,23/(R − 15,6))
- [ ] 🔬 Dokładniejsze prawo przycięcia kątów: więcej pomiarów (zoom 1,8, 2,1, 2,4; osobno dx przy 2,0 i 2,25), sprawdzić, czy limit dx i dy jest wspólny
- [x] 🔬 Oświetlenie modeli dwustronne (jaśniejsze z N i −N): drzewa OBERWELT jasne jak w grze
- [ ] OBERWELT, różnice z grą: panorama trochę niżej, brak ciemnego pasa lasu w tle, skała `Felsen` ciemna (w grze jasnoszara)
- [ ] Ciemnienie w kątach tuneli (BASELIGHT pod ziemią, splot różnic wypełnienia); teraz warstwa 2 szablonu
- [x] Tekstury: rozlanie koloru w przezroczyste teksele (bez szarych obwódek przy mipmapach)
- [ ] Bit 0x80 typu pola; młodszy bajt `.l2m`
- [x] Oświetlenie jak w grze (jasność bazowa z warstwy 2, stałe słońce, lampy `class_light`, nasycenie); teren i modele
- [x] 🔬 Mgła „wysokościowa” (170, 170, 155) to mgła wojny dla pól odkrytych, ale niewidocznych (współczynnik 0 dla widocznych). W przeglądarce nie występuje; należy do silnika (FOG_OF_WAR_RE)
- [x] Mgła wojny: rozdział 10 przewodnika (dane, odkrywanie promieniami, wygasanie, render, komendy) za Diggles-Unity
- [x] Podgląd mgły wojny w wigview (tryby `--fow`, szarość w głąb, czerń, lampy wygaszone, obiekty bez viewinfog ukryte)
- [ ] 🔬 Porównanie mgły ze zrzutami gry (menu z `timer_delete` FogRemover; sprzeczność: notatka „obraz czarny” vs prawo „wygasa do szarości”)
- [ ] Mgła wojny w silniku: obiekty odkrywające (`set_fogofwar`), wygasanie, czarne czworokąty pól nieodkrytych
- [x] Dekoracje bez opcji `anim` stoją w klatce 0 (tryby ANIM_STILL/ONCE/LOOP z `class_defaultanim`, `animrandom`)
- [x] UV terenu z palety BlockDB (trzy rzuty, ✅ na wszystkich punktach) zamiast trójpłaszczyznowego
- [x] 🔬 Przejścia gruntów maskami `Msk_*`: grunt w węźle (lewy górny narożnik pola), maska per blok z narożników (✅ eksperyment `ow_flat`)
- [x] UV detali `.l2m` z palety BlockDB (smugi mchu na podłogach OBERWELT usunięte)
- [ ] 🔬 Który grunt jest „A” w masce; trzy grunty w bloku
- [ ] 🔬 Detale `.l2m`: pełny eksperyment 16 układów × 2 zestawy na płaskiej ścianie; zestaw 2 (`urw2_*`) nie pasuje do reguły narożników (tabela w 04_teren.md §4.4); mech na filarze w menu
- [ ] 🔬 Powtórzyć zrzuty, które wyszły czarne (szachownica `ow_mix2`, OBERWELT w x = 200)
- [x] wigview: lampa podglądu dla szablonów bez własnych świateł (nie z gry, wyłączalna)
- [x] wigview: wersja (commit) w rogu ekranu i w tytule okna (`build/gen/ow_version.h` z `build.sh`)
- [ ] 🔬 Cienie jak w oryginale (plamy 32×32 z `.3db`, do 8 pól od lampy): zrzuty obiektu z lampą w różnych odległościach (potrzebna płyta). Cienie na żywo: tryb ulepszony w fazie 7
- [ ] 🔬 Cienie pod obiektami: mapy 32×32 z `.3db` (shadowIndex klatki), tylko w promieniu `MaxShadowLightDist` = 8 od lampy. Sposób rzutowania nierozpracowany (także przez Diggles-Unity); do ustalenia obserwacją gry (zrzuty z lampą i obiektem w różnych pozycjach)
- [x] Szczegółowe siatki 3D bloków z BlockDB (układ za Diggles-Unity, zweryfikowany); przednia płaszczyzna z = 14,5 bloku
- [ ] 🔬 Maska w `.PMP` (prawdopodobnie ścieżki ruchu)
- [ ] Obiekty bez modelu (Info_, CS_, CF_…) jako znaczniki 3D
- [x] Menu gry jako scenariusz (`systeminit.tcl`: unq_menue + dwa korytarze + set_view)
- [x] Składanie świata z szablonów (prawo przezroczystości z exe) i scenariusze stałe z `Gamesave/*.tcl` w wigview (`--scenario`)
- [ ] Świat generowany proceduralnie (kampania, sandbox: `StoryMgr load_level`, `lg_*`, WORLDGEN_ORIGINAL_RE)

## Faza 5: skrypty, czyli osadzony Tcl
- [x] Rozdział 11 przewodnika: interpretery obiektów, klasy, metody, zdarzenia, timery, maszyna stanów, takt gry (za Diggles-Unity i oficjalną pomocą komend)
- [x] Tempo animacji 10 kl./s (stała gry według Diggles-Unity) w wigview i eksporcie glTF
- [ ] 🔬 Zrzuty animowanego obiektu w serii: potwierdzić 10 kl./s i rozstrzygnąć, czy gra interpoluje (sprzeczność rejestr vs Diggles-Unity)
- [ ] 🔬 Timer w klasie z `userstartup.tcl` nie zadziałał: ustalić dlaczego (czas gry? `def_event`?), żeby móc sondować grę w trakcie
- [ ] Osadzenie Tcl (8.6 z systemu i sprawdzenie zgodności, albo 8.3 jak w oryginale: 🔬 decyzja po testach)
- [ ] Preprocesor: komentarze `//`, `//# IF/IFNOT/IFNDEF/ELSE/ENDIF/STOPIFNOT`, symbole `FULL` i `NOSTARTSEQ`
- [ ] `call` i `callnc` z normalizacją ścieżek (`scripts/` → `data/scripts/`, lowercase)
- [x] Dokładna lista natywnych komend (z gry, nie ze skanera)
- [ ] Podział 639 komend na moduły i priorytety
- [ ] Model obiektowy: `def_class`, `method`, `obj_init`, `new`, `del`, `sel`, `call_method`, `def_event`, `handle_event`, `state_*`, `def_attrib`
- [ ] Stuby dla wszystkich komend natywnych (logują wywołanie), tak by `systeminit.tcl` przeszedł do końca
- [ ] Kolejka zadań postaci (`tasklist_*`)

## Faza 6: silnik gry
- [ ] Pętla gry, czas (`gametime`), timery i zdarzenia
- [ ] Świat: obiekty, pozycje, kamera (`set_view`), mgła wojny
- [ ] Postacie: animacje sterowane skryptem (`set_anim`, `play_anim`), chodzenie i pathfinding, wspinanie, drabiny
- [ ] Kopanie (`dig_*`) i modyfikacja terenu
- [ ] Produkcja, ekwipunek, drzewko technologii
- [ ] GUI: `layout`, `textwin`, fonty `.kft` (🔬 format), ikony
- [ ] Dźwięk (`sound`, `adaptive_sound`) i muzyka
- [ ] Sekwencje fabularne (`sequencer.tcl`, `seq_audiostream`)
- [ ] AI (`ai init`, `std_ai.tcl`)
- [x] Odczyt mapy z zapisu `.sav`: teren, kamera, mgła wojny, obiekty ([docs/formaty/sav.md](docs/formaty/sav.md)); wigview: zakładka Zapisy obok Mapy (lista z Data/Gamesave, odświeżanie, wczytanie pliku z dowolnej ścieżki), `--save`, tryb mgły 4, pole „Odkryj mapę”, pasek postępu wczytywania (czasy etapów: `OW_PROGRESS_LOG=1`)
- [ ] 🔬 Pełny odczyt zapisu (zmienne Tcl obiektów, gracze, ekonomia) i zapis gry: własny format czy zgodność?
- [ ] Zapis: sprawdzić hipotezy (starszy bajt stanu = wariant tekstury, motyw świata z klas `Info_Fog_*`)

## Faza 7: dodatki
- [ ] Mody w formacie DigglesModManager, stosowane w pamięci (`change_*.tcl`, `$if`, `config.json`)
- [ ] Obsługa tekstur HD (`Data_HD`, HD mod)
- [ ] **Tryb ulepszony** (opcja w ustawieniach, domyślnie wyłączona; domyślny wygląd zostaje wierny oryginałowi):
  - [ ] cienie liczone na żywo (shadow mapping od lamp, cube mapy dla świateł punktowych) zamiast plam 32×32 z `.3db`
  - [ ] oświetlenie w pikselach zamiast w wierzchołkach (z zachowaniem charakteru: nasycenie, światło bazowe)
  - [ ] ewentualnie: wygładzanie krawędzi, tekstury HD, wyższa jakość mgły i panoramy
- [ ] Multiplayer (bardzo daleko)
