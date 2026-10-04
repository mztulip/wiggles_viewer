# OpenWiggles

Otwartoźródłowy silnik do gry **Wiggles** (ang. *Diggles – The Myth of Fenris*, SEK-Ost / Innonics, 2001).

Silnik ma uruchamiać grę na oryginalnych danych (assety trzeba mieć z własnej kopii gry), podobnie jak OpenMW czy OpenRCT2.
Osobno powstaje **przeglądarka assetów**, w której można oglądać modele, animacje i tekstury. Mogą to być dwie osobne aplikacje, które korzystają ze wspólnej biblioteki.

To nie jest gra z najwyższej półki graficznej, więc renderer ma być prosty.

## Cele

1. **`openwiggles`**: silnik gry. Wczytuje oryginalne dane z katalogu gry, renderuje świat i postacie, wykonuje skrypty Tcl z `Data/Scripts` i obsługuje rozgrywkę.
2. **`wigview`**: przeglądarka assetów i animacji:
   - modele `.3db` z podglądem animacji (odtwarzanie, pauza, wybór klatki),
   - tekstury (`.tga` i inne formaty gry),
   - przeglądanie struktury katalogu `Data/`,
   - opcjonalnie eksport do glTF/PNG.
3. **`libwiggles`**: wspólna biblioteka z parserami formatów plików, systemem plików wirtualnym nad `Data/` i podstawowym rendererem. Używają jej obie aplikacje.

## Decyzje techniczne

| Obszar | Wybór | Uzasadnienie |
|---|---|---|
| Język | C++17 | Wydajność i łatwe osadzenie interpretera Tcl (C API) |
| Grafika | **Vulkan 1.2 (domyślnie) i OpenGL 4.5** za wspólną warstwą `rhi` | Pierwotnie wybraliśmy sam OpenGL; na wczesnym etapie dodaliśmy Vulkana jako domyślny backend. Można zbudować oba albo tylko jeden (`./build.sh vulkan|opengl|both`), wybór przy starcie `--renderer` |
| Okno i wejście | SDL2 | Wieloplatformowe okno, kontekst GL lub powierzchnia Vulkana, wejście i audio |
| Ładowanie GL | własny mały loader (`src/lib/gfx/gl.h`) | |
| Shadery | GLSL 450, jedno źródło | `glslc` do SPIR-V dla Vulkana, tekst dla OpenGL (`src/shaders/`) |
| Matematyka | glm | |
| UI przeglądarki | Dear ImGui | Szybkie budowanie narzędzi |
| Skrypty | Tcl 8.3 (osadzony, jak w oryginale) | Oryginalna gra ma całą logikę w Tcl (`Data/Scripts/**/*.tcl`) |
| Build | Własne skrypty bash (`build.sh`) | Bez CMake: prosty skrypt wywołujący kompilator bezpośrednio |
| Platformy | Linux (główna), Windows | |

## Budowanie i uruchamianie

Wymagania (Arch): `gcc`, `sdl2-compat` lub `sdl2`, `glm`, `zlib`, a do renderowania `vulkan-icd-loader`, `vulkan-headers` i `shaderc` (kompilator `glslc`) dla Vulkana albo sterownik z OpenGL 4.5. Dear ImGui (z backendami OpenGL i Vulkan) jest w `third_party/`.

```bash
./build.sh                 # debug, oba backendy renderowania: build/debug/
./build.sh release         # zoptymalizowane, build/release/
./build.sh release vulkan  # tylko Vulkan:  build/release-vulkan/
./build.sh release opengl  # tylko OpenGL:  build/release-opengl/
./build.sh clean

build/release/wigview                          # przeglądarka (gra w ~/.wine/... albo OW_GAME=...)
build/release/wigview --game /ścieżka/do/Wiggles --model MANN --anim gehen_loop
build/release/wigview --model BABY --screenshot baby.png     # zrzut bez UI
build/release/wigview --template unq_menue                    # zakładka Mapy z szablonem menu gry
build/release/wigview --scenario single_Knockersdorf          # cała mapa złożona z 46 szablonów
build/release/wigview --scenario systeminit                   # menu gry z kamerą jak w oryginale
build/release/wigview --template unq_menue --center 7.37 12.86 --center-z 12 --dist 6 --pitch 20   # kadr zbliżony do menu gry
build/release/wigview --scenario OBERWELT --view 123.2 30 1.3 -0.6 0.2   # kamera gry jak set_view x y zoom dx dy
build/release/wigview --scenario systeminit.tcl --fow 1                   # podgląd mgły wojny (0 brak, 1 szara, 2/3 kadr widoczny, 4 z zapisu)
build/release/wigview --save save_SaveGame.sav                           # mapa z zapisu gry (teren, obiekty, kamera, mgła); w UI: zakładka Zapisy
OW_PROGRESS_LOG=1 build/release/wigview                                  # czasy etapów wczytywania map i zapisów na stderr
build/release/wigview --lang en                                          # język interfejsu (pl, en; nowe: lang/README.md)
build/release/wigview --renderer opengl                         # wybór backendu (domyślnie Vulkan)
build/release/test3db "/ścieżka/do/Wiggles"   # test parserów na wszystkich modelach i teksturach

# eksport glTF: obiekt (domyślnie pierwszy) i animacja (domyślnie wszystkie)
build/release/export_gltf "/ścieżka/do/Wiggles" MANN mann gehen_loop -o krasnal_chod.glb
build/release/export_gltf "/ścieżka/do/Wiggles" BABY                  # wszystkie 48 animacji
build/release/export_gltf "/ścieżka/do/Wiggles" KRISTALL --all-objects -o kristall/   # 303 pliki
```

