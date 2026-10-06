# Serwer HTTP w silniku (projekt, jeszcze nie zaimplementowany)

Narzędzie deweloperskie, nie element gry: wigview i `openwiggles` uruchomione z flagą `--http PORT` wystawiają serwer HTTP, przez który wykonujemy Tcl w działającym silniku i odczytujemy stan świata, tak jak w oryginale przez **WebTraq** ([BAZA_WIEDZY §8.6](BAZA_WIEDZY.md)). Punkt w [TODO](../TODO.md): „Faza 7: dodatki”.

## Po co

- **Ten sam skrypt w oryginale i u nas.** W laboratorium `tools/lab/live.sh tcl '…'` pyta działającą grę; z serwerem identyczny Tcl trafia też do naszego silnika, a wyniki (`get_pos`, `get_attrib`, `state_get`, czas) porównujemy wprost. To podstawa bota testowego (🤖 w TODO).
- **Testy bez klikania w piksele.** Testy interakcji w [`tools/tests/run.sh`](../tools/tests/README.md) sterują dziś wigview przez `xdotool` w Xvfb (współrzędne myszy, opóźnienia ImGui). Przez API: „przejmij postać 0”, „krok o 10 tur”, „gdzie jest krasnal”, „zrzut”.
- **Podgląd na żywo** stanu obiektów, zmiennych Tcl, zdarzeń i timerów bez dopisywania śladów do kodu.

## Dwa interfejsy na jednym serwerze

**1. Zgodny z WebTraq** (narzędzia z `tools/lab` działają bez zmian także z naszym silnikiem, inny tylko port):

| Adres | Odpowiedź |
|---|---|
| `/tclshell?script=…` | wykonanie w głównym interpreterze; strona jak w grze: `Result:`, potem `Ok.` albo `Error:` ze stosem |
| `/objlist`, `/objinfo:<nazwa>` | obiekty; karta obiektu (klasa, pozycja, obrót, stan, właściciel, zadania) |
| `/classlist`, `/classinfo:<klasa>` | klasy, metody, stany |
| `/index` | wersja, czas gry, klatki |

Błędu WebTraq z ucinaniem skryptu na pierwszym `:` **nie** powtarzamy.

**2. Własne API w JSON** (rozwijane pod nasze potrzeby):

| Adres | Działanie |
|---|---|
| `POST /api/eval` (treść: skrypt) | `{"ok": true, "result": "…"}` albo `{"ok": false, "error": "…"}` |
| `GET /api/objects` | obiekty sceny: indeks, ID skryptu, klasa, obiekt `.3db`, animacja, pozycja, widoczność |
| `GET /api/units` | postacie symulacji: pole, x, y, z, najmniejsza głębokość, stan, sterowanie |
| `POST /api/pause`, `/api/resume`, `/api/step?ticks=N` | pauza mapy i krok o N tur logiki (deterministyczne testy) |
| `POST /api/control?unit=K`, `/api/control/step?dx=1`, `/api/control/goto?x=…&y=…`, `/api/control/release` | ręczne prowadzenie postaci (to samo co klik i strzałki w wigview) |
| `GET /api/screenshot` | bieżąca klatka jako PNG |
| `GET /api/wait?cond=…&timeout=S` | czekanie na warunek Tcl (np. „krasnal doszedł”) |
| `GET /` | prosta strona podglądu w przeglądarce: lista obiektów, karta obiektu, pole Tcl |

## Zasady budowy

- **Bezpieczeństwo:** serwer domyślnie wyłączony; włącza go `--http PORT` (albo zmienna `OW_HTTP`); słucha tylko na `127.0.0.1`, bo wykonuje dowolny Tcl. Domyślny port inny niż 8082 WebTraq (np. 8083), żeby nie kolidował z grą w laboratorium.
- **Wątki:** interpreter Tcl, scena i symulacja nie są bezpieczne wątkowo. Wątek serwera tylko przyjmuje żądanie i wkłada je do kolejki; pętla główna (wigview i `openwiggles`) raz na klatkę wykonuje kolejkę i oddaje wynik przez `std::promise`. Bez wyścigów z renderowaniem i symulacją; przy pauzie mapy kolejka dalej działa.
- **Miejsce w kodzie:** moduł w `src/lib` (np. `lib/dev_server.{h,cpp}`), wspólny dla obu programów; [`src/lib`](../src/lib) nie dołącza niczego z `src/apps`, więc aplikacja przekazuje wywołania zwrotne (zrzut, pauza, sterowanie). Wpis do [KOD.md](KOD.md).
- **HTTP:** mała biblioteka w jednym nagłówku (np. [cpp-httplib](https://github.com/yhirose/cpp-httplib), MIT) w `third_party/` zamiast własnej obsługi gniazd; budowanie w [`build.sh`](../build.sh) bez CMake.

## Kolejność prac

1. Serwer z `/tclshell` i `POST /api/eval`, kolejka do pętli głównej, flaga `--http`; test w zestawie: eval `gethours` i `get_pos` przez `curl`.
2. `/api/objects`, `/api/units`, pauza i krok; przeniesienie testów interakcji z `xdotool` na API (szybsze i pewniejsze).
3. Sterowanie postacią i zrzut przez API; skrypt porównujący ten sam Tcl w laboratorium i u nas.
4. Strony zgodne z WebTraq (`/objlist`, `/objinfo`, `/classinfo`) i strona podglądu `/`.
