# TODO: OpenWiggles

Żywy plan. Odhaczamy `[x]`, dopisujemy nowe punkty, gdy coś odkryjemy.
Oznaczenia: 🔬 = najpierw trzeba zbadać format lub zachowanie w laboratorium (wyniki do [`docs/BAZA_WIEDZY.md`](docs/BAZA_WIEDZY.md)), 🔧 = dopracowanie działającego przybliżenia, 🤖 = bot testowy.
Przed pracą nad podsystemem: wiersz w [mapie Diggles-Unity](docs/DIGGLES_UNITY.md) (ich dokument, stan u nas) i sprawdzenie w działającym oryginale przez [`tools/lab/live.sh`](tools/lab/live.sh).

## Droga do grywalności (priorytet)
Najpierw działa mniej więcej, potem dopracowujemy (punkty „🔧” niżej).

### Następne kroki (kolejność)
Kolejność wg [mapy Diggles-Unity](docs/DIGGLES_UNITY.md) §6: najpierw to, co u nich działa, a u nas blokuje grywalność.
1. [ ] **G12 Czas gry**: komenda `gametime` (`start`/`stop`/`factor`), `gethours` = `gettime`/150, doba = 12 h = 30 min, data `dzień.miesiąc.rok`; plan dnia krasnali (`current_time_plan`, zegar na ekranie) (rozdziały [11.4](docs/silnik/11_obiekty_tcl_i_petla.md), [16.2](docs/silnik/16_rozgrywka.md)). Mały krok, potrzebny do pracy i odpoczynku
2. [ ] **G9 Potrzeby i harmonogram**: jedzenie, sen, nastrój, przejście praca ⇄ czas wolny wg planu dnia (`get_remaining_sparetime` dziś zawsze > 0); stany `work_idle`, `prodfill_dispatch`, `sparetime_dispatch` jak w zapisie kampanii (BAZA §8.7). Źródło: [GNOME_BEHAVIOR_COMPLETE_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/GNOME_BEHAVIOR_COMPLETE_RE.md), [re_parts/sparetime-idle](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/re_parts/sparetime-idle.md)
3. [ ] **G10 Dyspozytor pracy i produkcja** (największa luka): ognisko, skrzynie (pakowanie, rozpakowanie), zlecenia (LPM/PPM, 10 = bez końca), przydział do placówki, `prodfill`. Źródło: [WORK_DISPATCHER_NATIVE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/WORK_DISPATCHER_NATIVE.md), [PRODUCTION_PANEL_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/PRODUCTION_PANEL_RE.md), [PRODUCTION_SLOT_OVERRIDE_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/PRODUCTION_SLOT_OVERRIDE_RE.md)
4. [ ] **G8 Przedmioty i ekwipunek**: grzyby, kamienie, kryształy po kopaniu, podnoszenie, odkładanie (wolne miejsce), użycie. Źródło: [ITEM_PLACEMENT_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/ITEM_PLACEMENT_RE.md)
5. [ ] **Rozkazy gracza jak w grze**: kliknięcie → `EVENTGEN.TCL` (`evtgen_attrib`) → zdarzenie `evt_task_*` dla krasnala ([rozdział 16.9](docs/silnik/16_rozgrywka.md)), zamiast rozkazów w C++. Przy okazji punkt `/tclshell` w grze (zgodny z WebTraq) dla bota
6. [ ] **Kopanie przez skrypty gry** (`dig_mark`, `dig_starttask`, `dig_next`, `dig_apply`, `get_digedge`, `CObj_DigTask`), oznaczenia kopania, kilku kopaczy. Źródło: [DIGGING_MECHANIC](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/DIGGING_MECHANIC.md), [re_parts/dig-natives](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/re_parts/dig-natives.md), [re_parts/tcl-dig-complete](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/re_parts/tcl-dig-complete.md)
7. [ ] **Ruch jak w grze**: siatka 8×8 i maska `.pmp`, wspinanie, spadanie. Źródło: [ORIGINAL_PATHFINDING_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/ORIGINAL_PATHFINDING_RE.md), [re_parts/walk-executor](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/re_parts/walk-executor.md)
8. [ ] **G13 Generator świata** (nowa gra w kampanii i potyczce: `StoryMgr load_level`, `load_sandbox`, `lg_*`). Źródło: [WORLDGEN_ORIGINAL_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/WORLDGEN_ORIGINAL_RE.md), [LEVEL_GENERATOR](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/LEVEL_GENERATOR.md)
9. [ ] **Szanse na wyprzedzenie Diggles-Unity** (u nich też nie działa): drabiny, windy, drzwi ([LADDER_ELEVATOR_DOOR_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/LADDER_ELEVATOR_DOOR_RE.md)); walka, potwory, dyplomacja ([COMBAT_SYSTEM_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/COMBAT_SYSTEM_RE.md)); energia ([ENERGY_SYSTEM_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/ENERGY_SYSTEM_RE.md)); przejścia między światami; pełny zapis `.sav`

