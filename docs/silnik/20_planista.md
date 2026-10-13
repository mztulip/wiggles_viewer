# 20. Planista pracy krasnali

Rozdział opisuje, skąd krasnale biorą pracę bez klikania gracza. W grze nie ma skryptu, który przydziela krasnalom gracza kopanie, zbiory czy produkcję: robi to wbudowany planista (gra: `CProdPlanner`). Co pewien czas zbiera zadania ze świata, wybiera bezczynnego krasnala i wysyła mu zdarzenie `evt_autoprod_*`. Dalej prowadzą go niezmienione skrypty (`z_events.tcl`, `z_work_states.tcl`, `z_dignwalk.tcl`).

**Źródło i status.** Prawa i stałe pochodzą z opracowania [Zimennik/Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) (MIT): `Docs/WORK_DISPATCHER_NATIVE.md`, `Runtime/Engine/Sim/ProdDispatcher.cs` (adresy `FUN_…` gry i stałe odczytane z `.rdata` to ich źródło). U nas całość jest 🔶. Zaimplementowane: pula bezczynnych, kopanie, wynalazki, produkcja, dowóz materiałów, zbiory, pakowanie, rozpakowanie i budowa, porażki.

## 20.1 Pula bezczynnych

Krasnal wchodzi do puli komendą `gnome_idle this 1`. Wywołuje ją jego skrypt przy wejściu w stan `work_idle`, gdy jest w oknie pracy i nie ma zadania (`z_work_states.tcl`). `gnome_idle this 0` przy wyjściu ze stanu go wypisuje. Planista bierze z puli tylko krasnale, które:
- są w swoim oknie pracy (godzina doby = czas gry / 150 s, doba 12 h; okno `set_worktime`, rozdział 11.4);
- mają włączone automatyczne przydzielanie (`get_prodautoschedule` = 1, domyślnie tak);
- nie są przydzielone w ostatnich 2 turach logiki i nie odpoczywają po porażce (20.7).

## 20.2 Przegląd

Co 0,5 s planista przegląda jednego gracza, po kolei 0, 1, …, 7 (gracz 0 co 4 s). Przegląd:

```
zadania = kopanie (20.3) + zbiory (20.5) + zadania budynków (20.4, 20.6)
utrzymaj_prośby_kopania(gracz)
pula -= krasnale nie spełniające warunków 20.1
sortuj zadania malejąco po potrzebie
dla zadania (od największej potrzeby):
    pomiń, jeśli w promieniu 0,8 od niego nieudane zadanie jeszcze stygnie (2 s)
    krasnal = z puli najtańszy wg kosztu dopasowania (bez odpoczywających, bez trwającego planu;
              wynalazek: tylko krasnal spełniający wymagania)
    zarezerwuj miejsce zadania na 6 s (placelock w promieniu 0,8), inaczej następne zadanie
    przydziel (2 tury blokady), wyślij evt_autoprod_*; KONIEC przeglądu (jedno zadanie na przegląd)
```

**Koszt dopasowania** to ważony kwadrat odległości: Δx² + (5·Δy)² + (0,25·Δz)² (oś pionowa liczy się pięć razy mocniej, bo wspinanie jest wolne; głębokość słabo). Dla kopania koszt powyżej 120 odpada, a każdy inny kopiący krasnal gracza bliżej niż √15 od zadania mnoży koszt przez 1,5, a bliżej niż √30 jeszcze raz przez 1,5. Krasnale rozchodzą się więc do różnych frontów.

## 20.3 Kopanie: prośby i zadania

Gra nie robi zadania z każdego zaznaczonego pola. Każdy gracz ma listę **próśb** `{x, y, licznik, wiek}`:

1. **Kopiący odświeżają.** Krasnal, który kopie (nie jest w puli, a jego `current_worktask` = `dig`), ustawia licznik 10 pierwszej prośbie w odległości Manhattan < 5 od swojego punktu kopania (`current_digpos`), a bez takiej zakłada nową (licznik 1).
2. **Odliczanie.** Licznik maleje o 1 przy budowie zadań i o 1 przy utrzymaniu (2 na przegląd). Prośba z licznikiem ≤ 0, której pole nie ma już znacznika, znika; czekająca się starzeje (wiek + 1).
3. **Przegląd prostokąta znaczników** po 2000 pól na przegląd: zaznaczone pole daje nową prośbę tylko wtedy, gdy żadna istniejąca nie leży bliżej niż 20 pól (Manhattan). Cały zaznaczony obszar 10×10 to jedna prośba i jeden kopiący.

Zadanie powstaje z prośby o liczniku ≤ 0. Potrzeba = wiek · 0,5 + 1 (7,5, gdy prośba już wcześniej dała zadanie). Pozycją zadania jest krawędź zaznaczonego obszaru (`get_digedge` od pola prośby, 17.7), z = 14. Przydział ogłasza kopanie (`gnome_announce_dig`): wszystkie prośby w promieniu 20 dostają licznik 10, więc obszar zamyka się dla drugiego kopiącego. Skrypt kopiący sam ogłasza dalej przy każdym kroku.

Zdarzenie: `set_event <krasnal> evt_autoprod_dig -pos1 {x y 14}`. Skrypt ustawia `current_worktask dig`, zmienia czapkę, dodaje `dig_starttask` i przechodzi do `work_auto`. Dalej idzie z pętlą z rozdziału 19.1: krawędź, stanowisko, `dig_next`, `dig_check`, animacja, `dig_execute` (`dig_apply`). Gdy węzłów brak, wraca do `work_idle` i znów zgłasza się do puli.

`dig_check` sprawdza trzy pola obok węzła od strony krasnala. Kierunek daje `get_angle_num4 poz cpoz`: kąt od poz do cpoz w płaszczyźnie x/y, skwantowany do 0…3 (1 = +x, 2 = +y, 3 = −x, 0 = −y) wzorem `(63 − trunc((kąt − 3π/4 + 10π) / (−π/2))) & 3`.

## 20.4 Budynki: wynalazki, produkcja, dowóz

Budynek gracza (nie krasnal, nie w ekwipunku) daje zadania, jeśli nikt przy nim nie pracuje. Gra pamięta krasnala przy budynku (**zajęcie**). Zajęcie znika, gdy krasnal zgłosi bezczynność bez trwającego planu, przy jego porażce albo gdy krasnal zniknie. Zwolnienie zeruje postęp produkcji (`prod_cur`, `prod_max`) i zlecenia budowy i rozpakowania.

Kolejność dla jednego budynku: wynalazki, potem pierwsze pasujące z: pakowanie (20.6), budowa, rozpakowanie, produkcja. Wynalazki i produkcja wymagają, żeby budynek był **dostępny**: zbudowany (`get_buildupstate`), nie jest skrzynią (`get_boxed`), ma zamówienia (`set_prod_slot_cnt`), ma włączone `prodautoschedule`, `prod_enabled` i `prod_schedule`.

**Priorytet zamówienia.** Każde zamówienie (budynek, przedmiot) ma priorytet od 0 do 1. Rośnie o 0,0005 na turę logiki (0,0003 dla zamówienia „bez końca”, liczba 10). Spada do 0, gdy zamówienia nie ma.

**Wynalazek** (gra: kind 2). Zamówiony przedmiot, którego właściciel jeszcze nie zna (`get_owner_attrib gracz Bp<przedmiot>` = 0), daje zadanie z potrzebą = priorytet · 10. Wymagania czyta metoda budynku `prod_item_attribs przedmiot`: pary {atrybut wartość} z `techtreetunes` (zmienne `tttinvent_*`), np. `{exp_Nahrung 0.2}`. Zadanie powstaje tylko wtedy, gdy jakiś krasnal gracza je spełnia (`get_attrib` ≥ wartość), i tylko taki krasnal może je dostać. Zdarzenie: `evt_autoprod_workat -subject1 budynek -text1 Bp<przedmiot>`; dalej prowadzi skrypt krasnala (`z_work_prod.tcl`).

