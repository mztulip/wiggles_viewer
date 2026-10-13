# Diggles-Unity: mapa dokumentacji i porównanie z OpenWiggles

[Zimennik/Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) (MIT) to reimplementacja gry w Unity 6 na oryginalnych danych. Jest **grywalna**: świat się generuje, krasnale chodzą, kopią i budują, działają przerywniki, nieodkryty teren, oświetlenie i wiadomości. Ma ok. 50 dokumentów z rozpracowaniem oryginału (`*_RE.md`), z adresami funkcji exe, prawami i cytatami ze skryptów. Ta strona łączy każdy ich dokument z naszym rozdziałem, mówi, co już mamy, i co trzeba zrobić, żeby ich dogonić i wyprzedzić.

**Zasady korzystania** (z [CLAUDE.md](../CLAUDE.md)):
- Bierzemy od nich **wiedzę**, nie kod. Każde przejęte ustalenie ma odnośnik do ich dokumentu.
- Oni czytają kod exe (dekompilacja), my nie. Ich prawa traktujemy jako 🔶 hipotezę i **sprawdzamy w laboratorium** (zrzuty, sonda Tcl, WebTraq przez [`tools/lab/live.sh`](../tools/lab/live.sh), [BAZA_WIEDZY §8](BAZA_WIEDZY.md)). Dopiero potem w naszym rozdziale ustalenie dostaje ✅.
- Ich statusy (np. „zaimplementowane”, „verify”) dotyczą ich portu, nie zgodności z oryginałem. Gdzie ich obserwacja różni się od naszej, decyduje eksperyment na oryginale (przykład: interpreter obiektu nie jest w trybie bezpiecznym, [rozdział 11.1](silnik/11_obiekty_tcl_i_petla.md)).

**Przeszukiwanie.** Lokalna kopia (stan z pobrania zipa) leży w `../ref/Diggles-Unity-main/` obok repozytorium:
```
grep -rn -i "<fraza>" ../ref/Diggles-Unity-main/Docs              # dokumenty
grep -rln "<komenda Tcl>" ../ref/Diggles-Unity-main/Assets/Diggles/Runtime   # gdzie zaimplementowali komendę
```
Odnośniki niżej prowadzą do bieżącej wersji na GitHubie (gałąź `main`). Spis od autorów: [Docs/README.md](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/README.md), lista otwartych prac: [Docs/BACKLOG.md](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/BACKLOG.md).

**Oznaczenia w kolumnie „u nas”:** ✅ mamy i sprawdziliśmy z oryginałem, 🔶 mamy przybliżenie, ❌ brak.

## 1. Architektura, skrypty Tcl, czas

| Temat | Ich dokumenty | Ich stan | U nas | Następny krok |
|---|---|---|---|---|
| Mapa exe: start, pętla klatki, podsystemy | [ENGINE_ARCHITECTURE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/ENGINE_ARCHITECTURE.md) | RE + projekt portu | ✅ pętla 10 Hz i czas gry zmierzone ([11.7](silnik/11_obiekty_tcl_i_petla.md)) | kolejność kroków tury logiki sprawdzić przez WebTraq |
| Interpreter Tcl | [TCL_INTERPRETER_PLAN](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/TCL_INTERPRETER_PLAN.md) | własny interpreter C#, 7 faz gotowych | ✅ osadzony Tcl 8.6 ([2.1](silnik/02_start_i_skrypty.md), [11.9](silnik/11_obiekty_tcl_i_petla.md)) | zgodność 8.3 → 8.6 |
| Start skryptów, wczytywanie klas | [TCL_INIT_LOADING](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/TCL_INIT_LOADING.md) | działa | ✅ 2684 klasy bez błędów, pamięć podręczna klas ([2.4](silnik/02_start_i_skrypty.md)) | |
| Model obiektów, zasięg zmiennych | [TCL_OBJECT_MODEL](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/TCL_OBJECT_MODEL.md) | naprawione | ✅ zmierzone w grze ([11.1](silnik/11_obiekty_tcl_i_petla.md)); 🔶 u nas kopia zmiennych głównego interpretera | usunąć kopię (TODO) |
| Komendy silnika | [ENGINE_TCL_API.txt](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/ENGINE_TCL_API.txt) (oficjalna pomoc), [ENGINE_COMMAND_GAP](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/ENGINE_COMMAND_GAP.md) (ranking brakujących) | lista zaślepek z priorytetami | 🔶 stan 639 komend: [tcl_komendy_stan.md](tcl_komendy_stan.md) (moduły `cmd_*.cpp`), reszta zaślepki z dziennikiem (`openwiggles_tcl.log`) | kolejne partie wg priorytetów P1/P2 (TODO faza 5) |
| Testy bez okna | [CONTINUOUS_INTEGRATION](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/CONTINUOUS_INTEGRATION.md) | testy EditMode/PlayMode | 🔶 `test3db`, zrzuty `--screenshot`, `owscript` | bot porównawczy z oryginałem (TODO) |
| Wydajność | [PERFORMANCE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/PERFORMANCE.md) | otwarte problemy (zrywy przy generowaniu świata) | – | mierzyć, gdy dojdą duże światy |

