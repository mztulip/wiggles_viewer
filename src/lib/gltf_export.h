// Eksport obiektu z modelu .3db do glTF 2.0 (plik binarny .glb, tekstury osadzone jako PNG).
//
// Animacje Wiggles są wierzchołkowe, więc odwzorowujemy je morph targetami:
//  - każda eksportowana animacja to osobny węzeł z własną siatką,
//  - siatka = klatka 0 animacji, a cele morfingu = różnice pozycji i normalnych klatek 1..N-1,
//  - klip animacji glTF steruje wagami (w chwili i/fps waga celu i = 1). Interpolacja LINEAR
//    wag daje dokładnie liniowe przejście między klatkami, tak jak w grze.
// Przy kilku animacjach węzły innych animacji są ukryte (skala 0). Klip pokazuje swój węzeł
// (skala 1) i ukrywa węzeł pierwszej animacji, która jest domyślnie widoczna.
//
// Układ współrzędnych: gra ma układ lewoskrętny (Y w dół, Z do widza), glTF prawoskrętny z Y w górę,
// więc odbijamy oś Y: (x, y, z) -> (x, -y, z). Siatki są dwustronne, więc zmiana kolejności
// wierzchołków trójkątów nie ma znaczenia.
// UV nie wymaga zmian (oba formaty mają v=0 u góry tekstury).
#pragma once

#include "lib/model3db.h"

#include <string>
#include <vector>

namespace ow {

class Vfs;

struct GltfExportOptions {
	std::vector<int> animations; // indeksy w Object3db::animations; pusty = wszystkie
	float fps = 10.f; // tempo animacji gry (docs/silnik/11_obiekty_tcl_i_petla.md §11.6)
	float scale = 0.01f;     // jednostki modelu -> metry (krasnal ~2 m)
	bool interpolate = true; // LINEAR (jak gra) albo STEP
};

struct GltfExportStats {
	size_t animations = 0, frames = 0, primitives = 0, textures = 0;
	size_t incompatibleFrames = 0; // klatki o innej topologii niż klatka 0 animacji (zastąpione poprzednią)
	size_t bytes = 0;
};

// Eksportuje obiekt objectIndex do pliku .glb. sphere: 256 normalnych z SPHERE.DAT.
bool exportGltf(const Model3db& model, const Vfs& vfs, const std::vector<Vec3f>& sphere, int objectIndex,
                const GltfExportOptions& opt, const std::string& outPath, std::string& err,
                GltfExportStats* stats = nullptr);

} // namespace ow
