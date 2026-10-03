// Siatka terenu szablonu złożona ze szczegółowych siatek bloków BlockDB (3 podsiatki na blok).
//
// Pozycja wierzchołka (jednostki modelu, 1 blok = 200): x = (bx + p.x)·200, y = (by + p.y)·200
// (y w dół), z = p.z·200 (do widza; lita skała z = 14,5 bloku). Normalna = SPHERE.DAT[indeks].
// Wagi gruntów A/B/C interpolowane dwuliniowo między środkami bloków (typ pola .PMP).
//
// Otwory do tła (typ pola 5), za Zimennik/Diggles-Unity (MIT), Docs/RIM_CUTOUT_RENDER_RE.md
// i BACKDROP_PARALLAX_RE.md §4b: węzeł (= blok) jest "otwarty", gdy pierwszy niezerowy typ
// (7 młodszych bitów) spośród [węzeł, sąsiad S, sąsiad E, sąsiad SE] to 5. Alfa wierzchołka =
// dwuliniowo z narożników bloku (bx,by), (bx+1,by), (bx,by+1), (bx+1,by+1): 0 dla otwartego,
// 1 dla pozostałych; gra rysuje teren z alpha testem > 126/255 (ostra krawędź w połowie bloku).
// Bloki z czterema otwartymi narożnikami nie są rysowane.
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
	float alpha;     // 0..1: dwuliniowo z narożników bloku (0 = węzeł otwarty do tła); rysujemy, gdy > 126/255
	float base;      // jasność bazowa B = 16·warstwa2 (dwuliniowo z narożników) + 64 (game_lighting.h)
};

struct TerrainMesh {
	int width = 0, height = 0; // w blokach
	std::vector<TerrainVertex> vertices;
	std::vector<uint32_t> indices;
	std::vector<std::pair<uint32_t, uint32_t>> blockRanges; // [by*w+bx] -> (pierwszy indeks, liczba indeksów)
	std::vector<uint8_t> openNode;                          // [by*w+bx] -> 1, gdy węzeł otwarty do tła
};

// world: świat po nałożeniu szablonów (map_world.h).
TerrainMesh buildTerrainMesh(const Pmp& world, const BlockDb& db, const std::vector<Vec3f>& sphere);

} // namespace ow
