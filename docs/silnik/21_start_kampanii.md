# 21. Start kampanii i fabuła (StoryMgr)

Rozdział opisuje, jak gra zaczyna nową kampanię i jak śledzi postęp fabuły. Kampania nie ma, jak zwykłe scenariusze, stałej listy szablonów (rozdział 3). Świat składa obiekt `StoryMgr`: skrypt poziomu losuje układ jaskin wokół obozu, a potem świat dorasta w miarę kopania (generator, punkt G13 w TODO). Fabułę niosą wydarzenia (`sm_add_event`, `sm_set_event`), które ustawiają wyzwalacze i wywołanie zwrotne StoryMgr.

**Źródła i status.** Kolejność wywołań i treść skryptów: pliki gry (`Data/Gamesave/single_Campaign*.tcl`, `Data/Scripts/classes/story/STORY.TCL`, `Data/Scripts/gameplay/Campaign*.tcl`). Prawa komend `sm_*`: opracowanie [Zimennik/Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) (MIT), `Runtime/Tcl/Engine/DigglesTclEngine.cs` i `Docs/WORLDGEN_ORIGINAL_RE.md`. Układ startu sprawdzony w laboratorium (21.6). Generator świata (dorastanie) opisuje `WORLDGEN_ORIGINAL_RE.md`; u nas go jeszcze nie ma.

## 21.1 Łańcuch startu ✅

Menu „Nowa gra” wczytuje `single_Campaign.tcl`, który woła `single_CampaignX.tcl`:

```
obj_clear                                   ;# świat bez obiektów
sel /obj
set sm [new StoryMgr]                        ;# klasa z STORY.TCL, set_undeletable, stan idle
call_method $sm load_level Campaign
```

Metoda `load_level` wykonuje dwa skrypty w interpreterze StoryMgr (ich zmienne globalne to zmienne StoryMgr):
1. `scripts/gameplay/CampaignInit.tcl`: rejestr fabuły i generatora (dyplomacja ras, `ai init`, `sm_create_map 512 640`, `sm_set_digcount 32`, grupy szablonów `sm_def_temp_group`, unikalne szablony `sm_add_temp`, około 55 wydarzeń `sm_add_event`, lista zadań `newslist`);
2. `scripts/gameplay/Campaign.tcl`: mapa startowa (21.2).

`gameload` pliku `.tcl` w działającej grze nic nie robi (obserwacja przy bocie, [BOT.md](../BOT.md)): kampanię w działającej grze zaczyna dopiero powyższy skrypt wykonany wprost.

## 21.2 Mapa startowa: `Campaign.tcl` ✅

```
map create 512 640
sm_draw_stone -border ; sm_draw_stone -funnel     ;# granitowa ramka mapy i lejek
generate_color_variation 0 0 512 640 0
set_light_begin 33 ; set_view_begin 23
set_view 246.7 28.8 1.478 -0.29 0.05
set temppos {208 0} ; call templates/tcl/urw_unq_start.tcl          ;# obóz, 60×36 pól
set temppos {30 23} ; call templates/tcl/urw_unq_hoehlenmalerei.tcl ;# malowidła, 36×36
map_setlayer2 30 23 data/templates/urw_unq_hoehlenmalerei.l2m
sm_set_temp {{urw_unq_start 208 0} {urw_unq_hoehlenmalerei 30 23}}  ;# unikalne: już użyte
first_level 208 0                                                   ;# jaskinie pod obozem
del (wszystkie Wuker)                                               ;# start bez wukerów
FogRemover w (239, 14): fog_remove 0 46 22                           ;# odsłonięty obóz
```

**Granit na brzegach: `sm_draw_stone -border` i `-funnel`** ✅ (prawo za Diggles-Unity, `BdbPmpWorldBuilder.DrawStoneIntoWorld`, opis `FUN_005fc9d0`; wynik sprawdzony w laboratorium). Świat z `map create` to ziemia (klasa 2, do kopania). Malarz klasy kamienia zmienia pola klasy 1 i 2 w prostokącie losowo:

