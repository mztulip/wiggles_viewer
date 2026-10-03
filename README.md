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
| Grafika | **OpenGL 3.3 core** | Prostszy od Vulkana i w zupełności wystarcza dla gry z 2001 r. Działa wszędzie, także na starszym sprzętcie |
| Okno i wejście | SDL2 | Wieloplatformowe okno, kontekst GL, wejście i audio |
| Ładowanie GL | glad | |
| Matematyka | glm | |
| UI przeglądarki | Dear ImGui | Szybkie budowanie narzędzi |
| Skrypty | Tcl 8.3 (osadzony, jak w oryginale) | Oryginalna gra ma całą logikę w Tcl (`Data/Scripts/**/*.tcl`) |
| Build | Własne skrypty bash (`build.sh`) | Bez CMake: prosty skrypt wywołujący kompilator bezpośrednio |
| Platformy | Linux (główna), Windows | |

## Budowanie i uruchamianie

Wymagania (Arch): `gcc`, `sdl2-compat` lub `sdl2`, `glm`, sterownik z OpenGL 3.3. Dear ImGui jest w `third_party/`.

```bash
./build.sh                 # debug: wigview i test3db do build/debug/
./build.sh release         # zoptymalizowane, build/release/
./build.sh clean

build/release/wigview                          # przeglądarka (gra w ~/.wine/... albo OW_GAME=...)
build/release/wigview --game /ścieżka/do/Wiggles --model MANN --anim gehen_loop
build/release/wigview --model BABY --screenshot baby.png     # zrzut bez UI
build/release/test3db "/ścieżka/do/Wiggles"   # test parserów na wszystkich modelach i teksturach
```

Sterowanie w wigview: LPM obrót, PPM przesunięcie, kółko przybliżenie, spacja odtwarzanie, strzałki zmiana klatki, `I` interpolacja, `Esc` wyjście.

### Programy
| Program | Do czego służy |
|---|---|
| `wigview` | Przeglądarka: modele `.3db` z animacjami (z interpolacją klatek jak w oryginale) i tekstury. Z `--screenshot` renderuje jedną klatkę do PNG bez okna UI |
| `test3db` | **Test automatyczny** parserów na prawdziwych danych gry: wczytuje wszystkie modele `Data/3db` i tekstury `Data/Texture`, sprawdza spójność (zakresy indeksów, rozmiary pul, koniec pliku) i wypisuje statystyki. Kod wyjścia ≠ 0, gdy cokolwiek zawiedzie. Uruchamiamy go po każdej zmianie w parserach |

### Dokumentacja formatów
Specyfikacje w [`docs/formaty/`](docs/formaty/) są pisane tak, żeby dało się na ich podstawie napisać własny loader i przeglądarkę bez zaglądania do naszego kodu:
- [`3db.md`](docs/formaty/3db.md): modele i animacje
- [`tekstury.md`](docs/formaty/tekstury.md): TGA i katalogi tekstur
- [`sphere_dat.md`](docs/formaty/sphere_dat.md): tablica normalnych

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

Szczegółowa analiza jest w [`docs/BAZA_WIEDZY.md`](docs/BAZA_WIEDZY.md). Materiały referencyjne leżą poza repozytorium, w katalogu nadrzędnym (zipy i rozpakowane `../ref/`).

W katalogu nadrzędnym leżą zipy z repozytoriami społeczności i zapisane strony z forum, które pomagają przy pracy:

| Plik | Do czego się przyda |
|---|---|
| `Diggles3db-main.zip` | **Najważniejszy.** Parser formatu `.3db` (Python 3, Python 2.7, C#) i eksport do glTF. Opisuje strukturę: wersja, nazwa, materiały, meshe (linki do trójkątów, UV, punktów i jasności), obiekty, animacje |
| `barnacle-main.zip` | „Diggles SDK”: hook do oryginalnego exe z mostem Tcl↔Python. Pokazuje, jak działa interpreter skryptów w grze |
| `DigglesModManager-master.zip` | Menedżer modów (C#). Pokazuje, jak mody łatają skrypty (`change_*.tcl`) |
| `DigglesHDmod-master.zip` | Mod HD z teksturami w wyższej rozdzielczości (`Data_HD/`). Zawiera ~1171 tekstur TGA gry (przeskalowanych) oraz opis układu `Texture/` i `Ground/` |
| `BugFix`, `BetterStorage`, `Barrow`, `StoneDoor`, `RareResources`, `ProductionStatistics`, `DroppingResources`, `ElfPatch` | Mody gameplayowe. Przykłady skryptów Tcl i struktury `Data/Scripts` (klasy: `zwerg`, `work`, `items`, `story`, `init`…) |
| `WigSound-master.zip` | Obejście problemów z dźwiękiem. Zawiera skrypty `sequencer.tcl` i `Trigger_*.tcl` |
| `wng-game-master.zip`, `wng-server-master.zip` | Wiggles NextGen: porzucona próba silnika w Godot 4.3. Zawiera tylko pustą scenę testową, a pliki serwera mają 0 B. **Nic do wykorzystania** |
| `Translations-master.zip` | Tłumaczenia tekstów modów |
| `DigglesModActions-main.zip` | GitHub Actions do publikacji modów |
| `Game internal information - Wiggles Forum.html`, `plik_dat` | Notatki z forum: format `sphere.dat` (256 × vec3 float32 LE, normalne), klucze rejestru (`HKCU\Software\SekOst\Diggles`), parametry linii poleceń oryginalnej gry (`-mapedit`, `-console`, `-nofow`, `-safetcl`, `-logerrors`…) |

Repozytoria z linkami i autorami: [`CREDITS.md`](CREDITS.md). Społeczność: [Wiggles Forum](https://wiggles.ruka.at/forum/), [DigglesMods na GitHubie](https://github.com/DigglesMods).

## Licencja

Kod OpenWiggles jest udostępniony na licencji **GNU GPL v3** (plik [`LICENSE`](LICENSE)), tak jak większość repozytoriów społeczności [DigglesMods](https://github.com/DigglesMods).

Projekt **nie zawiera** żadnych danych z oryginalnej gry. Do uruchomienia potrzebna jest legalna kopia Wiggles/Diggles. Prawa do gry należą do jej twórców (SEK-Ost, Innonics).

## Podziękowania

Projekt korzysta z wiedzy zebranej przez społeczność Wiggles/Diggles. Lista autorów i repozytoriów jest w [`CREDITS.md`](CREDITS.md).
