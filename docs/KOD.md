# Mapa kodu

Gdzie w [`src/`](../src) jest implementacja każdego podsystemu, który rozdział przewodnika ją opisuje i czym ją sprawdzić. Przewodnik ([docs/README.md](README.md)) mówi, **jak działa gra**; ta strona mówi, **gdzie to jest u nas**.

## Warstwy i zależności

```
src/apps/wigview   src/apps/game   src/tools        programy
        \               |              /
         +--------- src/lib (libwiggles) ---------+  biblioteka
         |  formaty, vfs, skrypty, dźwięk, scena   |
         |  render/      renderery                 |
         |  render/rhi/  Vulkan / OpenGL           |
         +-----------------------------------------+
src/shaders (GLSL, osadzane przez build.sh)    third_party (imgui, dr_mp3)
```

- **Zależności idą tylko w dół:** [`src/lib`](../src/lib) nie dołącza niczego z [`src/apps`](../src/apps). [`build.sh`](../build.sh) przerywa budowanie, gdy ta zasada jest złamana (`check_layers`). Kod potrzebny grze i przeglądarce trafia do biblioteki.
- Renderery nie wołają OpenGL ani Vulkana bezpośrednio, tylko przez [`rhi.h`](../src/lib/render/rhi/rhi.h).
- Styl: [`.clang-format`](../.clang-format) (klamry w osobnych liniach). Budowanie: `./build.sh [debug|release] [cel]`, opis w [README](../README.md).

## Podsystemy biblioteki

