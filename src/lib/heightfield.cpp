#include "lib/heightfield.h"

#include "lib/blockdb.h"
#include "lib/map_template.h"

namespace ow {

Heightfield buildHeightfield(const Pmp& pmp, const BlockDb& db) {
	const int S = Heightfield::kSamplesPerBlock;
	Heightfield h;
	h.width = int(pmp.width) * S;
	h.height = int(pmp.height) * S;
	h.value.assign(size_t(h.width) * h.height, 15);
	h.blockId.assign(h.value.size(), 0);
	for (int by = 0; by < int(pmp.height); ++by)
		for (int bx = 0; bx < int(pmp.width); ++bx) {
			uint32_t id = pmp.block(bx, by);
			const BlockRecord* rec = db.record(id);
			for (int sy = 0; sy < S; ++sy)
				for (int sx = 0; sx < S; ++sx) {
					size_t i = size_t(by * S + sy) * h.width + size_t(bx * S + sx);
					h.value[i] = rec ? rec->depth[size_t(sy * S + sx)] : 15;
					h.blockId[i] = id;
				}
		}
	return h;
}

} // namespace ow
