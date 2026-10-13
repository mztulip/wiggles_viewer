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

`call_method <StoryMgr> load_level P` wykonuje tu tylko `scripts/gameplay/P.tcl`, a `split_template` działa jak w 21.2. Pozostałe komendy gry nic nie robią. `buildWorld` nakłada zapisane `.pmp` i `.l2m` jak szablony (rozdział 3.3). Świat zaczyna się w (0, 0) jak w grze (te same współrzędne obiektów i skryptów), a z prawej i od dołu jest przycięty do użytego obszaru z zapasem 12 pól litej skały.

**W grze** ([`world_sim`](../../src/lib/world_sim.cpp), `attachScripts`): po `obj_init` wszystkich obiektów StoryMgr wykonuje `call scripts/gameplay/CampaignInit.tcl` w swoim interpreterze (`is_storymgr` = 1). Potem idą zapisane `call_method`. Komendy fabuły: [`cmd_story`](../../src/lib/script/cmd_story.cpp).

**Ziarno.** `OW_LEVEL_SEED=N` ustala losowanie `first_level` (testy). Bez niego układ jest losowy przy każdym starcie, jak w grze.

**Różnice:**
- 🔶 brak `sm_draw_stone` (granitowa ramka i lejek) i `generate_color_variation`;
- 🔶 świat nie dorasta: `sm_scan` zwraca 0; generator to TODO G13;
- 🔶 przycięcie świata: z prawej i od dołu zapas 12 pól za startem, dalej koniec mapy (gra: 512 × 640);
- 🔶 `GameOver` i przejścia między światami nie sprawdzone.

## 21.6 Sprawdzenie w laboratorium ✅

Oryginał (`live2.sh`, ukryte okno), w działającej grze `obj_clear ; sel /obj ; set sm [new StoryMgr] ; call_method $sm load_level Campaign`, potem `set_view 246.7 28.8 1.478 -0.29 0.05`:
- krasnale w (231,54 30,44) (232,83 30,5) (234,00 30,44) (235,06 30,44) (236,10 30,5): u nas te same pozycje (test `campaign_start`);
- obraz obozu z kamery startowej zgadza się z naszym (drzewa, ścieżka, skała, płot), także beczki `Schatztonne` i grzyby w grze ze skryptami;
- jaskinie pod obozem różnią się między startami (losowanie `first_level`).