### Zrobione i dopracowanie
- [x] G1 Program `openwiggles`: świat z zapisu, scenariusza albo szablonu, kamera gry (WASD, mysz, kółko), HUD
- [x] G2 Krasnale (`Zwerg`, model `mann`): animacje chodu, wspinania, stania, kopania
- [x] G3 Zaznaczanie (LPM) i rozkaz „idź” (PPM): siatka pól i A* ([`src/lib/nav.h`](src/lib/nav.h))
- [x] G4 Kopanie (PPM na skale, przeciągnięcie PPM = obszar do kopania dla zaznaczonego krasnala): tunel 4523 z gruntem pola, znaleziska 4%, odkrywanie mgły przez krasnale
- [x] G5 Zapis: dekoracje spoza listy obiektów (nasiona w kubełkach, obiekty zastępcze) aż do znacznika 7
- [ ] Zapis: sekcje od znacznika 7 (światła klas, sterowanie, animacje, cząsteczki, fabuła, AI): potrzebne do wczytania zapisu do działającej gry
- [x] G6 Symulacja płynów (źródła `Wasser`/`Sulfur`/`Lava`, przepływ 10 Hz), zgodna z zapisem gry
- [ ] 🔧 Płyny: zatyczki, odpływy, `-height`/`-ventil`, źródła z zapisu, kolor wody bliżej gry, rzadka lista pól zamiast pełnego prostokąta
- [x] G7a Osadzony Tcl 8.6: preprocesor, `call`, wirtualny katalog, 2684 klasy bez błędów, interpreter na obiekt, `obj_init`, timery, zdarzenia, stany, zaślepki z dziennikiem (`owscript`, rozdział 11.9)
- [x] G7b Krasnale sterowane skryptami gry (czas wolny): atrybuty z `def_attrib`, `action walk/rotate/anim/wait` przez nawigację gry, `obj_query`, `get_place`, `placelock_*`, wektory i listy; zapis kampanii: 0 błędów skryptów w 60 s
- [x] Punkty zaczepienia `.3db` i `link_obj`/`get_linkpos`; przypięcia i właściciele z zapisu (czapki na głowach)
- [x] Zapis: zmienne Tcl obiektów i ID obiektów zgodne z zapisem (stan skryptów nakładany po obj_init)
- [ ] Zapis: kolejka timerów (rekordy pomocnicze = CEvent), atrybuty obiektów, akcje w toku
- [x] Krasnale przechodzą do czasu wolnego (`sparetime_dispatch`: łapanie much, spacery, animacje): poprawki `obj_query` (argumenty jako osobne słowa, bez pytającego, z w prostopadłościanie), `get_walkresult` = 2 w trakcie chodu, `get_place -random` omija rezerwacje i postacie, rezerwacje 3D
- [ ] G7c Kolejne komendy wg dziennika `openwiggles_tcl.log` (`action move`, `get_vectorxz`, `lcount`, `sparetime`, `dist_between` …), harmonogram pracy (`get_remaining_sparetime` dziś zawsze > 0), właściciel obiektów z zapisu
- [x] Pamięć podręczna klas: start skryptów 0,1 s zamiast 5,5 s
- [ ] 🔧 Skrypty: zgodność 8.3 → 8.6, odczyt zmiennych Tcl obiektów z zapisu (tryb bezpieczny niepotrzebny: gra go nie używa, rozdział 11.1)
- [x] Interfejs gry: panel krasnali z imionami i potrzebami, znacznik zaznaczenia, pauza i tempo
- [ ] Drzewko technologii i wynalazki (`init_techtree`, `gen_tt.tcl`, „czapka naukowca”), po G10
- [x] G11 Zapis i wczytanie gry z poziomu gry: własny format `.ows` (źródło + zmiany), F5/F9 (rozdział 14.7)
- [x] Zapis jak w grze: F5 zapisuje `.sav` łatą na wczytanym zapisie (czas, kamera, pola, płyny, pozycje obiektów); oryginalna gra go wczytuje (BAZA §9.6)
- [x] Menu wyboru gry (ImGui, start bez źródła i Esc): nasze zapisy, zapisy gry, scenariusze; F9 wczytuje najnowszy zapis
- [x] Przesuwanie kamery kursorem przy krawędzi okna
- [ ] 🔧 Krawędź okna: szerokość pasa i prędkość zmierzyć w grze; menu bliżej oryginału (szablony, nowa gra, opisy scenariuszy)
- [ ] 🔧 Zapis `.sav`: obiekty utworzone i usunięte w grze, zmienne Tcl, stany, timery, przepływy płynu; zapis gier ze scenariusza/szablonu (dziś `.ows`, potrzebny opis sekcji po znaczniku 7)
- [ ] 🔧 Mgła: obiekty odkrywające (`set_fogofwar`, `fog_remove`), odkrywanie promieniami (10.3) i wygasanie zamiast koła 6 pól ([FOG_OF_WAR_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/FOG_OF_WAR_RE.md))
- [ ] 🔧 Kopanie: kształty bloków z tablicy sąsiadów BlockDB (zszywanie krawędzi), twardość materiału, czas kopania; przebudowa tylko zmienionego kawałka terenu
- [ ] 🔧 Wskazywanie: promień kamery przecięty z płaszczyzną z = 11; lepiej z siatką terenu i modelami
- [x] Scena i rysowanie map w bibliotece ([`lib/map_scene.h`](src/lib/map_scene.h), `MapScene`), wigview dokłada tylko panel (`MapView`); gra nie kompiluje już plików przeglądarki
- [ ] 🔧 Scenariusze: krasnal w `single_Knockersdorf` stoi poza szablonami (atrapa przenoszona przez skrypty fabuły)
- [x] Ramka ekranu `BorderMask` w grze (domyślnie, `--no-border-mask`) i jako opcja w wigview (`--border-mask`, przełącznik w zakładkach map); prawo doprecyzowane: teksel 3·(p + 0,5 − 2) z próbkowaniem liniowym, zgodność z grą ±0,02 ([rozdział 9.4](docs/silnik/09_klatka.md))
- [x] 🔬 Ramka ekranu przy 800×600: szerokość jak przy 1024×768 (🔶 stała w pikselach, pomiar zaszumiony; [rozdział 9.4](docs/silnik/09_klatka.md))
- [ ] 🤖 Bot testowy (rozwijany na bieżąco, w miarę potrzeby): te same scenariusze w oryginale i w `openwiggles`, porównanie przebiegu
  - sterowanie: rozkazy gracza jako zdarzenia `evt_task_*` (`set_event`, rozdział 16.9), w oryginale przez WebTraq ([`tools/lab/live.sh`](tools/lab/live.sh)), u nas przez taki sam interfejs `/tclshell` w grze
  - scenariusz: wczytanie zapisu, rozkazy (idź, kop, podnieś), próbki stanu co sekundę (pozycje, stany, zadania, teren), zrzuty
  - porównanie z tolerancją (losowość, ok. 20 kl./s w oryginale): czas dojścia, kolejność stanów, wynik kopania; raport z zestawieniem zrzutów
  - [x] pierwszy krok: `set_event <krasnal> evt_task_walk -target <krasnal> -pos1 {x y z} -text1 userevent` działa w oryginale (BAZA §8.8)
