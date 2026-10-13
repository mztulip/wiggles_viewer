# Bot przechodzący kampanię

[`tools/bot/wigbot.py`](../tools/bot/wigbot.py) gra w kampanię przez powłokę Tcl gry. Jest narzędziem do nauki, jak działa oryginał (co gra robi sama, a co musi zrobić gracz), i do szukania błędów w obu grach. Wyniki trafiają do [bazy wiedzy §8.17](BAZA_WIEDZY.md#817-bot-kampania-do-pustelnika-2026-10-13).

## Uruchomienie

```
OW_HIDDEN=1 PULSE_SINK=owlab tools/lab/live2.sh start      # oryginał w niewidocznym Xvfb (dźwięk do pustego ujścia)
tools/bot/wigbot.py orig --new --speed 5 --minutes 80     # nowa kampania, gra ×5, limit 80 min czasu gry
tools/bot/wigbot.py orig --load Data/Gamesave/bot_chomiki.sav   # od zapisu z kamienia milowego
tools/bot/wigbot.py ow --port 18778                        # nasza gra albo wigview z --http 18778
```

Dziennik: `../lab/bot/<data_godzina>/` (poza repo):
- `journal.txt`: zdarzenia i decyzje (nowe obiekty, rozkazy, kopanie, głód, śmierci, fabuła, zapisy);
- `snapshots.jsonl`: stan świata w każdym kroku;
- `terrain.txt`: ostatnia mapa terenu („.” otwarte, „x” do kopania, „#” granit).

## Jak gra

Bot rozkazuje tak jak gracz. Wysyła krasnalom zdarzenia `evt_task_*`, maluje znaczniki kopania (`dig_mark`) i ustawia zamówienia budynków (`set_prod_slot_cnt`). Niczego w świecie nie tworzy ani nie przestawia. Co krok (domyślnie 4 s czasu gry):

1. **Stan**: procedura `bot_snapshot` z [`wigbot.tcl`](../tools/bot/wigbot.tcl) (krasnale z życiem, jedzeniem i ekwipunkiem, chomiki, wukery, ogniska, grzyby, wyzwalacze pustelnika, wydarzenia fabuły `sm_get_event`). Teren: `bot_open` (wysokości `get_hmap`) co 30 s, materiał `bot_matrect` (`get_material`) raz na pole.
2. **Jedzenie**: ognisko gracza dostaje zamówienie `Grillpilz` na zapas 2 sztuk na krasnala (najwyżej 9). Zbiory, dowóz i gotowanie prowadzi planista gry, a krasnale jedzą same w czasie wolnym.
3. **Obrona**: wuker bliżej niż 15 pól od krasnala: dwóch najbliższych zdrowych krasnali dostaje `evt_task_attack`.
4. **Dostawa**: z kompletem 6 chomików krasnale, które je niosą, idą do `Trigger_Einsiedler_2` (`evt_task_walk`).
5. **Chomiki**: wolny krasnal (jedzenie > 0,35) dostaje `evt_task_pickup` najbliższego chomika, do którego da się dojść, i trzyma ten cel do 60 s.
6. **Zwiad**: pustelnik nieznany: jeden krasnal idzie do najgłębszego osiągalnego pola, a na dnie bot zaznacza szyb 2 × 6 pól w dół.
7. **Zapisy**: kamienie milowe `chomiki` (6 w rękach) i `pustelnik` (wyzwalacze widoczne) w oryginale: `gamesave Data/Gamesave/bot_<nazwa>.sav`.

**Drogi**: Dijkstra po polach siatki (otwarte 1, ziemia 10, granit nieprzejezdny). Bez kopania liczą się tylko pola otwarte. Gdy do celu nie da się dojść, bot zaznacza skałę na najtańszej drodze (tunel szeroki na 2 pola: przy pionowym odcinku pole z prawej, przy poziomym pole nad), a kopanie przydziela planista gry ([rozdział 20](silnik/20_planista.md)). Przybliżenie: drogi nie sprawdzają podłóg ani wspinaczki.

## Połączenie z grą

| | oryginał (`orig`) | nasza gra (`ow`) |
|---|---|---|
| Tcl | WebTraq `GET /tclshell?script=…` (port 8082) | `POST /api/eval` ([SERWER_HTTP](SERWER_HTTP.md)) |
| procedury bota | plik `Data/ow_bot.tcl` + `source` | treść pliku w `/api/eval` |
| czas | rzeczywisty, przyspieszony `gametime factor N` | gra zatrzymana, `POST /api/step?seconds=…` |
| nowa kampania | `--new`: `obj_clear; sel /obj; set sm [new StoryMgr]; call_method $sm load_level Campaign` (początek `single_CampaignX.tcl`; `gameload` pliku `.tcl` nic nie robi) | jeszcze nie (brak `StoryMgr`, TODO) |

Pułapki WebTraq: skrypt ze znakami `<` albo `:` się psuje (bot odmawia wysłania). Długi skrypt w GET (kilka KB) jest obcinany i może wywrócić grę, dlatego zapytania o teren są krótkie i porcjowane. Wynik strony: `Result:<br>Ok.<br>wynik<br><hr>`.
