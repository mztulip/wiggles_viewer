#include "lib/heightfield.h"

#include "lib/blockdb.h"
#include "lib/map_template.h"

namespace ow
{

int groundFromPmpType(uint8_t type)
{
	switch (type & 0x0F)
	{
	case 0: // klasa domyślna świata, ta sama co 2
	case 2: return 2; // C, piasek
	case 3: return 1; // B, ziemia
	default: return 0; // A, skała (typ 4 i pozostałe)
	}
}

Heightfield buildHeightfield(const Pmp& pmp, const BlockDb& db)
{
	const int S = Heightfield::kSamplesPerBlock;
	Heightfield h;
	h.width = int(pmp.width) * S;
	h.height = int(pmp.height) * S;
	h.value.assign(size_t(h.width) * h.height, 15);
	h.blockId.assign(h.value.size(), 0);
	h.ground.assign(h.value.size(), 0);
	for (int by = 0; by < int(pmp.height); ++by)
		for (int bx = 0; bx < int(pmp.width); ++bx)
		{
			uint32_t id = pmp.block(bx, by);
			uint8_t type = pmp.type(bx, by);
			const BlockRecord* rec = db.record(id);
			uint8_t g = uint8_t(groundFromPmpType(type));
			for (int sy = 0; sy < S; ++sy)
				for (int sx = 0; sx < S; ++sx)
				{
					size_t i = size_t(by * S + sy) * h.width + size_t(bx * S + sx);
					h.value[i] = rec ? rec->depth[size_t(sy * S + sx)] : 15;
					h.blockId[i] = id;
					h.ground[i] = g;
				}
		}
	return h;
}

} // namespace ow
