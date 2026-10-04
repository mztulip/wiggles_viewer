# 11. Obiekty Tcl i pętla gry

Rozdział 2 opisał, jak gra wczytuje skrypty. Ten opisuje, jak skrypty **żyją** w trakcie gry: gdzie trzymane są zmienne obiektu, jak działają metody, zdarzenia, timery i maszyna stanów, oraz w jakim takcie silnik to wszystko wywołuje. To podstawa całej rozgrywki: zachowanie krasnali, zwierząt, budynków i fabuły jest napisane w tych mechanizmach.

**Źródła i status.**
- Sygnatury komend pochodzą z oficjalnej pomocy gry `Doc/wiggles_tcl.htm` (wersja angielska). Wyciąg zrobiło Diggles-Unity: `Docs/ENGINE_TCL_API.txt`. Polska wersja tego pliku nie ma (🔶 komenda `tclhelp <komenda>` pisze pomoc do konsoli gry; w naszej sondzie zwracała pusty wynik).
- Działanie w środku opisuje [Zimennik/Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) (`Docs/TCL_OBJECT_MODEL.md`, `TCL_INIT_LOADING.md`, `ENGINE_ARCHITECTURE.md`, kod `Runtime/Engine/Sim/DigglesGameSim.cs`). Status 🔶, chyba że zaznaczono inaczej.
- ✅ Naszą obserwacją potwierdzone jest tylko to, że kod klas działa poza głównym interpreterem (11.1).

## 11.1 Interpretery

Gra używa **wielu interpreterów Tcl**:

| Interpreter | Kiedy powstaje | Co w nim działa |
|---|---|---|
| **główny** | raz, przy starcie programu | `systeminit.tcl`, skrypty startowe, scenariusze, szablony, procedury globalne |
| **tymczasowy interpreter klasy** | przy `def_class` | ciało klasy (rejestracja metod, stanów, zdarzeń) |
| **interpreter obiektu** | przy pierwszym użyciu obiektu, po jednym na obiekt | `obj_init`, metody, ciała stanów, obsługa zdarzeń tego obiektu |

✅ Potwierdzenie z laboratorium: nasza sonda owija komendy natywne w głównym interpreterze (`rename`), a wywołania z ciał klas i metod przez nią nie przechodzą.

**Zmienne obiektu to zmienne globalne jego interpretera.** Przy tworzeniu interpretera obiektu silnik:
1. tworzy interpreter (`Tcl_CreateInterp`),
2. zapamiętuje w nim, który obiekt jest „tym” (`this`),
3. ustawia jako zmienne globalne wszystkie pola klasy (`member`) z wartościami domyślnymi,
4. rejestruje w nim komendy silnika,
5. 🔶 przełącza go w tryb bezpieczny (`Tcl_MakeSafe`), chyba że klasa wyłącza skrypty.

Wszystkie ciała (`obj_init`, metody, stany, zdarzenia) wykonują się **na poziomie globalnym** interpretera obiektu, bez ramki procedury. Wynikają z tego reguły, które trzeba odtworzyć dokładnie:

| W ciele obiektu | Odnosi się do |
|---|---|
| `set x 5`, `$x` | zmiennej tego obiektu (trwa między wywołaniami) |
| `global x` | **też** zmiennej tego obiektu, a nie zmiennej wspólnej dla gry. Skrypty używają `global` i gołego `$x` zamiennie |
| `this` | nie jest zmienną: komendy silnika rozpoznają słowo `this` jako bieżący obiekt |
| zmienna innego obiektu | tylko przez `ref_get $obj nazwa`, `ref_set $obj nazwa wartość`, `ref_checkvar` |
| wykonanie kodu w innym obiekcie | `obj_eval $obj {skrypt}`, `call_method $obj metoda …` |

Silnik nie musi tworzyć setek prawdziwych interpreterów. Wystarczy osobna tablica zmiennych na obiekt, podstawiana jako „globalna” na czas wykonywania jego kodu. Tak robi Diggles-Unity. Trzeba jednak zachować drugi poziom odczytu: wartości ustawione w głównym interpreterze przy starcie (np. stałe `ANIM_LOOP`, tabele `ttt*`) muszą być widoczne z ciał obiektów.

## 11.2 Definicja klasy

```tcl
def_class <nazwa> <kategoria> <typ> <epoka> {<flagi>} {
    ...ciało klasy...
}
```

| Pole | Wartości (z pomocy gry) |
|---|---|
| kategoria | `none food stone metal energy service` |
| typ | `none dummy gnome monster material tool production energy box blueprint store` |
| epoka | liczba (epoka drzewka technologii) |
| flagi | `reproduces moves lives` |

Ciało klasy wykonuje się **raz, przy definicji**. Typowo zawiera:

| Komenda | Znaczenie |
|---|---|
| `member x y …` | pola obiektu z wartościami domyślnymi |
| `method nazwa {arg…} {kod}` | metoda instancji; `method_static` bez instancji, `method_const` 🔶 nie zmienia obiektu |
| `obj_init {kod}`, `obj_exit {kod}` | kod przy tworzeniu i usuwaniu obiektu |
| `def_event nazwa`, `handle_event nazwa {kod}` | typ zdarzenia i jego obsługa w tej klasie |
| `state nazwa {kod}`, `state_enter nazwa {kod}`, `state_leave nazwa {kod}` | stan maszyny stanów, kod na wejściu i wyjściu |
| `inherit Klasa …` | **skopiowanie** metod, obsług i pól innej klasy w chwili definicji (bez późniejszego dziedziczenia wirtualnego) |
| `class_defaultanim`, `set_class_anim`, `set_class_animset` | animacje (rozdział 5 i 11.6) |
| `class_light`, `class_physic`, `class_viewinfog`, `class_collision`, … | właściwości klasy |
| `call scripts/…` | dołączenie wspólnego kodu (np. krasnal dołącza ok. 24 pliki `z_*.tcl`) |

`classinit.tcl` wczytuje klasy tak:
```tcl
foreach dir { characters items deco work sparetime story } {
    cd Data/scripts/classes/$dir
    set filelist [lsort [glob -nocomplain "*.tcl"]]   ;# kolejność alfabetyczna
    cd ../../../..
    foreach f $filelist {
        ;# characters: pliki z "_" w nazwie to części innych klas, pomijane
        ;# story: sequencer.tcl pomijany
        catch "call scripts/classes/$dir/$f"           ;# błąd jednego pliku nie przerywa wczytywania
    }
}
call scripts/classes/zwerg/zwerg.tcl  ;# + pzwerg, actors, baby
```
Silnik musi więc obsługiwać `cd`, `pwd`, `glob` i `file exists` w katalogu danych. W sumie powstaje ok. 680 klas. Opcjonalna pamięć podręczna klas (`startcache present|load|write`) przyspiesza start; można zwracać „brak” i zawsze wczytywać od nowa.

Przed `systeminit.tcl` silnik wykonuje jeszcze `data/scripts/misc/genattribs.tcl` (atrybuty doświadczenia i zasobów, `def_attrib`) i, jeśli istnieje, `data/userinit.tcl`.

## 11.3 Obiekty

```tcl
sel /obj                                      ;# kontekst: węzeł drzewa obiektów
set o [new <Klasa> ["<nazwa>" ["x y z" ["rx ry rz"]]]]   ;# zwraca liczbowy uchwyt
```

- Obiekty żyją w drzewie węzłów (`/obj` to węzeł zwykłych obiektów świata; `obj_list` wypisuje zawartość węzła, `ref2path` daje ścieżkę).
- `new` tworzy obiekt i wykonuje jego `obj_init` w interpreterze obiektu. `del $o` usuwa obiekt (z `obj_exit`).
- Większość komend obiektowych przyjmuje uchwyt albo słowo `this`: `set_pos this {x y z}`, `get_pos $o`, `set_anim this …`.
- `call_method $o metoda arg…` przekazuje argumenty do interpretera obiektu `$o` i wykonuje tam treść metody. `call_method_static Klasa metoda …` działa bez instancji, `check_method Klasa metoda` sprawdza, czy metoda istnieje.

## 11.4 Zdarzenia i timery

Zdarzenie to komunikat z typem i parametrami, kierowany do obiektu. Klasa obsługuje je blokiem `handle_event`.

```tcl
def_event evt_timer0                       ;# w ciele klasy: nowy typ zdarzenia
handle_event evt_timer0 { ... }            ;# obsługa w tej klasie

set_event <nadawca> <typ> -target $obj -subject1 $x -num1 5 -text1 "…" -pos1 {…} -delay 2.0
timer_event <obj> <typ> -attime [expr [gettime] + 1] -interval 20 -repeat -1 -userid 1
timer_unset <obj> <userid>
event_get this -subject1                   ;# w obsłudze: parametry bieżącego zdarzenia
```

| Parametr | Znaczenie |
|---|---|
| `-target` | odbiorca (`set_event`) |
| `-subject1..3`, `-text1..2`, `-num1..3`, `-pos1..3` | dane zdarzenia; czyta się je przez `event_get` (również `-origin`, `-timestamp`) |
| `-delay` | opóźnienie w sekundach (`set_event`) |
| `-attime` | chwila pierwszego wywołania w czasie gry, w sekundach (`gettime`) |
| `-interval`, `-repeat` | odstęp i liczba powtórzeń (−1 = bez końca, 0 = raz) |
| `-userid` | numer timera do późniejszego `timer_unset` |

