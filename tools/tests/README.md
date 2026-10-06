# Testy regresji

[`run.sh`](run.sh) sprawdza, czy to, co już działa, dalej działa. Uruchamiamy go z katalogu repozytorium **przed każdym commitem**; nowa funkcjonalność albo poprawka błędu dostaje od razu swój test.

```
tools/tests/run.sh                 # wszystkie testy (ok. 2 min; najpierw ./build.sh release)
tools/tests/run.sh sim             # tylko testy z „sim” w nazwie
tools/tests/run.sh --no-ui         # bez testów interakcji (Xvfb + xdotool)
tools/tests/run.sh --list          # lista testów z opisami
tools/tests/run.sh --update-refs   # nowe obrazy wzorcowe po zamierzonej zmianie wyglądu
```

Gra: `WIGGLES_DIR` albo kopia w laboratorium `../lab/Wiggles` (tam są też zapisy i szablony testowe). Wynik: `wynik: N ok, M błędów`, kod wyjścia 1 przy błędzie.

## Rodzaje testów

| Rodzaj | Jak | Przykłady |
|---|---|---|
| bez okna | `test3db`, `owscript --eval 'Tcl'` (wynik po przewinięciu logiki), `check_docs.py`, `i18n_update.py --check` | `script_clock`, `script_gnome` |
| scena i symulacja | `wigview --screenshot` z `--print-objects` (obiekty: klasa, obiekt `.3db`, animacja; postacie: x, y, z, najmniejsza głębokość) i śladem `OW_TRACE_SIM=2` (postacie co 0,5 s) | `classes`, `sim_walls`, `sim_corridor` |
| obraz wzorcowy | zrzut porównany z `../lab/testref/<nazwa>.png` (RMSE pomniejszonych obrazów ≤ `OW_REF_TOL`, domyślnie 2; render jest deterministyczny, bez zmian RMSE = 0). Wzorce leżą **poza repozytorium** (obrazy z danych gry); brakujący wzorzec test zapisuje i przechodzi | `ref_lava`, `ref_hats` |
| interakcja | wigview w niewidocznym Xvfb (OpenGL), `xdotool`; zdarzenia w śladzie: `[ui] sterowanie: …`, `[ui] wczytano mapę` | `ui_control`, `ui_keys` |
| serwer HTTP | program w Xvfb z `--http` (losowy port), zapytania `curl`, JSON przez `jget 'd["units"][0]["x"]'` ([SERWER_HTTP.md](../../docs/SERWER_HTTP.md)); preferowany sposób sprawdzania stanu w nowych testach | `http_wigview`, `http_game` |

## Dopisywanie testu

W `run.sh`: `test_ nazwa "opis"` i funkcja `t_nazwa`, która kończy się `ok [komentarz]`, `fail "co nie tak"` albo `skip "dlaczego"`. Pomocnicze: `http_start program …` / `hget` / `hpost` / `jget` (serwer HTTP), `wv …` (wigview bez okna, 640×480), `check_trace plik` (postacie w skale albo za ścianą), `compare_ref nazwa plik`, `ui_start …` / `ui_stop`, `click_at x y`, `xd …` (xdotool).

Pułapki:
- w Xvfb mysz najpierw przesunąć, odczekać i dopiero kliknąć: natychmiastowy klik ImGui przypisuje panelowi z poprzedniej klatki;
- ślad `OW_TRACE_SIM=2` jest co 0,5 s czasu symulacji; czas symulacji stoi przy pauzie mapy;
- obraz wzorcowy zmienia się przy każdej zamierzonej zmianie wyglądu: obejrzeć `/tmp/owtest_<nazwa>.png`, potem `--update-refs`.