- [ ] 🔧 Obiekty Tcl bez kopii zmiennych głównego interpretera, jak w grze (rozdział 11.1); sprawdzić, czy skrypty krasnali nadal działają

## Faza 0: rozpoznanie
- [x] Analiza repozytoriów społeczności w `../ref/` (w tym wng-game i wng-server: puste)
- [x] Przegląd zainstalowanej gry i struktury `Data/`
- [x] Potwierdzenie formatu `.3db` na wszystkich 43 plikach i ustalenie dekwantyzacji pozycji
- [x] Skaner natywnych komend Tcl ([`tools/scan_tcl_natives.py`](tools/scan_tcl_natives.py))
- [x] Uruchomić oryginał pod Wine w osobnym prefiksie ([`tools/lab/setup_lab.sh`](tools/lab/setup_lab.sh), `run_lab.sh`)
- [x] Sonda Tcl: dokładna lista 639 komend natywnych ([`docs/natywne_komendy_gry.txt`](docs/natywne_komendy_gry.txt)), trace startu
- [x] Śledzenie wewnątrz interpreterów obiektów (`obj_eval` + owijki komend) i podczas rozgrywki (`gameload` zapisu kampanii, 5 krasnali przez 90 s): osobny interpreter na obiekt, bez trybu bezpiecznego, bez zmiennych głównego interpretera, logika 10 Hz, timery działają ([`tools/lab/owprobe_deep.tcl`](tools/lab/owprobe_deep.tcl), [`owprobe_play.tcl`](tools/lab/owprobe_play.tcl), BAZA §8.7, rozdział 11)
- [x] `WebServer=1` uruchamia serwer twórców **WebTraq** (port 8082): obiekty, klasy z metodami, **powłoka Tcl do działającej gry**; narzędzie [`tools/lab/live.sh`](tools/lab/live.sh) (BAZA §8.6). `tclhelp` pusty (pomocy brak), `web_shell` bez efektu, `TCLDEFINES` z samą nazwą nie działa (❓ składnia), preprocesor: `IF`=`IFDEF`, `IFNOT`=`IFNDEF`, `FULL` zdefiniowany
- [x] Sonda `ClassList`/`ClassInfo`/`get_class_*`: 2836 klas (5 klas C++ silnika), typy, kategorie, ery, flagi (BAZA §8.7); metody i stany klas są w WebTraq (`/classinfo:<klasa>`)
- [x] Sonda `db_*` dla wszystkich 6445 animacji: długości zgodne z naszym parserem (6444; różnica = kolizja nazwy `wuerfel`), numery animacji = kolejność w pliku, powtórzona nazwa obiektu: wygrywa pierwsza wczytana baza (rozdział 5.1)
- [x] ~~Barnacle (SDK) pod Wine~~: niepotrzebny, bo WebTraq i `obj_eval` dają dostęp do stanu gry
- [x] Zrzuty obrazu z gry: `tools/lab/capture.sh menu|template|scene` (Xvfb, wirtualny pulpit Wine, pełny ekran 1024×768, pełna ścieżka C:\\Wiggles\\Wiggles.exe)
- [x] Kamera wigview odpowiadająca `set_view x y zoom dx dy` z gry (prawo z exe za Diggles-Unity); menu ze `systeminit.tcl` zgodne 1:1 ze zrzutem gry
- [x] „Winieta” to ramka ekranu z `Data/GUI/bordermask.tga` (rejestr `BorderMask=1`): ok. 10 px, 3 px czerni i gradient, zaokrąglone rogi; reszta ciemnych brzegów to treść sceny (rozdział 9.4)
- [x] `Instrukcja.pdf`: mechaniki rozgrywki streszczone w [rozdziale 16](docs/silnik/16_rozgrywka.md); doba 30 min potwierdzona w grze (`gethours` = `gettime`/150, 12 h na dobę)

