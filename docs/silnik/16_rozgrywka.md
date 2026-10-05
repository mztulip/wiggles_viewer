# 16. Rozgrywka: mechaniki według instrukcji

Rozdziały 1–15 opisują, jak gra buduje i rysuje świat oraz jak działają skrypty. Ten rozdział zbiera to, **co gracz robi i jakie zasady rządzą grą**, czyli cel, do którego zmierza silnik. Źródłem jest polska instrukcja gry (`Instrukcja.pdf` z instalacji, 17 stron, CD Projekt 2001), streszczona własnymi słowami. Większość zasad jest zapisana w skryptach `Data/Scripts`, więc przy implementacji trzeba je odnaleźć w skryptach i sprawdzić w laboratorium.

**Status.** Wszystko poniżej to 🔶 (deklaracja instrukcji), chyba że zaznaczono ✅ (sprawdzone w działającej grze, [BAZA_WIEDZY §8.7](../BAZA_WIEDZY.md)).

## 16.1 Cel i tryby gry

- **Fabuła:** Odyn wysyła krasnale (Wigglesy) w głąb ziemi, żeby obezwładnić wilka Fenrisa łańcuchem Gleipnir. Trzeba odnaleźć pierścienie łańcucha. Gra prowadzi przez **cztery światy** (w danych: `urw` Urwald, `kris` kryształy, `lava`, `swf` siarka; rozdział 3) i ponad 100 zadań, na przestrzeni wielu pokoleń krasnali.
- **Nowa gra:** samouczek (Wprowadzenie), **kampania** (misje fabularne) i **potyczka** w dwóch odmianach: piaskownica (potwory, tylko klan gracza) albo tryb zwykły (gracz wybiera jeden z czterech klanów i walczy z trzema pozostałymi). W danych kampania i potyczka generują świat losowo (`StoryMgr load_level`, `load_sandbox`, rozdział 2.6).

## 16.2 Czas

- ✅ **Doba trwa 30 minut czasu rzeczywistego**: 12 godzin gry po 150 s ([rozdział 11.4](11_obiekty_tcl_i_petla.md)).
- Data gry na ekranie ma postać `dzień.miesiąc.rok`, np. `1.2.5`.
- **Plan dnia** każdego krasnala ustawia się na zegarze: żółta część tarczy to praca, szara to odpoczynek. Słońce i księżyc przeciąga się po tarczy; kliknięcie środka przesuwa plan o godzinę (LPM do przodu, PPM do tyłu). Krasnale są najwydajniejsze przy zrównoważonym planie. W skryptach plan to zmienna krasnala `current_time_plan` (🔶).
- **Wiek:** krasnale żyją około **24 dni** gry (czyli ok. 12 godzin rzeczywistych). Mikstury (np. Fontanna młodości) cofają krasnala do dzieciństwa z zachowaniem doświadczenia.
- Szybkość gry zmienia `+`/`-` na klawiaturze numerycznej (rejestr `Misc\MaxGametimeFactor=4`, czyli najwyżej ×4; 🔶), `Pause` zatrzymuje grę.

## 16.3 Krasnal: stan i potrzeby

- Pasek **energii życiowej**, **zdrowie**, **czujność**, **sytość**, **nastrój** (lista krasnali po wybraniu imienia). Nad głową: zielony pasek zdrowia, **grzyb** = głodny, **łóżko** = zmęczony, **księżyc** = chce wolnego, czerwona twarz = niezadowolony. Czerwone symbole oznaczają brak lub kryzys, szare to wykonywana czynność.
- Na liście krasnali przy imieniu: pasek energii, **słońce/księżyc** (powinien teraz pracować/odpoczywać; czerwone słońce: nie ma pracy, szare: pracuje), twarz (samopoczucie), krzyżyk (wyłączony z rozkazów automatycznych).
- **Tryb ręczny:** krasnal z wyłączoną ikoną trybów nie bierze udziału w rozkazach automatycznych (kopanie z oznaczeń, produkcja) i słucha tylko gracza.
- ✅ W skryptach krasnal przełącza się między stanami pracy (`work_idle`, `prodfill_dispatch`) i czasu wolnego (`sparetime_dispatch`); kolejka zadań (`tasklist_*`) zawiera animacje, chodzenie i czynności ([BAZA_WIEDZY §8.7](../BAZA_WIEDZY.md)).