```
malarz(x0, y0, x1, y1, flagi, klasa, siła):     ;# prostokąt przycięty do mapy, bez ostatniego wiersza i kolumny
    s' = siła / (min(w, h) · 0,2)
    dla pola (x, y):
        p = 1
        flaga 4 i x > środekX: p −= ((środekX − x) / w)² / s'     ;# zanik w prawo
        flaga 8 i x < środekX: to samo                             ;# zanik w lewo
        flaga 1 i y > środekY, flaga 2 i y < środekY: to samo w pionie
        jeśli klasa pola ∈ {1, 2} i los(0..32767) ≤ p · 32768: klasa pola = klasa
-border: malarz(0, 0, W, 32, 1, 0, 2,5); malarz(0, 0, 32, H+4, 4, 0, 1); malarz(W−32, 0, W, H+4, 8, 0, 1)
-funnel: y = 16, b = (W/4)·2 − 20; dopóki b > 0: malarz(W−b, y, W, y+6, 8, 0, 1); malarz(0, y, b, y+6, 4, 0, 1); y += 4, b −= 8
```

Brzegi mapy to więc pas granitu: z boków pełny na zewnętrznych ok. 16 polach i zanikający do ok. 29 pól, u góry 32 wiersze. Pod powierzchnią „lejek”: dwa trójkąty granitu zwężające się w dół (do ok. y 136), więc od obozu da się kopać tylko w środkowej części mapy. Dołu mapy nic nie zamyka. Szablony nakładane później zastępują granit swoimi polami.

**Sprawdzenie w laboratorium:** materiał (`get_material`) w 14 punktach (brzegi, środek, lejek) w oryginale i u nas: `0 0 0 0 0 0 0 3 3 2 2 2 0 0` w obu. Liczba pól granitu w wierszach y = 2, 10, …, 154 (co 4. kolumna), oryginał: 114 115 115 116 94 95 81 79 71 55 52 43 36 28 20 11 12 11 11 11; u nas: 114 115 116 120 103 101 85 77 76 61 52 45 37 28 21 14 13 13 12 13 (różnice to losowanie).

**Szablony dzielone.** Duże szablony mają plik `.tcl` z jednym wywołaniem `split_template nazwa plik.pmp W H {części}`: lista części 16×16 pól `{skrypt plik.pmp dx dy}`. Wersja z STORY.TCL przy ustawionym `temppos` (start kampanii) od razu nakłada każdą część: `map_template data/templates/pmp/split/<pmp> (temppos+dx, temppos+dy)`, potem `call data/templates/tcl/split/<skrypt>` i jego `MapTemplateSet` w tym samym miejscu (warstwa `.l2m` części i jej obiekty). Przy `temppos 0` (dorastanie świata) części trafiają do kolejki i są nakładane po jednej co 0,2 s (`load_next_split`). Obóz `urw_unq_start` to 12 części (4 × 3), krasnale stoją w części `tBB`.

**`first_level startx starty`** losuje (`irandom`) jaskinie pod obozem. Środek układu: `std_x = startx + 36`, `std_y = starty + 40`.
- Para jaskiń z listy 5 szablonów (`urw_hol_001_a`, `urw_hol_014_b`, `urw_hol_018_c`, `urw_hol_019_b`, `urw_hol_027_a`), według jednej z 8 kombinacji; kombinacja dodaje do `std_y` 0 albo 4.
- Łącznik z 6 kombinacji tuneli `urw_gng_013/002/003`; pierwszy element kombinacji (0 albo 4) przesuwa `std_x` (`incr std_x [lrem conn 0]`).
- Zakończenia tuneli `urw_gng_021_*` z lewej i `urw_gng_022_*` z prawej, litera wariantu losowana.
- Osada Voodoo (`urw_unq_vodo_001_a` i tunele wokół niej) z jednej z 3, 6 albo 9 list, zależnie od łącznika.

Każdy szablon z listy: `sm_mark_temparea x y szablon` (pola mapy fabuły zajęte), `call data/templates/<szablon>.tcl`, `MapTemplateSet x y`. Przykład (ziarno 7 u nas): jaskinie i ognisko Voodoo na wierszach 46 i 70; w przebiegu bota w oryginale na 42 i 66.

