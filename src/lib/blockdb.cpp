#include "lib/blockdb.h"

#include "lib/binreader.h"
#include "lib/vfs.h"

namespace ow {

void BlockDb::load(const Vfs& vfs, int quality) {
	std::string path = "Data/BlockDB" + std::to_string(quality < 0 ? 0 : (quality > 2 ? 2 : quality)) + ".BDB";
	auto data = vfs.read(path);
	if (!data) throw ParseError("brak " + path);
	loadData(*data);
}

void BlockDb::loadData(const std::vector<uint8_t>& data) {
	BinReader r(data);
	uint32_t version = r.u32();
	r.u32();
	uint32_t blocks = r.u32();
	r.u32();
	uint32_t nIndex = r.u32();
	uint32_t nDesc = r.u32();
	uint32_t nPalette = r.u32();
	if (version != 11 || blocks != kBlocks) throw ParseError("nieznany nagłówek BlockDB");

	cells_.resize(size_t(blocks) * 3);
	for (auto& c : cells_) {
		c.vertCount = r.u16();
		c.descriptor = r.u32();
	}
	records_.resize(blocks);
	r.readInto(records_.data(), size_t(blocks) * sizeof(BlockRecord));
	r.readArray(indexPool_, nIndex);
	descriptors_.resize(nDesc);
	for (auto& d : descriptors_) d = {r.u32(), r.u32()};
	palette_.resize(nPalette);
	r.readInto(palette_.data(), size_t(nPalette) * sizeof(BdbPaletteEntry));

	size_t bulkSize = 0;
	for (auto& c : cells_) {
		c.bulkOffset = uint32_t(bulkSize);
		bulkSize += size_t(c.vertCount) * 3;
	}
	r.readArray(bulk_, bulkSize);
	if (!r.atEnd()) throw ParseError("nadmiarowe dane na końcu BlockDB");

	// Walidacja odwołań (renderer im ufa).
	for (const auto& c : cells_) {
		if (!c.vertCount) continue;
		if (c.descriptor >= descriptors_.size()) throw ParseError("deskryptor poza zakresem");
		const auto& d = descriptors_[c.descriptor];
		if (uint64_t(d[0]) + d[1] > indexPool_.size()) throw ParseError("zakres indeksów poza pulą");
		const uint8_t* p = bulk_.data() + c.bulkOffset;
		for (uint16_t i = 0; i < c.vertCount; ++i)
			if (uint16_t(p[i * 2] | (p[i * 2 + 1] << 8)) >= palette_.size()) throw ParseError("indeks palety poza zakresem");
		for (uint32_t i = 0; i < d[1]; ++i)
			if (indexPool_[d[0] + i] >= c.vertCount) throw ParseError("indeks trójkąta poza podsiatką");
	}
}

bool BlockDb::subMesh(uint32_t block, int sub, BdbSubMesh& out) const {
	if (block >= records_.size() || sub < 0 || sub > 2) return false;
	const Cell& c = cells_[size_t(block) * 3 + size_t(sub)];
	if (!c.vertCount) return false;
	const auto& d = descriptors_[c.descriptor];
	out.vertCount = c.vertCount;
	out.paletteRaw = bulk_.data() + c.bulkOffset;
	out.normalIdx = bulk_.data() + c.bulkOffset + size_t(c.vertCount) * 2;
	out.indices = indexPool_.data() + d[0];
	out.indexCount = d[1];
	return out.indexCount > 0;
}

} // namespace ow
