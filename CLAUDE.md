# CLAUDE.md: OpenWiggles

Otwartoźródłowy silnik do gry Wiggles / Diggles (2001) i osobna przeglądarka assetów. Dokumentacja i komunikacja są po polsku.

## Mapa dokumentacji
- [TODO.md](TODO.md): **żywy plan prac**. Odhaczaj wykonane punkty i dopisuj nowe, gdy coś odkryjesz
- [README.md](README.md): cele projektu, decyzje techniczne, plan pracy, lista materiałów referencyjnych
- [docs/README.md](docs/README.md): **wejście do dokumentacji**. Zasada: dokumentacja ma wystarczyć człowiekowi do samodzielnego napisania silnika, więc opisujemy, jak i dlaczego coś działa (algorytm, przykład liczbowy, źródło, status ✅/🔶/❓), a nie tylko wynik
- [docs/silnik/](docs/silnik/): **przewodnik po silniku**, rozdziały 1–16 od startu gry do klatki obrazu (w tym nieodkryty teren, obiekty Tcl, cząsteczki, płyny, gra, dźwięk, mechaniki rozgrywki z instrukcji) (skrypty, świat z szablonów, teren, obiekty, kamera, oświetlenie, tło, kolejność rysowania, niewiadome). Każde nowe ustalenie o działaniu gry trafia do właściwego rozdziału
- [docs/formaty/](docs/formaty/): **specyfikacje plików** (`3db.md`, `tekstury.md`, `teksty.md`, `sphere_dat.md`, `blockdb.md`, `szablony.md`, `sav.md`). Aktualizuj je razem z kodem
- [docs/KOD.md](docs/KOD.md): **mapa kodu**: podsystem → pliki w [`src/`](src) → rozdział przewodnika → narzędzie do sprawdzenia. Zacznij od niej, szukając implementacji
- [docs/BAZA_WIEDZY.md](docs/BAZA_WIEDZY.md): **dziennik badań**: materiały referencyjne, laboratorium (§8), przebieg eksperymentów (§9)
- **Diggles-Unity** (grywalny port w Unity, MIT): **[docs/DIGGLES_UNITY.md](docs/DIGGLES_UNITY.md)** to mapa wszystkich ich dokumentów z odnośnikami, stan u nas i plan „dogonić i wyprzedzić”. Przed pracą nad podsystemem przeczytaj ich dokument z tej mapy (lokalnie: `grep -rn "<fraza>" ../ref/Diggles-Unity-main/Docs`), wnioski sprawdź w laboratorium, a po pracy popraw wiersz mapy (stan, następny krok)
- [CREDITS.md](CREDITS.md): autorzy i linki do repozytoriów źródłowych (zipy w `../` odpowiadają tym repo na GitHubie)
- [LICENSE](LICENSE): MIT (do 2026-10-06 GPL v3). Nie dołączamy kodu na licencji GPL ani innych wymagających tej samej licencji; z repozytoriów GPL tylko wiedza
- [docs/natywne_komendy_gry.txt](docs/natywne_komendy_gry.txt): **dokładna** lista 639 komend natywnych zrzucona z działającej gry
- [tools/lab/README.md](tools/lab/README.md): **instrukcja narzędzi laboratorium** (`capture.sh`, `live.sh`, `live2.sh`): jak uruchamiać, przykłady, pułapki
- [`tools/lab/capture.sh`](tools/lab/capture.sh) robi zrzut obrazu z oryginalnej gry w niewidocznym Xvfb (`menu`, `template <szablon>`, `scene <plik.tcl>`). Używaj go jako wzorca wizualnego
- [`tools/lab/live.sh`](tools/lab/live.sh): gra z laboratorium w tle z wbudowanym serwerem WebTraq; `tcl '…'` wykonuje Tcl w działającej grze, `page`, `shot`, `stop` (§8.6 bazy wiedzy). Najszybszy sposób na pytania o stan gry. Pułapki: sprawdzaj, czy `tcl` zwraca `Ok.` (nie kieruj wyniku do `/dev/null`); dekoracje `Dummy_*` po utworzeniu nie mają obiektu Tcl; nowy krasnal potrzebuje `state_disable`; bez `FogRemover` zapis jest czarny (BAZA_WIEDZY §9.8–9.10)
- [`tools/lab/live2.sh`](tools/lab/live2.sh): **przeglądarka szablonów w oryginale**, gra w widocznym oknie (**do eksperymentów `OW_HIDDEN=1 live2.sh start`: niewidoczny Xvfb**, okno nie przejmuje klawiatury użytkownika); `start [szablon]`, `show <szablon>`, `next/prev [filtr]`, `list`, `view x y`, `shot` (z bufora klatki OpenGL przez [`glgrab.c`](tools/lab/glgrab.c), także przy schowanym oknie), `shot-all KATALOG [filtr]`, `stop`. Podmienia szablon w działającej grze w ok. 2 s (`map create` czyści mapę, obiekty Tcl usuwane, odsłonięcie prostokątem `FogRemover`, siatka lamp)
- §8 bazy wiedzy: laboratorium (gra pod Wine z sondą Tcl). Narzędzia w [`tools/lab/`](tools/lab), wyniki w `../lab/Wiggles/owprobe_*.txt` i `tclerrors.txt`
- [docs/tcl_natywne_kandydaci.txt](docs/tcl_natywne_kandydaci.txt): kandydaci na natywne komendy Tcl wraz z liczbą użyć (wynik [`tools/scan_tcl_natives.py`](tools/scan_tcl_natives.py))

