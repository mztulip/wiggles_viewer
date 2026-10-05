# Teksty gry: `MESSAGES.TXT`, `QUOTES.TXT`, `LOCALE.TXT`

Pliki w `Data/` z tłumaczeniami komunikatów i cytatami. Status: ✅ format sprawdzony na danych (`test3db`), ✅ zachowanie `lmsg` i `locale` porównane z działającą grą przez WebTraq ([BAZA_WIEDZY §8.8](../BAZA_WIEDZY.md)).

**Kodowanie.** Pliki są w kodowaniu Windows tej wersji językowej gry: polska w **Windows-1250** (`ł`, `ś`, `ż`…), zachodnie w Windows-1252. Gra nie przelicza kodowania: skrypty dostają bajty z pliku, a czcionka interfejsu rysuje je w stronie kodowej wydania. Końce linii CRLF.

## 1. `MESSAGES.TXT`: tłumaczenia komunikatów

```
## "Saving"
pl "Zapisywanie"
en "Saving"
de "Saving"

## " %d seconds idle"
pl "bezczynnośc trwa %1 sek."
en " %1 seconds idle"
de "seit %1 Sekunden frei"
```

- Hasło zaczyna wiersz `## ` i ma klucz w cudzysłowie. **Klucz to tekst między pierwszym a ostatnim cudzysłowem wiersza**, więc wewnętrzne cudzysłowy należą do klucza: `## ""erfuellt""` ma klucz `"erfuellt"` i jest innym hasłem niż `## "erfuellt"` (oba są w danych).
- Pod hasłem wiersze `<język> "<tekst>"`, tak samo z tekstem między pierwszym a ostatnim cudzysłowem. Hasło kończy pusty wiersz.
- Polska wersja: 659 nagłówków, 658 różnych kluczy (`FenrisFuss` występuje dwa razy z identycznym tekstem), każde hasło ma `pl`, `en` i `de`, w tej kolejności.
- `%1`, `%2` to parametry komunikatów silnika (np. „bezczynność trwa %1 sek.”).

### `lmsg` i `locale` w grze ✅

| Wywołanie | Wynik |
|---|---|
| `locale` | bieżący język tekstów: wartość rejestru `HKCU\Software\SekOst\Wiggles\Language` (w polskiej wersji `pl`); bez tej wartości pusty napis |
| `locale en` | ustawia język (od tej chwili `lmsg` zwraca teksty angielskie) i zwraca go |
| `lmsg Saving` | `Zapisywanie`: tekst hasła w bieżącym języku |
| `lmsg saving`, `lmsg NAME`, `lmsg {Saving }` | klucz bez zmian: **dopasowanie dokładne**, z rozróżnianiem wielkości liter i spacji |
| `lmsg <brak hasła>` | klucz bez zmian, a gra dopisuje do `Data/msgmisses.txt` wiersz `## "<klucz>"` |
| `lmsg { %d seconds idle}` | tekst z podstawionym `%1`, ale bez argumentu gra wstawia przypadkową liczbę; skrypty nie używają `lmsg` z parametrami |
| `lmsg` bez argumentu | błąd `invalid arg count (1 expected)` |

Bez wartości `Language` w rejestrze (`locale` puste) każde `lmsg` zwraca klucz, czyli gra wyświetla niemieckie i angielskie klucze z kodu. `Data/msgmisses.txt` z wydania (628 wierszy) to braki zebrane przez twórców.

**Nasz silnik** ([`lib/messages.h`](../../src/lib/messages.h), `lmsg` i `locale` w [`lib/script/engine.cpp`](../../src/lib/script/engine.cpp)): zachowanie jak wyżej, przy czym bez rejestru domyślnym językiem jest język pierwszego hasła w pliku (język wydania, w polskiej wersji `pl`). Gra `openwiggles` bierze język interfejsu, jeśli plik go ma, albo wartość `--locale`. Braki trafiają do dziennika skryptów (`openwiggles_tcl.log`), a nie do katalogu gry. Język wchodzi do klucza pamięci podręcznej klas, bo ciała klas wywołują `lmsg`.

Te same kody języków wybierają pliki zależne od języka: skrypty składają nazwy jak `Scripts/text/sequences/Tutorial_[locale].txt`.

## 2. `QUOTES.TXT`: cytaty

Wiersze `<język><TAB><autor><TAB><cytat>`, np. `de	Einstein	Zeit ist das, was man an der Uhr abliest`. W polskiej wersji 293 cytaty: 259 `de` i 34 `en`, bez polskich. Gra pokazuje je przy wczytywaniu, gdy w rejestrze `Misc\ShowQuotes=1` (❓ który język wybiera dla `pl`; skrypty pliku nie czytają).

## 3. `LOCALE.TXT`

Jedno słowo i CRLF; w polskiej wersji `de`. ✅ Gra **nie** bierze z niego języka: przy `Language=pl` `locale` zwraca `pl`, a bez wartości w rejestrze zwraca pusty napis, nie `de`. ❓ Rola pliku (instalator? język danych źródłowych?).

---
**Diggles-Unity** (ich opracowanie, 🔶 do sprawdzenia w laboratorium): [ENGINE_COMMAND_GAP](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/ENGINE_COMMAND_GAP.md) (`newsticker` z `lmsg`). Porównanie i stan: [mapa Diggles-Unity](../DIGGLES_UNITY.md).
