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
- [ ] Kamera wigview odpowiadająca `set_view x y zoom kąt kąt` z gry (porównania 1:1)
- [ ] Przejrzeć `Instrukcja.pdf` pod kątem mechanik gry

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
- [ ] Lista modeli do zrzutu wsadowego (miniatury wszystkich obiektów)
- [ ] Odtwarzacz dźwięków WAV i MP3 (opcjonalnie)

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
- [x] Teren: mapa wysokości z BlockDB (8×8 na blok, zweryfikowana z OBJ gry), renderer z tekstur gruntu (triplanar)
- [x] Nakładki detali z `.l2m` (narożniki po 2 bity, tekstury `detail/<świat>[2]_<kod>`, modulate×2)
- [x] Grunt podkładu z typu pola `.PMP` (2 = C, 3 = B, 4 = A) i bloki niewykopane (typ 0) ✅ z eksperymentu na zrzutach gry
- [x] Typ 5: otwory do tła (węzły otwarte, alpha test, kolor strefy `Info_Fog_*`, zanikanie modeli za skałą)
- [ ] Panorama powierzchni z paralaksą (`Panorama_layer_0..6`, BACKDROP_PARALLAX_RE §2)
- [ ] Przejścia gruntów maskami `Msk_*` (teraz interpolacja); bit 0x80; młodszy bajt `.l2m`
- [x] Oświetlenie jak w grze (jasność bazowa z warstwy 2, stałe słońce, lampy `class_light`, nasycenie); teren i modele
- [ ] Mgła wysokościowa (170, 170, 155), cienie 32×32 pod obiektami, mgła wojny
- [x] Szczegółowe siatki 3D bloków z BlockDB (układ za Diggles-Unity, zweryfikowany); przednia płaszczyzna z = 14,5 bloku
- [ ] Teksturowanie współrzędnymi u, v z palety BlockDB zamiast triplanar
- [ ] 🔬 Maska w `.PMP` (prawdopodobnie ścieżki ruchu)
- [ ] Obiekty bez modelu (Info_, CS_, CF_…) jako znaczniki 3D
- [x] Składanie świata z szablonów (prawo przezroczystości z exe) i scenariusze stałe z `Gamesave/*.tcl` w wigview (`--scenario`)
- [ ] Świat generowany proceduralnie (kampania, sandbox: `StoryMgr load_level`, `lg_*`, WORLDGEN_ORIGINAL_RE)

## Faza 5: skrypty, czyli osadzony Tcl
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
- [ ] 🔬 Zapis i odczyt gry (`.sav` = gzip; własny format czy zgodność?)

## Faza 7: dodatki
- [ ] Mody w formacie DigglesModManager, stosowane w pamięci (`change_*.tcl`, `$if`, `config.json`)
- [ ] Obsługa tekstur HD (`Data_HD`, HD mod)
- [ ] Multiplayer (bardzo daleko)
