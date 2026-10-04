# Dokumentacja OpenWiggles

Ta dokumentacja ma wystarczyć, żeby **samodzielnie napisać silnik** zgodny z oryginalnym Wiggles (Diggles, SEK-Ost 2001), bez zaglądania do naszego kodu. Opisuje, jak gra działa i dlaczego, a tam, gdzie czegoś nie wiemy, mówi o tym wprost.

## Jak czytać

1. **[Przewodnik po silniku](silnik/)**: rozdziały w kolejności, w jakiej powstaje obraz gry, od startu programu do gotowej klatki. Każdy opisuje zasadę działania, podaje algorytm w pseudokodzie i przykład liczbowy.
2. **[Formaty plików](formaty/)**: dokładny układ bajtów każdego pliku. Do nich odsyłają rozdziały przewodnika.
3. **[Baza wiedzy](BAZA_WIEDZY.md)**: dziennik badań. Skąd co wiemy, materiały referencyjne, laboratorium, otwarte pytania. Nie trzeba jej czytać, żeby pisać silnik.

## Przewodnik po silniku

| Rozdział | Treść |
|---|---|
| [1. Przegląd](silnik/01_przeglad.md) | Co robi silnik, katalogi danych, jednostki, układ współrzędnych, słowniczek |
| [2. Start gry i skrypty](silnik/02_start_i_skrypty.md) | `systeminit.tcl`, Tcl 8.3 z preprocesorem, klasy obiektów, scenariusze |
| [3. Świat z szablonów](silnik/03_swiat_z_szablonow.md) | `map create`, nakładanie szablonów, warstwy świata, obiekty szablonu |
| [4. Teren](silnik/04_teren.md) | Siatki bloków z BlockDB, rodzaje gruntu, nakładki detali, otwory do tła |
| [5. Obiekty i modele](silnik/05_obiekty.md) | Od nazwy klasy do modelu `.3db`, ustawienie w świecie, animacja |
| [6. Kamera](silnik/06_kamera.md) | `set_view`, perspektywa, przycinanie kątów, przejście do OpenGL |
| [7. Oświetlenie](silnik/07_oswietlenie.md) | Światło bazowe, niebo, słońce, lampy, nasycenie |
| [8. Tło: strefy horyzontu i panorama](silnik/08_tlo.md) | Kolor tła jaskiń, zanikanie w głąb, niebo z paralaksą, dywan łąki |
| [9. Klatka obrazu](silnik/09_klatka.md) | Kolejność rysowania i stany renderu w jednym miejscu |
| [10. Czego jeszcze nie wiemy](silnik/10_niewiadome.md) | Luki: mgła wojny, cienie, generator świata, rozgrywka |

## Formaty plików

| Plik | Format |
|---|---|
| `Data/3db/*.3db` | [3db.md](formaty/3db.md): modele i animacje wierzchołkowe |
| `Data/Texture/**.tga`, `Data/Ground/**.tga` | [tekstury.md](formaty/tekstury.md): TGA 32 bit, katalogi, wyszukiwanie |
| `Data/SPHERE.DAT` | [sphere_dat.md](formaty/sphere_dat.md): 256 normalnych |
| `Data/BlockDB0/1/2.BDB` | [blockdb.md](formaty/blockdb.md): kształty bloków terenu |
| `Data/Templates/*.pmp`, `*.l2m`, `*.tcl` | [szablony.md](formaty/szablony.md): szablony map |

## Oznaczenia

- ✅ **potwierdzone**: sprawdzone na wszystkich danych gry albo porównaniem ze zrzutem z oryginału.
- 🔶 **hipoteza**: działa i daje zgodny obraz, ale nie wiemy, czy gra robi to tak samo. Można polegać, warto mieć na oku.
- ❓ **nieznane**: wiadomo, że coś jest, nie wiadomo jak działa.

## Skąd ta wiedza

Formaty i prawa działania pochodzą z trzech źródeł:
1. **Dane gry**: każdy parser sprawdzamy na wszystkich plikach (np. „wskaźnik kończy dokładnie na końcu pliku dla 43 z 43 modeli”).
2. **Obserwacja działającej gry** w laboratorium pod Wine: zrzuty ekranu przy zmienianych parametrach, sonda Tcl, eksport sceny. **Nie deasemblujemy exe.** Metoda: [BAZA_WIEDZY.md §8](BAZA_WIEDZY.md).
3. **Opracowania społeczności**, zawsze z odnośnikiem i zweryfikowane eksperymentem. Najważniejsze: [Zimennik/Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) (MIT) i [djunkles/Diggles3db](https://github.com/djunkles/Diggles3db). Pełna lista w [CREDITS.md](../CREDITS.md).

Implementacja wzorcowa jest w `src/` (C++17, OpenGL 3.3). Przewodnik podaje jej pliki tylko pomocniczo: każde prawo da się zrealizować na podstawie samego opisu.
