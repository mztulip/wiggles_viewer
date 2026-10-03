# CLAUDE.md: OpenWiggles

Otwartoźródłowy silnik do gry Wiggles / Diggles (2001) i osobna przeglądarka assetów. Dokumentacja i komunikacja są po polsku.

## Mapa dokumentacji
- [TODO.md](TODO.md): **żywy plan prac**. Odhaczaj wykonane punkty i dopisuj nowe, gdy coś odkryjesz
- [README.md](README.md): cele projektu, decyzje techniczne, plan pracy, lista materiałów referencyjnych
- [docs/formaty/](docs/formaty/): **specyfikacje formatów** do samodzielnej implementacji (`3db.md`, `tekstury.md`, `sphere_dat.md`, `mapy.md`). Aktualizuj je razem z kodem
- [docs/BAZA_WIEDZY.md](docs/BAZA_WIEDZY.md): wiedza o grze wyciągnięta z `../ref/`
  - §1 format modeli `.3db` (animacja wierzchołkowa, pule danych)
  - §2 tekstury, katalogi `Data/Texture` i `Data/Ground`, `sphere.dat`
  - §3 skrypty Tcl 8.3: preprocesor, `call`, model obiektowy, lista natywnych komend
  - §4 dźwięk
  - §5 format modów (DigglesModManager)
  - §6 parametry i rejestr oryginalnego exe
  - §7 zainstalowana gra: struktura `Data/`, `systeminit.tcl`, mapy z szablonów (`.PMP`, `.l2m`, `BlockDB`), zapisy, wyniki skanera komend
  - §8 laboratorium: obserwacja działającej gry, trace komend, rejestr
  - Otwarte pytania: kolejność dalszych badań
- [CREDITS.md](CREDITS.md): autorzy i linki do repozytoriów źródłowych (zipy w `../` odpowiadają tym repo na GitHubie)
- [LICENSE](LICENSE): GPL v3
- [docs/natywne_komendy_gry.txt](docs/natywne_komendy_gry.txt): **dokładna** lista 639 komend natywnych zrzucona z działającej gry
- §8 bazy wiedzy: laboratorium (gra pod Wine z sondą Tcl). Narzędzia w `tools/lab/`, wyniki w `../lab/Wiggles/owprobe_*.txt` i `tclerrors.txt`
- [docs/tcl_natywne_kandydaci.txt](docs/tcl_natywne_kandydaci.txt): kandydaci na natywne komendy Tcl wraz z liczbą użyć (wynik `tools/scan_tcl_natives.py`)

## Gra (dane oryginalne)
- Instalacja: `~/.wine/drive_c/Program Files (x86)/Innonics/Wiggles`. **Tylko do odczytu.** `Wiggles.exe` to wersja 1.0 build 844 (polska)
- Najważniejsze: `Data/systeminit.tcl` (start), `Data/3db/`, `Data/Texture/`, `Data/Scripts/`, `Data/Templates/`, `Data/Gamesave/`
- Nazwy plików mają mieszaną wielkość liter (`BABY.3DB`, `baby.3db`), więc trzeba je wyszukiwać bez rozróżniania wielkości. Teksty są w CP1250 lub CP1252 z końcami linii CRLF

## Materiały źródłowe (`../ref/`, tylko do odczytu)
- `../ref/Diggles3db-main/python3/lib/parse_3db.py`: wzorcowy parser `.3db`. Próbka: `../ref/Diggles3db-main/assets/baby.3db`
- `../ref/DigglesHDmod-master/Data/`: tekstury TGA do testów przeglądarki
- `../ref/barnacle-main/scripts/abi.py`, `examples/hook.py`: wnętrze silnika (adresy, ładowanie skryptów)
- `../ref/WigSound-master/Files/Data/Scripts/`, `../ref/ProductionStatistics-master/Data/Scripts/`: pełne oryginalne skrypty Tcl
- `../ref/DigglesModManager-master/Service/ModdingService.cs`: logika łatek `change_*.tcl`
- `../ref/DigglesHDmod-master` ma 2,6 GB, więc nie przeszukuj go rekurencyjnie bez filtra

## Budowanie i weryfikacja
- `./build.sh [debug|release] [lib|imgui|wigview|test3db|export_gltf|all|clean]` (bash, bez CMake)
- glTF: `build/release/export_gltf <gra> <model> [obiekt] [animacja] -o x.glb`. Walidacja: `npx gltf-validator` (Khronos)
- `build/release/test3db "<gra>"` sprawdza parsery na wszystkich danych gry
- `build/release/wigview --model X --anim Y --frame N --screenshot out.png` renderuje bez UI, a `--template T --ui --screenshot out.png` szablon razem z interfejsem. Tak można samemu obejrzeć wynik (Read na PNG)
- Kod: `src/lib` (libwiggles: vfs, tga, model3db, png_write, `gfx/` z loaderem GL i rendererem), `src/wigview`, `src/tools`

## Zasady
- Kod i dokumentacja są w tym repozytorium (`openwiggles_git/`). Materiały referencyjne (`../ref/`, zipy) leżą w katalogu nadrzędnym i nie trafiają do repo.
- **Commity i PR-y bez żadnej wzmianki o AI** (żadnych `Co-Authored-By` ani „Generated with…”).
- C++17, OpenGL 3.3 core, SDL2, Dear ImGui, osadzony Tcl (8.3 jak w oryginale albo 8.6; decyzja w TODO, faza 5).
- **Bez CMake.** Budowanie odbywa się własnymi skryptami bash (`build.sh`).
- Repo nie może zawierać danych z oryginalnej gry (także wyników sondy z treścią skryptów; te zostają w `../lab/`).
- Eksperymenty na grze tylko na kopii w `../lab/` (osobny prefiks Wine), nigdy na instalacji w `~/.wine`.
- Po każdym kroku zaktualizuj `TODO.md`.
- **Uznawaj pracę autorów.** Każda wiedza wzięta z cudzego repo (dokumentacja, komentarze w kodzie, opisy commitów) ma odnośnik do repozytorium na GitHubie (lista w `CREDITS.md`). Nie kopiujemy cudzego kodu, tylko wiedzę o formatach.
- Nowe ustalenia o formatach dopisuj do `docs/BAZA_WIEDZY.md` (oznaczenia ✅ potwierdzone, 🔶 hipoteza).
