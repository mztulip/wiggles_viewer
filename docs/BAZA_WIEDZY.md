# Baza wiedzy: dziennik badań

**To jest dziennik badań, nie podręcznik.** Opis działania gry, z którego da się napisać silnik, jest w [przewodniku](silnik/) i [formatach plików](formaty/) (start: [docs/README.md](README.md)). Tutaj zapisujemy, skąd co wiemy: materiały referencyjne, metody i wyniki eksperymentów w laboratorium, surowe notatki.

Analiza zipów rozpakowanych do `../ref/` oraz zainstalowanej gry (`~/.wine/drive_c/Program Files (x86)/Innonics/Wiggles`, sekcja 7). Status: ✅ potwierdzone w kodzie lub danych, 🔶 hipoteza do weryfikacji.

**Źródła:** każda sekcja podaje repozytorium, z którego pochodzi wiedza. Pełna lista autorów z linkami jest w [`CREDITS.md`](../CREDITS.md). Zasada: przy każdej nowej informacji z cudzego repo dopisujemy link do niego.

## Podsumowanie: co realnie wykorzystamy

| Źródło | Wartość | Co bierzemy |
|---|---|---|
| [`Diggles3db`](https://github.com/djunkles/Diggles3db) | ⭐⭐⭐ | Pełny opis formatu `.3db` (modele i animacje) oraz przykładowy plik `baby.3db` z teksturą do testów |
| [`DigglesHDmod`](https://github.com/ZarfaZ/DigglesHDmod) | ⭐⭐⭐ | 1171 tekstur TGA, czyli komplet przeskalowanych tekstur gry. Opis układu katalogów `Texture/` i `Ground/` oraz logiki tekstur terenu |
| `barnacle` (autor do ustalenia) | ⭐⭐ | Wersja Tcl gry (**8.3.2**), nazwy wewnętrznych funkcji silnika, mechanizm ładowania skryptów (`call`, preprocesor, cache), punkt wejścia `systeminit.tcl` |
| [`WigSound`](https://github.com/DigglesMods/WigSound) | ⭐⭐ | **Pełne oryginalne skrypty** `sequencer.tcl`, `Trigger_Tutorial.tcl`, `Trigger_Tournament.tcl` oraz położenie dialogów mówionych (`Data/Sound/Sequenzen/*.mp3`) |
| [`ProductionStatistics`](https://github.com/DigglesMods/ProductionStatistics) | ⭐⭐ | Pełne skrypty `productionmanager.tcl` i `prodman_*.tcl` |
| [`DigglesModManager`](https://github.com/DigglesMods/DigglesModManager) | ⭐⭐ | Format modów (`config.json`, łatki `change_*.tcl`), informacja o `Texture/texmaps.bin` |
| Pozostałe mody (`BugFix`, `BetterStorage`, `Barrow`, `StoneDoor`, `RareResources`, `DroppingResources`, `ElfPatch`) | ⭐ | Fragmenty oryginalnych skryptów zawarte w łatkach i słownictwo komend Tcl |
| Forum (`.html`, `plik_dat`) | ⭐ | Format `sphere.dat`, parametry linii poleceń, klucze rejestru |
| `Translations-master`, `DigglesModActions-main` | – | Nieprzydatne dla silnika |
| `wng-game-master`, `wng-server-master` | – | „Wiggles NextGen”: porzucony projekt w Godot 4.3 (GPLv2). Tylko pusta scena testowa (pryzmat i płaszczyzna) oraz serwer z pustymi plikami. Brak kodu i wiedzy o formatach |

**Oryginalne dane gry są dostępne** w instalacji pod Wine (sekcja 7).

---

## 1. Format modeli `.3db` ✅

Specyfikacja: [`formaty/3db.md`](formaty/3db.md). Źródło struktury pliku: [djunkles/Diggles3db](https://github.com/djunkles/Diggles3db) (`../ref/Diggles3db-main/python3/lib/parse_3db.py` to najczystszy kod, `python2.7/3dbScript.py` ma asercje, `c-sharp/` to przeglądarka WPF).

Historia ustaleń (co poprawiliśmy względem narzędzi społeczności):
- **Dekwantyzacja pozycji** `(u16 − 32768) / 4` ✅. Pierwsza hipoteza (skalowanie do bbox) była błędna; potwierdzenie: rozpiętość punktów zgadza się z bbox klatki (stosunek 3,99 przed dzieleniem przez 4).
- **Bajt „brightness” to indeks normalnej w `SPHERE.DAT`** ✅ (wartości rozłożone równomiernie, gładkie gradienty normalnych zgodne z geometrią).
- **Orientacja**: układ lewoskrętny, y w dół ✅. Najpierw obracaliśmy model o 180° wokół X i to było **błędne** (lustrzane napisy na `brainposter` w `KRISTALL.3DB`); poprawnie jest odbić samą oś Y.
- **Skala**: 1 blok świata = 200 jednostek modelu ✅ (wzorzec: OBJ z `save_map`).
- **Interpolacja klatek** ✅: opcja exe `AnimIpol`, rejestr `Performance\AnimInterpolation=1`; 99,5% par kolejnych klatek ma zgodną topologię.
- Wszystkie 43 pliki gry czytają się dokładnie do końca; pule UV i normalnych mają zawsze tyle elementów co pozycje (191 448 linków).
- Istniejący eksport glTF społeczności obsługiwał tylko mesh 0 bez animacji; nasz eksportuje wszystkie animacje jako morph targets.

## 2. Tekstury i katalogi ✅

Źródło: [ZarfaZ/DigglesHDmod](https://github.com/ZarfaZ/DigglesHDmod), czyli `../ref/DigglesHDmod-master/Documentation.txt`, `ReadMe.txt` oraz same pliki.

- Wszystkie tekstury to **TGA 32-bit RGBA**, kwadratowe, o boku będącym potęgą 2. Oryginał ma do 256 px, HD mod do 1024 px, a superHD do 2048 px. ✅ W grze wszystkie 2052 pliki to typ 2 (bez RLE), 32 bit (`test3db`).
- Układ katalogów:
  - `Data/Texture/m256`, `m128`, `m064`, `m032`: tekstury modeli według oryginalnej rozdzielczości. Ta sama tekstura może występować w kilku katalogach, czyli działa to jak ręczne LOD-y.
  - `Data/Texture/Misc`: GUI, efekty (`fx_licht_b`), ikony (`energy`, `events`).
  - `Data/Texture/ClassIcons`: ikony klas obiektów (334 szt.).
  - `Data/Texture/Gray`: brak w HD modzie, przeznaczenie nieznane.
  - `Data/Texture/TEXMAPS.BIN`: magic `TexC`, lista nazw tekstur (🔶 cache metadanych lub atlasu). DigglesModManager przemianowuje go, gdy mod podmienia tekstury, więc **gra przebudowuje go sama, jeśli go brak**. Możemy go ignorować.
  - `Data/GUI/icons/*.tga`: ikony interfejsu (z modów).
  - `Data_HD/`: alternatywne tekstury HD, które gra też obsługuje (mod Barrow ma `Data_HD/Texture`).
  - Warianty językowe: `Data_de`, `Data_ru` (tekstury z napisami).
- `def_texture <nazwa> <plik1> <plik2> …` w skryptach (`init/txtinit.tcl`) definiuje **warianty tekstur**, np. losowy wygląd elfów i krasnali.
- `set_textureanimation` w skryptach oznacza animowane tekstury.

### Teren (`Data/Ground`) ✅
- 3 bazowe typy gruntu **A, B, C**, po 15 wariantów każdy (`gnd_c04.tga`). Kanał alfa jest pusty.
- 14 **masek przejść** (`Msk_AAAB01.tga` … `Msk_BBBA01.tga`): w bloku, którego narożniki mają dwa grunty, maska miesza je (biel = A, czerń = B). Prawo ustaliliśmy eksperymentem `ow_flat` ([rozdział 4.2](silnik/04_teren.md)).
- Detale zależne od świata, z alfą i prefiksem świata: `kris_0101.tga` (kryształy), `urw_0101.tga` (Urwald, czyli dżungla). Sufiks 4 bitów to narożniki bloku TL TR BL BR pokryte detalem ✅ (z `.l2m`; [rozdział 4.4](silnik/04_teren.md)). Pierwotna hipoteza „strony z pustą przestrzenią” była błędna.
- Ograniczenie silnika: maski przejść działają tylko do 64×64.

### `sphere.dat` ✅
256 × `vec3 f32 LE`, czyli tablica znormalizowanych wektorów. To kwantyzacja normalnych: normalna zapisana jest jako 1 bajt indeksu do tej tablicy. ✅ Używa jej pula normalnych w `.3db`. Źródło: [Wiggles Forum](https://wiggles.ruka.at/forum/), wątek „Game internal information”.

## 3. Skrypty: Tcl 8.3 z rozszerzeniami ✅

Źródła: `barnacle` (`../ref/barnacle-main`, MIT, autor do ustalenia), pełne skrypty z [DigglesMods/WigSound](https://github.com/DigglesMods/WigSound) (`Files/Data/Scripts/classes/story/`) i [DigglesMods/ProductionStatistics](https://github.com/DigglesMods/ProductionStatistics) (`Data/Scripts/classes/work/`).

### Interpreter
- Gra linkuje **Tcl 8.3.2** (`tcl83d.dll`). Barnacle buduje dokładnie tę wersję z `libs/tcl8.3.2`. W oryginale jest też **zlib 1.1.3**. ✅ Zapisy gry (`.sav`) to gzip.
- Punkt wejścia: `Data/systeminit.tcl`.
- Skrypty przechodzą przez **preprocesor** (`sTclObject_preprocessFile`) przed ewaluacją:
  - komentarze `//`: w `systeminit.tcl` preprocesor je wycina (linie zostają puste). ✅ Jednocześnie `//` jest **komendą natywną** (no-op, 40 wywołań podczas startu), więc komentarze, które przejdą przez preprocesor, na przykład w ciałach procedur czy w plikach bez preprocesora, wykonują się jako pusta komenda. Nasz silnik musi zrobić jedno i drugie.
  - ✅ dyrektywy warunkowe: `//# IF FULL`, `//# ELSE`, `//# ENDIF`, `//# IFNOT FULL`, `//# IFNDEF NOSTARTSEQ`, `//# STOPIFNOT FULL` (przerwij plik, jeśli warunek jest fałszywy). Jedyne użyte symbole to `FULL` (pełna wersja zamiast dema) i `NOSTARTSEQ`.
  - Komentarze Tcl występują też w formach `#` i `;#`.
- `call <plik>`: natywna komenda ładująca skrypt. Ścieżki zaczynające się od `scripts/` lub `templates/` dostają prefiks `data/`. Wielkość liter nie ma znaczenia (lowercase). Pliki z `data/templates` nie idą przez cache. Jest też `callnc` (🔶 call no-cache).
- `TclCache`: cache skompilowanych lub przetworzonych skryptów (`g_tclCache`).
- `findFileLocation`: wyszukiwanie pliku w wirtualnym systemie plików (`Data`, `Data_HD`, warianty językowe).
- Wersje exe z ABI: **1.0 build 844** i **2.1.1.10**.

### Model obiektowy (natywny, w C++)
Skrypty klas działają w dwóch fazach: `if {[in_class_def]} { … }` to faza definicji, reszta wykonuje się przy tworzeniu obiektu.

| Komenda | Znaczenie |
|---|---|
| `def_class`, `new`, `del`, `obj_valid`, `obj_query` | klasy i instancje, zapytania o obiekty |
| `method <nazwa> {args} {body}`, `call_method $obj <m> …`, `check_method` | metody per klasa |
| `def_event`, `handle_event`, `set_event`, `timer_event`, `event_get` | zdarzenia |
| `obj_init "…"` | kod inicjalizacji instancji |
| `def_attrib <nazwa> <min> <max> <default>`, `get_attrib`, `set_attrib`, `set_owner_attrib` | atrybuty z zakresami (np. `GnomeAge`) |
| `state_enable`/`state_disable`/`state_trigger`/`state_get` | maszyna stanów obiektu |
| `this` | bieżący obiekt |
| `tasklist_add`, `tasklist_addfront`, `tasklist_rem`, `tasklist_cnt` | **kolejka zadań postaci**. Zadania to stringi Tcl wykonywane po kolei, np. `"walk_near_item {$pos} 0.7"` |

### Najczęstsze natywne komendy (słownictwo do zaimplementowania)
Policzone w ~134 plikach `.tcl` z ref (liczba wystąpień):

- **Obiekty i świat:** `get_pos` (81), `set_pos`, `get_posx/y/z`, `get_objname`, `get_objclass`, `get_objtype`, `get_owner`, `set_owner`, `get_ref`/`ref_set`/`ref_get`, `set_visibility`, `set_hoverable`, `set_selectable`, `set_storable`, `set_physic`, `set_roty`, `get_hmap`, `get_place`, `dist_between`, `get_safe_pos`
- **Wektory:** `vector_add`, `vector_sub`, `vector_mul`, `vector_dist`, `vector_abs`, `vector_fix`, `vector_pack`/`unpackx/y/z`. Pozycja to lista `{x y z}`.
- **Animacja i ruch:** `play_anim`, `set_anim`, `prod_anim`, `walk_near_item`, `rotate_towards`, `beam` / `beamto_inv` / `beam_from_inv_to_pos`, `db_animlength`, `get_alternateanimdb`, `class_defaultanim`, `get_classaniminfo`
- **Ekwipunek:** `inv_add`, `inv_rem`, `inv_list`, `inv_find`, `inv_cnt`, `inv_getsize`, `inv_check`
- **Produkcja:** `get_prod_slot_cnt`, `set_prod_slot_cnt`, `prodslot_override`, `get_prod_slot_list`, `get_prod_enabled`, `get_prod_materialneed`, `set_worktime`
- **Kopanie:** `dig_next`, `dig_mark`, `dig_resetid`
- **Kamera i widok:** `get_view`, `set_view`, `set_viewpos`, `centerCamera`, `camera_set`, `set_camerafollow`, `start_fade`, `screenvibe`, `set_fogofwar`
- **GUI i tekst:** `layout` (532!), `textwin`, `hyperlink`, `localize`, `lmsg`, `newsticker`, `task_to_ticker`, `clear_ticker`, `ohlp_*` (style pomocy), `gui_printPickupString`
- **Czas i losowość:** `gettime`, `gethours`, `random`, `irandom`, `hf2i`
- **Dźwięk:** `soundfile_open`, `seq_audiostream open/play`, `adaptive_sound`, `get_soundmarker`
- **Cząsteczki:** `create_particlesource`, `set_particlesource`, `change_particlesource`, `blow_particlesource`
- **Gracz i ustawienia:** `get_local_player`, `set_diplomacy`, `get_mapedit`, `im_in_campaign`, `game_over`
- **Pomocnicze listy (natywne lub z bibliotecznego Tcl gry):** `lrep`, `lrem`, `lnand`, `lor`, `fincr`, `hmax`, `hmin`, `hceil`
- **Logowanie:** `log`, `slog`, `print`
- **Fabuła:** `elf_action` (296), `sequence_enable/disable`, `set_sequenceactive`, `create_trigger`, `sm_send_message`, `sm_get_event`

Pełną listę trzeba będzie zebrać z oryginalnych skryptów gry. Najlepiej zrobić narzędzie, które przejdzie po `Data/Scripts` i wypisze wszystkie niezdefiniowane komendy.

### Organizacja `Data/Scripts` (z modów)
```
ai/            std_ai.tcl
classes/       characters/ (fenris, raupe…), items/ (transport, werkzeuge, misc_items),
               sparetime/ (bar, disco, bowlingbahn…), story/ (sequencer, Trigger_*),
               work/ (lager, saegewerk, hochofen, productionmanager…),
               zwerg/ (z_work_prod, z_dignwalk, z_events, z_procs, z_spare_*, baby…)
init/          animinit, classinit, txtinit, fight
misc/          techtreetunes (drzewko technologii)
text/doc/<lang>/  tt_<Klasa>.tcl (tooltipy), info_*.tcl, diary_*.tcl
```
Nazwy są niemieckie: *Zwerg* to krasnal (wiggle), *Lager* to magazyn, *Sägewerk* to tartak, *Hochofen* to wielki piec.

## 4. Dźwięk ✅
Źródło: [DigglesMods/WigSound](https://github.com/DigglesMods/WigSound).
- Dialogi sekwencji: `Data/Sound/Sequenzen/<nazwa>.mp3`, odtwarzane przez `seq_audiostream open/play <start> <stop>`.
- Opcja `-soundlog` w oryginale zapisuje `log_asnd.txt`.
- ✅ (2026-10-05, skaner [`src/lib/sound_refs.cpp`](../src/lib/sound_refs.cpp), `test3db`) Wszystkie 1420 plików dźwiękowych dekodują się (WAV PCM 8/16 bit, 4× WAV z MP3, MP3). Indeks: 1015 `sound bind`, 2171 `sound anim`, 735 `sound animnoseq`, 147 `sound play`, 353 odwołania muzyki, 205 dialogów; 1227 plików ma odwołanie. Wszystkie klucze `sound anim obiekt.animacja` pasują do obiektów i animacji z `.3db`; klatki liczone od 0 (🔶: 493 odwołań do klatki 0, 4 poza animacją). 11 nazw `sound bind` bez pliku (żarty `mp_chat_*` w `catch`).
- ✅ Linie plików sekwencji zaczynające się od `-` nie są wyłączone: `sequencer.tcl` (`parse_next`) wykonuje je, a pomija tylko po przerwaniu sekwencji przez gracza; `+` wykonuje się zawsze. Opis: [rozdział 15](silnik/15_dzwiek.md).

## 5. Mody: format DigglesModManager ✅
Źródło: [DigglesMods/DigglesModManager](https://github.com/DigglesMods/DigglesModManager) (`Service/ModdingService.cs`).
Nasz silnik może kiedyś stosować mody sam, przy ładowaniu i w pamięci, bez nadpisywania plików gry.

- Mod to katalog `Mods/<Nazwa>/` z `config.json` (`version`, `author`, `name{lang}`, `description{lang}`, `settings[]` z `id/type/default/possibleValues`, `messages[]`, warunki katalogów) i drzewem `Data/…`.
- Plik o nazwie zwykłej **zastępuje** plik gry (z podstawieniem `$print:<VAR>`).
- Plik `change_<nazwa>` **łata** `<nazwa>` blokami tekstowymi:
  ```
  $start
  $replace | $before | $after
  …tekst szukany…
  $with    | $put
  …nowy tekst…
  $end
  ```
  oraz warunki `$if:<boolVar>`, `$if:!<var>`, `$if:mod:<InnyMod>`, `$ifend`.
- Znane mody: BetterStorage, Barrow, Rimless, PowerPotion, ProductionStatistics, CommandF9, DwarfStatistics, SwitcherGeneratorDungeon, RareResources, DroppingResources, ChangeGameSettings, BugFix, StoneDoor, ChangeCampaignClan, MushroomSettings, Recycling, ElfPatch.
- Pliki gry są w kodowaniu **Windows-1252** (ModManager czyta `Encoding.Default`). ✅ Teksty polskie (`Scripts/text/doc/pl`) są w **CP1250**. Pliki mają końce linii CRLF.

## 6. Oryginalny exe: parametry i rejestr ✅
Źródło: [Wiggles Forum](https://wiggles.ruka.at/forum/), wątek „Game internal information” (cech12, 2021).
- Exe: `Diggles.exe` lub `Wiggles.exe`.
- Parametry: `-network -server -mapedit -nosound -nomusic -soundlog -console -oldskirmish -nofow -minimal -resetoptions -safetcl -logerrors -version -infofile -novmem -vmem`. Warto zachować podobne flagi w naszym silniku, zwłaszcza `-mapedit`, `-console`, `-nofow` i `-logerrors`.
- Rejestr: `HKCU\Software\SekOst\Diggles\Misc\PerfStats=1` zapisuje `perfstats.xls`, a `AnimStats=1` zapisuje `animstats.txt`.
- `quotes.txt`: teksty pokazywane przy ładowaniu poziomu.

---

## Otwarte pytania
Aktualna lista luk: [„Czego jeszcze nie wiemy”](silnik/niewiadome.md). Plan prac: [TODO.md](../TODO.md).

---

## 7. Zainstalowana gra (polska wersja) ✅

Ścieżka: `~/.wine/drive_c/Program Files (x86)/Innonics/Wiggles`. Tylko do odczytu, nic tam nie zapisujemy.

### 7.1 Pliki główne
| Plik | Opis |
|---|---|
| `Wiggles.exe` (4 MB) | Wersja **1.0 build 844** (`$AUTOVERSION_BUILD 844`, 7.11.2001). Pasuje do `ABI_1_0_844_59495` z barnacle, więc adresy funkcji z `../ref/barnacle-main/scripts/abi.py` dotyczą tego exe. Renderer to **Direct3D 7** (`IDirect3DDevice7`). |
| `WigTweak.exe`, `SetMode.exe`, `WigHelper.exe` | Narzędzia konfiguracyjne (tryb graficzny i ustawienia) |
| `Instrukcja.pdf` | Polska instrukcja gry: opis rozgrywki i interfejsu |
| `callstat.bin` | 12 B, statystyki uruchomień |

### 7.2 `Data/` (941 MB)
| Ścieżka | Zawartość |
|---|---|
| `systeminit.tcl` | **Punkt wejścia.** Kolejność: `customcommands`, `shader`, `txtinit`, `animinit`, `classinit`, `soundinit`, `adaptiveinit` (muzyka), `lginit`, `talkinit`, `claninit`, `makettree` z `init_techtree`, `fight`, `ai init`, `map create 64 64`, menu startowe złożone z szablonów (`MapTemplateSet`), `gui_new_game`, `gametime start`, `load_done`. Opcjonalnie `data/userstartup.tcl` zamiast menu. |
| `3db/` (43 pliki, 210 MB) | Modele. Postacie (`MANN`, `FRAU`, `BABY`, `TROLL`, `DRACHE`, `FENRIR`, `ELFE`, `ODIN`…) mają 1 obiekt i wiele animacji (MANN: 11573 klatki, 1023 animacje). Zestawy obiektów (`KRISTALL`, `LAVAWELT`, `OBERWELT`, `metalltech`, `produktionsstaetten`, `JoinedObjs`…) mają setki obiektów. |
| `Texture/` (2054 pliki) | `m032`…`m256`, `Gray` (528), `Misc`, `ClassIcons`, `TEXMAPS.BIN`, `texlevels.txt` |
| `Ground/`, `Ground/detail` | Tekstury terenu (sekcja 2) |
| `GUI/` | `stdvis`, `stateicons`, `Fonts` (`.kft`), `deco`, `buttons`, `icons`, `docpix`, `scroller*.bin` |
| `Scripts/` (834 pliki .tcl, 6,5 MB) | Pełne skrypty: `ai`, `classes/{characters,deco,items,sparetime,story,work,zwerg}`, `gameplay`, `init`, `misc`, `sequences/{Kristall,Lava,Menue,Oberwelt,Schwefel,Urwald,Trailer,Test,soundmarker}`, `text/{doc,info,multiplayer,sequences,talk}` |
| `Templates/` (9136 plików) | Klocki map (sekcja 7.4) |
| `Gamesave/` | Definicje scenariuszy jako `.tcl` (`single_Campaign*.tcl`, `sandbox_*.tcl`, `multi_*.tcl`, `single_Tutorial.tcl`…) oraz zapis `save_SaveGame.sav` |
| `BlockDB0/1/2.BDB` (20/26/37 MB) | ✅ 118 688 kształtów bloków terenu w 3 poziomach szczegółów ([formaty/blockdb.md](formaty/blockdb.md)) |
| `SPHERE.DAT` | 3072 B = 256 × vec3 (sekcja 2) |
| `ALPHABET.BIN` | Kształty wektorowe (`Triangle`, `Rect`… z listami punktów f32). 🔶 Kształty znaczników lub kursorów |
| `LOCALE.TXT` | Kod języka (`de` w tej instalacji, mimo polskich tekstów) |
| `MESSAGES.TXT` | Tłumaczenia `lmsg`: `## "Klucz"`, a potem linie `pl "…"`, `en "…"`, `de "…"` |
| `QUOTES.TXT` | Cytaty przy ładowaniu: `lang<TAB>autor<TAB>tekst` |
| `texturelist.txt` | Lista tekstur ładowanych na starcie |
| `Music/` (136 MB) | MP3 (44,1 kHz i 22 kHz) |
| `Sound/` (1299 plików) | `InGame`, `Sprache`, `Sequenzen` (MP3 dialogów) oraz katalogi postaci (`Baby`, `Drache`, `Elfe`, `Troll`, `Wuker`…), `Produktion`, `Specials`. Efekty to **WAV PCM 8-bit mono 22 kHz** |
| `Start.avi` | Intro |

### 7.3 Tekstury: poziomy jakości (`texlevels.txt`)
Każda linia: nazwa tekstury oraz rozdzielczość dla poziomów Minimal, Low, High i Maximal (0=16, 1=32, 2=64, 3=128, 4=256 px, katalogi `mNNN`). Gdy brak danej wersji, kolejność szukania jest określona w nagłówku pliku (np. low: 32 → 64 → 16 → 128 → 256). Nasz silnik może po prostu brać największą dostępną.

### 7.4 Mapy: system szablonów ✅
Opis: [rozdział 3 przewodnika](silnik/03_swiat_z_szablonow.md), formaty: [formaty/szablony.md](formaty/szablony.md). `split/` zawiera warianty szablonów pocięte na ćwiartki (`_tAA`, `_tAB`, `_tBA`, `_tBB`) 🔶 (prawdopodobnie dla generatora poziomów).

### 7.5 Zapisy gry
`.sav` to **gzip**. Po rozpakowaniu jest binarka (~7 MB) zaczynająca się od `V1.0.844`. ✅ Część z mapą (kamera, pola 53 B, linie nieba/gruntu, mgła, obiekty z pozycją i obrotem) odczytana według Diggles-Unity i sprawdzona na 3 zapisach z laboratorium (2 szybkie zapisy menu 64×64, 96 obiektów; OBERWELT 500×200, 1381 obiektów). Mapowanie rodzaju gruntu na typ `.pmp` (0→4, 1→3, 2→2, 3→5) ustalone porównaniem pól zapisu z szablonami. Opis: [formaty/sav.md](formaty/sav.md).

### 7.6 Natywne komendy: wyniki skanera
[`tools/scan_tcl_natives.py`](../tools/scan_tcl_natives.py) przeszedł 3961 plików `.tcl` (skrypty i szablony) i znalazł 2301 procedur lub metod zdefiniowanych w skryptach oraz około 830 kandydatów na komendy natywne. Pełna lista jest w [`docs/tcl_natywne_kandydaci.txt`](tcl_natywne_kandydaci.txt) i jest zaszumiona: zawiera też słowa ze stringów i list.

Najczęstsze komendy, czyli pierwsze do zaimplementowania: `new`, `sel` (np. `sel /obj`, wybór kontekstu tworzenia), `set_physic`, `set_owner`, `set_boxed`, `set_autolight`, `loading_objdetail`, `call_method`, `sound`, `map_setlayer2`, `map_template`, `call`, `set_class_anim`, `smalltalk`, `tasklist_add`, `textwin`, `log`, `get_pos`, `set_anim`, `def_class`, `obj_query`, `set_particlesource`, `adaptive_sound`, `layout`, `obj_init`, `def_event`, `handle_event`, `timer_event`, `set_hoverable`, `vector_add`, `class_defaultanim`, `get_ref`, `state_enable/disable`, `random`, `set_pos`, `get_attrib/set_attrib`, `link_obj`, `set_visibility`, `lmsg`, `del`, `obj_valid`, `irandom`, `set_roty`, `set_textureanimation`, `set_collision`, `walk`, `start_fade`, `get_mapedit`, `tasklist_clear`.

---

## 8. Laboratorium: obserwacja działającej gry ✅

Oryginał działa pod Wine 11 (Direct3D 7 przez wined3d). Narzędzia są w [`tools/lab/`](../tools/lab):
- `setup_lab.sh` kopiuje grę do `../lab/Wiggles`, tworzy **osobny prefiks Wine** (`../lab/prefix`, gra widziana jako `C:\Wiggles`), ustawia rejestr na okno 1024×768 i wstrzykuje sondę do `systeminit.tcl`. Oryginalna instalacja nie jest ruszana.
- `run_lab.sh [sekundy] [parametry]` uruchamia grę z `-logerrors` i zamyka ją po czasie.
- `owprobe.tcl` to sonda w czystym Tcl 8.3. Nie potrzebuje barnacle: zrzuca `info commands`, procedury i zmienne globalne, przechwytuje `log` i owija komendy natywne przez `rename`.

Pliki wynikowe (w `../lab/Wiggles/`, poza repo, bo zawierają treść skryptów gry):

| Plik | Zawartość |
|---|---|
| `owprobe_start.txt`, `owprobe_end.txt` | wersja Tcl, komendy natywne, procedury, zmienne globalne na początku i na końcu `systeminit.tcl` |
| `owprobe_log.txt` | przechwycone `log` |
| `owprobe_trace.txt` | pierwsze 30 wywołań każdej komendy natywnej: argumenty i wynik |
| `owprobe_counts.txt` | liczniki wywołań |
| `tclerrors.txt` | **generowany przez grę** przy błędzie Tcl: komunikat i stos, a do tego cały skrypt **po preprocesorze** |

### 8.1 Ustalenia
- ✅ Interpreter: **Tcl 8.3.2**.
- ✅ **639 komend natywnych** (poza rdzeniem Tcl). Dokładna lista z licznikami jest w [`natywne_komendy_gry.txt`](natywne_komendy_gry.txt) i zastępuje szacunki skanera.
- ✅ Preprocesor działa przed Tcl: `tclerrors.txt` pokazuje skrypt bez komentarzy `//`, z rozwiązanymi `//# IF FULL` i liniami zachowanymi jako puste, więc numeracja się zgadza.
- ✅ Klasy C++ silnika (`ClassList`): `CObj`, `CTclRootObj`, `CObjMgr`, `CObj_World`, `CObj_DigTask`, a dalej klasy ze skryptów (`Kristallbrut`, `Drache`, `Fenris`…). Do introspekcji służą też `ClassID`, `ClassInfo`, `ClassName`.
- ✅ Kolejność startu jest potwierdzona logiem: `parse systeminit script`, `Class init`, `Adaptive Sound init`, `42 Attributkombinationen zum Erfinden nötig!` (techtree), `systeminit script succesfully parsed`.
- ✅ Na starcie, w głównym interpreterze, wywoływanych jest tylko 61 różnych komend. Najczęstsze: `sound` (3870), `call_method_static` (2836), `def_class` (2685), `has_cmap` (1826), `smalltalk` (898), `db_load` (656), `call` (417).
- ✅ Ciała klas i metody obiektów (`obj_init`, `method`, `tasklist_*`) nie przechodzą przez nasze owijki, bo wykonują się w **osobnych interpreterach, po jednym na obiekt** (sprawdzone przez `obj_eval`, §8.7).
- ✅ Gra sama zapisuje pliki: `Data/texturelist.txt` (tekstury wczytane na starcie), `Data/msgmisses.txt` (brakujące tłumaczenia `lmsg`), `Data/Scripts/gameplay/gen_tt.tcl` i `gen_exp.lst` (wygenerowane drzewko technologii), a także `Data/Gamesave/quick_Quicksave_<data>.sav`.

### 8.2 Sygnatury z trace (przykłady)
```
db_load mann                                   ładuje 3db/MANN.3DB po nazwie
db_animlength urw_sumpf.standard => 21         animacje adresowane jako <obiekt>.<animacja>, długość w klatkach
has_cmap muetze_a.standard => 0                czy obiekt ma cmap (patrz .3db)
new Dummy_Moos_uh_e {} {34.58 42.66 2.02} {0 0.2 0} => 2     klasa, nazwa, pozycja xyz, rotacja xyz; zwraca ID (int)
sel /obj                                       kontekst tworzenia obiektów
map create 64 64 {  }
map_template data/templates/gng_021.pmp 21 40  ścieżka znormalizowana do data/templates/
map_setlayer2 21 40 data/templates/urw_gng_021_a.l2m
set_view 32.368 40.858 1.38 -0.355 0.165       x y zoom + 2 kąty kamery
call_method 74 Editor_Set_Info {{xrange 10} {yrange 6} {on 1}}
call_method_static Grillpilz get_toolclasses => grillpilz
def_attrib Pilz 0 10000 0                      nazwa min max domyślna
def_texture ZFrau_Haare ZFrau_Haare.tga Zfrau_haar_black.tga …   nazwa bazowa i warianty
sound bind mp_chat_1 data/sound/sprache/m_ac_neg_a1.wav
adaptive_sound setintrostream atmooberwelt data/music/na002.mp3
smalltalk add ooi {15 7 0.0 50.0 0.0 0.0 1.0 "…"}
lg_set_templategroup Urwald.Std.Gng {urw_gng_001_a.tcl …}   generator poziomów: grupy szablonów
lg_set_templateprops urw_gng_050_a.tcl -gamecount 1 -levelcount 1
fight_action ZIdl1 Zwerg kungfu idle n n 0 kungfu_standanim 0
fight_weapon Steinschleuder 0.3 0.0 6.0 ballist exp_F_Ballistic 2 0 katschi { 13 }
def_hp_ratio Zwerg 0 2.0 0.15
ai init 0 data/scripts/ai/std_ai.tcl
init_techtree scripts/gameplay/gen_tt.tcl
```

### 8.3 Rejestr gry (pełny, z instalacji)
Klucz to `HKCU\Software\SekOst\Wiggles` (**nie** `Diggles`, jak podaje forum dla wersji angielskiej):
- główny: `Language`, `TCLDEFINES` (🔶 dodatkowe symbole preprocesora!), `WorkingDir`, `KeyLocation`
- `Misc`: `AnimStats`, `PerfStats`, `GameSaveCompression=7` (poziom gzip), `UseTclClassCache`, `ShowFMV`, `ShowQuotes`, `MaxGametimeFactor`, `AutoDigRange`, `AutoHarvestRange`…
- `Performance`: `AnimInterpolation` (interpolacja klatek animacji!), `LandscapeQuality`, `TextureQuality`, `ObjectQuality`, `MaxFramerate` (✅ 19: obraz ok. 20 kl./s, czas gry rośnie raz na klatkę, [rozdział 11.7](silnik/11_obiekty_tcl_i_petla.md)), `UseEnvMapping`, `MultiTexture`, `ScapeJitter`, `ValidateRenderJobs`, **`BorderMask`** (✅ czarna ramka ekranu z `Data/GUI/bordermask.tga`, [rozdział 9.4](silnik/09_klatka.md)), **`WebServer`** (✅ wbudowany serwer WebTraq na porcie 8082, §8.6), a także `LightGlow`, `EvtFrameSkip`, `LazyTextureLoad`, `MaxShadowLightDist`, `Particles`, `UseIdiObjs`, `TexVideoMemLimit`, `Rating*`
- `Screen`: `Fullscreen`, `Width`, `Height`, `Gamma`, `Brightness`, `TnL`, GUID-y Direct3D/DirectDraw
- `Control`, `Input`: mapowanie myszy i klawiszy; `Sound`: głośności

### 8.4 Komendy debugowe do zbadania
`tclhelp` (✅ zwraca pusty wynik także przez WebTraq i z `-console`: pomocy w tej wersji nie ma), `web_shell` (✅ przyjmuje 1 argument i nic widocznego nie robi; serwer WWW włącza rejestr, §8.6), `dump_var`, `proddump`, `output_3ddbstats`, `meminfo`, `perfwin`, `peek`/`poke`, `waterdbg`, `toggle_wf` (wireframe?), `render_gray`, `show_maped`, `save_map`, `save_scene`, `xtest`, `test`/`testd`, `vtune`.

### 8.5 Metoda: obserwacja, nie deasemblacja
Nie analizujemy kodu `Wiggles.exe`. Zachowanie silnika ustalamy, obserwując działającą grę: sonda Tcl (`get_*`, `save_map`, `save_scene`, `tclerrors.txt`), zrzuty ekranu z [`tools/lab/capture.sh`](../tools/lab/capture.sh) przy zmienianych parametrach (np. różne `set_view`, `set_ground_begin`) i porównania z naszym renderem. Wiedzę o wnętrzu exe bierzemy wyłącznie z cudzych opracowań (głównie Diggles-Unity) i zawsze sprawdzamy ją takim eksperymentem.

**Przepis na czysty eksperyment** (sprawdzony przy UV terenu, przejściach gruntów i detalach):
1. W kopii laboratoryjnej utwórz szablon testowy (`Data/Templates/ow_*.pmp`, `.tcl`, ewentualnie `.l2m`) skryptem w Pythonie. Najczytelniejsza jest **płaska ściana z samych bloków 59** (wykopane do dna, z = 7, UV = (x, y)), na której układa się badane wartości na osobnych blokach.
2. Dodaj scenariusz `Data/Gamesave/ow_*.tcl`: `map create 64 64`, `set_light_begin 1000` (równe światło dzienne zamiast lamp), `call` szablonu, `MapTemplateSet 16 16`, `set_view … 0 0` (kamera na wprost).
3. Zrzut z gry: `tools/lab/capture.sh scene <plik> <scena.tcl> 60`, gdzie scena woła ten scenariusz i tworzy `FogRemover` (`fog_remove 0 50 50` to promienie, które nie odkryją litej skały wokół; `fog_remove 0 -50 -50` to prostokąt przez skałę).
4. Nasz render: `wigview --game ../lab/Wiggles --scenario ow_* --size 1024x768 --screenshot …`, zestawienie `magick … +append`.
5. Wnioski ilościowe: mierz kolor w regionach zrzutu (np. zieloność ćwiartek bloku). Uwaga na skalę: współrzędne bierz z pełnej rozdzielczości, nie z pomniejszonego zestawienia.

**Płyta CD.** Gra wymaga płyty w napędzie (komunikat „CD not found.”). Po restarcie komputera płytę trzeba zamontować: `udisksctl mount -b /dev/sr0` (prefiks laboratorium ma `d::` → `/dev/sr0`). Płyta potrafi się odmontować w trakcie serii uruchomień; przed eksperymentem sprawdź `findmnt /dev/sr0` (`live.sh start` robi to sam), bo bez płyty gra kończy się po cichu i wyniki są puste.

### 8.6 WebTraq: wbudowany serwer twórców ✅

Wartość rejestru `HKCU\Software\SekOst\Wiggles\Performance\WebServer=1` uruchamia w grze serwer HTTP **„SEK WebTraq”** na porcie **8082** (odpowiada ok. 15 s po starcie; wersja na stronie: `V 1.0.844r Build 59495 (7.11.2001 15:36)`). Strony:

| Adres | Zawartość |
|---|---|
| `/index` | system, pamięć, **data gry**, statystyki klatek (klatki logiki, klatki obrazu, średnie FPS) |
| `/objlist`, `/objinfo:<nazwa>` | wszystkie obiekty; karta obiektu: klasa, pozycja, obrót, stan, właściciel, flagi (`CanClimb`, `AlternateAnimDB`…), ekwipunek, lista zadań |
| `/classlist`, `/classinfo:<klasa>`, `/classinfomethod:<klasa>:<metoda>` | klasy: id, metody (z treścią), stany, instancje |
| `/tclshell?script=…` | **wykonanie Tcl w głównym interpreterze działającej gry** (wynik: `Ok.` albo `Error:` ze stosem). ✅ Serwer ucina skrypt na pierwszym znaku `:` (także zakodowanym w adresie), więc w skryptach nie używamy `::`; skrypt działa na poziomie globalnym, więc wystarczą zwykłe nazwy zmiennych |
| `/tclcommand` | lista komend: w tej wersji pusta |
| `/screenshot`, `/autoscreenshot`, `/jumpview:<x>:<y>` | zrzuty i skok kamery (nie sprawdzane) |

Niezależnie od rejestru gra zawsze nasłuchuje na TCP **12345** (nie odpowiada po HTTP; 🔶 gra sieciowa). Parametr `-console` i komenda `web_shell` nie otwierają portów.

**Narzędzie:** [`tools/lab/live.sh`](../tools/lab/live.sh) uruchamia grę z laboratorium w tle (Xvfb) z włączonym WebTraq i daje dostęp bez restartów:
```
tools/lab/live.sh start [scena.tcl]          # scena jako userstartup.tcl (np. wczytanie zapisu przez gameload)
tools/lab/live.sh tcl 'list [gettime] [gethours]'
tools/lab/live.sh page /objinfo:Ole
tools/lab/live.sh shot zrzut.png
tools/lab/live.sh stop
```
To zastępuje wstrzykiwanie sond przez `userstartup.tcl` przy pytaniach o stan gry.

**Przeglądanie szablonów w oryginale** (instrukcja: [tools/lab/README.md](../tools/lab/README.md)): [`tools/lab/live2.sh`](../tools/lab/live2.sh) uruchamia grę w widocznym oknie i podmienia szablony bez restartu (`next lava_unq` przechodzi po kolejnych szablonach pasujących do filtra). ✅ Ustalone przy tym: `map create W H` w działającej grze czyści całą mapę razem ze statycznymi dekoracjami, a obiekty Tcl (`obj_query 0 -limit 100000`) trzeba usunąć osobno (`del`); `fog_remove 0 -N -N` odsłania prostokąt ±N pól przez skałę, a dodatnie wartości to promienie zatrzymywane w skale, więc ich zwiększanie niewiele daje (rozdział 10.4); strona WebTraq `/screenshot` odsyła do `file/screenshot.jpg`, którego serwer nie wydaje. ✅ Wine 11 (WoW64) rysuje Direct3D 7 gry przez OpenGL na **EGL** (`win32u.so` ładuje `libEGL` przez `dlopen` i pobiera funkcje przez `dlsym`/`eglGetProcAddress`; `libGL`/GLX nie jest ładowane), więc zrzut z bufora klatki robi biblioteka [`glgrab.c`](../tools/lab/glgrab.c) przechwytująca `eglSwapBuffers` przez `LD_PRELOAD`; działa przy schowanym oknie. `WINE_D3D_CONFIG=renderer=vulkan` w prefiksie laboratorium daje biały ekran i zawieszenie (w wersji GOG działa), więc warstwa Vulkana `VK_LAYER_LUNARG_screenshot` odpada. Barnacle (SDK do wstrzykiwania kodu do exe) nie jest potrzebny.

### 8.7 Faza 0: sondy głębokie (2026-10-05)

Sondy: [`tools/lab/owprobe_deep.tcl`](../tools/lab/owprobe_deep.tcl) (świat z jednego szablonu; klasy, bazy modeli, interpretery obiektów, takt i timery) i [`tools/lab/owprobe_play.tcl`](../tools/lab/owprobe_play.tcl) (wczytanie zapisu przez `gameload` i 90 s obserwacji krasnali). Uruchomienie: `tools/lab/capture.sh scene zrzut.png <sonda> 130`. Wyniki w `../lab/Wiggles/owdeep_*.txt` i `owplay_*.txt` (poza repo).

**Klasy** (`ClassList`, `ClassInfo`, `get_class_*`): 2836 klas, w tym 5 klas C++ silnika (`CObj`; `CTclRootObj`, `CObjMgr` z bazą `CObj`; `CObj_World`; `CObj_DigTask`). Typy: `dummy` 2398, `tool` 140, `material` 91, `info` 91, `production` 48, `monster` 32, `protection` 11, `transport` 8, `elevator` 6, `gnome` 5, `energy` 4, `baby` 1. Kategorie (poza `none`): `metal` 110, `wood` 50, `food` 42, `stone` 33, `fight` 17, `service` 16, `energy` 16. Ery 0–4 (99/56/26/13 klas w erach 1–4). Flagi występują tylko u postaci: `moves`, `lives`, `reproduces` (`Zwerg` ma wszystkie trzy). `ClassInfo` podaje tylko klasę bazową i liczbę instancji. Wartości są takie same jak w nagłówku `def_class <nazwa> <kategoria> <typ> <era> {<flagi>}`.

**Bazy modeli**: [rozdział 5.1](silnik/05_obiekty.md). Długości wszystkich 6445 animacji zgodne z naszym parserem (poza kolizją nazwy `wuerfel`), numery animacji zgodne z kolejnością w pliku, przy powtórzonej nazwie obiektu wygrywa pierwsza wczytana baza.

**Interpretery obiektów**: [rozdział 11.1](silnik/11_obiekty_tcl_i_petla.md). Osobny interpreter na obiekt, bez trybu bezpiecznego, metody jako procedury, bez zmiennych głównego interpretera.

**Takt i czas**: [rozdział 11.7](silnik/11_obiekty_tcl_i_petla.md). Logika 10 Hz (ciało stanu krasnala co 0,1 s), czas gry raz na klatkę (ok. 20 kl./s), takt `exec_deferred` ok. 30 Hz; timery klasy z `userstartup.tcl` działają; doba = 12 godzin gry po 150 s = 30 min ([rozdział 11.4](silnik/11_obiekty_tcl_i_petla.md)).

**Rozgrywka** (zapis kampanii, 5 krasnali, 90 s): trzy krasnale przechodzą cyklicznie `work_idle` ⇄ `prodfill_dispatch` (napełnianie produkcji) co 10–20 s, dwa są cały czas w `sparetime_dispatch` (czas wolny). Najczęstsze polecenia na liście zadań: `sparetime_idle_loop`, `play_anim`, `prod_anim`, `unlock_item`, `add_attrib`, `adjust_gnome_rotation`, `walk_around`. Komendy wołane stale 2 razy na sekundę przez każdego krasnala: `isunderwater`, `net`, `vector_add` (180 w 90 s; 🔶 cykliczny timer sprawdzający wodę), `get_linkpos` 1 raz na sekundę. W świecie z samego szablonu krasnal nie może chodzić (`get_walkresult` = 3), więc do obserwacji zachowań potrzebny jest zapis. Obiekty z wczytanego zapisu dostają nowe, małe numery (4–8), a nie numery z pliku `.sav`.

**Preprocesor**: [rozdział 2.2](silnik/02_start_i_skrypty.md). `FULL` zdefiniowany, `IF` = `IFDEF`, `IFNOT` = `IFNDEF`; `TCLDEFINES` z samą nazwą symbolu nie działa.

**Ramka ekranu** (dawne „winieta”): [rozdział 9.4](silnik/09_klatka.md). Zrzuty menu i zapisu kampanii z `BorderMask` 1/0 oraz `LightGlow` 1/0: różnią się tylko pikselami 0–10 od krawędzi; `LightGlow` nie zmienia tych scen.


### 8.8 Faza 2: formaty pomocnicze i nieznane pola `.3db` (2026-10-05)

- `lmsg`/`locale` w działającej grze (WebTraq): dopasowanie dokładne, brak hasła = klucz + wpis w `msgmisses.txt`, `locale en` przełącza teksty, bez `Language` w rejestrze `locale` jest puste; `LOCALE.TXT` (`de`) nie jest źródłem języka ([teksty.md](formaty/teksty.md)).
- `TEXMAPS.BIN`: układ potwierdzony co do bajta i pikseli; zrzuty menu z paczką i bez niej się różnią (bez niej tekstury rozmyte), więc gra rysuje z paczki ARGB4444. `TextureQuality` 0/2/4 nie zmienia menu ([tekstury.md](formaty/tekstury.md)).
- `.3db` (statystyki wszystkich 43 modeli): `unknown30` = prostokąt cienia (1:1 z `shadowIndex`), `cmap` = maska 0/1 w kratkach 1/8 bloku z przesunięciem `i16`, rekord zaczepienia ma kąty Eulera (bajty 8–19), `move`/`rotate` na klatkę ([3db.md](formaty/3db.md)).
- Rozkaz `set_event <krasnal> evt_task_walk -target <krasnal> -pos1 {x y z} -text1 userevent` działa: krasnal przeszedł 5 bloków (stan `task`, potem `work_idle`), prędkość 0,85 bloku/s na rozbiegu i 1,59 bloku/s w biegu (zgodnie z `move` animacji `gehen` 0,81 i `laufen_schnell` 1,71).

### 8.9 Flagi linku `.3db`: eksperyment w laboratorium (2026-10-05)

Metoda: w kopii `Data/3db/flusslandschaft.3db` w laboratorium podmieniona flaga obu klatek łąki `wiese_rund_a` (offsety 83 647 i 83 865, drugie słowo rekordu linku), scena: szablon `urw_gng_021_a`, łąka `Dummy_Wiese_rund` na środku, `set_view 18 18 1.2 -0.25 0`, z lampami i bez. Wyniki (jasność i maks. G w obszarze trawy): 1 → 41/154, 0 → trawa znika, 2 → 60/255 (addytywnie), 3 → 38/154 (miękko przejrzysta), 4 → 40/154 (jak 1), 8 → 45/198, 9 → 35/89. Bez lamp: 1 → maks. G 57, 8 → 198 (bez oświetlenia). Z teksturą `Character_Drache_b_256` podstawioną za mech: 1 i 4 mają ten sam układ wzoru, 5 inny (mapowanie otoczenia). Statystyki: flaga nie zależy od alfy tekstury; Diggles-Unity flag nie używa (rodzaj rysowania zgaduje z alfy TGA). Opis: [3db.md](formaty/3db.md) „Tryb rysowania linku”. Plik w laboratorium przywrócony.

### 8.10 Tryby 4, 6, 7, 9, 11 i alfa trybu 8: kolumny lawy (2026-10-06)

Powód: w wigview w szablonie `lava_unq_vampy_tuer` nie było trzonów kolumn lawy (`Dummy_Lava_krst_*`, tylko misy i strużki). Kolumny mają wszystkie linki w trybie 8, a tekstura `lava_seulen_128` ma alfę < 0,5 na 79% powierzchni (biała u góry, gdzie lawa, i w plamkach; jest też wariant `_noalpha`). Nasz shader wycinał alfą < 0,5 także w trybie 8.

Metoda: [`tools/lab/set_link_flags.py`](../tools/lab/set_link_flags.py) ustawia wszystkim 170 linkom obiektów `lava_krst_*` w kopii `LAVAWELT.3DB` jeden tryb (oryginał w `.orig`, potem `--restore`); dla każdego trybu restart gry, szablon `lava_unq_vampy_tuer`, `set_view 24 18 1.8 -0.25 0.1`, zrzut z bufora klatki. Wyniki:
- 1 i 4: identyczne; trzony wycięte alfą (zostają lawa u góry i plamki), całość ciemna (oświetlona lampami szablonu). Dokładnie to, co rysował wigview.
- 8 (oryginał): pełne trzony, nieprzezroczyste; lawa i plamki jasne, reszta ciemna skała.
- 9: pełne trzony, nieprzezroczyste i ciemne (oświetlone).
- 11: pełne trzony, nieprzezroczyste i jasne, z wyraźnym wzorem skały w dolnej części tekstury (bez oświetlenia i bez maski).
- 6, 7: słabo widoczne, półprzezroczyste; 7 ma inny układ wzoru niż 6.
- 2: tylko jasne części (addytywnie, ważone alfą).

Wniosek: ✅ 8, 9 i 11 nie wycinają alfą; ✅ 4 = 1; 🔶 w 8 alfa działa jak maska samoświecenia (porównanie 8 i 11). Zrzut trybu 3 nieudany (kamera odjechała), tryb znany z §8.9. Plik w laboratorium przywrócony. Uwaga do metody: widoczne okno `live2.sh` przejmuje klawiaturę użytkownika; serie zrzutów robić w Xvfb (`live.sh`).
---

## 9. Mapy i teren: przebieg badań

### 9.1 Ustalenia z działającej gry ✅
- `get_map_width`/`get_map_height` dla `map create 64 64` zwracają **512**, czyli 8 jednostek mapy na blok szablonu. Przesunięcia w `MapTemplateSet x y` są w blokach.
- **Teren to mapa wysokości** w płaszczyźnie (x, y) ekranu, a wysokość oznacza głębokość wydrążenia w głąb ekranu (0 = lita skała na przedniej płaszczyźnie). Gra 2,5D.
- `save_map {x y w h} plik` zapisuje **Wavefront OBJ** „obj outputfile for template”: zgrubną mapę wysokości szablonów (blok = 200 jednostek, siatka co 50 lub 25, głębokości w krokach 100; korytarz `GNG_001` ma głębokość 400). To format edytora map i najlepszy wzorzec do weryfikacji dekodowania.
- `save_scene plik` zapisuje VRML 2.0 („Wiggles VRML97 exporter”), a `hmap` zapisuje `hmap.raw` 512×512 u8 (🔶 nie jest to prosta głębokość: dominuje 240, w jaskiniach szum).
- `data/userstartup.tcl` zastępuje menu startowe, co pozwala ustawić dowolne szablony i zrzucić wzorce ([`tools/lab/userstartup_templates.tcl`](../tools/lab/userstartup_templates.tcl)).
- Exe: `BlockDB ILT load`, `BlockDB mesh load %d Blocks`, `Scape texturizer (TM)`, shadery `scape_solid`, `scape_blend`, `scape_fow`, `scape_urw_mt0/1`, `scape_lav_mt0/1`. `LandScapeQuality [value] (2)` wybiera plik `BlockDB0/1/2`.

### 9.2 Jak rozpracowaliśmy `.PMP` i BlockDB
- Struktura `.PMP`: rozmiar pliku zgadza się co do bajta dla wszystkich szablonów przy układzie typ / blok / maska / warstwa 2 ([formaty/szablony.md](formaty/szablony.md)).
- BlockDB: na początku znaleźliśmy tylko rekordy 89 B (siatka 8×8 = głębokość, zgodna z OBJ z `save_map`). Pełny układ (komórki, indeksy, deskryptory, paleta, dane podsiatek) pochodzi z Diggles-Unity i sprawdziliśmy go sumą rozmiarów sekcji ([formaty/blockdb.md](formaty/blockdb.md)).
- **Typ pola** ustaliliśmy eksperymentem: korytarz `GNG_021` z podmienionymi typami pól w `userstartup.tcl` i zrzuty z gry (`../lab/zrzuty/gra/typ_*.png`). Diggles-Unity zgadywało mapowanie typ → tekstura; nasze pochodzi z obrazu. Pierwszy wniosek „typ 0 = niewykopany tunel” obalił szablon z samymi zerami, który okazał się w całości przezroczysty.
- **Głębokość obiektów** `z · 0,5 + 7`: bez niej scena menu nie zgadzała się ze zrzutem; z nią zgadza się 1:1.

### 9.3 Powierzchnia (OBERWELT): ustalenia z laboratorium ✅
Seria zrzutów `tools/lab/capture.sh scene` z jednym zmienianym parametrem, porównana z renderem wigview (`--view x y zoom dx dy`, przeszukiwanie parametrów po RMSE pomniejszonych obrazów). Opis praw: rozdziały [6](silnik/06_kamera.md), [7](silnik/07_oswietlenie.md) i [8](silnik/08_tlo.md) przewodnika:
- `set_light_begin -10000` gasi panoramę i ok. 4× przyciemnia obiekty powierzchni, więc niebo daje światło bazowe i włącza panoramę.
- `set_ground_begin` przesuwa dywan łąki. W danych gry nie występuje, domyślnie zachowanie odpowiada `set_light_begin`.
- Gra przycina kąty `set_view` przy dużym zoomie (2,5 → ±0,15). `get_view 1` zwraca wartości zadane, a `get_view` bez argumentu zaraz po starcie zwraca śmieci (kamera jeszcze niezainicjowana).
- Strefa horyzontu działa tylko w zasięgu `size` obiektu `Info_Fog_*` od ogniska kamery.
- Modele wyglądają na oświetlane dwustronnie (drzewa z normalnymi do środka są jasne).
- 🔶 Własna klasa zdefiniowana w `userstartup.tcl` (`def_class` + `timer_event`) tworzy się (`new` zwraca uchwyt), ale jej zdarzenie timera się nie wywołało. Do zbadania, jeśli będą potrzebne odczyty w trakcie gry.

### 9.4 Cząsteczki: kalibracja płomieni (2026-10-04)
- Zrzut gry: `tools/lab/capture.sh template ../lab/zrzuty/czasteczki/kris_unq_060_gra.png kris_unq_060 24.5 21 1.0 0 0 60`; nasz: `wigview --template kris_unq_060 --view 8.5 5 1.0 0 0`.
- Parametry typów 2 i 3 z Diggles-Unity dawały płomień ok. 3,5× za szeroki i czerwony. Po zmianie skali rozmiaru (0,25 → 0,12), koloru (barwa z atlasu, alfa 1 − t²), animacji (klatka wg t²) i rozrzutu prędkości płomień kielicha zgadza się ze zrzutem. Szczegóły: [rozdział 12 §12.6](silnik/12_czasteczki.md).
- Uwagi poboczne: gra rysuje w tym szablonie wodę (u nas brak), a płomień lamp `Dummy_Haengelicht` (typ 3) u nas nie jest widoczny.

### 9.5 Płyny: zapis z wodą (2026-10-04)
- W kopii gry w laboratorium dopisaliśmy do klasy `Wasser` (`Scripts/classes/items/WASSER.TCL`) zdarzenie, które 40 s po starcie źródła woła `gamesave ow_woda`; potem przywróciliśmy oryginał. `capture.sh template … kris_unq_060 24.5 21 1.0 0 0 75`. Gra zapisała plik `ow_woda` (bez rozszerzenia) w katalogu gry; kopia w `../lab/zapisy/ow_woda.sav`.
- Z porównania bajtów: rekord pola zapisu ma płyn na przesunięciach 19–44 (typ ×2, wypełnienie f32 ×2, przepływ f32 ×4). Szczegóły: [formaty/sav.md](formaty/sav.md).
- Deskryptor płynu w BlockDB (ostatnie 4 bajty rekordu 89 B) zgadza się z opisem Diggles-Unity.
- Ten zapis ma tylko 18 obiektów na liście; dekoracje szablonu są w „nasionach” kubełków przestrzennych (odczytane, [formaty/sav.md](formaty/sav.md)).
- ✅ Symulacja przepływu ([`src/lib/fluid_sim.cpp`](../src/lib/fluid_sim.cpp)) po 410 taktach od startu daje w basenie te same wypełnienia co ten zapis (do setnej części; rozdział 13 §13.5). Porównanie: mały program z `buildWorld` (szablon w 16,16), `FluidSim`, `parseSaveGame`.

### 9.6 Zapis łatą wczytany przez grę (2026-10-04)
- `openwiggles --save save_kampania.sav --no-scripts --save-to x.sav`: po rozpakowaniu `x.sav` jest bajt w bajt równy oryginałowi.
- Po `--warmup 20` (skrypty, krasnale chodzą) różni się 168 bajtami (czas, pozycje, obroty).
- Wczytanie w grze: `capture.sh scene out.png load.tcl 120`, gdzie `load.tcl` woła `gameload Data/Gamesave/save_owtest.sav`. Sama nazwa (`gameload owtest`) zwraca 0, ale nic nie wczytuje (czarny ekran); ścieżka względem katalogu gry działa (kontrola: `save_SaveGame.sav`). ✅ Gra wczytała nasz zapis: krasnale w nowych miejscach, reszta stanu (beczki, przedmioty) bez zmian.

### 9.7 Cień i `cmap` w `.3db`: dopasowanie do bryły (2026-10-05)
Pytanie: w jakiej płaszczyźnie leżą `cmap` i obraz cienia oraz jak powstały. Metoda: trójkąty klatki próbkowane gęsto (siatka barycentryczna 8×8), rzut na kandydującą płaszczyznę, zbiór kratek porównany z maską przez IoU (część wspólna / suma), średnia po klatkach. Program jednorazowy na `libwiggles` (`parseModel3db`, `Model3db::dequant`).
- **`cmap`**, 1799 klatek (co 11.) z `MANN`, `BRUT`, `FENRIR`, `produktionsstaetten`, `KRISTALL`, `TROLL`, kratka 25 jednostek: x/z 0,54, x/z z odbiciem z 0,22, x/y (y w górę) 0,34. Przesunięcie o kratkę i środek kratki zmieniają wynik o ±0,02. Rzut w masce 88%, maska w rzucie 62%: maska jest większa. Poszerzenie rzutu krzyżem o 1 kratkę: IoU 0,70 (maska w rzucie 86%), pełne 3×3 albo tylko w jednej osi: 0,66. Tylko część bryły przy ziemi (wysokość < 25/50/100) pasuje gorzej (0,29/0,35/0,44). Wniosek: ✅ płaszczyzna x/z, 🔶 rzut całej bryły poszerzony o kratkę (dokładna reguła ❓).
- **Cień**, 39 obrazów z 8 modeli (do 3 klatek na obraz), piksele > 40 na prostokącie `shadowQuad`: rzut (x, z + k·wysokość) daje IoU 0,70 dla k = 1, 0,65 dla 0,75, 0,58 dla 1,25; odbicie x 0,62. Wniosek: 🔶 rozmyty rzut światłem pod 45° od tyłu, cień pada ku widzowi.
- Podgląd obu: `wigview --model MANN.3DB --anim schlafen_boden_loop --shadow-cmap`.

### 9.8 Cienie w działającej grze (2026-10-05)
`live.sh start` z `gameload Data/Gamesave/save_kampania.sav`, kamera `set_view … 0.6 -0.2 0.2`, restart gry przy każdej zmianie `Performance\MaxShadowLightDist` (rejestr laboratorium). Zrzuty poza repo: `../lab/wyniki/cien_*.png`.
- **Powierzchnia** (krasnale, beczki, grzyby przy ścieżce, `set_view 245 30.5`): przy 0, 8 i 100 brak widocznej plamy pod obiektami; nieruchome beczki i grzyby identyczne. Dwa dodatkowe `Misc_LightSrc_a` obok krasnali nic nie zmieniły.
- **Jaskinia** (`set_view 240 42.5`, `FogRemover`, `Misc_LightSrc_a` w (242; 41,5; 13)): zrzuty przy 0 i 8 różnią się (poza poruszającymi się zwierzętami) **długimi ciemnymi smugami na podłodze**: od stworzeń po lewej w lewo, od tych po prawej w prawo, czyli od lampy. Przy 0 smug nie ma. 🔶 Cienie rzucane od lamp punktowych w zasięgu `MaxShadowLightDist`, mocno wydłużone przy nisko położonym świetle.
- Próba z nieruchomym grzybem (`new Pilz`) i lampą przestawianą `set_pos` na lewo i prawo: różnica to głównie oświetlenie ściany i podłogi, cień grzyba nieczytelny (kadr zasłonięty rośliną). ❓ Do powtórzenia na płaskiej, pustej podłodze z jednym obiektem.
- Przy okazji (§9.9): krasnal stojący przy `Misc_LightSrc_a` w jaskini ma pod sobą wyraźną ciemną plamę na podłodze, przesuniętą od lampy (`../lab/wyniki/cien_krasnal_lampa.png`).

### 9.9 Interpolacja animacji: próba metodą zrzutów, nierozstrzygnięta (2026-10-05)
Cel: czy gra przy `AnimInterpolation=1` rysuje pozy pośrednie między klatkami animacji (10 kl./s animacji, ok. 20 kl./s obrazu, więc z interpolacją co drugi obraz byłby pozą pośrednią). Metoda: nowy krasnal w jaskini zapisu kampanii (`new Zwerg`, lampa obok), wzorce `set_anim $G <anim> k 0` (`ANIM_STILL`, klatka k), potem `set_anim … 0 2` (pętla) i seria 40–60 zrzutów wycinka (`import`, ok. 0,24 s na zrzut); każdy zrzut przypisany do najbliższego wzorca (RMSE). Pułapki, przez które wynik jest nieważny:
- **AI krasnala** przesuwa go i zmienia animacje: `state_disable $G` (licznik `state_getenablecnt` = 0) pomaga, ale pierwsza próba zdążyła go przestawić; po wyłączeniu sprawdzić `get_pos` i dopiero wtedy ustalić kadr.
- **Animacje z polem `rotate` lub `move`** (`drehen_links`) w pętli obracają lub przesuwają cały obiekt, a w `ANIM_STILL` nie, więc wzorce nie pasują do pętli.
- **Pulsująca poświata** wokół nowego krasnala i migotanie lampy dają szum RMSE porównywalny z różnicą sąsiednich klatek (`lager_ablegen_hoch`: szum 8864, sąsiednie klatki 4775–8943); ta animacja wyprowadza też postać poza kadr.
Decyzja: pomiaru nie kontynuujemy, interpolacja zostaje włączona u nas niezależnie od oryginału. Gdyby kiedyś był potrzebny: obiekt bez AI i bez poświaty (np. dekoracja z animacją pętli albo krasnal z zapisu po `state_disable`), animacja bez `move`/`rotate` o dużym ruchu w miejscu, wzorce z naszego renderera zamiast z gry, porównanie serii przy `AnimInterpolation` 1 i 0.

### 9.10 Cząsteczki: wszystkie 43 typy w działającej grze (2026-10-05)
Metoda (`live.sh`, zapis kampanii, jaskinia `set_view 237,4 41,5 0,8 0 0`, lampa obok): nośnikiem jest nowy krasnal (`new Zwerg`, `state_disable`, przed każdym typem `set_pos` w to samo miejsce), a źródło ustawia `change_particlesource $G 1 <typ> {0 0 0} {0 0 0} 64 2 0 10; set_particlesource $G 1 1` (slot 1, punkt zaczepienia 10 = usta, jak w `Trigger_Tournament.tcl`). Zrzut wycinka 360×420 po 3 s. Pułapki: dekoracje `Dummy_*` (np. kielich ognia) po utworzeniu nie mają już obiektu Tcl (`invalid object`), więc zmiana ich źródła nic nie daje; błędy WebTraq trzeba sprawdzać (`Ok.`), a nie kierować do `/dev/null`; slot 0 krasnala też działa i jego źródło zostaje, dopóki go nie wyłączyć. Zrzuty: `../lab/zrzuty/czasteczki/typy/gra_<typ>.png`, nasze `wigview --particle <typ>` obok: `nasz_<typ>.png`.

Wygląd w grze (źródło ciągłe 64/2, więc typy jednorazowe dają większe chmury niż w skryptach); „nasz” to podgląd wigview w innej skali kamery, więc porównujemy charakter i rozmiar względem płomienia typu 2:

| typ | w grze | nasz |
|---|---|---|
| 0 | duża jasna biało-żółta kula żaru przy głowie | rzadkie drobne kropki (za małe) |
| 2 | płomień | ✅ zgodny (kalibracja §9.4) |
| 3 | duży ogień, języki do ok. 1,5 bloku w górę | węższy słup |
| 4 | małe białe „Z” | niewidoczne |
| 1, 5, 9, 10, 15, 16, 19, 23 | prawie nic w tym ustawieniu | drobne kropki albo nic |
| 6 | biały dym wokół postaci | małe kłęby |
| 7 | biała smuga spadająca w dół | brak |
| 8 | różowa plama na ziemi | brak |
| 11, 13, 14 | iskry i odłamki wokół postaci (11 szeroko) | drobne kropki |
| 12 | duży biały obłok (ponad 1 blok) | punkt |
| 17, 26 | brązowe wióry przy nogach | drobne |
| 18 | żółte iskry przy nogach | drobne |
| 20, 21 | biało-szary pył i mgiełka przy głowie | mały obłok |
| 22 | białe smugi w górę po bokach postaci | nikłe |
| 24 | białe płatki przy nogach | rozproszone płatki (podobne) |
| 25 | biała kreska (promień) | kreska (podobna) |
| 27 | pomarańczowa kula ognia, ok. 0,5 bloku | mała, podobna barwa |
| 28, 29 | niebieskie kule, fioletowo-biały słup dymu (~1 blok) | nikłe |
| 30–32 | fiolet, niebieski znak, biały błysk-gwiazda | brak |
| 33 | zielono-żółte iskry wokół | niebieskie kropki |
| 34 | ogromna biało-żółta kula ognia (~1,5 bloku) | mała |
| 35 | duży szary dym | mały biały |
| 36, 38 | niebieski, błękitny obłok „puf” (~1 blok) | kropka, brak |
| 37 | ognisty wybuch z czarnymi odłamkami | brak |
| 39 | turkusowy pierścień na cały kadr | brak |
| 40 | pomarańczowy wybuch na cały kadr | brak |
| 41 | duży biały błysk-gwiazda | brak |
| 42 | brązowa kula pyłu na cały kadr | mała |

Wniosek: poza płomieniami 2 i 27 oraz typami 24 i 25 nasze typy są za małe albo niewidoczne, najbardziej wybuchy i obłoki 33–42 (u nas rozmiarem jak płomień, w grze wielokrotnie większe).

**Kamera przy tej okazji:** `set_view x y 0.8 0 0` w grze dał kadr identyczny z naszym `--view x y 1.0 0 0`. Sprawdzenie przez `get_view` po `set_view` (WebTraq): 0,5 / 0,9 / 0,99 → 1,0; 1,2 → 1,2; 2,5 → 2,5; 3,0 / 4,0 / 6,0 → 2,5. Porównanie obrazów (RMSE skali szarości, 256×192): gra 0,5 najbliżej naszego 1,0 (5307 wobec 9873 dla 0,5), gra 1,2 najbliżej naszego 1,2 (3981). ✅ Zakres `zoom` w trakcie rozgrywki to [1,0; 2,5] ([rozdział 6.1](silnik/06_kamera.md)).

**Kalibracja w tej samej scenie.** `wigview --save save_kampania.sav --fow 0 --view 237.375 41.5 1.0 0 0 --emit <typ> 237.375 41.6 13 --size 1024x768`, wycinek jak w grze. Miara: piksele jaśniejsze o > 25 od tła (typ 5 nic nie rysuje, więc jego zrzut jest tłem), zasięg w blokach (percentyle 3–97, 202 piksele na blok przy `zoom` 1) i suma przyrostu jasności. Szum tła w grze (krasnal zmienia pozę) to ok. 150 jednostek jasności. Zmiany w [`particles.cpp`](../src/lib/particles.cpp): obłoki 36–40 oraz typy 7, 22, 41 emitują `naKlatkę` w każdej klatce (bez losowej bramki; gra przy źródle 64/2 dała gęste chmury), 32 bramka 0,4; rozmiary: 7 → 7, 12 → 10, 13 → 2,5, 14 → 6, 17 i 26 → 2,5, 18 → 2, 19 → 7, 22 → 9, 24 → 1,2, 28 → 2,5, 29 → 6, 30 → 2, 32 → 9, 33 → 7, 34 i 35 → 5, 36–38 → 13, 39 → 28, 40 → 20, 41 → 10, 42 → 20 (wcześniej 0,5–6,5). Stosunek jasności gra/nasz przed → po: 12: 75 → 0,8; 33: 27 → 1,1; 36–38: 19–101 → 0,8–0,9; 39: 3154 → 1,7; 40: 66 → 1,1; 41: ∞ → 2,0; 42: 139 → 1,1; zasięgi 36–42 zgodne z grą co do 0,1–0,3 bloku. Typy „ziemne”: po dodaniu podłoża z mapy wysokości (pierwsza lita próbka pod emiterem) i powiększeniu (1 → 1,2, 8 → 4, 15 → 2,5, 17 i 26 → 1,6) stosunek jasności 8: ∞ → 1,2; 17: 43 → 0,8; 26: 35 → 0,7. Kształt: 0 (promień 0,25 → 0,08, rozmiar 4: zwarta kolumna, zasięg 0,43×0,93 wobec 0,40×0,81 w grze), 3 (promień 0,25, rozmiar 3: 0,69×1,21 wobec 0,93×1,78), 6 (szary, rozmiar 3: 0,91×1,08 wobec 0,72×1,14); jasność 0,4–0,7 gry. Bez zmian: 10, 16, 23, 25, 31 (słabe w obu, na poziomie szumu).

---

## 10. Diggles-Unity: indeks dokumentacji

Pełna mapa ich dokumentacji (wszystkie dokumenty z odnośnikami), stan u nas, najważniejsze pliki ich kodu i plan „dogonić i wyprzedzić”: **[DIGGLES_UNITY.md](DIGGLES_UNITY.md)**. Każdy rozdział przewodnika i specyfikacja formatu kończą się linią z odnośnikami do ich odpowiedników.