| Podsystem | Kod | Najważniejsze | Opis | Sprawdzanie |
|---|---|---|---|---|
| Pliki gry | [vfs](../src/lib/vfs.h), [text](../src/lib/text.h), [binreader](../src/lib/binreader.h) | `Vfs::read/list/resolve` (bez rozróżniania wielkości liter), `Vfs::addOverlay` (mody: `Data/` + `Data_<język>`, `--overlay`), `cp1250ToUtf8`/`cp1252ToUtf8` | [rozdział 1](silnik/01_przeglad.md), [tekstury.md](formaty/tekstury.md) (nakładki) | wszystkie narzędzia |
| Teksty gry | [messages](../src/lib/messages.h) ([.cpp](../src/lib/messages.cpp)) | `Messages` (`MESSAGES.TXT`), `parseQuotes`; `lmsg` i `locale` w silniku skryptów | [teksty.md](formaty/teksty.md) | `test3db` (teksty) |
| Modele `.3db` | [model3db](../src/lib/model3db.h) ([.cpp](../src/lib/model3db.cpp)), [sphere](../src/lib/sphere.h) | `parseModel3db`, `Model3db::linkOffset`, pola `linkTable`, `shadowQuad`, `MeshLink::flags` (tryb rysowania), `CMap3db` (maska kolizji), `parseSphereDat` | [3db.md](formaty/3db.md), [sphere_dat.md](formaty/sphere_dat.md), [rozdział 5](silnik/05_obiekty.md) | `test3db`, `wigview --model` |
| Tekstury | [tga](../src/lib/tga.h), [texture_find](../src/lib/texture_find.h), [render/texture_cache](../src/lib/render/texture_cache.h) | `loadTga`, `findTexture`, `TextureCache::getPath` | [tekstury.md](formaty/tekstury.md) | `test3db`, `wigview --texture` (zakładka Tekstury) |
| Bloki terenu | [blockdb](../src/lib/blockdb.h) ([.cpp](../src/lib/blockdb.cpp)) | `BlockDb::load` | [blockdb.md](formaty/blockdb.md) | `test3db` |
| Szablony map | [map_template](../src/lib/map_template.h) ([.cpp](../src/lib/map_template.cpp)) | `.PMP`, `.l2m`, `parseTemplateScript` | [szablony.md](formaty/szablony.md), [rozdział 3](silnik/03_swiat_z_szablonow.md) | `test3db` |
| Składanie świata | [map_world](../src/lib/map_world.h) ([.cpp](../src/lib/map_world.cpp)) | `parseScenario`, `buildWorld`, `buildWorldFromSave`, `applySkyLight`, `GameView`, `kGameZoomMin/Max` | [rozdział 3](silnik/03_swiat_z_szablonow.md), [rozdział 6](silnik/06_kamera.md) | `wigview --scenario` |
| Teren | [heightfield](../src/lib/heightfield.h), [terrain_mesh](../src/lib/terrain_mesh.h), [render/terrain_renderer](../src/lib/render/terrain_renderer.h), shadery [terrain.vert](../src/shaders/terrain.vert)/[.frag](../src/shaders/terrain.frag) | `buildHeightfield`, `buildTerrainMesh`, `TerrainRenderer::build` | [rozdział 4](silnik/04_teren.md) | `wigview --template`, [`tools/lab/capture.sh`](../tools/lab/capture.sh) |
| Klasy obiektów | [class_db](../src/lib/class_db.h) ([.cpp](../src/lib/class_db.cpp)) | `ClassDb::find/lights/particles/horizon` (klasa → model, lampy, cząsteczki, strefa) | [rozdział 5](silnik/05_obiekty.md) | `test3db` (szablony) |
| Rysowanie modeli | [render/model_renderer](../src/lib/render/model_renderer.h) ([.cpp](../src/lib/render/model_renderer.cpp)), shadery [model.vert](../src/shaders/model.vert)/[.frag](../src/shaders/model.frag) | `ModelRenderer::drawFrames` (interpolacja klatek, tryby rysowania z `MeshLink::flags`) | [rozdział 5](silnik/05_obiekty.md), [rozdział 9](silnik/09_klatka.md) | `wigview --model --screenshot` |
| **Scena mapy** | [map_scene](../src/lib/map_scene.h) ([.cpp](../src/lib/map_scene.cpp)) | `MapScene`: `loadTemplate/loadScenario/loadSavePath`, `render`, `updateZone`, `baseLight`, `updateFog`, obiekty dla gry (`setObjectPose`, `playAnim`, `linkObject`) | [rozdział 9](silnik/09_klatka.md) (kolejność rysowania) | `wigview --scenario/--save`, gra |
| Oświetlenie | [render/game_lighting](../src/lib/render/game_lighting.h) ([.cpp](../src/lib/render/game_lighting.cpp)), [game_lighting.glsl](../src/shaders/game_lighting.glsl) | `uploadScene`, `GameLight`, `MapScene::baseLight` | [rozdział 7](silnik/07_oswietlenie.md) | `wigview --scenario`, porównanie z `capture.sh` |
| Ramka ekranu | [render/border_mask](../src/lib/render/border_mask.h) ([.cpp](../src/lib/render/border_mask.cpp)), [border_mask.frag](../src/shaders/border_mask.frag) | `BorderMask::draw` | [rozdział 9.4](silnik/09_klatka.md) | `wigview --border-mask`, `openwiggles` |
| Tło | [render/panorama_renderer](../src/lib/render/panorama_renderer.h), `MapScene::updateZone` | panorama, dywan łąki, strefy horyzontu | [rozdział 8](silnik/08_tlo.md) | `wigview --scenario systeminit` |
| Nieodkryty teren | `MapScene::updateFog`, `objectVisibleInFog` ([map_scene.cpp](../src/lib/map_scene.cpp)), [game_lighting.glsl](../src/shaders/game_lighting.glsl) | bajt mgły na pole, tekstura narożników | [rozdział 10](silnik/10_nieodkryty_teren.md) | `wigview --fow N` |
| Cząsteczki | [particles](../src/lib/particles.h) ([.cpp](../src/lib/particles.cpp)), [render/particle_renderer](../src/lib/render/particle_renderer.h) | `particleTypes`, `ParticleEmitter` (`setFloorDrop`), `scanParticleUsage`, `MapScene::addEmitter`/`floorDrop` | [rozdział 12](silnik/12_czasteczki.md) | `wigview --particle N`, `wigview --emit` (ta sama scena co zrzuty gry, [BAZA §9.10](BAZA_WIEDZY.md)) |
| Płyny | [fluid_sim](../src/lib/fluid_sim.h) ([.cpp](../src/lib/fluid_sim.cpp)), [render/fluid_renderer](../src/lib/render/fluid_renderer.h) | `FluidSim::update`, `FluidRenderer::build` | [rozdział 13](silnik/13_plyny.md) | `wigview --save` |
| Zapis gry | [save_game](../src/lib/save_game.h) ([.cpp](../src/lib/save_game.cpp)) | `parseSaveGame`, `writeSavePatch` | [sav.md](formaty/sav.md) | `test3db` (zapisy) |
| Skrypty Tcl | [script/engine](../src/lib/script/engine.h) ([.cpp](../src/lib/script/engine.cpp)) | `script::Engine` (`boot`, `eval`), `Host` (świat dostarczany przez grę), obiekty, zdarzenia, timery | [rozdział 2](silnik/02_start_i_skrypty.md), [rozdział 11](silnik/11_obiekty_tcl_i_petla.md) | `owscript`, gra |
| Nawigacja | [nav](../src/lib/nav.h) ([.cpp](../src/lib/nav.cpp)) | `NavGrid` (🔶 przybliżenie) | [rozdział 14](silnik/14_gra.md) | gra |
| Dźwięk | [sound](../src/lib/sound.h) ([.cpp](../src/lib/sound.cpp)), [sound_refs](../src/lib/sound_refs.h) ([.cpp](../src/lib/sound_refs.cpp)) | `decodeSound`, `AudioOut` (mikser SDL), `scanSoundRefs` | [rozdział 15](silnik/15_dzwiek.md) | `test3db` (dźwięki), zakładka Dźwięki |
| Eksport glTF | [gltf_export](../src/lib/gltf_export.h) ([.cpp](../src/lib/gltf_export.cpp)), [png_write](../src/lib/png_write.h) | `exportGltf` (morph targets) | [3db.md](formaty/3db.md) | `export_gltf`, `npx gltf-validator` |
| Tłumaczenia | [i18n](../src/lib/i18n.h) ([.cpp](../src/lib/i18n.cpp)), [lang/](../lang/README.md) | `tr()`, `N_()`, `strf()` (komunikaty z parametrami) | [lang/README.md](../lang/README.md) | `tools/i18n_update.py --check` |
| Warstwa renderowania | [render/rhi/rhi.h](../src/lib/render/rhi/rhi.h), [rhi_vk.cpp](../src/lib/render/rhi/rhi_vk.cpp), [rhi_gl.cpp](../src/lib/render/rhi/rhi_gl.cpp), [render/gl.h](../src/lib/render/gl.h), [common.glsl](../src/shaders/common.glsl) | `rhi::Device`, makra `UBO`/`TEX`/`OW_CLIP` | [README: renderowanie](../README.md) | zrzuty `--renderer vulkan` i `opengl` + `magick compare` |