**Renderowanie: Vulkan albo OpenGL.** Kod renderujący korzysta z warstwy `src/lib/rhi/` (tekstury, bufory, potoki, uniformy), za którą stoją dwa backendy: Vulkan 1.2 (`rhi_vk.cpp`) i OpenGL 4.5 (`rhi_gl.cpp`). Shadery są pisane raz (`src/shaders/*.vert|*.frag`, GLSL 450): `build.sh` kompiluje je przez `glslc` do SPIR-V dla Vulkana i osadza tekst dla OpenGL. Program wybiera Vulkana, a gdy ten nie ruszy (i OpenGL jest wkompilowany), przełącza się na OpenGL. `--renderer` wymusza backend bez przełączania. Zrzuty ekranu (`--screenshot`) Vulkan renderuje poza ekranem, więc działają też w Xvfb. Obie ścieżki dają ten sam obraz (porównanie co do piksela przy budowie backendu). Walidacja Vulkana: `OW_VK_VALIDATION=1`. Nazwa backendu jest w tytule okna i w rogu ekranu.

W zakładce Mapy: przełączniki warstw (teren, obiekty, detale `.l2m`, strefa horyzontu, panorama i dywan łąki), **oświetlenie jak w grze**, **lampa podglądu** (dodatkowa lampa nad kadrem, domyślnie dla szablonów bez własnych świateł; nie pochodzi z gry), tryb mgły wojny i siatka 2D pól. W prawym dolnym rogu okna i w tytule widać wersję programu (commit), żeby zrzuty ekranu dało się przypisać do wersji.

Sterowanie w wigview: LPM obrót, PPM przesunięcie, kółko przybliżenie, spacja odtwarzanie, ←/→ zmiana klatki, ↑/↓ poprzedni/następny model (albo tekstura w zakładce Tekstury), PgUp/PgDn poprzednia/następna animacja, `I` interpolacja, **Ctrl+F wyszukiwarka** (modele, obiekty w modelach, tekstury, szablony; kilka słów = wszystkie muszą pasować), `Esc` wyjście.

### Programy
| Program | Do czego służy |
|---|---|
| `wigview` | Przeglądarka: modele `.3db` z animacjami (z interpolacją klatek jak w oryginale), tekstury i **szablony map** (siatka PMP 2D i obiekty 3D; terenu jeszcze brak). `--screenshot` renderuje jedną klatkę do PNG, z `--ui` razem z interfejsem |
| `export_gltf` | Eksport do glTF 2.0 (`.glb`) z teksturami i animacjami. Ten sam eksport jest w wigview (sekcja „Eksport glTF”). Pliki otworzysz w Blenderze, Godocie, three.js albo na https://gltf-viewer.donmccurdy.com |
| `test3db` | **Test automatyczny** parserów na prawdziwych danych gry: wczytuje wszystkie modele `Data/3db` i tekstury `Data/Texture`, sprawdza spójność (zakresy indeksów, rozmiary pul, koniec pliku) i wypisuje statystyki. Kod wyjścia ≠ 0, gdy cokolwiek zawiedzie. Uruchamiamy go po każdej zmianie w parserach |

### Dokumentacja
Dokumentacja jest pisana tak, żeby na jej podstawie dało się samodzielnie napisać silnik, bez zaglądania do naszego kodu. Start: [`docs/README.md`](docs/README.md).
- [**Przewodnik po silniku**](docs/silnik/): jak gra działa, od startu do klatki obrazu (skrypty, składanie świata, teren, obiekty, kamera, oświetlenie, tło), z algorytmami i przykładami liczbowymi
- [**Formaty plików**](docs/formaty/): [`3db.md`](docs/formaty/3db.md) (modele i animacje), [`tekstury.md`](docs/formaty/tekstury.md), [`sphere_dat.md`](docs/formaty/sphere_dat.md), [`blockdb.md`](docs/formaty/blockdb.md) (kształty terenu), [`szablony.md`](docs/formaty/szablony.md) (`.pmp`, `.l2m`, skrypty szablonów)
- [`BAZA_WIEDZY.md`](docs/BAZA_WIEDZY.md): dziennik badań i materiały źródłowe

## Plan pracy

Szczegółowy plan z odhaczaniem jest w [`TODO.md`](TODO.md). Poniżej tylko zarys.

