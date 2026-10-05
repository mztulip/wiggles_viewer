# 5. Obiekty i modele

Obiekt w świecie to instancja klasy (rozdział 2.5) z pozycją i obrotem. Klasa wskazuje **obiekt w bazie modeli `.3db`** i jego animację. Rozdział opisuje drogę od `new Dummy_Moos_um_a … ` do narysowanych trójkątów.

## 5.1 Bazy modeli ✅

Przy starcie `Scripts/init/ANIMINIT.TCL` wczytuje komendą `db_load <nazwa>` około 40 baz z `Data/3db/` (`mann`, `frau`, `werkzeuge`, `oberwelt`, `kristall`, `lavawelt`, …). Format: [3db.md](../formaty/3db.md).

Baza zawiera **obiekty** (nazwane zestawy animacji), a animacja to lista klatek-meshy. Skrypty adresują animację jako `<obiekt>.<animacja>`, np. `urw_sumpf.standard` albo `mann.gehen_loop`, bez podawania pliku. **Nazwy obiektów tworzą więc jedną wspólną przestrzeń dla wszystkich wczytanych baz.** Silnik powinien po wczytaniu baz zbudować słownik `nazwa obiektu → (baza, obiekt)` (bez rozróżniania wielkości liter). **Przy powtórzonej nazwie wygrywa pierwsza wczytana baza** (✅: obiekt `wuerfel` jest w `WERKZEUGE.3DB`, 46 klatek, i w `MUETZEN.3DB`, 11 klatek; `db_animlength wuerfel.standard` zwraca 46, a `werkzeuge` jest wczytywane wcześniej).

✅ Komendy baz w działającej grze, porównane z naszym parserem dla wszystkich 6445 animacji ([BAZA_WIEDZY §8.7](../BAZA_WIEDZY.md)):

| Komenda | Wynik |
|---|---|
| `db_load <nazwa>` | wczytuje `Data/3db/<nazwa>.3db`; zwraca pusty napis (nie numer bazy) |
| `db_search <nazwa>` | numer bazy = kolejność wczytania (`mann` 0, `frau` 1, `werkzeuge` 2, `halbzeuge` 3, `muetzen` 4…), −1 gdy nie wczytana |
| `db_findanim <baza> <obiekt> <animacja>` | numer animacji w obiekcie = kolejność w pliku (1023 z 1023 zgodne dla `mann`) |
| `db_animlength <obiekt>.<animacja>` | liczba klatek; zgodna z naszym parserem dla 6444 z 6445 animacji (jedyna różnica to kolizja `wuerfel` opisana wyżej) |

## 5.2 Od klasy do modelu ✅

Model klasy ustala komenda w ciele klasy:

```tcl
class_defaultanim <obiekt>.<animacja> [tryb]
```

`tryb` to stała z `ANIMINIT.TCL`: **0 = `ANIM_STILL`** (pokaż pierwszą klatkę, nie animuj), **1 = `ANIM_ONCE`** (odtwórz raz), **2 = `ANIM_LOOP`** (zapętl). Bez podania 🔶 przyjmij 0.

Źródła klas:

1. **Listy dekoracji** w `Scripts/classes/deco/dummy_<baza>.tcl`, przetwarzane przez procedury z `ANIMINIT.TCL` (rozdział 2.5):

   | procedura | nazwa klasy | uwagi |
   |---|---|---|
   | `SetDummyClassesAll`, `SetDummyClassesNoZ`, `SetFrontDummyClassesNoPhys`, `SetCaveSkinClasses` | `Dummy_<Nazwa>` | model = pierwszy obiekt listy, animacja `standard` |
   | `SetWeaponClasses` | `<Nazwa>` (bez przedrostka) | narzędzia i broń |

   Opcje pozycji listy: `anim` włącza tryb 2 (pętla), `animrandom` dodatkowo losuje klatkę startową, `txtanim …` animację tekstury. **Bez opcji `anim` dekoracja stoi nieruchomo w pierwszej klatce.** Parametry procedury (`noviewinfog`, `nozwrite`, `nophysic`…) ustawiają właściwości klasy; `nozwrite` oznacza rysowanie bez zapisu do bufora głębi (materiał `modelnozwrite`).

