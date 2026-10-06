# Narzędzia laboratorium: oryginalna gra pod Wine

Wszystkie skrypty uruchamiają **kopię** gry z `../lab/Wiggles` (osobny prefiks Wine `../lab/prefix`), nigdy instalację w `~/.wine`. Przygotowanie laboratorium: [`setup_lab.sh`](setup_lab.sh). Opis laboratorium i wyniki eksperymentów: [BAZA_WIEDZY §8–9](../../docs/BAZA_WIEDZY.md).

**Przed każdym użyciem** płyta musi być zamontowana (bez niej gra kończy się po cichu):
```
findmnt /dev/sr0 || udisksctl mount -b /dev/sr0
```
Skrypty uruchamiamy z katalogu repozytorium (`openwiggles_git`).

| Skrypt | Okno | Do czego |
|---|---|---|
| [`capture.sh`](capture.sh) | niewidoczne (Xvfb) | jeden zrzut: menu, szablon albo własna scena; gra startuje i kończy się za każdym razem (ok. 1 min) |
| [`capture2.sh`](capture2.sh) | **widoczne** | jak `capture.sh`, ale w widocznym oknie Wine i z odsłonięciem szablonu prostokątem `fog_remove 0 -50 -50` (przez skałę, cały szablon); zrzut okna przez `xdotool` + `import`, więc okno musi być widoczne |
| [`live.sh`](live.sh) | niewidoczne (Xvfb) | gra działa w tle; wykonywanie Tcl w działającej grze i zrzuty bez restartów |
| [`live2.sh`](live2.sh) | **widoczne** | przeglądanie szablonów map w oryginale, podmiana szablonu w ok. 2 s |

Do eksperymentów z trybem rysowania linków `.3db`: [`set_link_flags.py`](set_link_flags.py) (podmiana `flags` w kopii pliku modelu, `--list`, `--restore`).

`live.sh` i `live2.sh` używają tego samego portu WebTraq (8082), więc nie działają równocześnie.

## `live2.sh`: przeglądanie szablonów w oryginale

```
tools/lab/live2.sh start [szablon]         # uruchom grę w oknie i pokaż szablon (domyślnie ostatni oglądany albo pierwszy z listy)
tools/lab/live2.sh show lava_unq_vampy_tuer   # pokaż konkretny szablon (nazwa pliku z Data/Templates bez .tcl)
tools/lab/live2.sh next unq_raetsel        # następny szablon pasujący do filtra (filtr zostaje zapamiętany)
tools/lab/live2.sh next                    # kolejny z tym samym filtrem
tools/lab/live2.sh prev                    # poprzedni
tools/lab/live2.sh list lava               # lista szablonów pasujących do filtra (* = bieżący)
tools/lab/live2.sh view 8 2 1.6            # kamera względem lewego górnego rogu szablonu: x y [zoom dx dy]
tools/lab/live2.sh tcl 'get_view'          # dowolny Tcl w działającej grze (bez znaku ":" w skrypcie)
tools/lab/live2.sh shot plik.png           # zrzut prosto z bufora klatki gry (1024×768; .png, .jpg albo .ppm)
tools/lab/live2.sh shot-all zrzuty/ lava_unq   # wszystkie szablony pasujące do filtra po kolei: zrzuty/<szablon>.png
tools/lab/live2.sh stop                    # zamknij grę
OW_HIDDEN=1 tools/lab/live2.sh start X     # to samo w niewidocznym Xvfb (okno nie przejmuje klawiatury; shot działa)
```

Porównanie jasności z wigview: `--lab-lamps` stawia te same lampy co `ow_show` (zamiast lampy podglądu), np. `build/release/wigview --template lava_unq_vampy_tuer --lab-lamps --view 8 2 1.8 -0.25 0.1 --size 1024x768 --screenshot nasz.png`.