## Programy

| Program | Kod | Co tam jest |
|---|---|---|
| `wigview` | [main.cpp](../src/apps/wigview/main.cpp) | okno, kamera, zakładki Modele i Tekstury, wyszukiwarka Ctrl+F, miniatury (`--thumbnails`), opcje wiersza poleceń (opis na początku pliku) |
| | [map_view](../src/apps/wigview/map_view.h) ([.cpp](../src/apps/wigview/map_view.cpp)) | zakładki Mapy i Zapisy: panel ImGui nad `MapScene` (listy, siatka 2D, przełączniki) |
| | [particle_view](../src/apps/wigview/particle_view.h) | zakładka Cząsteczki |
| | [sound_view](../src/apps/wigview/sound_view.h) ([.cpp](../src/apps/wigview/sound_view.cpp)) | zakładka Dźwięki |
| | [files_view](../src/apps/wigview/files_view.h) ([.cpp](../src/apps/wigview/files_view.cpp)) | zakładka Pliki: drzewo `Data/`, statystyka typów, podgląd tekstu i hex, przejście do zakładki typu (`Viewer::openFile`) |
| `openwiggles` (gra) | [main.cpp](../src/apps/game/main.cpp) | `Game` (pętla, kamera, krasnale, kopanie), `ScriptHost` (most skrypty ↔ `MapScene`), zapis `.ows` ([rozdział 14](silnik/14_gra.md)) |
| `test3db` | [test3db.cpp](../src/tools/test3db.cpp) | parsery na wszystkich danych gry: modele, tekstury, szablony, zapisy, dźwięki |
| `export_gltf` | [export_gltf.cpp](../src/tools/export_gltf.cpp) | eksport modelu do `.glb` z wiersza poleceń |
| `owscript` | [owscript.cpp](../src/tools/owscript.cpp) | silnik skryptów bez okna: tworzy obiekt klasy (domyślnie `Zwerg`), przewija logikę i zapisuje dziennik zaślepek komend i błędów ([rozdział 11 §11.9](silnik/11_obiekty_tcl_i_petla.md)) |

## Narzędzia poza [`src/`](../src)

| Narzędzie | Do czego |
|---|---|
| [build.sh](../build.sh) | budowanie bez CMake; generuje `build/gen/` (wersja, shadery, tłumaczenia) |
| [tools/check_docs.py](../tools/check_docs.py) | spójność dokumentacji: wszystko osiągalne z `CLAUDE.md`, brak zepsutych linków i nieaktualnych ścieżek |
| [tools/i18n_update.py](../tools/i18n_update.py) | napisy `tr()` → `lang/*.po` |
| [tools/scan_tcl_natives.py](../tools/scan_tcl_natives.py) | komendy natywne używane w skryptach gry ([wynik](tcl_natywne_kandydaci.txt)) |
| [tools/lab/live.sh](../tools/lab/live.sh) | gra z laboratorium w tle z serwerem WebTraq: Tcl w działającej grze (`tcl`), strony obiektów i klas (`page`), zrzuty (`shot`); [BAZA_WIEDZY.md §8.6](BAZA_WIEDZY.md) |
| [tools/lab/owprobe_deep.tcl](../tools/lab/owprobe_deep.tcl), [owprobe_play.tcl](../tools/lab/owprobe_play.tcl) | sondy Fazy 0: klasy, bazy modeli, interpretery obiektów, takt i timery; obserwacja krasnali po `gameload` ([§8.7](BAZA_WIEDZY.md)) |
| [tools/lab/](../tools/lab/setup_lab.sh) | laboratorium: oryginalna gra pod Wine z sondą Tcl ([setup_lab.sh](../tools/lab/setup_lab.sh), [run_lab.sh](../tools/lab/run_lab.sh), [capture.sh](../tools/lab/capture.sh) do zrzutów wzorcowych); metoda w [BAZA_WIEDZY.md §8](BAZA_WIEDZY.md) |