## 16.4 Czynności gracza

| Czynność | Sterowanie |
|---|---|
| wybór krasnala | LPM na krasnalu albo na imieniu na liście; kilku: ramka LPM albo SHIFT + klik; odwołanie: PPM na planszy |
| ruch | wybrany krasnal + LPM na celu; **bieg**: dwuklik na celu |
| podniesienie przedmiotu | wybrany krasnal + LPM na przedmiocie (skrzynię tylko z ALT) |
| upuszczenie | klik na przedmiocie w ekwipunku i wskazanie miejsca (cień); dwuklik: upuść przed krasnalem |
| użycie (zjedzenie grzyba, wypicie piwa) | ALT + LPM na przedmiocie w ekwipunku |
| atak | wybrany krasnal + LPM na wrogu; neutralnego tylko z ALT (psuje dyplomację) |
| przydział do placówki | wybrany krasnal + ALT + LPM na placówce |
| grupy | CTRL + 0–9 przypisuje, 0–9 wybiera |

**Podświetlenie pod kursorem:** białe = do wybrania, czerwone = wróg do ataku, niebieskie = przedmiot do podniesienia, zielone = placówka do przydziału. Kursor dostaje ikonki czynności (bieg, kopanie, podnoszenie, atak, pakowanie skrzyni i inne).

## 16.5 Kopanie

- PPM w dowolnym miejscu otwiera menu kopania: tunele i jaskinie różnych rozmiarów oraz gumkę (usunięcie jednego albo wszystkich oznaczeń).
- Oznaczenie (zielony znacznik) ustawia się LPM; dłuższy tunel: przeciągnięcie z wciśniętym LPM.
- **Jaskinię można oznaczyć tylko przy istniejącym tunelu**, czyli najpierw tunel, potem jaskinia.
- Dostępne rozmiary jaskiń zależą od **doświadczenia w kamieniach**: na początku tylko malutkie, potem małe, średnie, duże.
- Oznaczenia kopią krasnale, które nie mają innej pracy; bezpośredni rozkaz: wybrany krasnal + klik na oznaczeniu. Pierwszy tunel powinien zaczynać się od powierzchni.
- W danych: komendy `dig_mark`, `dig_next`, `dig_apply`, zadania `CObj_DigTask`; nasze przybliżenie: [rozdział 14](14_gra.md).

## 16.6 Walka i dyplomacja

- Atakujący krasnal sam wybiera broń z ekwipunku według **doświadczenia i odległości**: daleko broń miotająca (proca, łuk, karabin), blisko sieczna lub kłująca.
- Krasnale **uciekają same, gdy są poważnie ranne**.
- Klany są wobec gracza przyjazne, neutralne albo wrogie. Przyjaznych nie można atakować; atak na neutralny klan (ALT) czyni go wrogim.
- W danych: `fight_action`, `fight_weapon`, `def_hp_ratio`, `set_diplomacy`/`get_diplomacy` ([BAZA_WIEDZY §8.2](../BAZA_WIEDZY.md)).

## 16.7 Produkcja, transport, wynalazki

