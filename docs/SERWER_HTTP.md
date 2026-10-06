# Serwer HTTP w silniku

Narzędzie deweloperskie, nie element gry: wigview i `openwiggles` uruchomione z `--http PORT` (albo zmienną `OW_HTTP=PORT`) wystawiają na `127.0.0.1:PORT` serwer HTTP, przez który wykonujemy Tcl w działającym silniku i odczytujemy stan świata, tak jak w oryginale przez **WebTraq** ([BAZA_WIEDZY §8.6](BAZA_WIEDZY.md)). Kod: [`lib/dev_server`](../src/lib/dev_server.h) (serwer i kolejka), [`lib/dev_api`](../src/lib/dev_api.h) (trasy); testy: `tools/tests/run.sh http`.

```
wigview --template kris_caves_1_b --simulate --populate 2 0 --http 8083
curl 'http://127.0.0.1:8083/tclshell?script=list%20%5Bgettime%5D%20%5Bgethours%5D'
curl -X POST --data 'llength [obj_query 0 -class Zwerg -limit 100]' http://127.0.0.1:8083/api/eval
curl http://127.0.0.1:8083/api/units
curl -X POST 'http://127.0.0.1:8083/api/control?unit=0'; curl -X POST 'http://127.0.0.1:8083/api/control/step?dx=1'
curl -o klatka.png http://127.0.0.1:8083/api/screenshot
xdg-open http://127.0.0.1:8083/          # strona podglądu: pole Tcl, pauza, krok, tabela postaci z „steruj”
```

Pełna lista adresów: `GET /api` (tekst). W wigview Tcl działa po włączeniu symulacji (`POST /api/simulate?on=1`); bez niej `/api/eval` zwraca błąd z tą podpowiedzią.

## Po co

- **Ten sam skrypt w oryginale i u nas.** W laboratorium `tools/lab/live.sh tcl '…'` pyta działającą grę; z serwerem identyczny Tcl trafia też do naszego silnika, a wyniki (`get_pos`, `get_attrib`, `state_get`, czas) porównujemy wprost. To podstawa bota testowego (🤖 w TODO).
- **Testy bez klikania w piksele.** Testy interakcji w [`tools/tests/run.sh`](../tools/tests/README.md) sterują dziś wigview przez `xdotool` w Xvfb (współrzędne myszy, opóźnienia ImGui). Przez API: „przejmij postać 0”, „krok o 10 tur”, „gdzie jest krasnal”, „zrzut”.
- **Podgląd na żywo** stanu obiektów, zmiennych Tcl, zdarzeń i timerów bez dopisywania śladów do kodu.

## Dwa interfejsy na jednym serwerze

**1. Zgodny z WebTraq** (ten sam Tcl i to samo czytanie odpowiedzi co w `tools/lab/live.sh`, inny tylko port):

| Adres | Odpowiedź |
|---|---|
| `/tclshell?script=…` | wykonanie w głównym interpreterze; strona jak w grze: `Result:`, potem `Ok.` albo `Error:` ze stosem |
| `/objlist`, `/objinfo:<nazwa>` | obiekty; karta obiektu (klasa, pozycja, obrót, stan, właściciel, zadania) |
| `/classlist`, `/classinfo:<klasa>` | klasy, metody, stany |
| `/index` | program, mapa, czas gry |

Błędu WebTraq z ucinaniem skryptu na pierwszym `:` **nie** powtarzamy.

**2. Własne API w JSON** (rozwijane pod nasze potrzeby):

| Adres | Działanie |
|---|---|
| `POST /api/eval` (treść: skrypt) | `{"ok": true, "result": "…"}` albo `{"ok": false, "error": "…"}` |
| `GET /api/state` | program, mapa, czas gry, pauza, symulacja, prowadzona postać |
| `GET /api/objects` | obiekty sceny: indeks, ID skryptu, klasa, nazwa, obiekt `.3db`, animacja, pozycja, obrót |
| `GET /api/units` | postacie symulacji: pole, x, y, z, najmniejsza głębokość, stan (`idle`, `walk`, `dig`, `fall`), sterowanie, reszta trasy `path` (punkty `[x, y, z]`) |
| `POST /api/simulate?on=1\|0` | wigview: symulacja (skrypty) |
| `POST /api/pause`, `/api/resume`, `/api/step?seconds=S` | pauza mapy i przewinięcie świata o S s (domyślnie 0,1 = jedna tura logiki), także w pauzie: deterministyczne testy |
| `POST /api/control?unit=K`, `/api/control/step?dx=1&dy=0`, `/api/control/goto?x=…&y=…`, `/api/control/release` | ręczne prowadzenie postaci (to samo co klik i strzałki w wigview; skrypt postaci wstrzymany) |
| `GET /api/screenshot` | bieżąca klatka jako PNG |
| `GET /api/wait?cond=TCL&timeout=S` | czekanie, aż `expr {TCL}` da prawdę (sprawdzane co 0,1 s w wątku głównym); 408 po czasie |
| `GET /` | prosta strona podglądu w przeglądarce: lista obiektów, karta obiektu, pole Tcl |

Odpowiedzi: 404 z podpowiedzią przy nieznanym adresie, 503, gdy pętla główna nie odebrała żądania w 30 s (np. trwa wczytywanie mapy). Nieznane komendy Tcl są w silniku zaślepkami (wynik 0), więc błąd zgłasza dopiero prawdziwy błąd skryptu (`error …`).

## Zasady budowy

- **Bezpieczeństwo:** serwer domyślnie wyłączony; włącza go `--http PORT` (albo zmienna `OW_HTTP`); słucha tylko na `127.0.0.1`, bo wykonuje dowolny Tcl. Domyślny port inny niż 8082 WebTraq (np. 8083), żeby nie kolidował z grą w laboratorium.
- **Wątki:** interpreter Tcl, scena i symulacja nie są bezpieczne wątkowo. Wątek serwera tylko przyjmuje żądanie i wkłada je do kolejki; pętla główna (wigview i `openwiggles`) raz na klatkę wykonuje kolejkę i oddaje wynik przez `std::promise`. Bez wyścigów z renderowaniem i symulacją; przy pauzie mapy kolejka dalej działa.
- **Miejsce w kodzie:** moduł w `src/lib` (np. `lib/dev_server.{h,cpp}`), wspólny dla obu programów; [`src/lib`](../src/lib) nie dołącza niczego z `src/apps`, więc aplikacja przekazuje wywołania zwrotne (zrzut, pauza, sterowanie). Wpis do [KOD.md](KOD.md).
- **HTTP:** mała biblioteka w jednym nagłówku (np. [cpp-httplib](https://github.com/yhirose/cpp-httplib), MIT) w `third_party/` zamiast własnej obsługi gniazd; budowanie w [`build.sh`](../build.sh) bez CMake.

## Stan

✅ Zrobione wszystkie cztery kroki planu: Tcl i strony WebTraq (`/tclshell`, `/objlist`, `/objinfo:`, `/classlist`, `/classinfo:`, `/index`), API JSON (eval, stan, obiekty, postacie, symulacja, pauza i krok, sterowanie, zrzut, czekanie), strona podglądu `/`, w wigview i `openwiggles`. Testy `http_wigview` i `http_game` w [`tools/tests/run.sh`](../tools/tests/README.md).

Dalej: skrypt porównujący ten sam Tcl w laboratorium (`live.sh tcl`) i u nas; przeniesienie testów interakcji z `xdotool` tam, gdzie nie chodzi o samo wejście (klawisze, mysz).