Co robi podmiana szablonu (procedura `ow_show` w grze, zdefiniowana przez `userstartup.tcl`):
1. usuwa obiekty Tcl poprzedniego szablonu i czyści mapę (`map create`; znikają też statyczne dekoracje),
2. wstawia szablon w blok (16, 16),
3. odsłania go prostokątem `FogRemover` dopasowanym do wymiaru szablonu (ujemny zasięg = prostokąt przez skałę),
4. stawia lampy co 5 bloków na całym szablonie (bez lamp jaskinie w grze są czarne) i obiekt mgły świata (`Info_Fog_Lava_c`, `Info_Fog_Kris_d` … po prefiksie nazwy),
5. ustawia kamerę na środek szablonu; zoom dobiera do szerokości szablonu (gra przyjmuje tylko 1,0–2,5).

Wymiar szablonu skrypt czyta z pliku `.pmp`, który podaje pierwsza linia `.tcl` (`map_template data/templates/X.pmp`): pierwsze 8 bajtów to szerokość i wysokość w blokach.

**Zrzuty z bufora klatki** ([`glgrab.c`](glgrab.c)): `start` buduje `../lab/glgrab.so` i uruchamia grę z `LD_PRELOAD`. Biblioteka przechwytuje `eglSwapBuffers` (Wine 11 rysuje OpenGL przez EGL; wywołania GLX też obsługuje) i na prośbę (plik `../lab/grab/grab.request` ze ścieżką) zapisuje tylny bufor OpenGL tuż przed pokazaniem klatki. Obraz jest piksel w piksel taki, jaki rysuje gra, bez ramek okna i rozciągania przez menedżer okien, i **działa także przy zasłoniętym albo schowanym oknie** (gra rysuje dalej). `shot` czeka na klatkę do 15 s (wczytywanie dużego szablonu); `shot-all` po każdej podmianie czeka 3 s (`OW_SETTLE=5` wydłuża). Gra uruchomiona bez `glgrab.so` (starszym `live2.sh`) nie odpowie na `shot`: `stop`, potem `start`.

Ograniczenia: myszą w grze nie odjeżdżaj daleko, bo poza szablonem jest pusta mapa. Szablony większe niż 112×80 bloków nie zmieszczą się w mapie `map create 128 96`.

## `capture.sh`: pojedynczy zrzut

```
tools/lab/capture.sh menu  out.png [sekundy=45]
tools/lab/capture.sh template out.png <szablon> [x y zoom kąt1 kąt2] [sekundy=60]
tools/lab/capture.sh scene out.png <plik.tcl> [sekundy=60]
```

`template` wstawia szablon w blok (16, 16), więc środek kamery to 16 + połowa wymiaru szablonu (przykład: szablon 16×4 → `24 18`). Bez `x y` kamera patrzy na (18, 18). Mgła jest odsłaniana z dodatnim zasięgiem (`fog_remove 0 50 50`), czyli promieniami zatrzymywanymi w skale: przy szablonach z kilkoma komorami widać tylko tę przy środku. Pełne odsłonięcie daje ujemny zasięg (`-50 -50`, prostokąt przez skałę), jak w `live2.sh`.

Porównanie z naszym renderem tego samego kadru: `build/release/wigview --template <szablon> --view <x−16> <y−16> zoom kąt1 kąt2 --size 1024x768 --screenshot nasz.png`.

`capture2.sh` przyjmuje te same argumenty (`menu`, `template`, `scene`); w trybie `scene` domyślnie czeka 10 s, a w `menu` 30 s.

## `live.sh`: gra w tle, Tcl i zrzuty

```
tools/lab/live.sh start [scena.tcl]        # np. scena z "gameload Data/Gamesave/save_kampania.sav"
tools/lab/live.sh tcl 'list [gettime] [gethours]'
tools/lab/live.sh page /objinfo:Ole
tools/lab/live.sh shot zrzut.png
tools/lab/live.sh stop
```

Pułapki (BAZA_WIEDZY §9.8–9.10):
- sprawdzaj, czy `tcl` zwraca `Ok.`; błąd idzie w treści wyniku, a nie w kodzie wyjścia;
- WebTraq ucina skrypt na pierwszym znaku `:` (więc bez `::`);
- dekoracje `Dummy_*` po utworzeniu nie mają już obiektu Tcl (`invalid object`);
- nowy krasnal (`new Zwerg`) chodzi sam: `state_disable $G`;
- zapis bez `FogRemover` jest czarny (mgła wojny);
- `set_view` przycina zoom do 1,0–2,5.