## Renderowanie: Vulkan i OpenGL
- [x] Warstwa `rhi` (tekstury, bufory, potoki, uniformy w buforze pierścieniowym, odczyt klatki, ImGui)
- [x] Backend OpenGL 4.5 (render menu identyczny co do piksela z wersją sprzed przebudowy)
- [x] Backend Vulkan 1.2 (swapchain, dwie klatki w locie, deskryptory z pamięcią podręczną, usuwanie zasobów z opóźnieniem, tryb pozaekranowy do zrzutów); obraz zgodny z OpenGL, warstwa walidacji bez błędów
- [x] Shadery z jednego źródła GLSL 450 (`glslc` → SPIR-V, tekst dla OpenGL); `build.sh vulkan|opengl|both`; `--renderer`, domyślnie Vulkan z przełączeniem na OpenGL
- [ ] Sprawdzić Vulkana w oknie na prawdziwym ekranie (zmiana rozmiaru okna, minimalizacja, siatka)
- [ ] Ewentualnie: bufory w pamięci karty (staging) dla kart dyskretnych; dynamic rendering

## Faza 1: szkielet projektu i build
- [x] Struktura katalogów: [`src/lib`](src/lib) (libwiggles, renderery w [`src/lib/render/`](src/lib/render)), [`src/apps/wigview`](src/apps/wigview), [`src/apps/game`](src/apps/game), [`src/tools`](src/tools), `third_party/`; [`build.sh`](build.sh) pilnuje, żeby biblioteka nie zależała od programów
- [x] [`build.sh`](build.sh) (bez CMake): kompilacja przyrostowa (zależności z `.d`), równoległa, debug/release, cele `lib`, `imgui`, `wigview`, `test3db`, `clean`
- [x] Zależności: SDL2 i glm z systemu, Dear ImGui 1.92.9 w `third_party/`, własny loader GL zamiast glad, Tcl 8.6 z systemu, dr_mp3 w `third_party/`
- [x] Ścieżka do gry: `--game`, zmienna `OW_GAME`, domyślnie instalacja w `~/.wine`
- [x] `git init` (repo `openwiggles_git`; `ref/` i zipy zostają poza repo)
- [x] `.gitignore` (`build/`, pliki wynikowe)

## Faza 2: libwiggles, czyli formaty plików
- [x] Wirtualny system plików: wyszukiwanie bez rozróżniania wielkości liter, wiele korzeni
- [x] VFS: nakładki w układzie modów (`Data/` + `Data_<język>`), `--overlay` w wigview i grze, sprawdzone na DigglesHDmod ([tekstury.md](docs/formaty/tekstury.md)); łatki `change_*.tcl`: faza 7
- [x] Loader TGA (typy 2/3/10/11, 8–32 bit); gra używa wyłącznie typu 2, 32 bit
- [x] Wyszukiwanie tekstury po nazwie materiału (`m256` → `m128` → `m064` → `m032` → `Gray` → `Misc` → `ClassIcons`)
- [x] Parser `.3db` w C++ z walidacją odwołań; `test3db` przechodzi wszystkie 43 modele i 2052 tekstury
- [x] `.3db`: pola mesha `shadowIndex` i `cmapIndex` rozpoznane ✅
- [x] Pola `.3db`: tablica punktów zaczepienia, prostokąt cienia, `cmap` (maska 1/8 bloku z przesunięciem), kąty Eulera w rekordach zaczepienia, `move`/`rotate` na klatkę (zmierzone w grze); pola w kodzie przemianowane ([3db.md](docs/formaty/3db.md))
- [x] `.3db`: `flags` linku to tryby rysowania (0 niewidoczny, 1 zwykły, 2 addytywnie, 3 mieszanie alfa, 5 mapowanie otoczenia, 8 bez oświetlenia; zrzuty z gry); bajty 6–7 rekordu zaczepienia to dopełnienie wyrównania
- [x] Renderer modeli: tryby rysowania wg `flags` linku: 0 pominięty, 2 addytywnie (osobny przebieg bez zapisu głębi, bez oświetlenia 🔶), 5–7 mapowanie otoczenia z normalnej 🔶, 8 bez oświetlenia; przełącznik „Tryby rysowania” i `--no-draw-modes` w wigview
- [x] Tryb 3 (miękka alfa): fazy rysowania w scenie (`ModelDrawOptions::phase`): najpierw rdzenie (alfa ≥ 0,5) z zapisem głębi, potem miękkie brzegi i blask trybu 2 bez zapisu, od najdalszego obiektu; bez tego w menu przez brzegi lian prześwitywało niebo
- [x] 🔬 Tryby 4, 9, 11 i alfa trybu 8 (eksperyment na kolumnach lawy, BAZA_WIEDZY §8.10): 4 = 1, 8/9/11 nieprzezroczyste (alfa nie wycina; w wigview brakowało trzonów kolumn w `lava_unq_vampy_tuer`), 9 oświetlony, 11 bez oświetlenia, 8 z alfą jako maską samoświecenia 🔶
- [ ] 🔬 Tryby 6, 7: dziś półprzezroczyste z mapowaniem otoczenia w fazie przezroczystej 🔶; dokładny wzór mieszania i różnica 6/7 (zrzuty w Xvfb przez `live.sh`)
- [x] Detale terenu lawy: alfa tekstur `detail/lava*` to maska żyłek (wcześniej czerwone placki zamiast żyłek, `lava_unq_vampy_tuer`) 🔶
- [x] Szablony lawy (`lava_unq_vampy_tuer`) w porównaniu z grą (BAZA_WIEDZY §8.11): jasny grunt to lampa podglądu (`--lab-lamps` jak w laboratorium daje ciemną skałę), żyłki lawy świecą (mieszanie alfa bez oświetlenia 🔶), pełne pokrycie detalu `<świat>_1111.tga` bez litery ✅, drzwi `Tuer_kaserne` i kraty z `set standanim` ✅, tryb 8 z progiem alfy 0,5 🔶
- [x] Krasnale i wukery w płytkich korytarzach: głębokość co najmniej 1,5 przed tylną ścianą (eksperyment w grze, BAZA_WIEDZY §8.12), dodawane postacie na właściwej głębokości, wuker z modelem `wuker`, wskazywanie pól w `openwiggles` przez trafienie w teren
- [ ] Oświetlone części modeli w trybie 8 (dolne części kolumn lawy) wciąż cieplejsze i jaśniejsze niż w grze: kalibracja oświetlenia modeli lampami
- [x] Specyfikacje formatów do samodzielnej implementacji: [`docs/formaty/`](docs/formaty) (3db, tekstury, SPHERE.DAT)
- [x] `SPHERE.DAT`: tablica normalnych, używana przez `.3db` ✅
- [x] Dekoder tekstów CP1250/CP1252 → UTF-8 ([`src/lib/text.h`](src/lib/text.h), skrypty w silniku)
- [x] `MESSAGES.TXT` (`lmsg`, `locale` jak w grze, `--locale`), `QUOTES.TXT`, `LOCALE.TXT` ([teksty.md](docs/formaty/teksty.md)); CP1250 w `text.h`
- [x] `texlevels.txt` i `TEXMAPS.BIN` opisane; gra rysuje z `TEXMAPS.BIN` (ARGB4444)
- [x] Wierność tekstur: kwantyzacja do ARGB4444 jak `TEXMAPS.BIN` i przypisanie `TextureQuality` do poziomów `texlevels.txt`: niepotrzebne, to były mechanizmy oryginału dla słabych kart graficznych (mniej pamięci, mniejsze tekstury); używamy pełnych 8-bitowych TGA (decyzja)