**Produkcja** (kind 3). Pierwsze zamówienie wynalezionego przedmiotu. Materiały daje metoda `prod_item_materials przedmiot`. Każdy przedmiot w ekwipunku budynku pokrywa jedną pozycję (budynki z `prod_materialneed` = 0, np. farmy, nie potrzebują materiałów).
- Wszystko jest: zadanie z potrzebą = liczba zamówionych + 2, zdarzenie `evt_autoprod_workat -subject1 budynek -text1 przedmiot`. Jeden pracujący na budynek.
- Czegoś brak: dla każdej brakującej pozycji najbliższe **źródło** tej klasy: wolne w świecie albo na półce magazynu (`get_instore`), gracza albo niczyje, nie zablokowane (`get_lock`), nie w drodze. Źródło tuż przy budynku (|Δx| < 4, |Δy| < 2, |Δz| < 4) budynek bierze sam: `set_event budynek evt_prodplace_transferprod -target budynek -subject1 przedmiot`. Dalsze źródło daje zadanie **dowozu** (kind 4) z pozycją budynku. Bez źródła nie ma zadania, czeka zbiór (20.5).

**Dowóz** to plan krokowy (20.6): czapka `evt_change_muetze -text1 transport` → `evt_autoprod_pickup` (z półki: `evt_autoprod_pickup_store`) `-subject1 przedmiot -text1 {klasa}` → `evt_autoprod_transferprod -subject1 budynek -subject2 przedmiot -text1 {klasa}`.

## 20.5 Zbiory

Brakującej pozycji nie ma nigdzie i nikt jej nie wiezie, a da ją **zasób**: kapelusz (`Pilzhut`) i trzon (`Pilzstamm`) daje grzyb (`Pilz`). Planista szuka najbliższego wolnego grzyba gracza albo niczyjego i daje zadanie z potrzebą = niedobór · 0,2: `evt_autoprod_harvest -subject1 grzyb -text1 Pilz`. Jedno zadanie zbioru na przegląd. Po wysłaniu gracz ma 150 tur logiki (15 s) przerwy w zbiorach, a porażka krasnala skraca ją do 2 s. Skrypt (`evt_autoprod_harvest_proc`) innym zasobom niż grzyb sam zleca wydobycie (`mine`).

## 20.6 Pakowanie, budowa, rozpakowanie: plany krokowe

Część zadań to **plany**: kilka zdarzeń po kolei. Każde zgłoszenie bezczynności krasnala (`gnome_idle this 1` po skończonym kroku) przesuwa plan o krok, a nie zwalnia budynku. Bezczynność po ostatnim kroku kończy plan i od razu zwalnia budynek (🔶: gdyby zwolnienie czekało na następną bezczynność, wyzerowałoby zlecenie wydane w międzyczasie, np. rozpakowanie zaraz po pakowaniu). Krok, który się nie udał (np. przedmiot nie trafił do rąk), kończy plan jak porażka (20.7).

| Zlecenie | Skąd | Kroki |
|---|---|---|
| pakowanie | `set_prod_pack budynek 1` (przycisk w oknie budynku) | czapka `pack` → `evt_autoprod_pack -subject1 budynek` |
| budowa | `set_prod_buildup` (budynek stoi, nie jest skrzynią) | `evt_autoprod_unpack -subject1 budynek -text1 {klasa} -pos1 {pozycja budynku} -num1 0` |
| rozpakowanie | `set_prod_unpack` (skrzynia, miejsce z okna budowy) | czapka `unpack` → podniesienie skrzyni → `evt_autoprod_unpack … -pos1 {miejsce} -num1 jako_skrzynia` |
| dowóz | 20.4 | czapka `transport` → podniesienie → `evt_autoprod_transferprod` |

