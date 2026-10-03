# Baza wiedzy: co wynika z materiałów referencyjnych

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

Źródło: [djunkles/Diggles3db](https://github.com/djunkles/Diggles3db), czyli `../ref/Diggles3db-main/python3/lib/parse_3db.py` (najczystszy kod), `python2.7/3dbScript.py` (ma asercje), `c-sharp/` (przeglądarka WPF z odtwarzaniem animacji).

Wszystkie liczby są little-endian. `str` oznacza `u32` z długością, a po nim bajty bez znaku `\0`.

```
str   magic            = "3DDB 1.0"
str   name             np. "baby"

u16   material_count
  str name             nazwa tekstury bez rozszerzenia, np. "Character_ZBaby_a"
  str path             oryginalna ścieżka z serwera SEK (nieużywana)
  u32 unknown          zawsze 1 (assert w wersji py2.7)

u32   mesh_count       (baby: 479; każdy mesh to JEDNA KLATKA animacji)
  u16 link_count       (baby: 2, po jednym na materiał)
    u16 material       indeks materiału
    u16 unknown        (baby: zawsze 1)
    u16 triangles      indeks do puli trójkątów
    u16 texcoords      indeks do puli UV
    u16 points         indeks do puli pozycji
    u16 brightness     indeks do puli jasności
  vec3f bbox_min       ✅ AABB klatki w jednostkach modelu (te same co pozycje po dekwantyzacji)
  vec3f bbox_max
  u8[0x80] unknown     🔶 32 × u32 rosnących indeksów (0,1,2,3…), może odnośniki do rekordów 20-bajtowych
  u16 unknown          (baby: 0)
  u8[0x30] unknown     🔶 12 × f32, prawdopodobnie 4 × vec3 (punkty zaczepienia lub dodatkowa geometria)
  u16 unknown          (baby: 0xFFFF)

u16   object_count     (baby: 1, "baby")
  str name
  u16 count
    u32 anim_index     lista animacji należących do obiektu

u16   anim_count       (baby: 48)
  str name             np. "aufwachen", "drehen_links", "auf_po_fallen"
  u16 frame_count
    u32 mesh_index     kolejne klatki, indeksy do tablicy meshy
  u16 unknown          🔶 flagi (0, 3, …)
  f32 unknown          🔶
  str source_file      ścieżka do źródłowego pliku `.sex` (eksporter SEK)
  vec3f move           🔶 ruch korzenia na cykl (drehen_*: x/y ≠ 0)
  vec3f rotate         🔶 obrót na cykl w radianach (drehen_links: z = +0.1309 = 7.5°)

u16   shadow_count
  u8[32*32]            mapy cieni 32×32 w skali szarości (rzut cienia pod postacią)

u16   cmap_count
  u16 w, u16 h, u16 ?, u16 ?
  u8[w*h]              🔶 "cube/color map", 8-bit

u16   n_tri_pools, n_uv_pools, n_point_pools, n_bright_pools
u32   n_unknown20
u16[n_tri_pools]       liczności każdej puli
u16[n_uv_pools]
u16[n_point_pools]
u16[n_bright_pools]
u8[20] × n_unknown20   🔶 nieznane rekordy

dane pul, kolejno:
  trójkąty:  u16 indeksy (po 3 na trójkąt)
  UV:        f32 u, f32 v
  pozycje:   3 × u16, dekwantyzacja: (v - 32768) / 4.0   ✅ (zgadza się z bbox, współczynnik 3.99)
  normalne:  u8 na wierzchołek = indeks do SPHERE.DAT (256 normalnych)   ✅ (wcześniej mylnie „brightness”)
```

Po wczytaniu wskaźnik ma być dokładnie na końcu pliku. ✅ Sprawdzone na wszystkich 43 plikach gry: wszystkie mają wersję `3DDB 1.0` i wszystkie parsują się do końca.

### Wnioski dla renderera
- **Animacja jest wierzchołkowa (vertex animation)**, bez kości. Każda klatka to osobny mesh. Odtwarzanie polega na wybieraniu `mesh_index` dla danej klatki. Opcjonalnie można interpolować pozycje między klatkami, bo topologia (pula trójkątów) jest zwykle współdzielona.
- Pule są **współdzielone** między klatkami: UV i trójkąty mają ten sam indeks, a zmieniają się tylko `points` i `brightness`. Wystarczy wysłać UV i indeksy do GPU raz, a pozycje per klatka.
- **Dekwantyzacja pozycji:** ✅ `(u16 - 32768) / 4.0`, czyli stały punkt z krokiem 0,25 jednostki. Wspólny układ dla wszystkich klatek, więc animacja nie „pływa”. Hipoteza z bbox była błędna. Baby ma około 100 jednostek wysokości, a oś Y jest prawdopodobnie skierowana w dół (zakres y: −97…5). 🔶 Relacja jednostek modelu do jednostek świata jest do ustalenia.
- **Normalne:** ✅ bajt per wierzchołek (w narzędziach społeczności nazywany „brightness”) to **indeks normalnej w `SPHERE.DAT`**. Wartości rozkładają się równomiernie na 0–255, a podgląd normalnych w wigview daje gładkie, spójne gradienty zgodne z geometrią. Oświetlenie liczymy więc sami (Lambert z normalną `sphere[idx]`).
- **Orientacja:** ✅ oś Y modelu jest skierowana w dół (góra postaci ma ujemne y). W wigview obracamy model o 180° wokół osi X, co prostuje go bez lustrzanego odbicia.
- **UV:** ✅ konwencja Direct3D (v=0 u góry tekstury). Tekstury ładujemy od góry, więc bez odwracania v.
- **Pule UV i normalnych mają zawsze tyle elementów co pula punktów** (✅ 191 448 linków we wszystkich modelach), więc wierzchołek = indeks wspólny dla pozycji, UV i normalnej.
- Teksturę materiału szuka się po nazwie w katalogach w kolejności: `Texture/m256`, `m128`, `m064`, `m032`, `Gray`, `Misc`, `ClassIcons` (`.tga`). Ta kolejność pochodzi z wersji py2.7.
- Istniejący eksport do glTF obsługuje **tylko mesh 0** i nie ma animacji. Nasza przeglądarka będzie pierwsza z pełnym odtwarzaniem.

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
- 14 **masek przejść** (`msk_aabb01.tga`, `Msk_AAAB01.tga`): na krawędzi między dwoma gruntami jeden polygon dostaje oba grunty, a maska służy jako alfa. To klasyczny *texture splatting*.
- Detale zależne od świata, z alfą i prefiksem świata: `kris_0101.tga` (kryształy), `urw_0101.tga` (prawdopodobnie Urwald, czyli dżungla). Sufiks w postaci 4 bitów (`0001`…`1111`) 🔶 oznacza, z których stron sąsiaduje pusta przestrzeń (marching squares lub autotiling).
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
- Wersje exe z ABI: **1.0 build 844** i **2.1.1.10**. Barnacle zawiera adresy funkcji silnika (`scripts/abi.py`), które przydadzą się, gdyby trzeba było coś podejrzeć w deasemblerze.

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

## Otwarte pytania (kolejność badań)
1. ~~Dekwantyzacja pozycji `.3db`~~ ✅ rozwiązane. Pozostaje skala względem świata i orientacja osi.
2. Znaczenie bloków `0x80` i `0x30` w meshu, rekordów 20-bajtowych i `cmap`.
3. Formaty map: `.PMP` (częściowo, sekcja 7.4), `.l2m`, `BlockDB*.BDB`, `.ENT` oraz zapisów `.sav` (gzip, struktura nieznana).
4. Format `texmaps.bin` (prawdopodobnie do pominięcia).
5. Jak działa preprocesor Tcl poza `//`.
6. ~~Skaner natywnych komend~~ ✅ `tools/scan_tcl_natives.py`, wynik w `docs/tcl_natywne_kandydaci.txt`. Pozostaje oczyszczenie listy i podział na moduły.
7. Fonty `.kft`, `ALPHABET.BIN`, `GUI/*.bin` (scrollery).
8. ~~Uruchomić oryginał pod Wine~~ ✅ działa (sekcja 8). Dalej: śledzenie w interpreterach obiektów i podczas rozgrywki, `tclhelp`, `web_shell`, `WebServer`.

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
| `BlockDB0/1/2.BDB` (20/26/37 MB) | 🔶 Baza geometrii bloków terenu (jaskinie), prawdopodobnie 3 poziomy szczegółowości. Nagłówek: `u32 11`, `u32 0`, `u32 0x1CFA0`… |
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

### 7.4 Mapy: system szablonów ✅/🔶
- Świat to siatka. `map create 64 64` tworzy mapę, a potem nakłada się na nią **szablony** (klocki).
- Szablon to plik `Templates/tcl/<świat>_<typ>_<nr>_<wariant>.tcl` z procedurą `MapTemplateSet {xoffset yoffset}`, która:
  1. `map_template <plik>.pmp $x $y` nakłada geometrię i typy pól,
  2. `map_setlayer2 $x $y <plik>.l2m` nakłada drugą warstwę (🔶 dekoracje, wypełnienie lub materiał),
  3. tworzy obiekty dekoracyjne: `new Dummy_<Nazwa> "" "<x y z>" "<rot>"` z `set_owner -1`, `set_physic`, `set_autolight`, `set_boxed`, pogrupowane według `loading_objdetail high/medium/low/off`.
- Prefiksy światów: `urw` (Urwald, czyli dżungla), `kris` (kryształy), `lava`, `swf` (Schwefel, czyli siarka), `unq_*` (unikalne). Typy: `gng` (Gang, czyli korytarz), `hol` (Höhle, czyli jaskinia), `gsg`, `start`.
- **`.PMP`** ✅ częściowo: `u32 w, u32 h`, potem `u8[w*h]` (typ pola), potem `u32[w*h]` (🔶 indeks bloku w `BlockDB`), potem ogon o nieznanej strukturze.
- **`.l2m`**: magic `M2L\0`, `u32 w, u32 h`, potem `u16[w*h]` (🔶 flagi bitowe, wartości typu `0xC0xx`).
- **`.ENT`**: często puste (0 B). 🔶 Encje szablonu.
- `split/` zawiera warianty szablonów pocięte na ćwiartki (`_tAA`, `_tAB`, `_tBA`, `_tBB`).

### 7.5 Zapisy gry
`.sav` to **gzip**. Po rozpakowaniu jest binarka (~7 MB) zaczynająca się od `V1.0.844`. Struktura nieznana.

### 7.6 Natywne komendy: wyniki skanera
`tools/scan_tcl_natives.py` przeszedł 3961 plików `.tcl` (skrypty i szablony) i znalazł 2301 procedur lub metod zdefiniowanych w skryptach oraz około 830 kandydatów na komendy natywne. Pełna lista jest w `docs/tcl_natywne_kandydaci.txt` i jest zaszumiona: zawiera też słowa ze stringów i list.

Najczęstsze komendy, czyli pierwsze do zaimplementowania: `new`, `sel` (np. `sel /obj`, wybór kontekstu tworzenia), `set_physic`, `set_owner`, `set_boxed`, `set_autolight`, `loading_objdetail`, `call_method`, `sound`, `map_setlayer2`, `map_template`, `call`, `set_class_anim`, `smalltalk`, `tasklist_add`, `textwin`, `log`, `get_pos`, `set_anim`, `def_class`, `obj_query`, `set_particlesource`, `adaptive_sound`, `layout`, `obj_init`, `def_event`, `handle_event`, `timer_event`, `set_hoverable`, `vector_add`, `class_defaultanim`, `get_ref`, `state_enable/disable`, `random`, `set_pos`, `get_attrib/set_attrib`, `link_obj`, `set_visibility`, `lmsg`, `del`, `obj_valid`, `irandom`, `set_roty`, `set_textureanimation`, `set_collision`, `walk`, `start_fade`, `get_mapedit`, `tasklist_clear`.

---

## 8. Laboratorium: obserwacja działającej gry ✅

Oryginał działa pod Wine 11 (Direct3D 7 przez wined3d). Narzędzia są w `tools/lab/`:
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
- 🔶 Ciała klas i metody obiektów (`obj_init`, `method`, `tasklist_*`) nie przechodzą przez nasze owijki. Prawdopodobnie wykonują się w **osobnych interpreterach** (per klasa lub per obiekt, `CTclRootObj`). Do zbadania.
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
- `Performance`: `AnimInterpolation` (interpolacja klatek animacji!), `LandscapeQuality`, `TextureQuality`, `ObjectQuality`, `MaxFramerate`, `UseEnvMapping`, `MultiTexture`, `ScapeJitter`, `ValidateRenderJobs`, **`WebServer`** (🔶 wbudowany serwer WWW, por. komenda `web_shell`)
- `Screen`: `Fullscreen`, `Width`, `Height`, `Gamma`, `Brightness`, `TnL`, GUID-y Direct3D/DirectDraw
- `Control`, `Input`: mapowanie myszy i klawiszy; `Sound`: głośności

### 8.4 Komendy debugowe do zbadania
`tclhelp` (zwraca pusty wynik, 🔶 pisze do konsoli), `web_shell`, `dump_var`, `proddump`, `output_3ddbstats`, `meminfo`, `perfwin`, `peek`/`poke`, `waterdbg`, `toggle_wf` (wireframe?), `render_gray`, `show_maped`, `save_map`, `save_scene`, `xtest`, `test`/`testd`, `vtune`.

### 8.5 barnacle (SDK) jako następny krok
Barnacle hookuje `Wiggles.exe` 1.0.844, czyli **dokładnie naszą wersję**, i daje Pythona wewnątrz gry. Przyda się, gdy czysty Tcl nie wystarczy: śledzenie interpreterów obiektów albo odczyt pamięci i struktur C++. Wymaga zbudowania pod Windows (Python 3.6 32-bit, scons), czyli wejścia pod Wine.