## Gra (dane oryginalne)
- Instalacja: `~/.wine/drive_c/Program Files (x86)/Innonics/Wiggles`. **Tylko do odczytu.** Programy szukają gry w kolejności: `--game`, zmienna `WIGGLES_DIR`, `OW_GAME`, (wigview) wybór z okna „Ścieżki…” zapisany w `wigview.ini`, ta instalacja. `Wiggles.exe` to wersja 1.0 build 844 (polska)
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
- `./build.sh [debug|release] [vulkan|opengl|both] [lib|imgui|wigview|game|owscript|test3db|export_gltf|all|clean]` (bash, bez CMake). Domyślnie oba backendy renderowania w `build/<tryb>/`; jeden backend w `build/<tryb>-vulkan|opengl/`
- Renderowanie przez warstwę [`src/lib/render/rhi/`](src/lib/render/rhi) (Vulkan `rhi_vk.cpp`, OpenGL 4.5 `rhi_gl.cpp`); shadery tylko w [`src/shaders/`](src/shaders) (GLSL 450, makra `UBO`/`TEX`/`OW_CLIP` z `common.glsl`). **Nie wołać OpenGL ani Vulkana bezpośrednio z rendererów.** Nowa funkcja renderowania = obie ścieżki; po zmianie porównać zrzuty `--renderer vulkan` i `--renderer opengl` (`magick compare -metric AE`)
- glTF: `build/release/export_gltf <gra> <model> [obiekt] [animacja] -o x.glb`. Walidacja: `npx gltf-validator` (Khronos)
- `build/release/test3db "<gra>"` sprawdza parsery na wszystkich danych gry
- `build/release/wigview --model X --anim Y --frame N --screenshot out.png` renderuje bez UI, a `--template T --ui --screenshot out.png` szablon razem z interfejsem. Tak można samemu obejrzeć wynik (Read na PNG). Porównania z grą: `--scenario S --view x y zoom dx dy` (kamera jak `set_view`), `--game ../lab/Wiggles` dla szablonów testowych z laboratorium, `--fow N` (nieodkryty teren), `--emit TYP X Y Z` (źródło cząsteczek w punkcie gry), `--border-mask`, `--no-draw-modes`, `--lab-lamps` (lampy jak `live2.sh`, do porównań jasności), `--simulate [--warmup S]` (symulacja skryptów, `OW_TRACE_SIM=1` wypisuje postacie, `=2` ich ślad co 0,5 s), **`--http PORT`** (serwer deweloperski: Tcl i stan świata przez HTTP, [docs/SERWER_HTTP.md](docs/SERWER_HTTP.md); też w `openwiggles`). Przeglądanie: `--texture`, `--file`, `--sound`, `--shadow-cmap`, `--thumbnails KATALOG`. Pełna lista opcji: komentarz na początku [`src/apps/wigview/main.cpp`](src/apps/wigview/main.cpp). Zrzuty bez UI w Vulkanie mają rozmiar z `--size`, z UI i w OpenGL rozmiar okna (menedżer okien może go zmienić)
- **Użytkownik uruchamia `build/debug/wigview`**: po zmianach budować debug i release
- **Testy regresji: [`tools/tests/run.sh`](tools/tests/README.md) przed każdym commitem**; nowa funkcjonalność albo poprawka błędu dostaje od razu test w tym zestawie (bez okna, symulacja ze śladem, obraz wzorcowy w `../lab/testref`, interakcja w Xvfb)
- **Napisy interfejsu przez `tr("…")`** (po polsku, [`src/lib/i18n.h`](src/lib/i18n.h)). Po dodaniu lub zmianie napisów uruchom [`tools/i18n_update.py`](tools/i18n_update.py) i uzupełnij [`lang/en.po`](lang/en.po) (`--check` musi przejść). Opis: [lang/README.md](lang/README.md)
- Wersja programu (commit) jest w `build/gen/ow_version.h`, generowanym przez [`build.sh`](build.sh); wigview pokazuje ją w rogu ekranu i w tytule okna (zrzuty użytkownika da się przypisać do wersji)
- **Priorytet: jak najszybciej grywalna gra** (`openwiggles`, [`src/apps/game`](src/apps/game)). Najpierw działające przybliżenie, oznaczone 🔶, a punkt „dopracować” idzie do TODO (sekcja „Droga do grywalności”)
- Kod:
  - [`src/lib`](src/lib) (libwiggles): formaty i dane (vfs z nakładkami modów, tga, model3db, blockdb, szablony, zapisy, `messages` (teksty `lmsg`), `sound`, `sound_refs`), `script/` (silnik Tcl), `map_scene` (scena mapy wspólna dla gry i przeglądarki), `render/` (renderery modeli, terenu, cząsteczek, płynów, panoramy, ramki ekranu; `render/rhi/` warstwa Vulkan/OpenGL)
  - [`src/apps/wigview`](src/apps/wigview) (przeglądarka: tylko UI) i [`src/apps/game`](src/apps/game) (gra `openwiggles`), [`src/tools`](src/tools) (test3db, export_gltf, owscript), [`src/shaders`](src/shaders)
  - **[`src/lib`](src/lib) nie dołącza niczego z [`src/apps`](src/apps)** (sprawdza to [`build.sh`](build.sh)). Kod potrzebny grze i przeglądarce trafia do biblioteki