**`lrem` zwraca usunięty element** (`incr std_x [lrem conn 0]`, `set nextevent [lrem evt_hit_list 0]` w `Krake.tcl`), a zmienna dostaje listę bez niego. Do 2026-10-13 nasz silnik zwracał nową listę (błąd, test `script_misc`).

## 21.3 StoryMgr w czasie gry ✅

`obj_init` StoryMgr ustawia zmienne (strefa `Urwald`, `maxfailures 4`, liczniki złóż na strefę, `StartCnt 5`), rejestruje się wywołaniem zwrotnym `sm_register this callback`, rejestruje i ustawia wydarzenie `GameOverCheck` i wchodzi w stan `idle`. Stan `idle` co turę logiki (10 Hz):

```
StartCnt > 0: StartCnt--, wait_time 1.0, koniec                 ;# rozgrzewka 5 s
co 7. turę, gdy GameOverCheck: brak krasnali i dzieci gracza -> GameOver (unq_tod)
przeprowadzka (Reloc_count): przesunięcie mapy o 4 pola w dół na krok
check_uebergang ×3: przejścia między światami (Urwald -> Metall -> Kristall -> Lava)
load_next: jeden szablon (albo część szablonu) z kolejki dorastania na turę -> koniec
area_list = sm_scan: pozycja przebicia kopania; != 0 -> generate_next x y   ;# generator, TODO G13
wait_time 0.5
```

## 21.4 Wydarzenia fabuły ✅ (za Diggles-Unity)

| komenda | działanie |
|---|---|
| `sm_add_event nazwa [klasa] [słowo]` | rejestruje wydarzenie z wartością 0; `klasa` to klasa wyzwalacza, `słowo` komunikatu domyślnie `activate`; zwraca numer wydarzenia. Ponowna rejestracja: gra zgłasza błąd („Story Event redefinition”), skrypty wołają ją w `catch`; u nas numer istniejącego, bez zmiany wyzwalacza |
| `sm_set_event nazwa [wartość]` | ustawia wartość (domyślnie 1) |
| `sm_get_event nazwa` | wartość; nieznane wydarzenie 0 |
| `sm_send_message obj słowo` | (1) każde nieustawione wydarzenie z klasą = klasa `obj` i słowem = `słowo` dostaje 1; (2) wywołanie zwrotne `<procedura> <klasa obj> <słowo> <id obj>` w interpreterze obiektu z `sm_register` |
| `sm_register obj procedura` | wywołanie zwrotne StoryMgr (`callback` w STORY.TCL) |

**Lista zadań.** `CampaignInit.tcl` dopisuje do `newslist` StoryMgr wpisy `{{wyzwalacz [słowo]} {podkomenda wpis wydarzenieQL [wykluczające]}}` (słowo domyślnie `shut`). Wyzwalacz wysyła `sm_send_message this shut` (sekwencja skończona). `callback` znajduje pasujące wpisy i, jeśli żadne z wydarzeń wykluczających nie jest ustawione, ustawia wydarzenie `QL_*` i wypisuje wpis dziennika (`newsticker_new/upd/ful`, okno `diary_15`). Przykład zadania pustelnika:

```
{Trigger_Einsiedler_1} {new 020 QL_Bring_Hamsters}          ;# pustelnik prosi o 6 chomików
Trigger_Einsiedler_2: 6 chomików w promieniu 5 -> urw_23b, Karte, Einsiedler_Karte_erhalten,
                      QL_Bring_Hamsters_done                ;# przebieg bota, BAZA_WIEDZY §8.17
```

## 21.5 Implementacja u nas 🔶

**Przebieg wstępny** ([`level_script`](../../src/lib/level_script.h)). Scenariusz, którego czytanie wiersz po wierszu (`parseScenario`, rozdział 3) nie daje szablonów, a który woła `load_level`, jest wykonywany w osobnym interpreterze Tcl. Jego komendy tylko zapisują wynik:
- `map create`, `map_template`, `map_setlayer2` (w kolejności skryptu);
- `new`/`qnew` z `set_owner`, `set_physic`, `set_pos`, `del`, `obj_clear`, `obj_query -class`;
- `call_method` (do powtórzenia po starcie, np. `fog_remove`);
- `irandom`, `lrem`, `set_view`, `set_light_begin`, `set_fow_begin`.