1. **Formaty plików**: parser `.3db` (modele i animacje), tekstury, `sphere.dat` (tablica normalnych), mapy i zapisy.
2. **Przeglądarka `wigview`**: najpierw same tekstury, potem modele statyczne, potem animacje.
3. **Renderer**: siatki z materiałami i teksturami, oświetlenie z prekalkulowanej jasności wierzchołków (`brightness` w `.3db`).
4. **Środowisko skryptowe**: osadzony Tcl i implementacja natywnych komend, które wywołują skrypty gry. Lista komend do ustalenia na podstawie skryptów i modów.
5. **Świat gry**: teren, kopanie, ścieżki postaci (Wiggles), budynki.
6. **Dźwięk i muzyka**.
7. **Kompatybilność z modami** (format DigglesModManager: pliki `change_*.tcl` i `config.json`).

## Materiały referencyjne

Szczegółowa analiza jest w [`docs/BAZA_WIEDZY.md`](docs/BAZA_WIEDZY.md), autorzy i licencje w [`CREDITS.md`](CREDITS.md). Lokalne kopie (zipy i rozpakowane `../ref/`) leżą poza repozytorium, w katalogu nadrzędnym.

| Repozytorium | Do czego się przyda |
|---|---|
| [Zimennik/Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) | **Najobszerniejsze źródło.** Reimplementacja gry w Unity 6 (MIT) z ok. 80 dokumentami inżynierii wstecznej (`Docs/`): siatki BlockDB, renderowanie i oświetlenie, interpreter Tcl, nawigacja, kopanie, generator świata, format zapisów |
| [djunkles/Diggles3db](https://github.com/djunkles/Diggles3db) | Parser formatu `.3db` (Python 3, Python 2.7, C#) i eksport do glTF |
| barnacle („Diggles SDK”, autor nieustalony) | Hook do oryginalnego exe z mostem Tcl↔Python. Pokazuje, jak działa interpreter skryptów w grze |
| [DigglesMods/DigglesModManager](https://github.com/DigglesMods/DigglesModManager) | Menedżer modów (C#). Pokazuje, jak mody łatają skrypty (`change_*.tcl`) |
| [ZarfaZ/DigglesHDmod](https://github.com/ZarfaZ/DigglesHDmod) | Mod HD: ~1171 przeskalowanych tekstur TGA oraz opis układu `Texture/` i `Ground/` |
| [BugFix](https://github.com/DigglesMods/BugFix), [BetterStorage](https://github.com/DigglesMods/BetterStorage), [Barrow](https://github.com/DigglesMods/Barrow), [StoneDoor](https://github.com/DigglesMods/StoneDoor), [RareResources](https://github.com/DigglesMods/RareResources), [ProductionStatistics](https://github.com/DigglesMods/ProductionStatistics), [DroppingResources](https://github.com/DigglesMods/DroppingResources), [ElfPatch](https://github.com/DigglesMods/ElfPatch) | Mody gameplayowe (DigglesMods): przykłady skryptów Tcl i struktury `Data/Scripts` |
| [DigglesMods/WigSound](https://github.com/DigglesMods/WigSound) | Obejście problemów z dźwiękiem; zawiera skrypty `sequencer.tcl` i `Trigger_*.tcl` |
| [Wiggles-nextGen/wng-game](https://github.com/Wiggles-nextGen/wng-game), [wng-server](https://github.com/Wiggles-nextGen/wng-server) | Porzucona próba silnika w Godot 4.3: pusta scena testowa, pliki serwera mają 0 B. **Nic do wykorzystania** |
| [DigglesMods/Translations](https://github.com/DigglesMods/Translations) | Tłumaczenia tekstów modów |
| [DigglesMods/DigglesModActions](https://github.com/DigglesMods/DigglesModActions) | GitHub Actions do publikacji modów |
| [Wiggles Forum](https://wiggles.ruka.at/forum/): wątek „Game internal information” | Format `sphere.dat`, klucze rejestru, parametry linii poleceń oryginalnej gry (`-mapedit`, `-console`, `-nofow`, `-safetcl`, `-logerrors`…) |

Społeczność: [Wiggles Forum](https://wiggles.ruka.at/forum/), [DigglesMods na GitHubie](https://github.com/DigglesMods).

## Licencja

Kod OpenWiggles jest udostępniony na licencji **GNU GPL v3** (plik [`LICENSE`](LICENSE)), tak jak większość repozytoriów społeczności [DigglesMods](https://github.com/DigglesMods).

Projekt **nie zawiera** żadnych danych z oryginalnej gry. Do uruchomienia potrzebna jest legalna kopia Wiggles/Diggles. Prawa do gry należą do jej twórców (SEK-Ost, Innonics).

## Podziękowania

Projekt korzysta z wiedzy zebranej przez społeczność Wiggles/Diggles. Lista autorów i repozytoriów jest w [`CREDITS.md`](CREDITS.md).