2. **Zwykłe klasy** w pozostałych plikach `Scripts/classes/**`: `def_class <Klasa> … { … class_defaultanim … }`.

3. **Klasy z modelem tylko w edytorze**: `if {[get_mapedit]} {class_defaultanim …}` (światła `Misc_LightSrc_*`, część `Info_*`). W grze **nie rysuj** ich modelu, ale zachowaj ich działanie (światło, strefa).

Te reguły przypisują model do 86,9% z 33 905 obiektów we wszystkich szablonach. Reszta to głównie znaczniki bez wyglądu (`Info_*`, punkty trybów sieciowych `CS_*`, `CF_*`) i klasy, które ustawiają animację dopiero w kodzie (`set_anim` w `obj_init`, np. postacie).

## 5.3 Ustawienie w świecie ✅

Dla obiektu z pozycją `(x, y, z)` i obrotem `(rx, ry, rz)` ze skryptu:

```
P = (x, y, z · 0,5 + 7)                       [bloki], rozdział 1.5
M = T(P · 200) · Ry(−ry) · Rx(−rx) · Rz(−rz)  [jednostki modelu]
wierzchołek_świata = M · wierzchołek_modelu
```

`T` to przesunięcie, a `Rx`, `Ry`, `Rz` to zwykłe macierze obrotu (konwencja kolumnowa, jak w glm: `rotate(kąt, oś)`). Minusy biorą się stąd, że gra mnoży wektory przez macierze transponowane. Kolejność: najpierw obrót wokół z, potem x, na końcu y, wszystko w układzie gry (y w dół). Źródło: [Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) `Docs/COORDINATE_SYSTEM.md` §3.4. ✅ Scena menu zgadza się ze zrzutem gry.

Model nie ma skali. Jego jednostki (1/200 bloku) pasują wprost do świata.

## 5.4 Animacja ✅/🔶

Odtwarzanie opisuje [3db.md](../formaty/3db.md) §8:
- klatka to osobny mesh (animacja wierzchołkowa);
- interpolacja: w rejestrze jest `AnimInterpolation=1` i opcja exe `AnimIpol`, a Diggles-Unity twierdzi, że gra nie interpoluje. **Decyzja projektu:** interpolujemy liniowo zawsze (płynniejszy obraz), bez rozstrzygania, jak robi to oryginał (rozdział 11.6);
- prędkość 🔶: **10 klatek/s** według stałej gry podanej przez Diggles-Unity (`klatka = start + round((teraz − t0)·10 + 0,05)`, rozdział 11.6). Wcześniej przyjmowaliśmy 20 kl./s na oko;
- obiekty w trybie `ANIM_STILL` pokazują klatkę 0; przy `animrandom` każda instancja zaczyna od losowej klatki;
- ✅ **ruch i obrót postaci z animacji:** pola `move` i `rotate` animacji to przesunięcie (jednostki modelu, 200 = blok) i obrót (radiany) **na klatkę** ([3db.md](../formaty/3db.md) §3.5). Krasnal: chód `gehen_loop` 16,22 jednostki na klatkę → 0,81 bloku/s przy 10 kl./s, bieg `laufen_schnell_loop` 34,29 → 1,71 bloku/s; w grze zmierzone 0,85 i 1,59 bloku/s ([BAZA_WIEDZY §8.8](../BAZA_WIEDZY.md)), co potwierdza także tempo 10 kl./s. Obrót `drehen_links` daje razem 90°.

## 5.5 Rysowanie modelu

