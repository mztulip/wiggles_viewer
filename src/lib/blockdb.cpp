#include "lib/blockdb.h"

#include "lib/binreader.h"
#include "lib/vfs.h"

namespace ow {

void BlockDb::load(const Vfs& vfs) {
	auto data = vfs.read("Data/BlockDB0.BDB");
	if (!data) throw ParseError("brak Data/BlockDB0.BDB");
	loadData(*data);
}

void BlockDb::loadData(const std::vector<uint8_t>& data) {
	BinReader r(data);
	uint32_t version = r.u32();
	r.u32();
	uint32_t blocks = r.u32();
	if (version != 11 || blocks != kBlocks) throw ParseError("nieznany nagłówek BlockDB");
	r.skip(16); // u32 0, liczba wierzchołków?, liczba elementów puli, liczba siatek?, u32 4 (🔶)
	r.skip(size_t(blocks) * 3 * 6); // ILT: 3 × (u16, u32) na blok -> szczegółowe siatki (🔶)
	records_.resize(blocks);
	r.readInto(records_.data(), size_t(blocks) * sizeof(BlockRecord));
}

} // namespace ow