Zlecenia budowy i rozpakowania znikają przy wysłaniu (nieudane przyniesienie skrzyni przywraca rozpakowanie). Zlecenie pakowania zostaje do spakowania: dopiero `set_boxed budynek 1` (metoda `packtobox` z `genericprod.tcl`) je zdejmuje. Skrypt pakowania (`prod_builddown`) sprawdza `get_prod_pack` w trakcie, więc wyzerowanie zlecenia przerywa pakowanie. Rozbieranie prowadzi `prod_builddown_rep`: krasnal staje w punktach zaczepienia budynku (`get_linkpos`, obrót `get_linkrot`) i krok po kroku obniża `buildupstep` do skrzyni.

## 20.7 Porażki

`gnome_failed_work this` / `gnome_stopped_work this` (skrypt woła je, gdy chód się nie udał albo zadanie przerwano) zdejmuje rezerwację krasnala, zwalnia jego budynki i kończy jego plan. Krasnal 5 s nie dostaje nowych zadań, a jego zadanie w promieniu 0,8 przez 2 s nie wraca do nikogo. Bez tego zadanie z nieosiągalnym miejscem przydzielało się i odpadało w kółko.

## 20.8 Rozkaz gracza

Kliknięcie prawym przyciskiem w skałę to w grze rozkaz `evt_task_dig -pos1 {x y}` dla zaznaczonego krasnala (`evt_task_dig_proc` czyści jego zadania i daje `dig_starttask`), a przeciągnięcie maluje znaczniki tunelu (`dig_mark` tryb 1), które zbiera planista. U nas tak samo, gdy krasnale mają skrypty (`WorldSim::scriptDigging`): przeciągnięcie maluje znaczniki w węzłach pól [x0, x1 + 1] × [y0, y1 + 1], a kliknięcie zaznacza pole i wysyła `evt_task_dig`.

## 20.9 Nasza implementacja

[`lib/prod_planner`](../../src/lib/prod_planner.h) ([.cpp](../../src/lib/prod_planner.cpp)) w [`WorldSim`](../../src/lib/world_sim.h): `tick` po kroku symulacji, `logicTick` co turę logiki; komendy skryptów w [`script/cmd_work.cpp`](../../src/lib/script/cmd_work.cpp) (`gnome_idle`, `gnome_failed_work`, `gnome_stopped_work`, `gnome_announce_dig`, `prod_gnome_state`). Ślad: `OW_TRACE_PLAN=1`.

**Różnice (🔶):** przegląd co 0,5 s zamiast 1 s (jak w Diggles-Unity); jedna siatka znaczników (prośby kopania tylko dla gracza 0); potrzeba produkcji = liczba + 2 (jak w Diggles-Unity; gra: priorytet · 1000); bez energii budynków; miejsce rozpakowania (`order_pos`, `order_asbox`) jeszcze nie przychodzi z okna budowy, więc rozpakowanie stawia budynek tam, gdzie leży skrzynia; bez zbiorczego dowozu kilku przedmiotów jednym kursem; podniesienie, które się nie udało, liczymy jak porażkę (inaczej krasnal wracałby po ten sam przedmiot co przegląd).

**Sprawdzenie:** test `http_planner` (wigview: znaczniki przez `dig_mark`, po 90 s symulacji węzły wykopane do 11 i znaczniki wykonane), `http_planner_prod` (ognisko w `urw_gng_042_a`: wynalazek `BpGrillpilz`, dowóz kapelusza, `Grillpilz` z produkcji, zbiór grzyba, pakowanie do skrzyni, rozpakowanie z powrotem w budynek; wszystko prowadzą skrypty gry) i `dig_standoff` w grze (zadanie planisty, wycięte węzły).