`call_method <StoryMgr> load_level P` wykonuje tu tylko `scripts/gameplay/P.tcl`, a `split_template` działa jak w 21.2. Pozostałe komendy gry nic nie robią. `buildWorld` nakłada zapisane `.pmp` i `.l2m` jak szablony (rozdział 3.3) i maluje pasy granitu `sm_draw_stone` (21.2), w kolejności skryptu. Świat to cała mapa z `map create` (512 × 640), bez przycięcia: te same współrzędne obiektów i skryptów co w grze i miejsce na wzrost świata. Wczytanie trwa ok. 2 s.

**Nakładanie w trakcie gry** (krok 2 wzrostu świata). `map_template PLIK X Y` i `map_setlayer2 X Y PLIK` w działającej grze nakładają szablon tym samym prawem co przy składaniu świata (rozdział 3.3; gra: `FUN_005992b0`, za Diggles-Unity). Szablon wygrywa z tym, co było pod nim, także z wykopanym tunelem; znaczniki kopania zostają. Potem odświeżane są nawigacja, chodzenie i materiał pól zmienionego prostokąta, a teren jest przebudowywany raz na turę logiki (🔶 cała siatka, ok. 170 ms dla 512 × 640; gra przelicza tylko zmieniony prostokąt). Kolejkę StoryMgr (`temp_buffer`) wypełnia `fill_temp_buffer` posortowany przez `lg_sort_level` (rosnąco po kwadracie odległości lewego górnego rogu szablonu od punktu `act_sort_x/y`, 🔶 za Diggles-Unity), a `load_next` nakłada jeden szablon na turę (21.3).

**W grze** ([`world_sim`](../../src/lib/world_sim.cpp), `attachScripts`): po `obj_init` wszystkich obiektów StoryMgr wykonuje `call scripts/gameplay/CampaignInit.tcl` w swoim interpreterze (`is_storymgr` = 1). Potem idą zapisane `call_method`. Komendy fabuły: [`cmd_story`](../../src/lib/script/cmd_story.cpp).

**Ziarno.** `OW_LEVEL_SEED=N` ustala losowanie `first_level` i pasów granitu (testy). Bez niego układ jest losowy przy każdym starcie, jak w grze.

**Różnice:**
- 🔶 brak `generate_color_variation` i pozostałych opcji `sm_draw_stone` (`-exlimit`, `-utmat`, `-matlist` należą do wzrostu świata);
- 🔶 świat nie dorasta sam: `sm_scan` zwraca 0 (nacisk kopania i generator `lg_*` to TODO G13); kolejka `load_next` i nakładanie w trakcie gry już działają;
- 🔶 `GameOver` i przejścia między światami nie sprawdzone.

## 21.6 Sprawdzenie w laboratorium ✅

Oryginał (`live2.sh`, ukryte okno), w działającej grze `obj_clear ; sel /obj ; set sm [new StoryMgr] ; call_method $sm load_level Campaign`, potem `set_view 246.7 28.8 1.478 -0.29 0.05`:
- krasnale w (231,54 30,44) (232,83 30,5) (234,00 30,44) (235,06 30,44) (236,10 30,5): u nas te same pozycje (test `campaign_start`);
- obraz obozu z kamery startowej zgadza się z naszym (drzewa, ścieżka, skała, płot), także beczki `Schatztonne` i grzyby w grze ze skryptami;
- jaskinie pod obozem różnią się między startami (losowanie `first_level`).

## 21.7 Wzrost świata: mapa fabuły i nacisk kopania ✅

Świat kampanii dorasta tam, gdzie gracz kopie. StoryMgr trzyma **mapę fabuły**: siatkę pól 4 × 4 pól świata (128 × 160 dla mapy 512 × 640), w której 1 znaczy „zajęte” (już wyrośnięte, wykopane albo zarezerwowane). Prawa za Diggles-Unity (`WORLDGEN_ORIGINAL_RE.md` A1–A2, `StoryDigNotify`); wynik sprawdzony w laboratorium (niżej).

