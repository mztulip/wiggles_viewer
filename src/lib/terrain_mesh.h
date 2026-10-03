// Siatka terenu szablonu złożona ze szczegółowych siatek bloków BlockDB (3 podsiatki na blok).
//
// Pozycja wierzchołka (jednostki modelu, 1 blok = 200): x = (bx + p.x)·200, y = (by + p.y)·200
// (y w dół), z = p.z·200 (do widza; lita skała z = 14,5 bloku). Normalna = SPHERE.DAT[indeks].
// Wagi gruntów A/B/C interpolowane dwuliniowo między środkami bloków (typ pola .PMP).
#pragma once

#include "lib/model3db.h" // Vec3f

#include <cstdint>
#include <utility>
#include <vector>

namespace ow {

struct Pmp;
class BlockDb;

struct TerrainVertex {
	float pos[3];
	float normal[3];
	float ground[3]; // wagi gruntów A, B, C (suma 1)
};

struct TerrainMesh {
	int width = 0, height = 0; // w blokach
	std::vector<TerrainVertex> vertices;
	std::vector<uint32_t> indices;
	std::vector<std::pair<uint32_t, uint32_t>> blockRanges; // [by*w+bx] -> (pierwszy indeks, liczba indeksów)
};

// showUndug: bloki niewykopane (typ 0) z zaplanowanym kształtem zamiast litej skały (blok 0).
TerrainMesh buildTerrainMesh(const Pmp& pmp, const BlockDb& db, const std::vector<Vec3f>& sphere, bool showUndug = false);

} // namespace ow