## Zasady
- Kod i dokumentacja są w tym repozytorium (`openwiggles_git/`). Materiały referencyjne (`../ref/`, zipy) leżą w katalogu nadrzędnym i nie trafiają do repo.
- **Commity i PR-y bez żadnej wzmianki o AI** (żadnych `Co-Authored-By` ani „Generated with…”).
- **Styl kodu: `.clang-format`** (klamry `{}` w osobnych liniach, styl Allman; wcięcia tabulatorami). Po zmianach: `clang-format -i` na zmienionych plikach z [`src/`](src) (shadery z `--assume-filename=x.cpp`), nigdy na `third_party/`.
- C++17, Vulkan 1.2 (domyślnie) i OpenGL 4.5 za warstwą `rhi`, SDL2, Dear ImGui, osadzony Tcl (8.3 jak w oryginale albo 8.6; decyzja w TODO, faza 5).
- **Bez CMake.** Budowanie odbywa się własnymi skryptami bash ([`build.sh`](build.sh)).
- Repo nie może zawierać danych z oryginalnej gry (także wyników sondy z treścią skryptów; te zostają w `../lab/`).
- Eksperymenty na grze tylko na kopii w `../lab/` (osobny prefiks Wine), nigdy na instalacji w `~/.wine`.
- **Nie deasemblujemy ani nie analizujemy kodu exe.** Zachowanie gry ustalamy obserwacją (laboratorium, sonda Tcl, zrzuty przy zmienianych parametrach) i odtwarzamy. Ustalenia z cudzych opracowań (adresy `FUN_…` z Diggles-Unity) to tylko cytowane źródło, które sprawdzamy eksperymentem.
- Po każdym kroku zaktualizuj `TODO.md`.
- Po zmianach w dokumentacji albo przenosinach plików: [`tools/check_docs.py`](tools/check_docs.py) (drzewo odnośników od CLAUDE.md, zepsute linki, nieaktualne ścieżki; `--fix` zamienia ścieżki w backtickach na odnośniki). Nowy plik `.md` podepnij odnośnikiem z dokumentu nadrzędnego, nowy moduł dopisz do [docs/KOD.md](docs/KOD.md)
- **Uznawaj pracę autorów.** Każda wiedza wzięta z cudzego repo (dokumentacja, komentarze w kodzie, opisy commitów) ma odnośnik do repozytorium na GitHubie (lista w `CREDITS.md`). Nie kopiujemy cudzego kodu, tylko wiedzę o formatach.
- Nowe ustalenia: zasada działania do przewodnika ([`docs/silnik/`](docs/silnik)), układ bajtów do [`docs/formaty/`](docs/formaty), przebieg eksperymentu do [`docs/BAZA_WIEDZY.md`](docs/BAZA_WIEDZY.md). Oznaczenia: ✅ potwierdzone, 🔶 hipoteza, ❓ nieznane.