- **Skrzynia:** każdy wyprodukowany budynek i placówka pojawia się jako drewniana skrzynia. Rozpakowanie: klik na skrzyni, potem ikona skrzyni ze strzałką w górę i ustawienie cienia (zielony = wolno postawić). Każdą placówkę można z powrotem spakować (ikona ze strzałką w dół) i przenieść.
- **Ognisko** (pierwsza skrzynia na starcie): źródło jedzenia (smażone grzyby) i miejsce badań. Ikony białe są dostępne od razu, czerwone później albo przy braku surowców.
- **Zlecenia:** klik na ikonie produkcji dodaje sztukę (LPM +1, PPM −1); **10 sztuk oznacza produkcję bez końca** (to samo daje SHIFT + klik). Pasek pod ikoną pokazuje postęp. Anulowanie zlecenia traci zużyte już surowce; dostarczone, a niezużyte, zostają w placówce.
- **Wynalazki:** ikony ze znakiem zapytania. Ponad 100 wynalazków (budynki, przedmioty, ulepszenia). Krasnal z odpowiednim doświadczeniem i wolnym czasem zakłada „czapkę naukowca” i prowadzi badania; żółta ikona oznacza, że taki krasnal jest wolny. W danych: drzewko technologii generowane przy starcie (`init_techtree`, `gen_tt.tcl`, rozdział 2).
- Przydział krasnala do placówki: pracuje tam, o ile nie dostanie innych rozkazów.

## 16.8 Doświadczenie i rozmnażanie

| Dziedzina | Rośnie przy |
|---|---|
| Drewno | rąbaniu grzybów, wyrobach z drewna |
| Jedzenie | gotowaniu |
| Kamienie | kopaniu, wyrobach z kamienia |
| Konstrukcja i transport | stawianiu budynków, noszeniu surowców |
| Metale | obróbce metali |
| Alchemia | energii, zaawansowanej technice, magii |
| Usługi | pracy w usługach (np. barman) |
| Kung-fu | walce wręcz |
| Broń jednoręczna | mieczach, maczugach |
| Broń dwuręczna | mieczach i toporach dwuręcznych |
| Broń dystansowa | procy, łuku |
| Obrona | tarczach, unikaniu i blokowaniu ciosów |

- Doświadczenie przyspiesza pracę i umożliwia nowe odkrycia. **Ma górny limit:** gdy w jednej dziedzinie przekroczy próg, w pozostałych samo maleje (specjalizacja).
- **Rozmnażanie:** krasnale zakochują się i rozmnażają tylko **w czasie wolnym i w dobrym nastroju**. Dzieci dziedziczą doświadczenie rodziców.

## 16.9 Rozkazy gracza w skryptach

Interfejs nie steruje krasnalem bezpośrednio. Kliknięcie przechodzi przez skrypt `Scripts/init/EVENTGEN.TCL`: dla zaznaczonego krasnala i wskazanego celu (miejsce, przedmiot, wróg, placówka) skrypt wybiera **zdarzenie rozkazu** komendą `evtgen_attrib -evtid <zdarzenie> -pos1 <pozycja> -subject1 <obiekt> -cursor <kursor> -desc <opis> [-text1 userevent]` i podaje kursor oraz opis do podpowiedzi. Silnik wysyła to zdarzenie do krasnala, a ten obsługuje je w `Scripts/classes/zwerg/Z_EVENTS.TCL` (`handle_event evt_task_walk { evt_task_walk_proc }` itd.).

| Zdarzenie | Rozkaz |
|---|---|
| `evt_task_walk` | idź (bieg: ❓ parametr) |
| `evt_task_pickup`, `evt_task_putdown`, `evt_task_snapitem`, `evt_task_dropall` | podnieś, połóż, chwyć, upuść wszystko |
| `evt_task_useitem`, `evt_task_objaction`, `evt_task_switch` | użyj przedmiotu, czynność na obiekcie, przełącz |
| `evt_task_dig`, `evt_task_mine`, `evt_task_harvest` | kop, wydobywaj, zbieraj |
| `evt_task_open_box`, `evt_task_buildup`, `evt_task_workprod_prefer` | rozpakuj skrzynię, stawiaj budynek, przydział do placówki |
| `evt_task_attack`, `evt_task_defend`, `evt_task_guard`, `evt_task_prodattack`, `evt_task_buildingconquer`, `evt_task_bombe`, `evt_task_kidnap`, `evt_task_convert` | walka i akcje specjalne |