## Faza 3: wigview, czyli przeglądarka assetów
- [x] Okno SDL2 + ImGui (dziś Vulkan albo OpenGL 4.5 przez `rhi`), lista modeli `Data/3db`, polski font
- [x] Drzewo całego `Data/` (inne typy plików): zakładka Pliki z nakładkami modów, statystyka typów w katalogu (14 171 plików), opis typu z odnośnikiem do specyfikacji, podgląd tekstu (CP1250/CP1252) i hex, filtr ścieżek, „Otwórz w zakładce” dla modeli, tekstur, dźwięków, szablonów, scenariuszy i zapisów; `--file ŚCIEŻKA`
- [x] Podgląd tekstur TGA (lista katalogów i plików, filtr, rozmiar): katalogi `Texture/`, `Ground/` i `GUI/` (z podkatalogami)
- [x] Tekstury: zoom (suwak, Ctrl+kółko, „Dopasuj”, „1:1”), podgląd kanałów (RGBA, RGB, alfa, R, G, B), tło szachownicy, wartość piksela pod kursorem; `--texture GUI/bordermask.tga --texture-channel 2`
- [x] Podgląd modelu `.3db`: kamera orbitalna, lista obiektów i animacji z filtrami
- [x] Odtwarzanie animacji: play, pauza, krok po klatce, prędkość, pętla
- [ ] 🔬 Znaczenie pól animacji (flagi, f32, ruch/obrót korzenia); tempo: logika 10 Hz i obraz ok. 20 kl./s zmierzone (rozdział 11.7), interpolację zostawiamy włączoną niezależnie od oryginału (decyzja: płynniejszy obraz to zaleta; próba pomiaru w [BAZA_WIEDZY §9.9](docs/BAZA_WIEDZY.md))
- [x] Interpolacja między klatkami (oryginał też interpoluje: `AnimIpol`, domyślnie włączona); przełącznik `I`
- [x] Esc zamyka wigview
- [x] Mapy i zapisy: obiekt pod kursorem podświetlony, kliknięcie (bez przeciągania) zaznacza obiekt (trafienie w trójkąty bieżącej klatki) albo pole terenu; panel „Zaznaczenie”: klasa, imię, ID, pozycja, model i animacja (przejście do zakładki Modele), tryb animacji klasy, właściciel, przypięcie, parametry z edytora, zmienne Tcl z zapisu, a z symulacją stan skryptu, zadania i potrzeby krasnala; pole terenu: blok BlockDB, typ i grunt, warstwa .l2m, głębokość, płyn, mgła; `--click X Y` do zrzutów. Krasnale z zapisu bez symulacji mają model `mann.3db`
- [x] Symulacja w wigview: przełącznik „Symulacja (skrypty)” w zakładkach map (domyślnie wyłączony; `--simulate`, `--warmup S`): skrypty obiektów jak w grze przez wspólny [`world_sim`](src/lib/world_sim.h) (krasnale i wukery chodzą, pierścienie i inne obiekty dostają efekty i animacje ze skryptów); opcja „Dodaj postacie na mapie bez krasnali” (domyślnie wyłączona, liczba krasnali i wukerów; `--populate K [W]`) stawia postacie przy środku kadru na suchych polach; „Od nowa”; wyłączenie wczytuje mapę od nowa
- [ ] Symulacja: wukery chodzą mechanizmem krasnali (siatka nawigacji, animacje `gehen_loop`/`kletter_hoch`); walka z krasnalami i zachowania potworów nie są sprawdzone; most nad wodą w `kris_unq_060` nie jest chodliwy w nawigacji
- [x] Ścieżki: katalog gry z `--game`, `WIGGLES_DIR`, `OW_GAME`, wyboru w oknie „Ścieżki…” (wigview.ini; zmiana restartuje wigview) albo `~/.wine`; katalog zapisów z wyboru albo `--saves-dir`; `WIGGLES_DIR` także w grze i `owscript`; przy starcie okno postępu z etapami (`OW_PROGRESS_LOG=1` wypisuje czasy), skan użyć cząsteczek w skryptach dopiero przy otwarciu zakładki (start 3,3 s → 0,6 s)
- [x] Shader: tekstura × oświetlenie z normalnych `SPHERE.DAT`, alpha test, tryby porównawcze, siatka
- [x] Orientacja osi: Y w dół, układ lewoskrętny; w OpenGL odbicie osi Y (poprawione: obrót odwracał napisy) ✅
- [x] Skala modeli względem świata gry: 1 blok = 200 jednostek modelu, bez skalowania ✅ ([3db.md](docs/formaty/3db.md), [rozdział 5](docs/silnik/05_obiekty.md))
- [x] Podgląd cieni 32×32 i `cmap` z `.3db`: sekcja „Cień i cmap” w zakładce Modele (widok z góry x/z z bbox, `shadowQuad`, kratkami maski), `--shadow-cmap`; ustalona płaszczyzna i sposób rzutu ([3db.md §3.6–3.7](docs/formaty/3db.md), [BAZA_WIEDZY §9.7](docs/BAZA_WIEDZY.md))
- [ ] 🔬 Cień w grze: 🔶 rysuje go tylko od lamp w zasięgu `MaxShadowLightDist` (smugi od lampy w jaskini, przy 0 znikają; na powierzchni brak, [BAZA_WIEDZY §9.8](docs/BAZA_WIEDZY.md)). Dalej: płaska pusta podłoga, jeden nieruchomy obiekt, lampa w kilku pozycjach, żeby zmierzyć rozciągnięcie obrazu; potem renderer
- [x] Zrzut PNG z linii poleceń (`wigview --screenshot`)
- [x] Eksport do glTF (`.glb`, morph targets, osadzone tekstury) w wigview i narzędziu `export_gltf`; walidator Khronos bez uwag
- [x] Nawigacja klawiaturą: ↑/↓ modele lub tekstury, PgUp/PgDn animacje
- [x] Wyszukiwarka Ctrl+F: modele, obiekty w modelach, tekstury, szablony
- [x] Cząsteczki: zakładka z 43 typami (atlas, parametry, użycie w skryptach, podgląd emitera), `--particle N`; emitery z `class_particle` na mapach; rozdział [12](docs/silnik/12_czasteczki.md)
- [x] 🔬 Cząsteczki: zrzuty wszystkich 43 typów z gry (krasnal jako nośnik, `change_particlesource … 64 2 0 10`), tabela wyglądu i porównanie z naszym podglądem ([BAZA_WIEDZY §9.10](docs/BAZA_WIEDZY.md))
- [x] Cząsteczki: pierwsza kalibracja rozmiaru i emisji w tej samej scenie (`wigview --emit`, pomiar zasięgu i jasności względem zrzutów z gry): 36–42 i 7, 12–14, 17–19, 22, 24, 26, 28–30, 32–35 zgodne co do czynnika 2 ([BAZA_WIEDZY §9.10](docs/BAZA_WIEDZY.md))
- [x] Cząsteczki „ziemne”: podłoże z mapy wysokości (`MapScene::floorDrop`, pierwsza lita próbka pod emiterem) zamiast płaszczyzny 3 bloki niżej; rozmiary 1, 8, 15, 17, 26 wg zrzutów gry
- [x] Cząsteczki: kształt 0 (zwarta kolumna żaru), 3 (szerszy ogień), 6 (szary dym): zasięg jak w grze, jasność w granicach czynnika 2,5
- [ ] Cząsteczki: barwy 8, 12 i 42; przepisać prawa typów zamiast wspólnego modelu; płomień typu 3 przy `Dummy_Haengelicht` niewidoczny
- [x] Kamera: gra przycina `zoom` do [1,0; 2,5] (✅ `get_view` po `set_view`); u nas `kGameZoomMin/Max` w grze (kółko myszy, `gameCamera`) i w emulacji kamery wigview
- [ ] 🔬 Kamera w przerywnikach: skrypty używają `zoom` 0,5–6,0; czy wtedy zakres jest inny
- [x] Płyny z zapisu: typ i wypełnienie pola (`.sav`), deskryptor płynu z BlockDB, rysowanie ściany i powierzchni ([rozdział 13](docs/silnik/13_plyny.md))
- [x] Języki interfejsu: polski (źródłowy) i angielski, pliki gettext `lang/*.po` osadzane przy budowaniu, wybór w oknie i `--lang`, zapamiętany w `wigview.ini` ([lang/README.md](lang/README.md))
- [x] Tłumaczenia: komunikaty błędów z [`src/lib`](src/lib) (parsery, zapis, dźwięk, glTF) przez `tr`/`strf`; konsola i dziennik skryptów zostają po polsku ([lang/README.md](lang/README.md))
- [ ] Font dla pism spoza DejaVu Sans (gdy pojawi się tłumaczenie np. na rosyjski albo japoński: wybór fontu według języka)
- [x] Zrzut wsadowy miniatur: `wigview --thumbnails KATALOG [--model M] [--size 160x160]` zapisuje `<model>/<obiekt>.png` (animacja `standard`, klatka 0) i `index.html` z siatką; wszystkie 2583 obiekty w ok. 8 s. W Vulkanie zrzuty bez UI mają rozmiar z `--size` niezależnie od menedżera okien
- [x] Dźwięki: zakładka z plikami Data/Sound i Data/Music (WAV PCM, WAV/MP3, MP3 przez dr_mp3), odtwarzanie z przebiegiem fali i przewijaniem, odwołania w skryptach (`sound bind/anim/play`, muzyka, dialogi, wzmianki) z kontekstem i przejściem do animacji; w zakładce Modele dźwięki klatek animacji; `--sound`; Ctrl+F szuka też dźwięków po nazwie logicznej ([rozdział 15](docs/silnik/15_dzwiek.md))
- [ ] 🔬 Dźwięki: porównać z grą głośność `sound play` od odległości kamery i wybór dźwięku po materiale (`sound anim … stone …`)

