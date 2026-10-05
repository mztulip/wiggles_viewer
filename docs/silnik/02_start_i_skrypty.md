# 2. Start gry i skrypty Tcl

Prawie całe zachowanie gry jest w skryptach. Silnik musi więc przede wszystkim umieć wykonać `Data/systeminit.tcl` i komendy, które ten skrypt woła. Ten rozdział opisuje, jak przebiega start, jak gra czyta skrypty i które komendy wystarczą, żeby zbudować świat.

## 2.1 Interpreter ✅

- Oryginał używa **Tcl 8.3.2** (potwierdzone w działającej grze sondą: `info patchlevel`).
- Silnik rejestruje **639 komend natywnych**. Pełna lista z liczbą wywołań przy starcie jest w [natywne_komendy_gry.txt](../natywne_komendy_gry.txt).
- Błędy skryptów gra zapisuje do `tclerrors.txt` (z opcją `-logerrors`): komunikat, stos i cały skrypt **po preprocesorze**.

## 2.2 Preprocesor ✅

Zanim tekst pliku trafi do Tcl, przechodzi przez preprocesor gry:

1. **Komentarze `//`**: od `//` do końca linii tekst jest usuwany, a linia zostaje (pusta), żeby numeracja linii się zgadzała.
2. **Dyrektywy warunkowe** zaczynają się od `//#`:
   ```
   //# IF FULL           dalej tylko gdy symbol FULL zdefiniowany
   //# IFNOT FULL        dalej tylko gdy nie
   //# IFNDEF NOSTARTSEQ dalej tylko gdy NOSTARTSEQ nie zdefiniowany
   //# ELSE
   //# ENDIF
   //# STOPIFNOT FULL    przerwij cały plik, jeśli FULL nie jest zdefiniowany
   ```
   W danych występują tylko symbole `FULL` (pełna wersja, nie demo, więc zdefiniuj go) i `NOSTARTSEQ` (wyłącza menu startowe). Dodatkowe symbole można podać w rejestrze (`TCLDEFINES`).
3. **`//` jako komenda.** Gra rejestruje też komendę natywną `//`, która nic nie robi. Komentarze, które nie przejdą przez preprocesor (np. w treści przekazanej jako string do `def_class`), wykonują się więc jako pusta komenda. Silnik musi zrobić jedno i drugie.

Zwykłe komentarze Tcl `#` i `;#` działają normalnie.

## 2.3 Wczytywanie plików: `call` ✅

`call <ścieżka>` wczytuje plik, przepuszcza przez preprocesor i wykonuje go w bieżącym interpreterze.

