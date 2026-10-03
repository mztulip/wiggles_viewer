# Podziękowania i źródła

OpenWiggles nie powstałby bez pracy społeczności Wiggles/Diggles. Poniżej są projekty, z których czerpiemy wiedzę: opisy formatów, działanie skryptów i struktura danych. **Nie kopiujemy ich kodu.** Silnik jest pisany od nowa, a wiedzę z tych repozytoriów opisujemy w [`docs/BAZA_WIEDZY.md`](docs/BAZA_WIEDZY.md) z odnośnikiem do źródła.

Wszelkie prawa do gry *Wiggles / Diggles – The Myth of Fenris* należą do jej twórców (SEK-Ost, Innonics). Repozytorium nie zawiera danych z gry.

## Repozytoria

| Repozytorium | Autor | Licencja | Co z niego wiemy |
|---|---|---|---|
| [djunkles/Diggles3db](https://github.com/djunkles/Diggles3db) | djunkles; starsze przeglądarki C# i Python 2.7 pochodzą z [forum Wiggles](https://wiggles.ruka.at/forum/viewtopic.php?f=10&t=105&start=10#p1389) | brak | Format modeli `.3db` |
| barnacle („Diggles SDK”) | *autor do ustalenia* | MIT | Tcl 8.3.2 w grze, ładowanie skryptów (`call`, preprocesor, cache), adresy funkcji exe |
| [ZarfaZ/DigglesHDmod](https://github.com/ZarfaZ/DigglesHDmod) | ZarfaZ | brak | Układ tekstur i logika tekstur terenu (`Ground/`) |
| [DigglesMods/DigglesModManager](https://github.com/DigglesMods/DigglesModManager) | DigglesMods (cech12 i inni) | GPL-3.0 | Format modów i łatek `change_*.tcl` |
| [DigglesMods/WigSound](https://github.com/DigglesMods/WigSound) | DigglesMods | GPL-3.0 | Skrypty sekwencji, dialogi `Sound/Sequenzen` |
| [DigglesMods/BugFix](https://github.com/DigglesMods/BugFix) | cech12 | GPL-3.0 | Fragmenty skryptów postaci |
| [DigglesMods/BetterStorage](https://github.com/DigglesMods/BetterStorage) | cech12, Chris1231, Zed2000 | GPL-3.0 | Skrypty magazynu i transportu |
| [DigglesMods/Barrow](https://github.com/DigglesMods/Barrow) | Chris1231 | GPL-3.0 | Struktura modu, `Data_HD` |
| [DigglesMods/StoneDoor](https://github.com/DigglesMods/StoneDoor) | Chris1231 | GPL-3.0 | Drzewko technologii, tooltipy |
| [DigglesMods/RareResources](https://github.com/DigglesMods/RareResources) | Chris1231 | GPL-3.0 | Produkcja |
| [DigglesMods/ProductionStatistics](https://github.com/DigglesMods/ProductionStatistics) | zed2000 | GPL-3.0 | Pełne skrypty menedżera produkcji |
| [DigglesMods/DroppingResources](https://github.com/DigglesMods/DroppingResources) | cech12 (pomysł: iGilga) | GPL-3.0 | Skrypty kopania |
| [DigglesMods/ElfPatch](https://github.com/DigglesMods/ElfPatch) | cech12 | GPL-3.0 | `def_texture` i warianty tekstur |
| [DigglesMods/Translations](https://github.com/DigglesMods/Translations) | DigglesMods | GPL-3.0 | (tłumaczenia modów) |
| [DigglesMods/DigglesModActions](https://github.com/DigglesMods/DigglesModActions) | DigglesMods | MIT | (CI modów) |
| [Wiggles-nextGen/wng-game](https://github.com/Wiggles-nextGen/wng-game) | Wiggles-nextGen | GPL-2.0 | Wcześniejsza próba nowego silnika (Godot) |
| [Wiggles-nextGen/wng-server](https://github.com/Wiggles-nextGen/wng-server) | Wiggles-nextGen | GPL-2.0 | j.w. (serwer) |

## Inne źródła
- [Wiggles Forum](https://wiggles.ruka.at/forum/), a w nim wątek „Game internal information” (cech12, 2021) z informacjami od zespołu Mirage (ParaWorld mod) o `sphere.dat`, parametrach exe i rejestrze.

## Warte sprawdzenia
- [Zimennik/Diggles-Unity](https://github.com/Zimennik/Diggles-Unity)
- [4BooM04/DigglesSteamdeckModes](https://github.com/4BooM04/DigglesSteamdeckModes)