## Dokumentacja
- [x] Przewodnik po silniku [`docs/silnik/`](docs/silnik): rozdziały 1–16 (start i skrypty, świat, teren, obiekty, kamera, oświetlenie, tło, klatka, nieodkryty teren, obiekty Tcl i pętla, cząsteczki, płyny, gra, dźwięk, rozgrywka) z algorytmami i przykładami liczbowymi
- [x] Formaty rozdzielone: `blockdb.md`, `szablony.md` (zamiast `mapy.md`); `BAZA_WIEDZY.md` jako dziennik badań
- [x] Mapa kodu [docs/KOD.md](docs/KOD.md) (podsystem → pliki → rozdział → sprawdzanie), odnośniki z rozdziałów do kodu, [`tools/check_docs.py`](tools/check_docs.py) (drzewo od CLAUDE.md, zepsute linki, nieaktualne ścieżki, `--fix`)
- [x] Mapa Diggles-Unity [docs/DIGGLES_UNITY.md](docs/DIGGLES_UNITY.md): wszystkie ich dokumenty z odnośnikami, stan u nas, ich kod, plan „dogonić i wyprzedzić”; linia z odnośnikami na końcu każdego rozdziału i formatu
- [ ] Przy każdym nowym ustaleniu: dopisać zasadę do rozdziału przewodnika, a nie tylko do bazy wiedzy
- [ ] Rozdziały do napisania, gdy będzie wiedza: cienie, generator świata, praca i produkcja (dyspozytor), interfejs gry (`layout`, `.kft`), walka
- [ ] Przy pracy nad podsystemem: poprawić jego wiersz w [docs/DIGGLES_UNITY.md](docs/DIGGLES_UNITY.md) (stan u nas, następny krok)

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
- [x] 🔬 Mgła „wysokościowa” (170, 170, 155) to nieodkryty teren dla pól odkrytych, ale niewidocznych (współczynnik 0 dla widocznych). W przeglądarce nie występuje; należy do silnika (FOG_OF_WAR_RE)
- [x] Nieodkryty teren: rozdział 10 przewodnika (dane, odkrywanie promieniami, wygasanie, render, komendy) za Diggles-Unity
- [x] Podgląd nieodkrytego terenu w wigview (tryby `--fow`, szarość w głąb, czerń, lampy wygaszone, obiekty bez viewinfog ukryte)
- [ ] 🔬 Porównanie mgły ze zrzutami gry (menu z `timer_delete` FogRemover; sprzeczność: notatka „obraz czarny” vs prawo „wygasa do szarości”)
- [ ] Nieodkryty teren w silniku: punkt „🔧 Mgła” w Drodze do grywalności; czarne czworokąty pól nieodkrytych
- [x] Dekoracje bez opcji `anim` stoją w klatce 0 (tryby ANIM_STILL/ONCE/LOOP z `class_defaultanim`, `animrandom`)
- [x] UV terenu z palety BlockDB (trzy rzuty, ✅ na wszystkich punktach) zamiast trójpłaszczyznowego
- [x] 🔬 Przejścia gruntów maskami `Msk_*`: grunt w węźle (lewy górny narożnik pola), maska per blok z narożników (✅ eksperyment `ow_flat`)
- [x] UV detali `.l2m` z palety BlockDB (smugi mchu na podłogach OBERWELT usunięte)
- [ ] 🔬 Który grunt jest „A” w masce; trzy grunty w bloku
- [ ] 🔬 Detale `.l2m`: pełny eksperyment 16 układów × 2 zestawy na płaskiej ścianie; zestaw 2 (`urw2_*`) nie pasuje do reguły narożników (tabela w 04_teren.md §4.4); mech na filarze w menu
- [ ] 🔬 Powtórzyć zrzuty, które wyszły czarne (szachownica `ow_mix2`, OBERWELT w x = 200)
- [x] wigview: lampa podglądu dla szablonów bez własnych świateł (nie z gry, wyłączalna)
- [x] wigview: wersja (commit) w rogu ekranu i w tytule okna (`build/gen/ow_version.h` z [`build.sh`](build.sh))
- [ ] 🔬 Cienie jak w oryginale (plamy 32×32 z `.3db`, do 8 pól od lampy): zrzuty obiektu z lampą w różnych odległościach (potrzebna płyta). Cienie na żywo: tryb ulepszony w fazie 7
- [ ] 🔬 Cienie pod obiektami: mapy 32×32 z `.3db` (shadowIndex klatki), tylko w promieniu `MaxShadowLightDist` = 8 od lampy. Sposób rzutowania nierozpracowany (także przez Diggles-Unity); do ustalenia obserwacją gry (zrzuty z lampą i obiektem w różnych pozycjach)
- [x] Szczegółowe siatki 3D bloków z BlockDB (układ za Diggles-Unity, zweryfikowany); przednia płaszczyzna z = 14,5 bloku
- [ ] 🔬 Maska w `.PMP` (prawdopodobnie ścieżki ruchu)
- [ ] Obiekty bez modelu (Info_, CS_, CF_…) jako znaczniki 3D
- [x] Menu gry jako scenariusz (`systeminit.tcl`: unq_menue + dwa korytarze + set_view)
- [x] Składanie świata z szablonów (prawo przezroczystości z exe) i scenariusze stałe z `Gamesave/*.tcl` w wigview (`--scenario`)
- [ ] Świat generowany proceduralnie: G13 w „Następnych krokach”