- Wielkość liter nie ma znaczenia.
- Ścieżki zaczynające się od `scripts/` lub `templates/` dostają przedrostek `data/`, więc `call scripts/init/animinit.tcl` oznacza `Data/Scripts/init/ANIMINIT.TCL`.
- `callnc` robi to samo bez pamięci podręcznej (🔶 gra cache'uje przetworzone skrypty, a pliki z `data/templates` zawsze czyta od nowa).

## 2.4 Przebieg startu ✅

`Data/systeminit.tcl` w skrócie (kolejność potwierdzona logiem gry):

```tcl
call scripts/init/customcommands.tcl   ;# procedury pomocnicze w Tcl
call scripts/init/shader.tcl           ;# materiały renderu
call scripts/init/txtinit.tcl          ;# warianty tekstur (def_texture)
call scripts/init/animinit.tcl         ;# db_load wszystkich modeli + procedury Set*Classes
call scripts/init/classinit.tcl        ;# definicje wszystkich klas obiektów
call scripts/init/soundinit.tcl        ;# dźwięki, potem muzyka, generator poziomów (lginit),
...                                    ;# dialogi, klany, drzewko technologii, walka
ai init 0 data/scripts/ai/std_ai.tcl

map create 64 64 {  }                  ;# pusty świat 64×64 bloków
if { [file exists data/userstartup.tcl] } {
    call userstartup.tcl               ;# własny skrypt zamiast menu (przydatne w laboratorium)
} else {
    ;# MENU STARTOWE: to zwykła scena złożona z trzech szablonów
    call data/templates/unq_menue.tcl ; MapTemplateSet 25 28
    call templates/urw_gng_021_a.tcl  ; MapTemplateSet 21 40
    call templates/urw_gng_022_a.tcl  ; MapTemplateSet 45 40
    set_view 32.368 40.858 1.38 -0.355 0.165
    sel /obj
    set FR [new FogRemover]            ;# zdejmuje nieodkryty teren w kadrze
    ...
    gui_new_game                       ;# okno menu
}
gametime start
load_done
```

Menu nie jest specjalnym ekranem: to **mapa jak każda inna**, z kamerą ustawioną na wnętrze jaskini, i dopiero na nią nakładane jest okno interfejsu. Wczytanie samego menu wymaga więc tych samych mechanizmów co poziom gry (rozdziały 3–9).

## 2.5 Klasy obiektów ✅

Każdy obiekt w świecie (krasnal, grzyb, kamień, światło) jest instancją **klasy** zdefiniowanej w skryptach:

```tcl
def_class <Nazwa> <materiał> <kategoria> <?> {<?>} {
    ;# treść klasy, wykonywana przy definicji klasy:
    class_defaultanim <obiekt>.<animacja>   ;# model i animacja z baz .3db
    class_light {0 -0.5 0} 6 {0.9 0.9 0.8}  ;# światło klasy (rozdział 7)
    class_physic 0
    method <nazwa> {args} { ... }           ;# metody
    def_event <zdarzenie> ; handle_event <zdarzenie> { ... }
    obj_init { ... }                        ;# kod wykonywany dla każdej nowej instancji
}
```

Instancję tworzy `new <Klasa> "<nazwa>" "<x y z>" "<rx ry rz>"`, a komenda zwraca liczbowe ID obiektu. Przedtem skrypt wybiera kontekst tworzenia przez `sel /obj`.

**Klasy dekoracji** (ich jest najwięcej) nie są pisane ręcznie, tylko generowane z list. Procedura `SetDummyClassesAll` (i podobne) jest zdefiniowana w Tcl w `Scripts/init/ANIMINIT.TCL`, a wołana w plikach `Scripts/classes/deco/dummy_<baza>.tcl`:

```tcl
SetDummyClassesAll {} {
    { Lava_seule_a lava_seule_a }
    { Gras_03 {gras_03_a gras_03_b gras_03_c gras_03_d} }
    { Feuer_a feuer_a anim }
}
```

Dla każdej pozycji `{ Nazwa obiekty opcje }` procedura wykonuje:
```tcl
def_class Dummy_<Nazwa> none dummy 0 {} "class_defaultanim <pierwszy obiekt>.standard <tryb> ; class_viewinfog … ; class_physic … ; class_collision …"
```
Model to więc **pierwszy obiekt z listy** z animacją `standard`. Opcja `anim` włącza odtwarzanie animacji (tryb 2 = pętla), a `animrandom` dodatkowo losuje klatkę startową. Pozostałe obiekty listy (warianty, np. `gras_03_b`) nie są używane w `class_defaultanim` (❓ do czego służą). Szczegóły odczytu modelu z klasy: rozdział 5.

**Klasy widoczne tylko w edytorze.** Część klas (światła `Misc_LightSrc_*`, niektóre `Info_*`) ustawia model tylko w trybie edytora: `if {[get_mapedit]} {class_defaultanim …}`. W grze są niewidoczne, ale działają (świecą, ustawiają strefy).

## 2.6 Scenariusze: skąd się bierze mapa ✅

Mapy stałe to skrypty w `Data/Gamesave/`, np. `single_Knockersdorf.tcl` (46 szablonów) albo `OBERWELT.TCL` (samouczek na powierzchni). Mają tę samą budowę co menu:

```tcl
map create 500 200                       ;# rozmiar świata w blokach
set_view 131.78 23.95 2.5 0.22 0.57      ;# kamera startowa
set_light_begin 33                       ;# niebo powyżej wiersza 33 (rozdział 7)
call data/templates/obw_tutorial.tcl
MapTemplateSet 32 0                      ;# szablon z lewym górnym rogiem w bloku (32, 0)
call data/templates/obw_szene.tcl
MapTemplateSet 288 32
...
proc s2010 {} { ... }                    ;# logika samouczka
```

Scenariusz może wołać inne przez `call data/gamesave/…`. Kampania i tryb swobodny (`StoryMgr load_level`, `load_sandbox`) **generują świat losowo** komendami `lg_*`. Tego jeszcze nie opisujemy ([niewiadome](niewiadome.md)).

## 2.7 Minimalny zestaw komend do pokazania mapy

Żeby wczytać menu albo scenariusz stały i narysować go poprawnie, nie trzeba całego Tcl gry. Wystarczy obsłużyć te komendy (pozostałe mogą na początek nic nie robić):

| Komenda | Działanie | Rozdział |
|---|---|---|
| `db_load <nazwa>` | wczytaj `Data/3db/<nazwa>.3db` | 5 |
| `def_class`, `class_defaultanim`, `class_light`, `horizon_set` w ciele klasy | zapamiętaj model, światło i strefę klasy | 5, 7, 8 |
| `map create W H` | utwórz pusty świat | 3 |
| `call` | wczytaj skrypt | 2.3 |
| `map_template <plik.pmp> x y` | nałóż teren szablonu | 3 |
| `map_setlayer2 x y <plik.l2m>` | nałóż detale szablonu | 3 |
| `loading_objdetail high/medium/low/off` | poziom szczegółów kolejnych obiektów | 3 |
| `sel /obj`, `new Klasa "" "x y z" "rx ry rz"` | utwórz obiekt | 3, 5 |
| `set_owner`, `set_physic`, `set_autolight`, `set_boxed` | właściwości obiektu (do pominięcia przy samym obrazie) | |
| `set_view x y zoom dx dy` | kamera | 6 |
| `set_light_begin N`, `set_ground_begin N` | niebo i grunt panoramy | 7, 8 |

Jeśli nie chcesz od razu osadzać Tcl, możesz czytać scenariusze i szablony prostym parserem linii: rozpoznaj powyższe komendy i wylicz wyrażenia `[expr {5.99 + $xoffset}]` jako liczbę plus przesunięcie szablonu. Tak działa nasza przeglądarka ([`src/lib/map_world.cpp`](../../src/lib/map_world.cpp), [`src/lib/map_template.cpp`](../../src/lib/map_template.cpp)). Pliki klas (`def_class` …) czyta się podobnie, szukając w ich treści `class_defaultanim`, `class_light` i `horizon_set` ([`src/lib/class_db.cpp`](../../src/lib/class_db.cpp)).

---
**W kodzie OpenWiggles** (pomocniczo, zasada działania jest opisana wyżej): [`lib/script/engine.h`](../../src/lib/script/engine.h), [`lib/map_world.h`](../../src/lib/map_world.h) (`parseScenario`). Wszystkie moduły: [mapa kodu](../KOD.md).
