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
| [`live.sh`](live.sh) | niewidoczne (Xvfb) | gra działa w tle; wykonywanie Tcl w działającej grze i zrzuty bez restartów |
| [`live2.sh`](live2.sh) | **widoczne** | przeglądanie szablonów map w oryginale, podmiana szablonu w ok. 2 s |

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
tools/lab/live2.sh shot plik.png           # zrzut samego obrazu gry (1024×768)
tools/lab/live2.sh stop                    # zamknij grę
```

Co robi podmiana szablonu (procedura `ow_show` w grze, zdefiniowana przez `userstartup.tcl`):
1. usuwa obiekty Tcl poprzedniego szablonu i czyści mapę (`map create`; znikają też statyczne dekoracje),
2. wstawia szablon w blok (16, 16),
3. odsłania go prostokątem `FogRemover` dopasowanym do wymiaru szablonu (ujemny zasięg = prostokąt przez skałę),
4. stawia lampy co 5 bloków na całym szablonie (bez lamp jaskinie w grze są czarne) i obiekt mgły świata (`Info_Fog_Lava_c`, `Info_Fog_Kris_d` … po prefiksie nazwy),
5. ustawia kamerę na środek szablonu; zoom dobiera do szerokości szablonu (gra przyjmuje tylko 1,0–2,5).

Wymiar szablonu skrypt czyta z pliku `.pmp`, który podaje pierwsza linia `.tcl` (`map_template data/templates/X.pmp`): pierwsze 8 bajtów to szerokość i wysokość w blokach.

Ograniczenia: `shot` działa tylko przy widocznym oknie (inaczej zgłasza błąd, nie robi zrzutu całego ekranu). Myszą w grze nie odjeżdżaj daleko, bo poza szablonem jest pusta mapa.

## `capture.sh`: pojedynczy zrzut

```
tools/lab/capture.sh menu  out.png [sekundy=45]
tools/lab/capture.sh template out.png <szablon> [x y zoom kąt1 kąt2] [sekundy=60]
tools/lab/capture.sh scene out.png <plik.tcl> [sekundy=60]
```

`template` wstawia szablon w blok (16, 16), więc środek kamery to 16 + połowa wymiaru szablonu (przykład: szablon 16×4 → `24 18`). Bez `x y` kamera patrzy na (18, 18). Mgła jest odsłaniana z dodatnim zasięgiem (`fog_remove 0 50 50`), czyli promieniami zatrzymywanymi w skale: przy szablonach z kilkoma komorami widać tylko tę przy środku. Pełne odsłonięcie daje ujemny zasięg (`-50 -50`, prostokąt przez skałę), jak w `live2.sh`.

Porównanie z naszym renderem tego samego kadru: `build/release/wigview --template <szablon> --view <x−16> <y−16> zoom kąt1 kąt2 --size 1024x768 --screenshot nasz.png`.

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