## 2. Krasnale i rozgrywka

| Temat | Ich dokumenty | Ich stan | U nas | Następny krok |
|---|---|---|---|---|
| Zachowanie krasnala (chód, wspinanie, kopanie, praca, czas wolny) | [GNOME_BEHAVIOR_COMPLETE_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/GNOME_BEHAVIOR_COMPLETE_RE.md) + [re_parts/](https://github.com/Zimennik/Diggles-Unity/tree/main/Docs/re_parts) (`walk-executor`, `climb-*`, `walkfloor`, `sparetime-idle`, `tcl-work-layer`, `dig-*`, `animsets`…) | specyfikacja 1:1 | 🔶 czas wolny ze skryptów gry działa ([14](silnik/14_gra.md)); zegar i okno pracy (`gametime`, `get_remaining_sparetime`, `auto_choose_workingtime`, [11.4](silnik/11_obiekty_tcl_i_petla.md)) ✅, więc krasnale przechodzą w `work_idle`/`prodfill_dispatch`; ubytek potrzeb ze skryptu (`evt_zwerg_attribupdate`) działa; kobiety modelem `frau` ✅; praca ❌ | praca przez dyspozytora (niżej) |
| Wyszukiwanie drogi, ruch | [ORIGINAL_PATHFINDING_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/ORIGINAL_PATHFINDING_RE.md), [NAVIGATION_AND_MOVEMENT](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/NAVIGATION_AND_MOVEMENT.md) | `Calc1`, `MetaWalk`; problemy na schodach | 🔶 A* po polach ([14](silnik/14_gra.md), [`lib/nav.h`](../src/lib/nav.h)) | siatka 8×8 i maska `.pmp` |
| Kopanie, tunele, jaskinie | [DIGGING_MECHANIC](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/DIGGING_MECHANIC.md), [ROOFED_TUNNEL_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/ROOFED_TUNNEL_RE.md) | działa; kilku kopaczy zbiega się na jednym polu | 🔶 jak w grze ([19](silnik/19_kopanie.md)): materiał, znaczniki, węzły i graf BlockDB, `dig_*` sprawdzone w grze; krasnale kopią skryptem gry | obiekty blokujące wykop, dekoracje usuwane przez wykop, stemple jaskiń |
| Przydział pracy, produkcja | [WORK_DISPATCHER_NATIVE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/WORK_DISPATCHER_NATIVE.md), [PRODUCTION_PANEL_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/PRODUCTION_PANEL_RE.md), [PRODUCTION_SLOT_OVERRIDE_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/PRODUCTION_SLOT_OVERRIDE_RE.md) | zaimplementowane | 🔶 planista etap 1 ([20](silnik/20_planista.md)): pula, prośby kopania, `evt_autoprod_dig`; stan produkcji (`cmd_prod`) | etap 2: zbiory, produkcja, wynalazki, pakowanie, dowóz |
| Odkładanie przedmiotów, wolne miejsce | [ITEM_PLACEMENT_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/ITEM_PLACEMENT_RE.md) | zaimplementowane | ❌ | przy przedmiotach (G8) |
| Przyczepianie, czapki, punkty zaczepienia | [PLACEMENT_ATTACHMENT_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/PLACEMENT_ATTACHMENT_RE.md), [HAT_LINK_SYSTEM](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/HAT_LINK_SYSTEM.md) | zaimplementowane | ✅ punkty zaczepienia, `link_obj`, czapki z zapisu; osadzenie jak u nich: punkt bez z×2, orientacja punktu, interpolacja z rodzicem ([3db.md](formaty/3db.md) §9) | dopinanie przy budowie |
| Drabiny, windy, drzwi | [LADDER_ELEVATOR_DOOR_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/LADDER_ELEVATOR_DOOR_RE.md) | częściowo, na liście braków | ❌ | **szansa na wyprzedzenie** |
| Walka, broń, dyplomacja, SI przeciwnika | [COMBAT_SYSTEM_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/COMBAT_SYSTEM_RE.md) | „prawie w całości nie działa” | ❌ ([16.6](silnik/16_rozgrywka.md)) | **szansa na wyprzedzenie** |
| Energia | [ENERGY_SYSTEM_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/ENERGY_SYSTEM_RE.md) | prawo opisane, w grze brak | ❌ | **szansa na wyprzedzenie** |
| Płyny | [FLUID_SYSTEM_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/FLUID_SYSTEM_RE.md), [WATER_PHASE1_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/WATER_PHASE1_RE.md) | w README „nie działa” | 🔶 dane z zapisu, rysowanie, przepływ 10 Hz ([13](silnik/13_plyny.md)) | **już przed nimi**; zatyczki, odpływy, `-ventil` |
| Samouczek | [TUTORIAL_MODE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/TUTORIAL_MODE.md) | przebieg częściowy | ❌ | po produkcji |
| Mechaniki z punktu widzenia gracza | – | – | 🔶 [16](silnik/16_rozgrywka.md) z instrukcji | |

## 3. Świat i obraz

| Temat | Ich dokumenty | Ich stan | U nas | Następny krok |
|---|---|---|---|---|
| Układ współrzędnych | [COORDINATE_SYSTEM](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/COORDINATE_SYSTEM.md) | aktualne | ✅ ([1](silnik/01_przeglad.md), [6](silnik/06_kamera.md)) | |
| Teren z BlockDB, `.pmp`, `.l2m` | [RENDERING_PIPELINE_COMPLETE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/RENDERING_PIPELINE_COMPLETE.md) | działa; lawa i siarka biorą zły wariant tekstury | ✅ ([4](silnik/04_teren.md), [blockdb.md](formaty/blockdb.md), [szablony.md](formaty/szablony.md)) | wybór wariantów 01–15 ([niewiadome](silnik/niewiadome.md)) |
| Oświetlenie, shadery | [RENDERING_LIGHTING_RESEARCH](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/RENDERING_LIGHTING_RESEARCH.md) | własny shader | ✅ prawo zgodne ze zrzutami ([7](silnik/07_oswietlenie.md)) | |
| Pochodnie w tunelach, ciemność | [TUNNEL_TORCH_LIGHTING_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/TUNNEL_TORCH_LIGHTING_RE.md) | zaimplementowane | ❌ | po kopaniu przez skrypty |
| Tło, paralaksa, krawędzie wycięć | [BACKDROP_PARALLAX_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/BACKDROP_PARALLAX_RE.md), [RIM_CUTOUT_RENDER_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/RIM_CUTOUT_RENDER_RE.md) | tła jaskiń i ruchoma łąka na liście braków | ✅ otwory, strefy horyzontu, panorama, dywan ([8](silnik/08_tlo.md)) | dokładne przycinanie wycięć |
| Nieodkryty teren | [FOG_OF_WAR_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/FOG_OF_WAR_RE.md) | zaimplementowane | ✅ obraz ([10](silnik/10_nieodkryty_teren.md)); 🔶 odkrywanie kołem | odkrywanie promieniami (10.3) |
| Generator świata kampanii i potyczki | [WORLDGEN_ORIGINAL_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/WORLDGEN_ORIGINAL_RE.md), [LEVEL_GENERATOR](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/LEVEL_GENERATOR.md) + [worldgen-re-reports/](https://github.com/Zimennik/Diggles-Unity/tree/main/Docs/worldgen-re-reports) | wierne 1:1 | ❌ (tylko scenariusze, szablony i zapisy) | **największa luka do nowej gry**: `lg_*` |
| Cząsteczki | [PARTICLE_SYSTEM_REWRITE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/PARTICLE_SYSTEM_REWRITE.md) | „większość efektów brakuje” | 🔶 43 typy, wspólny model; zrzuty wszystkich typów z gry i kalibracja rozmiaru/emisji 25 typów w tej samej scenie (`--emit`, [12](silnik/12_czasteczki.md), [BAZA §9.10](BAZA_WIEDZY.md)) | kształt 0/3/6, typy „ziemne” 8/10/15, prawa typów |
| Animacje, zestawy animacji | [ANIMATION_CONTROLLER](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/ANIMATION_CONTROLLER.md) | zaimplementowane | ✅ format i interpolacja ([3db.md](formaty/3db.md), [5](silnik/05_obiekty.md)) | zestawy animacji (`animset`) dla chodu |
| Twarze (oczy, usta) | [FACE_ANIMATION](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/FACE_ANIMATION.md) | zaimplementowane | ❌ | |
| Ramka ekranu | – | – | ✅ prawo zmierzone ([9.4](silnik/09_klatka.md)) | **wiemy więcej niż oni** |
| Tryby rysowania fragmentów modelu (`flags` linku) | – | flag nie używają, przezroczystość zgadują z alfy TGA | ✅ tryby zmierzone w grze ([3db.md](formaty/3db.md)) | **wiemy więcej niż oni**; wdrożyć w rendererze |

## 4. Prezentacja, dźwięk, zapis

| Temat | Ich dokumenty | Ich stan | U nas | Następny krok |
|---|---|---|---|---|
| Przerywniki (sekwencer) | [CUTSCENE_SYSTEM](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/CUTSCENE_SYSTEM.md) | gotowe | ❌ (wiemy: linie `-`/`+`, [15.6](silnik/15_dzwiek.md)) | po rozgrywce |
| Dźwięk przestrzenny, muzyka | [SOUND_SYSTEM_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/SOUND_SYSTEM_RE.md), [AUDIO_SETUP](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/AUDIO_SETUP.md) | działa | 🔶 dekoder, mikser, indeks odwołań ([15](silnik/15_dzwiek.md)) | `sound` w silniku skryptów, zanik z odległością |
| Menu na dole ekranu, wiadomości, dziennik, panel krasnala | [BOTTOM_MENU_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/BOTTOM_MENU_RE.md), [NEWSTICKER_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/NEWSTICKER_RE.md), [DOCUMENT_WINDOW_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/DOCUMENT_WINDOW_RE.md), [UI_DIGGLE_INFO_PANEL_SPEC](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/UI_DIGGLE_INFO_PANEL_SPEC.md), [UI_ARCHITECTURE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/UI_ARCHITECTURE.md) | menu i wiadomości działają; minimapa, drzewko technologii, opcje w braku | 🔶 prosty HUD w ImGui ([14](silnik/14_gra.md)) | interfejs z grafik gry (`layout`, `.kft`) |
| Format zapisu `.sav` | [ORIGINAL_SAVE_FORMAT_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/ORIGINAL_SAVE_FORMAT_RE.md), [ORIGINAL_SAVE_ACTION_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/ORIGINAL_SAVE_ACTION_RE.md) + [save-re-reports/](https://github.com/Zimennik/Diggles-Unity/tree/main/Docs/save-re-reports) (pola obiektów, typy, stan silnika, akcje, płyny) | „w większości zgodny, jeszcze niepewny” | ✅ odczyt mapy i obiektów ([sav.md](formaty/sav.md)), **zapis łatą, który oryginał wczytuje** (BAZA §9.6) | sekcje od znacznika 7 wg ich `save-re-reports` |

## 5. Ich kod (do szukania, nie do kopiowania)

Katalog [`Assets/Diggles/Runtime`](https://github.com/Zimennik/Diggles-Unity/tree/main/Assets/Diggles/Runtime):

| Obszar | Ich pliki | Nasz odpowiednik ([mapa kodu](KOD.md)) |
|---|---|---|
| Interpreter i komendy silnika | [`Tcl/TclInterpreter.cs`](https://github.com/Zimennik/Diggles-Unity/blob/main/Assets/Diggles/Runtime/Tcl/TclInterpreter.cs), [`Tcl/Engine/DigglesTclEngine.cs`](https://github.com/Zimennik/Diggles-Unity/blob/main/Assets/Diggles/Runtime/Tcl/Engine/DigglesTclEngine.cs), [`Tcl/Engine/`](https://github.com/Zimennik/Diggles-Unity/tree/main/Assets/Diggles/Runtime/Tcl/Engine) | [`lib/script/engine.cpp`](../src/lib/script/engine.cpp) |
| Symulacja, tura logiki, dyspozytor pracy | [`Engine/Sim/DigglesGameSim.cs`](https://github.com/Zimennik/Diggles-Unity/blob/main/Assets/Diggles/Runtime/Engine/Sim/DigglesGameSim.cs), [`Engine/Sim/ProdDispatcher.cs`](https://github.com/Zimennik/Diggles-Unity/blob/main/Assets/Diggles/Runtime/Engine/Sim/ProdDispatcher.cs) | program gry `openwiggles` (osobne repozytorium, src/game/main.cpp) (pętla), dyspozytora brak |
| Nawigacja | [`Navigation/Calc1Pathfinder.cs`](https://github.com/Zimennik/Diggles-Unity/blob/main/Assets/Diggles/Runtime/Navigation/Calc1Pathfinder.cs), [`Navigation/`](https://github.com/Zimennik/Diggles-Unity/tree/main/Assets/Diggles/Runtime/Navigation) | [`lib/nav.h`](../src/lib/nav.h) |
| Świat: generator, nieodkryty teren, kopanie | [`World/LevelGenerator.cs`](https://github.com/Zimennik/Diggles-Unity/blob/main/Assets/Diggles/Runtime/World/LevelGenerator.cs), [`World/FogOfWarState.cs`](https://github.com/Zimennik/Diggles-Unity/blob/main/Assets/Diggles/Runtime/World/FogOfWarState.cs), [`World/DigMarkOverlay.cs`](https://github.com/Zimennik/Diggles-Unity/blob/main/Assets/Diggles/Runtime/World/DigMarkOverlay.cs), [`World/`](https://github.com/Zimennik/Diggles-Unity/tree/main/Assets/Diggles/Runtime/World) | [`lib/map_world.h`](../src/lib/map_world.h), [`lib/map_scene.cpp`](../src/lib/map_scene.cpp) |
| Płyny | [`Tcl/Engine/FluidSystem.cs`](https://github.com/Zimennik/Diggles-Unity/blob/main/Assets/Diggles/Runtime/Tcl/Engine/FluidSystem.cs) | [`lib/fluid_sim.h`](../src/lib/fluid_sim.h) |
| Modele, animacje | [`Model/DigglesModelLibrary.cs`](https://github.com/Zimennik/Diggles-Unity/blob/main/Assets/Diggles/Runtime/Model/DigglesModelLibrary.cs), [`Engine/DigglesAnimController.cs`](https://github.com/Zimennik/Diggles-Unity/blob/main/Assets/Diggles/Runtime/Engine/DigglesAnimController.cs) | [`lib/model3db.h`](../src/lib/model3db.h), [`lib/render/model_renderer.h`](../src/lib/render/model_renderer.h) |
| Cząsteczki | [`Engine/Particles/ParticleTypeTable.cs`](https://github.com/Zimennik/Diggles-Unity/blob/main/Assets/Diggles/Runtime/Engine/Particles/ParticleTypeTable.cs), [`Engine/Particles/CpuParticleEmitter.cs`](https://github.com/Zimennik/Diggles-Unity/blob/main/Assets/Diggles/Runtime/Engine/Particles/CpuParticleEmitter.cs) | [`lib/particles.h`](../src/lib/particles.h) |
| Kamera | [`Engine/DigglesCameraController.cs`](https://github.com/Zimennik/Diggles-Unity/blob/main/Assets/Diggles/Runtime/Engine/DigglesCameraController.cs) | [`apps/wigview/main.cpp`](../src/apps/wigview/main.cpp) (`viewProj`) |
| Dźwięk, wiadomości | [`Engine/WorldSoundService.cs`](https://github.com/Zimennik/Diggles-Unity/blob/main/Assets/Diggles/Runtime/Engine/WorldSoundService.cs), [`Engine/AudioController.cs`](https://github.com/Zimennik/Diggles-Unity/blob/main/Assets/Diggles/Runtime/Engine/AudioController.cs), [`Tcl/Engine/NewsTickerSystem.cs`](https://github.com/Zimennik/Diggles-Unity/blob/main/Assets/Diggles/Runtime/Tcl/Engine/NewsTickerSystem.cs) | [`lib/sound.h`](../src/lib/sound.h) |
| Zapis | [`Save/`](https://github.com/Zimennik/Diggles-Unity/tree/main/Assets/Diggles/Runtime/Save) | [`lib/save_game.h`](../src/lib/save_game.h) |

## 6. Dogonić i wyprzedzić

**Gdzie już jesteśmy przed nimi:** płyny (przepływ działa, u nich nie), zapis `.sav`, który wczytuje oryginalna gra, ramka ekranu, tryby rysowania z flag `.3db`, użycie `TEXMAPS.BIN` przez grę, pomiary w działającym oryginale (WebTraq, sondy interpreterów, [BAZA_WIEDZY §8.6–8.7](BAZA_WIEDZY.md)), przeglądarka zasobów z dźwiękami i odwołaniami w skryptach, natywny silnik C++ z Vulkanem i OpenGL zamiast Unity.

**Co blokuje grywalność u nas, a u nich działa** (kolejność wg [TODO](../TODO.md), „Droga do grywalności”):
1. **Dyspozytor pracy i produkcja** (krasnale same pracują, ognisko, skrzynie): [WORK_DISPATCHER_NATIVE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/WORK_DISPATCHER_NATIVE.md), [PRODUCTION_PANEL_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/PRODUCTION_PANEL_RE.md).
2. **Kopanie przez skrypty gry**: [DIGGING_MECHANIC](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/DIGGING_MECHANIC.md), [re_parts/dig-natives.md](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/re_parts/dig-natives.md), [re_parts/tcl-dig-complete.md](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/re_parts/tcl-dig-complete.md).
3. **Ruch jak w grze** (`Calc1`, wspinanie, drabiny): [ORIGINAL_PATHFINDING_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/ORIGINAL_PATHFINDING_RE.md), [re_parts/walk-executor.md](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/re_parts/walk-executor.md).
4. **Przedmioty i ekwipunek**: [ITEM_PLACEMENT_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/ITEM_PLACEMENT_RE.md).
5. **Generator świata** (nowa gra w kampanii i potyczce): [WORLDGEN_ORIGINAL_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/WORLDGEN_ORIGINAL_RE.md).

**Gdzie możemy ich wyprzedzić, bo u nich też nie działa:** walka i potwory, energia, drabiny, windy i drzwi, przejścia między światami, pełny zapis i wczytanie `.sav`, płyny, większość cząsteczek, minimapa i drzewko technologii. Przewagą jest laboratorium: każde prawo z ich dokumentów sprawdzamy na działającym oryginale przez [`live.sh`](../tools/lab/live.sh) zamiast tylko w kodzie exe, a bot porównawczy (TODO) da nam test zgodności, którego oni nie mają.

---
Aktualizacja tej strony: przy każdej pracy nad podsystemem popraw wiersz (stan u nas, następny krok). Nowe dokumenty Diggles-Unity: porównaj [ich spis](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/README.md) z tabelami i odśwież lokalną kopię w `../ref/`.
