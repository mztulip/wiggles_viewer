# 20. Planista pracy krasnali

Rozdział opisuje, skąd krasnale biorą pracę bez klikania gracza. W grze nie ma skryptu, który przydziela krasnalom gracza kopanie, zbiory czy produkcję: robi to wbudowany planista (gra: `CProdPlanner`). Co pewien czas zbiera zadania ze świata, wybiera bezczynnego krasnala i wysyła mu zdarzenie `evt_autoprod_*`. Dalej prowadzą go niezmienione skrypty (`z_events.tcl`, `z_work_states.tcl`, `z_dignwalk.tcl`).

**Źródło i status.** Prawa i stałe pochodzą z opracowania [Zimennik/Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) (MIT): `Docs/WORK_DISPATCHER_NATIVE.md`, `Runtime/Engine/Sim/ProdDispatcher.cs` (adresy `FUN_…` gry i stałe odczytane z `.rdata` to ich źródło). U nas całość jest 🔶. Etap 1: pula bezczynnych, kopanie, porażki. Zbiory, produkcja, pakowanie i dowóz są w TODO.

## 20.1 Pula bezczynnych

Krasnal wchodzi do puli komendą `gnome_idle this 1`. Wywołuje ją jego skrypt przy wejściu w stan `work_idle`, gdy jest w oknie pracy i nie ma zadania (`z_work_states.tcl`). `gnome_idle this 0` przy wyjściu ze stanu go wypisuje. Planista bierze z puli tylko krasnale, które:
- są w swoim oknie pracy (godzina doby = czas gry / 150 s, doba 12 h; okno `set_worktime`, rozdział 11.4);
- mają włączone automatyczne przydzielanie (`get_prodautoschedule` = 1, domyślnie tak);
- nie są przydzielone w ostatnich 2 turach logiki i nie odpoczywają po porażce (20.4).

## 20.2 Przegląd

Co 0,5 s planista przegląda jednego gracza, po kolei 0, 1, …, 7 (gracz 0 co 4 s). Przegląd:

```
zadania = zbierz_zadania(gracz)            ;# na razie kopanie (20.3)
utrzymaj_prośby_kopania(gracz)
pula -= krasnale nie spełniające warunków 20.1
sortuj zadania malejąco po potrzebie
dla zadania (od największej potrzeby):
    pomiń, jeśli w promieniu 0,8 od niego nieudane zadanie jeszcze stygnie (2 s)
    krasnal = z puli najtańszy wg kosztu dopasowania (bez odpoczywających)
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

## 20.4 Porażki

`gnome_failed_work this` / `gnome_stopped_work this` (skrypt woła je, gdy chód się nie udał albo zadanie przerwano) zdejmuje rezerwację krasnala. Krasnal 5 s nie dostaje nowych zadań, a jego zadanie w promieniu 0,8 przez 2 s nie wraca do nikogo. Bez tego zadanie z nieosiągalnym miejscem przydzielało się i odpadało w kółko.

## 20.5 Rozkaz gracza

Kliknięcie prawym przyciskiem w skałę to w grze rozkaz `evt_task_dig -pos1 {x y}` dla zaznaczonego krasnala (`evt_task_dig_proc` czyści jego zadania i daje `dig_starttask`), a przeciągnięcie maluje znaczniki tunelu (`dig_mark` tryb 1), które zbiera planista. U nas tak samo, gdy krasnale mają skrypty (`WorldSim::scriptDigging`): przeciągnięcie maluje znaczniki w węzłach pól [x0, x1 + 1] × [y0, y1 + 1], a kliknięcie zaznacza pole i wysyła `evt_task_dig`.

## 20.6 Nasza implementacja

[`lib/prod_planner`](../../src/lib/prod_planner.h) ([.cpp](../../src/lib/prod_planner.cpp)) w [`WorldSim`](../../src/lib/world_sim.h): `tick` po kroku symulacji, `logicTick` co turę logiki; komendy skryptów w [`script/cmd_work.cpp`](../../src/lib/script/cmd_work.cpp) (`gnome_idle`, `gnome_failed_work`, `gnome_stopped_work`, `gnome_announce_dig`, `prod_gnome_state`). Ślad: `OW_TRACE_PLAN=1`.

**Różnice (🔶):** przegląd co 0,5 s zamiast 1 s (jak w Diggles-Unity); jedna siatka znaczników (prośby kopania tylko dla gracza 0); jeszcze brak zadań zbiorów (kind 6), produkcji (kind 3), wynalazków (kind 2), pakowania i rozpakowania (kind 0/1) oraz dowozu materiałów (kind 4).

**Sprawdzenie:** test `http_planner` (wigview: znaczniki przez `dig_mark`, po 60 s symulacji węzły wykopane do 11 i znaczniki wykonane) i `dig_standoff` w grze (zadanie planisty, wycięte węzły).
