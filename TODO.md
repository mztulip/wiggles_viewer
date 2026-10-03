# TODO: OpenWiggles

Żywy plan. Odhaczamy `[x]`, dopisujemy nowe punkty, gdy coś odkryjemy.
Oznaczenia: 🔬 = najpierw trzeba zbadać format lub zachowanie (wyniki do `docs/BAZA_WIEDZY.md`).

## Faza 0: rozpoznanie
- [x] Analiza repozytoriów społeczności w `../ref/` (w tym wng-game i wng-server: puste)
- [x] Przegląd zainstalowanej gry i struktury `Data/`
- [x] Potwierdzenie formatu `.3db` na wszystkich 43 plikach i ustalenie dekwantyzacji pozycji
- [x] Skaner natywnych komend Tcl (`tools/scan_tcl_natives.py`)
- [ ] Uruchomić oryginał pod Wine (`-console -logerrors`) jako punkt odniesienia; sprawdzić, co loguje
- [ ] Przejrzeć `Instrukcja.pdf` pod kątem mechanik gry

## Faza 1: szkielet projektu i build
- [ ] Struktura katalogów: `src/lib` (libwiggles), `src/wigview`, `src/openwiggles`, `third_party/`
- [ ] `build.sh` (bez CMake): kompilacja przyrostowa, tryb debug i release, osobne cele `lib`, `wigview`, `openwiggles`, `clean`
- [ ] Zależności: SDL2 i Tcl z systemu (pkg-config), glad, glm i Dear ImGui w `third_party/`
- [ ] Konfiguracja ścieżki do gry (zmienna środowiskowa lub plik konfiguracyjny i parametr `--data`)
- [x] `git init` (repo `openwiggles_git`; `ref/` i zipy zostają poza repo)
- [ ] `.gitignore` (`build/`, pliki wynikowe)

## Faza 2: libwiggles, czyli formaty plików
- [ ] Wirtualny system plików: wyszukiwanie bez rozróżniania wielkości liter (`BABY.3DB` vs `baby.3db`), nakładanie `Data_HD`, wariantów językowych i modów
- [ ] Loader TGA (RGBA 32-bit, ewentualnie 24-bit i RLE: 🔬 sprawdzić, czy gra ich używa)
- [ ] Wyszukiwanie tekstury po nazwie materiału (`m256` → `m128` → `m064` → `m032` → `Gray` → `Misc` → `ClassIcons`)
- [ ] Parser `.3db` w C++: materiały, meshe, obiekty, animacje, pule, test „do końca pliku” na wszystkich 43 plikach
- [ ] 🔬 Nieznane pola `.3db`: blok 0x80, blok 0x30, rekordy 20 B, `cmap`, pola animacji (ruch korzenia, flagi)
- [ ] `SPHERE.DAT`
- [ ] Dekoder tekstów: CP1250 i CP1252 → UTF-8, `MESSAGES.TXT` (`lmsg`), `QUOTES.TXT`
- [ ] 🔬 `texlevels.txt`, `TEXMAPS.BIN` (prawdopodobnie do pominięcia)

## Faza 3: wigview, czyli przeglądarka assetów
- [ ] Okno SDL2 + OpenGL 3.3 + ImGui, drzewo plików `Data/`
- [ ] Podgląd tekstur TGA (zoom, kanał alfa, informacje o pliku)
- [ ] Podgląd modelu `.3db`: kamera orbitalna, lista obiektów i animacji
- [ ] Odtwarzanie animacji: play, pauza, krok po klatce, prędkość, pętla
- [ ] Shader: tekstura × `brightness` wierzchołka, przezroczystość alfa
- [ ] 🔬 Orientacja osi i skala modeli (oś Y w dół?)
- [ ] Podgląd cieni 32×32 i `cmap` z `.3db`
- [ ] Eksport do glTF (z animacją jako morph targets) i PNG
- [ ] Odtwarzacz dźwięków WAV i MP3 (opcjonalnie)

## Faza 4: mapy i teren
- [ ] 🔬 `.PMP`: znaczenie typów pól, indeksy bloków, ogon pliku
- [ ] 🔬 `BlockDB0/1/2.BDB`: geometria bloków terenu
- [ ] 🔬 `.l2m` (warstwa 2) i `.ENT`
- [ ] 🔬 Renderowanie gruntu: grunty A/B/C, maski przejść, detale per świat (`Ground/`)
- [ ] Podgląd szablonu mapy w wigview (pojedynczy `Templates/*.tcl`)
- [ ] Złożenie całej mapy z `Gamesave/*.tcl`

## Faza 5: skrypty, czyli osadzony Tcl
- [ ] Osadzenie Tcl (8.6 z systemu i sprawdzenie zgodności, albo 8.3 jak w oryginale: 🔬 decyzja po testach)
- [ ] Preprocesor: komentarze `//`, `//# IF/IFNOT/IFNDEF/ELSE/ENDIF/STOPIFNOT`, symbole `FULL` i `NOSTARTSEQ`
- [ ] `call` i `callnc` z normalizacją ścieżek (`scripts/` → `data/scripts/`, lowercase)
- [ ] Oczyszczenie listy natywnych komend (`docs/tcl_natywne_kandydaci.txt`) i podział na moduły
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