| komenda | działanie |
|---|---|
| `sm_create_map W H` | mapa fabuły W/4 × H/4, pusta; nowa gra: zatrzask, nacisk 0, próg 5, strefy i rejestry od zera |
| `sm_set_digcount N` | próg nacisku (kampania: 32) |
| `sm_map_set cx cy [cw ch]` / `sm_map_get cx cy` | prostokąt pól fabuły zajęty (cały w mapie, inaczej nic) / stan pola (poza mapą 1) |
| `sm_mark_temparea x y szablon` | pola pod szablonem (rozmiar zaokrąglony w górę do 4) zajęte (`first_level`) |
| `sm_set_temp {{szablon x y} …}` | szablony unikalne użyte (nie wylosują się drugi raz) |
| `sm_draw_stone -border` / `-funnel` | oprócz granitu (21.2) zajmuje pola fabuły swoich pasów; `-border` także 9 górnych wierszy: świat nie rośnie przy brzegach ani przy powierzchni |
| `sm_add_zone`, `sm_set_zone nr y`, `sm_force_zone nr`, `sm_get_zone y` | strefy (Urwald, Metall, Kristall, Lava): strefa nr sięga do y = granica nr (−1: bez końca) |
| `sm_scan` | zatrzaśnięte przebicie jako pole fabuły „cx cy” (i oznaczenie go jako oddanego), inaczej 0 |

**Nacisk kopania** (gra: `CStoryMgr::DigNotify`), przy każdym `dig_apply`, przed wycięciem węzła (x, y):

```
jeśli węzeł bliżej niż 8 pól od brzegu mapy: koniec
jeśli sm_dig_message: wywołanie zwrotne "<callback> dig shut -1"     ;# w danych gry wyłączone
zatrzask = brak; pole fabuły węzła = zajęte
jeśli półbajt znacznika węzła nie jest znacznikiem tunelu (≠ 0, bit 3 zgaszony): koniec
dla (dx, dy, kierunek, waga) w (+4, 0, 8, 2), (0, +4, 2, 4), (−4, 0, 4, 2), (0, −4, 1, 1):
    sąsiad = (x+dx, y+dy); jeśli jego pole fabuły zajęte albo nie ma znacznika tunelu: następny
    nacisk += waga
    jeśli nacisk > próg: zatrzask = {kierunek, sąsiad}
    koniec                                                            ;# najwyżej jeden przyrost na kopnięcie
```

Nacisku nie zeruje ani `sm_scan`, ani przebicie; zeruje go dopiero udana generacja (`lg_get_level`, krok 5). Po przekroczeniu progu każde kopnięcie z oznaczonym, wolnym sąsiadem zatrzaskuje więc przebicie od nowa. StoryMgr w stanie `idle` co 0,5 s woła `sm_scan`, a z wynikiem woła `generate_next` (21.3).

**Doświadczenie w laboratorium** (czas gry zatrzymany, `gametime stop`): znaczniki tunelu w kolumnach x 300–301, y 60–99 (`dig_mark 0 x y 1`), potem `dig_apply {x y 10} krasnal` w dół. Po szybie x 300 do y 95 szyb x 301 od y 78 dał w oryginale i u nas tę samą serię `sm_scan`: 0 przez 14 kopnięć, potem `{75 24}` cztery razy (sąsiad y+4 wchodzi w wolne pole fabuły 24). Wykopane pola fabuły (75, 15–23) są zajęte w obu. U nas pierwszy zatrzask przychodzi po 9. kopnięciu (9 × 4 = 36 > 32). W oryginale pierwsze kopnięcia tej serii zwracały 0 (❓ może przez wcześniejszy nieudany skrypt), a wynik `dig_apply` na szybie x 300 szedł w rytmie 1, 0, 0, 1 (u nas zawsze 1; różnica wycinania, rozdział 19).