## Faza 5: skrypty, czyli osadzony Tcl
- [x] Rozdział 11 przewodnika: interpretery obiektów, klasy, metody, zdarzenia, timery, maszyna stanów, takt gry (za Diggles-Unity i oficjalną pomocą komend)
- [x] Tempo animacji 10 kl./s (stała gry według Diggles-Unity) w wigview i eksporcie glTF
- [ ] 🔬 Zrzuty animowanego obiektu w serii: potwierdzić 10 kl./s i rozstrzygnąć, czy gra interpoluje (sprzeczność rejestr vs Diggles-Unity)
- [x] Timer w klasie z `userstartup.tcl` działa (zdarzenia co 0,5 s; czas gry rusza po `gametime start`, BAZA §8.7)
- [x] Osadzenie Tcl: 8.6 z systemu (G7a); zgodność z 8.3 pilnowana w punkcie 🔧 Skrypty
- [x] Preprocesor: komentarze `//`, `//# IF/IFNOT/IFNDEF/ELSE/ENDIF/STOPIFNOT`, `FULL` zdefiniowany (zachowanie sprawdzone w grze, rozdział 2.2)
- [x] `call` i `callnc` z normalizacją ścieżek
- [x] Dokładna lista natywnych komend (z gry, nie ze skanera)
- [ ] Podział 639 komend na moduły i priorytety: zacząć od rankingu Diggles-Unity ([ENGINE_COMMAND_GAP](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/ENGINE_COMMAND_GAP.md)) i naszego dziennika zaślepek
- [x] Model obiektowy: `def_class`, `method`, `obj_init`, `new`, `del`, `sel`, `call_method`, `def_event`, `handle_event`, `state_*`, `def_attrib` (G7a)
- [x] Zaślepki komend natywnych z dziennikiem (`openwiggles_tcl.log`); klasy wczytują się bez błędów
- [x] Kolejka zadań postaci (`tasklist_*`, G7b)