Timer, gdy nadejdzie jego czas, wstawia zdarzenie do kolejki obiektu. Kolejkę obiekt obsługuje w turze logiki (11.7). Przykład z gry (`FogRemover`):
```tcl
timer_event this evt_timer0 -attime [expr {[gettime] + 1}]
timer_event this evt_fogremove -repeat -1 -interval 20 -userid 1 -attime [expr {[gettime] + 2}]
```

❓ Obserwacja z laboratorium: klasa zdefiniowana w `userstartup.tcl` z `timer_event` w `obj_init` dała się utworzyć, ale jej zdarzenie nie przyszło. Może czas gry stoi przy ekranie startowym, może brakuje kroku (np. `def_event` przed `handle_event`). Do sprawdzenia.

## 11.5 Maszyna stanów

Każdy obiekt ma co najwyżej jeden **bieżący stan** i **licznik włączeń**.

```tcl
state idle { ... }                 ;# w klasie: ciało stanu
state_enter idle { ... }           ;# kod przy wejściu
state_leave idle { ... }           ;# kod przy wyjściu

state_trigger this work            ;# przejście do stanu "work" (bez argumentu: wyjście z maszyny)
state_triggerfresh this work       ;# to samo plus wyzerowanie i włączenie licznika
state_disable this                 ;# licznik −1
state_enable this                  ;# licznik +1
state_reset this                   ;# wyzerowanie licznika
state_get this                     ;# nazwa bieżącego stanu
```

**Ciało bieżącego stanu wykonuje się w każdej turze logiki** (11.7), dopóki obiekt ma stan, a licznik nie wyłącza maszyny (🔶 licznik ujemny = wyłączona). Silnik nie ogranicza tempa. Robią to same skrypty: stan kończy się zwykle tak:

```tcl
set_idle_anim
state_disable this                         ;# nie myśl, dopóki animacja trwa
action this wait 1 {state_enable this}     ;# po sekundzie włącz z powrotem
```

Akcje (`action this walk …`, `wait`, `anim` …) to wbudowane zachowania silnika (chodzenie, wspinanie, czekanie), które trwają wiele tur i na koniec wykonują podany kod. Kolejka zadań (`tasklist_add`, `tasklist_get`, `tasklist_rem` …) to lista napisów ze zleceniami dla postaci, z której korzystają skrypty krasnali.

## 11.6 Animacja sterowana skryptem 🔶

```tcl
set_anim <obj> <obiekt>.<animacja> <klatka_startowa> <tryb> [<ciąg_dalszy>]
```

- Tryby: 0 `ANIM_STILL` (stoi w klatce startowej), 1 `ANIM_ONCE` (raz, potem zatrzymuje się na ostatniej klatce **bez** powiadomienia), 2 `ANIM_LOOP`.
- **Tempo: 10 klatek na sekundę**, liczone od chwili startu: `klatka = start + round((teraz − t0) · 10 + 0,05)`. Silnik sam nie łączy `_start → _loop → _end`. Robią to skrypty albo akcje, znając długość animacji (`db_animlength`).
- `set_class_anim stan obiekt.anim` buduje w klasie mapę „stan logiczny → animacja”, a `set_class_animset N {…}` zestawy zastępcze dla sposobów chodzenia (krasnal ma ich 17: zwykły, bieg, zmęczony, pływanie, z koszem…). Akcja chodzenia wybiera zestaw i animacje `start`/`loop`/`stop`.

⚠ **Sprzeczność do wyjaśnienia:** według Diggles-Unity gra **nie interpoluje** klatek, a w rejestrze naszej instalacji jest `Performance\AnimInterpolation=1` i opcja exe `AnimIpol`, co sugeruje interpolację (rozdział 5.4, [3db.md](../formaty/3db.md) §8). Rozstrzygnie seria zrzutów animowanego obiektu w laboratorium.

## 11.7 Takt gry

Pętla główna (jedna iteracja na klatkę obrazu):

```
dopóki gra działa:
    krok_logiki()  — stałym krokiem, nadrabia najwyżej 10 kroków na klatkę
    rysuj()         (rozdział 9)
    uśpij wątek, żeby utrzymać tempo
```

Diggles-Unity przyjmuje (🔶, częściowo z obserwacji, częściowo ze stałych gry) **40 kroków symulacji na sekundę** (25 ms) i **turę logiki („think”) co 4. krok, czyli 10 razy na sekundę**. W turze logiki, w tej kolejności:

1. mapa świateł i porządki menedżera obiektów,
2. sztuczna inteligencja przeciwników (każdy gracz SI co 50 tur),
3. planowanie produkcji,
4. fizyka,
5. **dla każdego aktywnego obiektu: zdarzenia z kolejki (`handle_event`), potem ciało stanu**,
6. aktualizacje i przemieszczenia obiektów,
7. mgła wojny (co 4. turę serwera, rozdział 10.2),
8. mapa (kopanie, woda, walidacja światła bazowego),
9. wyzwalacze fabularne (`Trigger_*`).

Akcje (chodzenie, animacje) mają własny przebieg, niezależny od ciał stanów. W trybie przerywnika (`set_sequence 1`) myślą tylko obiekty oznaczone jako aktywne w sekwencji.

## 11.8 Minimalny plan implementacji

1. Interpreter Tcl 8.3 albo zgodny (8.6 z ostrożnością: inne `expr` i kodowanie), preprocesor (rozdział 2.2), `call`, `cd`, `pwd`, `glob`, `file exists`.
2. Rejestr klas: `def_class` wykonuje ciało w kontekście klasy i zapisuje pola, metody, stany, zdarzenia, animacje i właściwości.
3. Obiekty: uchwyty, drzewo `/obj`, tablica zmiennych na obiekt jako „globalne” w czasie wykonywania jego kodu, `this`, `ref_get`/`ref_set`, `call_method`.
4. Kolejka zdarzeń na obiekt, timery w czasie gry, `event_get`.
5. Maszyna stanów z licznikiem włączeń, wołana w każdej turze logiki po zdarzeniach.
6. Takt: 40 kroków na sekundę, tura logiki co 4. krok.
7. Komendy natywne stopniowo, zaczynając od najczęstszych ([natywne_komendy_gry.txt](../natywne_komendy_gry.txt)); nieznane mogą na początek logować wywołanie i zwracać pusty wynik, a `catch` w `classinit.tcl` i tak izoluje błędy plików.

## 11.9 Nasza implementacja (stan) 🔶

`src/lib/script/engine.cpp`, test bez okna: `build/release/owscript [KLASA [SEKUNDY]]` (dziennik zaślepek i błędów), w grze: `openwiggles` (dziennik `openwiggles_tcl.log`, wyłączenie: `--no-scripts`).

- **Tcl 8.6 z systemu.** Tekst skryptów (CP1252/1250) zamieniamy na UTF-8 przed wykonaniem. Preprocesor jak w 2.2 (symbol `FULL` zdefiniowany).
- **Start**: `genattribs.tcl`, `customcommands.tcl`, `shader.tcl`, `txtinit.tcl`, `animinit.tcl`, `classinit.tcl`, `talkinit.tcl`, `claninit.tcl`, `fight.tcl` (bez menu, dźwięku, SI). Wynik: **2684 klasy w ok. 5 s, bez błędów**. `makettree.tcl` i `init_techtree` pomijamy (gra ma gotowy `gen_tt.tcl`).
- **Katalog bieżący** `cd`/`pwd`/`glob`/`file exists` działa wirtualnie po katalogu gry (start: katalog gry, nie `Data`). `locale` zwraca `pl` (pliki `*_pl.tcl`).
- **Zapis plików przez skrypty** (`open … w`) trafia do `~/.cache/openwiggles/<ścieżka>`; odczyt najpierw stamtąd, potem z gry. Instalacji gry nie zmieniamy.
- **Klasy.** `def_class` wykonuje ciało w tymczasowym interpreterze. Wcześniej kopiuje do niego zmienne głównego interpretera, bo ciała w cudzysłowach (`obj_init "… $ANIM_STILL"`) podstawiają je od razu. `in_class_def` = 1 w ciele klasy. `get_class_name/type/category/era/flags` bez argumentu zwracają klasę z definicji (z tego korzysta `techtreetunes.tcl` przy zmiennych `ttt*_<klasa>`). Po wykonaniu zapamiętujemy procedury i zmienne globalne ciała.
- **Obiekty**: interpreter na obiekt; kolejno zmienne głównego interpretera, zmienne i procedury klasy, pola (`member`), `obj_init`. Klasy z `class_disablescripting` nie dostają interpretera. W zapisie kampanii skrypty ma 118 obiektów.
- **Takt** 10 Hz: timery → kolejka zdarzeń → `handle_event` → ciało stanu (gdy licznik > 0 i nie trwa akcja).
- **Zaślepki**: nieznana komenda trafia do procedury `unknown`, liczy się w dzienniku i zwraca `0`. Krasnal ze skryptem „umiera” po kilku sekundach, bo atrybuty (np. zdrowie) wynoszą 0. Dlatego w grze krasnale steruje na razie gracz (rozdział 14), a skrypty mają pozostałe obiekty.