## 21.8 Wzrost świata: generator (lg_*) 🔶

Gdy `sm_scan` odda przebicie, StoryMgr woła `generate_next cx cy` (STORY.TCL). Ten składa jeden **klaster** szablonów i wrzuca go do kolejki `temp_buffer`, z której `load_next` nakłada po jednym szablonie na turę (21.3, krok 2). Prawa za Diggles-Unity (`LevelGenerator.cs`, `DigglesTclEngine.cs`, `Docs/LEVEL_GENERATOR.md`, `WORLDGEN_ORIGINAL_RE.md`, adresy `FUN_…` to ich źródło). U nas sprawdzone tylko tyle, że świat rośnie, a w klastrze pojawia się szablon pustelnika (test `http_growth`); porównania z oryginałem brak (❓).

**Rejestr szablonów.** `lginit.tcl` (przy starcie gry, po klasach) przez procedurę `LG_set_templategroup` z `lgtools.tcl` przegląda katalogi szablonów (`glob data/templates/tcl/urw_gng_*.tcl`) i rejestruje grupy: `Urwald.Std.Gng` (korytarze), `.Gsg` (pierścienie), `.Hol` (jaskinie; `-levelcount 1`, czyli najwyżej jedna na generację). Dla każdego szablonu generator czyta przy pierwszym użyciu:
- wymiary: z pliku `.pmp`, a dla szablonu dzielonego z nagłówka `split_template`;
- obiekty klas filtra (`lg_addfilterclass {Zwerg Wuker Troll Pilz Kohle …}`) z wierszy `new`;
- **drzwi** z pliku `.ent` (`map_entry 1 Xpx Ypx`, 8 pikseli na pole). Współrzędna jest kwantowana do wielokrotności 4 pól. Reszta z dzielenia przez 32 piksele mówi, na której krawędzi leżą drzwi, i dokłada +2 w połowie krawędzi (pozioma: x+2; pionowa: y+2). Kierunek wynika z krawędzi: x = 0 lewo, y = 0 góra, x = W prawo, y = H dół. Drzwi poza krawędzią odpadają.

**`generate_next`** (skrót STORY.TCL):

```
strefa = sm_get_zone(y·4); lg_tp_clear; lg_tp_addtemplatesets "<strefa>.Std"; lg_tp_objfilter {Troll Zwerg}
linfo = sm_get_level_info x y            ;# obszar w polach fabuły + szablon unikalny
za mały (< 16 pól)? zajmij pole, koniec
linfo = lg_set_starttemplatepro 1 1      ;# element startowy przy przebiciu, "nodigmark" = koniec
jest szablon unikalny? lg_tp_setfilter "<strefa>.Std.Gsg"   ;# bez pierścieni
define_obj_counts                         ;# lg_set_objcount: brakujące złoża i grzyby strefy
lg_set_templateratio 0 0.10 1.0 ; lg_start ; level = lg_get_level
pusty: failcount++ (nacisk kopania zostaje, kolejne kopnięcie ponawia); inaczej sm_set_temp, fill_temp_buffer
```

**`sm_get_level_info cx cy`** (FUN_005fac90):
1. Surowy wolny prostokąt rośnie od pola przebicia paskami wolnych pól fabuły, kolejno w lewo, w górę, w dół i w prawo, dopóki się da.
2. Wybór szablonu unikalnego z rejestru `sm_add_temp nazwa lo hi [-opcje]` (CampaignInit.tcl). Kandydaci idą w kolejności rejestracji. Odpadają:
   - szablon już użyty;
   - szablon z innej strefy;
   - przebicie płytsze niż `lo` (względem początku strefy);
   - przy `-mindist`: wzorzec jeszcze nieużyty;
   - przy `-middist`: przebicie w środkowej kolumnie mapy.

   Dla pozostałych losowanie: przechodzi, gdy waga ≥ los. Waga to `-nonrelevant` albo rampa (y − lo + 1)/(hi − lo); poza oknem jest ≥ 1, czyli pewna. Potem szablon musi się zmieścić w wolnym prostokącie, inaczej idzie następny kandydat.
