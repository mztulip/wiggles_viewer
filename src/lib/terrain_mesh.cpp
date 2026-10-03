#include "lib/terrain_mesh.h"

#include "lib/blockdb.h"
#include "lib/heightfield.h"
#include "lib/map_template.h"

#include <algorithm>
#include <cmath>

namespace ow {

TerrainMesh buildTerrainMesh(const Pmp& pmp, const BlockDb& db, const std::vector<Vec3f>& sphere, bool showUndug) {
	constexpr float B = 200.f;
	TerrainMesh m;
	m.width = int(pmp.width);
	m.height = int(pmp.height);
	m.blockRanges.assign(size_t(m.width) * m.height, {0, 0});

	// Węzły otwarte do tła (typ 5), z sondą [self, S, E, SE] jak w grze.
	m.openNode.assign(size_t(m.width) * m.height, 0);
	for (int y = 0; y < m.height; ++y)
		for (int x = 0; x < m.width; ++x) {
			const int probe[4][2] = {{x, y}, {x, y + 1}, {x + 1, y}, {x + 1, y + 1}};
			for (auto& p : probe) {
				if (p[0] >= m.width || p[1] >= m.height) continue;
				uint8_t t = pmp.type(p[0], p[1]) & 0x7F;
				if (!t) continue;
				m.openNode[size_t(y) * m.width + x] = t == 5;
				break;
			}
		}
	auto open = [&](int x, int y) {
		x = std::clamp(x, 0, m.width - 1);
		y = std::clamp(y, 0, m.height - 1);
		return m.openNode[size_t(y) * m.width + x] != 0;
	};

	auto groundAt = [&](float gx, float gy, float w[3]) {
		// Dwuliniowo między środkami bloków (bx+0.5, by+0.5).
		float fx = gx - 0.5f, fy = gy - 0.5f;
		int x0 = int(std::floor(fx)), y0 = int(std::floor(fy));
		float tx = fx - float(x0), ty = fy - float(y0);
		w[0] = w[1] = w[2] = 0.f;
		for (int dy = 0; dy <= 1; ++dy)
			for (int dx = 0; dx <= 1; ++dx) {
				int bx = std::clamp(x0 + dx, 0, m.width - 1), by = std::clamp(y0 + dy, 0, m.height - 1);
				float k = (dx ? tx : 1 - tx) * (dy ? ty : 1 - ty);
				w[groundFromPmpType(pmp.type(bx, by))] += k;
			}
	};

	for (int by = 0; by < m.height; ++by)
		for (int bx = 0; bx < m.width; ++bx) {
			uint8_t type = pmp.type(bx, by);
			bool o00 = open(bx, by), o10 = open(bx + 1, by), o01 = open(bx, by + 1), o11 = open(bx + 1, by + 1);
			if (o00 && o10 && o01 && o11) continue; // cały blok otwarty do tła: nie rysujemy
			uint32_t id = (isUndug(type) && !showUndug) ? 0u : pmp.block(bx, by); // blok 0 = lita skała
			uint32_t first = uint32_t(m.indices.size());
			for (int sub = 0; sub < 3; ++sub) {
				BdbSubMesh sm;
				if (!db.subMesh(id, sub, sm)) continue;
				uint32_t base = uint32_t(m.vertices.size());
				for (uint16_t i = 0; i < sm.vertCount; ++i) {
					const BdbPaletteEntry& p = db.palette(sm.paletteIndex(i));
					TerrainVertex v;
					v.pos[0] = (float(bx) + p.x) * B;
					v.pos[1] = (float(by) + p.y) * B;
					v.pos[2] = p.z * B;
					Vec3f n = sphere.size() == 256 ? sphere[sm.normalIdx[i]] : Vec3f{0, 0, 1};
					v.normal[0] = n.x;
					v.normal[1] = n.y;
					v.normal[2] = n.z;
					groundAt(float(bx) + p.x, float(by) + p.y, v.ground);
					float lx = std::clamp(p.x, 0.f, 1.f), ly = std::clamp(p.y, 0.f, 1.f);
					v.alpha = (1 - lx) * (1 - ly) * (o00 ? 0.f : 1.f) + lx * (1 - ly) * (o10 ? 0.f : 1.f) +
					          (1 - lx) * ly * (o01 ? 0.f : 1.f) + lx * ly * (o11 ? 0.f : 1.f);
					m.vertices.push_back(v);
				}
				for (uint32_t i = 0; i < sm.indexCount; ++i) m.indices.push_back(base + sm.indices[i]);
			}
			m.blockRanges[size_t(by) * m.width + bx] = {first, uint32_t(m.indices.size()) - first};
		}
	return m;
}

} // namespace ow