Rozkazy automatyczne (oznaczenia kopania, produkcja) przychodzą jako `evt_autoprod_*` (`dig`, `walk`, `pickup`, `buildup`, `workat`…). Zdarzenie można wysłać ze skryptu komendą `set_event <źródło> <zdarzenie> -target <obiekt> -pos1 <pozycja> …` (✅ sprawdzone w grze: `set_event <krasnal> evt_task_walk -target <krasnal> -pos1 {x y z} -text1 userevent` prowadzi krasnala do celu, [BAZA_WIEDZY §8.8](../BAZA_WIEDZY.md)). Dzięki temu bot testowy może wydawać rozkazy tak jak gracz, bez klikania (TODO).

## 16.10 Interfejs i sterowanie kamerą

- Kamera: CTRL albo kółko myszy + ruch obraca, + PPM przesuwa, + LPM przybliża; kółko i `*`/`/` na klawiaturze numerycznej zmieniają przybliżenie; cyfry klawiatury numerycznej przełączają ekrany, `5` centruje.
- Klawisze funkcyjne: F1 pomoc, F2 szybki zapis, F3 lista krasnali, F4 lista placówek, F5 wynalazki, F6 drzewo genealogiczne, F7 dziennik, F8 centrowanie na krasnalu, TAB przełącza między krasnalami lub budynkami, ESC menu.
- Ekran: menu (zapis, wczytanie, krasnale z imionami i genealogią, wykres odkryć, produkcja, dziennik, pomoc), **pasek wiadomości** u góry (klik LPM: skok kamery albo okno szczegółów; PPM usuwa; przycisk przywraca stare), lista krasnali lub budynków (czapka/domek), ekwipunek, zegar z planem dnia i datą, **minimapa** z lupą (kolory: zielony = własne przedmioty, dzieci i tunele; biały = własne krasnale; żółty = neutralne; niebieski = złoża i jaskinie).
- Domyślne przypisania przycisków myszy i klawiszy można zmienić w ustawieniach (rejestr `Control`, `Input`, [BAZA_WIEDZY §8.3](../BAZA_WIEDZY.md)).

---
**Diggles-Unity** (ich opracowanie, 🔶 do sprawdzenia w laboratorium): [GNOME_BEHAVIOR_COMPLETE_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/GNOME_BEHAVIOR_COMPLETE_RE.md), [DIGGING_MECHANIC](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/DIGGING_MECHANIC.md), [COMBAT_SYSTEM_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/COMBAT_SYSTEM_RE.md), [WORK_DISPATCHER_NATIVE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/WORK_DISPATCHER_NATIVE.md), [PRODUCTION_PANEL_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/PRODUCTION_PANEL_RE.md), [ITEM_PLACEMENT_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/ITEM_PLACEMENT_RE.md), [LADDER_ELEVATOR_DOOR_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/LADDER_ELEVATOR_DOOR_RE.md), [ENERGY_SYSTEM_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/ENERGY_SYSTEM_RE.md), [TUTORIAL_MODE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/TUTORIAL_MODE.md), [BOTTOM_MENU_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/BOTTOM_MENU_RE.md), [UI_DIGGLE_INFO_PANEL_SPEC](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/UI_DIGGLE_INFO_PANEL_SPEC.md). Porównanie i stan: [mapa Diggles-Unity](../DIGGLES_UNITY.md).

**W kodzie OpenWiggles** (pomocniczo, zasada działania jest opisana wyżej): pierwsze przybliżenie rozgrywki w [`apps/game/main.cpp`](../../src/apps/game/main.cpp) ([rozdział 14](14_gra.md)), skrypty krasnali w [`lib/script/engine.h`](../../src/lib/script/engine.h). Wszystkie moduły: [mapa kodu](../KOD.md).