| Element | Zasada |
|---|---|
| geometria | dla każdego linku klatki: pozycje (po dekwantyzacji), UV, indeks normalnej; trójkąty z puli |
| tekstura | materiał linku → plik TGA ([3db.md](../formaty/3db.md) §6); filtrowanie liniowe, zawijanie REPEAT |
| tryb rysowania | **z pola `flags` linku** ✅: 1 zwykły, 2 addytywnie (blaski), 3 mieszanie alfa (postacie), 5 mapowanie otoczenia (kryształy, chrom), 8 bez oświetlenia (lawa), 0 niewidoczny; tabela i rzadkie tryby w [3db.md](../formaty/3db.md) „Tryb rysowania linku” |
| przezroczystość | w trybie 1 **alpha test** alfa ≥ 0,5 (liście, włosy, kraty), bez mieszania 🔶. Tryb 3: rdzeń (alfa ≥ 0,5) jak 1, a miękkie brzegi i blask trybu 2 w osobnej fazie po wszystkich obiektach sceny, bez zapisu głębi, od najdalszego (rozdział 9.2); inaczej miękki brzeg zasłania obiekty rysowane później |
| culling | wyłączony: modele mają cienkie, dwustronne elementy |
| głębia | test i zapis (poza klasami `nozwrite`) |
| oświetlenie | per wierzchołek, prawo z rozdziału 7, z jasnością bazową B w punkcie (x, y) obiektu |
| zanikanie w głąb | modele za terenem (z < 0) przechodzą w kolor strefy horyzontu (rozdział 8) |
| nieodkryty teren | obiekt rysuje się tylko przy dostatecznie odkrytym otoczeniu; dekoracje (`viewinfog`) także w szarej mgle (rozdział 10.5) |

**Dane pomocnicze klatki** ([3db.md](../formaty/3db.md) §3.2, §3.7): prostokąt cienia na ziemi (`shadowQuad`, 4 narożniki) razem z mapą cienia 32×32 (🔶 rozmyty rzut bryły światłem pod 45°, cień pada w +z ku widzowi; 🔶 gra rysuje cienie tylko od lamp w zasięgu `MaxShadowLightDist`, wydłużone w stronę od lampy, [niewiadome](niewiadome.md)) i maska zajętości `cmap` w kratkach 1/8 bloku, też na ziemi w płaszczyźnie x/z (✅ płaszczyzna, 🔶 rzut bryły poszerzony o kratkę; kolizje: `animinit.tcl` daje klasie `class_collision 1`, gdy model ma `cmap`). Oba widać w wigview w sekcji „Cień i cmap” (`--shadow-cmap`).

**Kolor przy alpha teście.** Przezroczyste piksele tekstur mają dowolny kolor (często szaroniebieski). Przy filtrowaniu i mipmapach miesza się on z sąsiednimi liśćmi i daje szare obwódki. Przed wysłaniem tekstury do GPU warto „rozlać” kolor nieprzezroczystych sąsiadów na przezroczyste piksele (alfa bez zmian).

## 5.6 Przykład

```tcl
new Dummy_Moos_um_a "" "[expr 2.53098 + $xoffset] [expr 2.5 + $yoffset] 13.54" "0 6.12319 0"
```
w szablonie nałożonym w (21, 40):

1. Klasa `Dummy_Moos_um_a` pochodzi z listy `SetDummyClassesAll` w `classes/deco/dummy_flusslandschaft.tcl`: `{ Moos_um_a moos_um_a }`. Model to obiekt `moos_um_a` (baza `flusslandschaft.3db`), animacja `standard`, tryb 0 (nieruchomy).
2. Pozycja: x = 23,531, y = 42,5, z = 13,54 · 0,5 + 7 = 13,77 bloku, czyli tuż za przednią płaszczyzną (14,5), na dnie płytkiego korytarza.
3. Obrót: tylko ry = 6,123 rad, czyli M = T · Ry(−6,123).

---
**Diggles-Unity** (ich opracowanie, 🔶 do sprawdzenia w laboratorium): [ANIMATION_CONTROLLER](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/ANIMATION_CONTROLLER.md), [HAT_LINK_SYSTEM](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/HAT_LINK_SYSTEM.md), [COORDINATE_SYSTEM](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/COORDINATE_SYSTEM.md). Porównanie i stan: [mapa Diggles-Unity](../DIGGLES_UNITY.md).

**W kodzie OpenWiggles** (pomocniczo, zasada działania jest opisana wyżej): [`lib/class_db.h`](../../src/lib/class_db.h), [`lib/model3db.h`](../../src/lib/model3db.h), [`lib/render/model_renderer.h`](../../src/lib/render/model_renderer.h), [`lib/map_scene.cpp`](../../src/lib/map_scene.cpp) (`objectMatrix`, `resolve`). Wszystkie moduły: [mapa kodu](../KOD.md).
