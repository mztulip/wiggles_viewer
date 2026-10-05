# Tłumaczenia / Translations

Interfejs OpenWiggles (na razie przeglądarka `wigview`) jest po polsku, a pozostałe języki to pliki gettext `.po` w tym katalogu. Język wybiera się listą w prawym górnym rogu okna albo opcją `--lang KOD`. Wybór zapisuje się w `wigview.ini`, a przy pierwszym uruchomieniu brany jest język systemu (`LANG`).

*The OpenWiggles user interface (currently the `wigview` viewer) is written in Polish; every other language is a gettext `.po` file in this directory. Pick the language from the list in the top right corner of the window or with `--lang CODE`. The choice is saved in `wigview.ini`; on first start the system language (`LANG`) is used.*

| Plik / File | Język / Language |
|---|---|
| (wbudowany / built in) | Polski (`pl`), język źródłowy / source language |
| [en.po](en.po) | English (`en`) |
| [openwiggles.pot](openwiggles.pot) | szablon / template (nie edytować / do not edit) |

## Dodanie języka / Adding a language

1. Utwórz plik z wszystkimi napisami (kod języka ISO 639-1 i nazwa w tym języku):
   *Create the file with all strings (ISO 639-1 code and the language's own name):*
   ```
   tools/i18n_update.py --new de "Deutsch"
   ```
   Albo skopiuj `openwiggles.pot` jako `de.po` w Poedit / Weblate i uzupełnij w nagłówku `Language: de` oraz `X-Language-Name: Deutsch`.
   *Or create `de.po` from `openwiggles.pot` in Poedit / Weblate and set `Language: de` and `X-Language-Name: Deutsch` in the header.*
2. Przetłumacz `msgstr` (dowolny edytor tekstu albo Poedit). Pomocą jest angielskie `en.po`, bo `msgid` są po polsku.
   *Translate the `msgstr` entries (any text editor or Poedit). Use `en.po` as a reference, since the `msgid`s are Polish.*
3. Przebuduj: `./build.sh` osadza wszystkie `lang/*.po` w programie. Nowy język pojawia się na liście.
   *Rebuild: `./build.sh` embeds every `lang/*.po` into the program, and the new language shows up in the list.*

## Zasady / Rules

- **Znaczniki printf** (`%s`, `%d`, `%zu`, `%.1f`…) muszą zostać w tej samej kolejności. Tłumaczenie z innymi znacznikami jest odrzucane przy starcie (komunikat na stderr) i program pokazuje tekst polski.
  *Keep **printf placeholders** in the same order. A translation with different placeholders is rejected at start-up (message on stderr) and the Polish text is shown instead.*
- `\n` to nowa linia (dymki podpowiedzi). *`\n` is a line break (tooltips).*
- Pusty `msgstr` albo wpis oznaczony `#, fuzzy` oznacza brak tłumaczenia, więc pokazywany jest tekst polski.
  *An empty `msgstr` or an entry marked `#, fuzzy` counts as untranslated, so the Polish text is shown.*
- Nazwy z gry i plików (`Info_Fog_*`, `set_light_begin`, `.l2m`, `Data/Gamesave`) zostawiamy bez zmian.
  *Keep game and file names (`Info_Fog_*`, `set_light_begin`, `.l2m`, `Data/Gamesave`) unchanged.*
- Font interfejsu to DejaVu Sans (łaciński, cyrylica, grecki). Pismo spoza tego zakresu (np. chińskie) wymaga dodania fontu w [`src/apps/wigview/main.cpp`](../src/apps/wigview/main.cpp).
  *The UI font is DejaVu Sans (Latin, Cyrillic, Greek); other scripts (e.g. Chinese) need an extra font in [`src/apps/wigview/main.cpp`](../src/apps/wigview/main.cpp).*

## Dla programistów / For developers

- Napis interfejsu w kodzie: `tr("Tekst po polsku")`. Wołamy `tr()` przy każdym użyciu (bez zapamiętywania wyniku w zmiennych statycznych), więc zmiana języka działa od razu.
- Napis tłumaczony później (np. przekazywany jako nazwa etapu): `N_("…")`, a w miejscu wyświetlenia `tr(zmienna)`.
- Identyfikatory ImGui: przyrostek `##id` / `###id` nie jest tłumaczony. `tr("Mapy###mapy")` daje „Maps###mapy”, a `###` utrzymuje ten sam identyfikator we wszystkich językach (np. wybrana zakładka nie zmienia się po zmianie języka).
- Po dodaniu albo zmianie napisów: [`tools/i18n_update.py`](../tools/i18n_update.py). Odświeża `openwiggles.pot` i scala każdy `.po`: nowe napisy dostają pusty `msgstr`, usunięte trafiają na koniec jako `#~`. Potem uzupełnij `en.po`. `tools/i18n_update.py --check` kończy się kodem 1, gdy w którymś języku czegoś brakuje.
- Implementacja: [`src/lib/i18n.h`](../src/lib/i18n.h), [`src/lib/i18n.cpp`](../src/lib/i18n.cpp) (parser podzbioru `.po`: bez `msgctxt` i liczby mnogiej).
- Komunikaty na konsoli i błędy z biblioteki ([`src/lib`](../src/lib), np. błędy parsera) zostają po polsku.