3. Wynik (FUN_005fa9a0): prostokąt co najmniej 16 (do 24) × 8 pól wokół pola przebicia, na środku, a z szablonem unikalnym w losowym miejscu.

**`lg_set_starttemplatepro`** (FUN_00650f90):
- Element startowy stoi zawsze o jedno pole fabuły dalej niż zatrzask, w kierunku kopania (krok wstępny), więc nie nachodzi na tunel gracza. Krasnal przekopuje się do niego sam.
- Przebieg kotwiczenia: do 4 prób. W każdej znacznik tunelu w środku pola zatrzasku, świeżo wylosowany element 4 × 4 (przy kopaniu w dół korytarz poprzeczny z drzwiami lewo i prawo, w inne strony jedne drzwi „dalej”) i miejsce: wyrównane do 4, ≥ 8 pól od brzegu mapy, nie na polu pod szablonem.
- Z szablonem unikalnym prostokąt jest ustawiany na nowo: przebicie przy krawędzi od strony kopania. Szablon unikalny (albo grupa `sm_def_temp_group`) ląduje w ≤ 4 losowych miejscach daleko od przebicia.

**`lg_start`** (FUN_00589790): najpierw elementy wymuszone, czyli startowy i unikalny, każdy dokładnie w swoim miejscu; nieudany element psuje całą generację. Potem jeden klaster:

```
dopóki są otwarte drzwi:
    d = pierwsze otwarte drzwi (czoło frontu)
    pula posortowana kosztem (niżej lepiej); dla elementu i jego drzwi przeciwnych do d:
        TryPlace(element, d − drzwi): wyrównanie do 4, w obszarze, bez nakładania,
            drzwi w tym samym polu co otwarte drzwi frontu domykają je (para), pozostałe dochodzą do frontu,
            margines 4 pól po stronach bez drzwi zajęty na stałe (ściany jaskiń nie stykają się),
            walidator: każde otwarte drzwi ≥ 2 pola od brzegu obszaru i z co najmniej jednym wolnym polem wokół
        pierwsze udane: następne drzwi
    żaden element nie pasuje: cała generacja się nie udaje (prawo pełnego zamknięcia)
```

**Koszt** (FUN_00586710, stałe z `.rdata` gry): (2 · |20 − (W + H)| · (2 − postęp) + pasmo drzwi · 100 + 200 · min(limit gry, 5) + 250 · użyć w tej generacji + los) · wartość · 100 · nacisk złóż. Postęp = zapełnienie obszaru / 0,10. Pasmo drzwi = |docelowa liczba drzwi − drzwi elementu|; cel to 3 na początku, potem 2,5, 2 i 1 (od 80 % postępu ×10: klaster się domyka). Odpadają (koszt maksymalny):
- element przekraczający limit klasy obiektów;
- wariant z obiektami użyty już w tej sesji gry;
- element za blisko innego z obiektami;
- element przepełniający obszar;
- element ponad `-levelcount`.

Elementy 4 × 4 idą na koniec listy.

**`lg_get_level`**: lista `{szablon x y}`; pola fabuły pod elementami zajęte; ograniczenia złóż od zera. Gdy coś wyszło, nacisk kopania jest zerowany, więc następne przebicie wymaga nowego kopania.

**Różnice u nas:**
- 🔶 brak routera korytarza między elementem startowym a szablonem unikalnym (FUN_00587480): szablon unikalny stoi osobno i krasnale dokopują się do niego same;
- 🔶 brak przycinania ślepych odnóg (FUN_00585390);
- 🔶 brak dziedzińców `mat` grup;
- 🔶 brak `sm_draw_stone -exlimit/-utmat`;
- 🔶 losowanie z własnego generatora (`OW_LEVEL_SEED`), nie ze wspólnego MT19937 gry;
- 🔶 pole pod szablonem: z nałożeń przy składaniu i w trakcie gry, a w świecie z zapisu przybliżone jako blok inny niż 0;
- 🔶 rytm `dig_apply`: patrz 21.7.

`OW_TRACE_WORLDGEN=1` wypisuje każdy krok (`[świat] …`).