## Faza 6: silnik gry
- [x] Pętla gry 10 Hz, timery i zdarzenia (G7a); `gametime` w G12
- [x] Świat: obiekty, pozycje, kamera gry, nieodkryty teren z zapisu (G1, `MapScene`); `set_view` jako komenda Tcl i odkrywanie: 🔧 Mgła
- [x] Postacie: animacje sterowane skryptem, chodzenie (G2, G7b); ruch jak w grze i drabiny w „Następnych krokach”
- [x] Modyfikacja terenu przy kopaniu (G4); kopanie przez skrypty w „Następnych krokach”
- [ ] Produkcja, ekwipunek, drzewko technologii: G10, G8 w „Następnych krokach”
- [ ] GUI: `layout`, `textwin`, fonty `.kft` (🔬 format), ikony
- [ ] Dźwięk (`sound`, `adaptive_sound`) i muzyka: dekodowanie i mikser są gotowe ([`src/lib/sound.h`](src/lib/sound.h), [rozdział 15](docs/silnik/15_dzwiek.md)); brakuje komend Tcl, głośności od odległości i motywów muzyki
- [ ] Sekwencje fabularne (`sequencer.tcl`, `seq_audiostream`; linie `-`/`+`, rozdział 15.6). Źródło: [CUTSCENE_SYSTEM](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/CUTSCENE_SYSTEM.md)
- [ ] AI przeciwników (`ai init`, `std_ai.tcl`). Źródło: [COMBAT_SYSTEM_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/COMBAT_SYSTEM_RE.md)
- [x] Odczyt mapy z zapisu `.sav`: teren, kamera, nieodkryty teren, obiekty ([docs/formaty/sav.md](docs/formaty/sav.md)); wigview: zakładka Zapisy obok Mapy (lista z Data/Gamesave, odświeżanie, wczytanie pliku z dowolnej ścieżki), `--save`, tryb mgły 4, pole „Odkryj mapę”, pasek postępu wczytywania (czasy etapów: `OW_PROGRESS_LOG=1`)
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
