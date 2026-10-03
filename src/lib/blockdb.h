// Data/BlockDB0/1/2.BDB: baza 118688 bloków terenu (opis: docs/formaty/mapy.md).
//
// Każdy blok szablonu (ID z .PMP) ma rekord 89 B z siatką 8×8 głębokości. Wartości 0..15:
// 15 = lita skała na przedniej płaszczyźnie, mniej = wydrążenie, głębokość = (15 - v) * 100
// jednostek modelu (✅ zgodne z OBJ eksportowanym przez grę). Rekordy są identyczne we
// wszystkich trzech plikach; pliki różnią się tylko szczegółowymi siatkami 3D (jakość).
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace ow {

class Vfs;

struct BlockRecord {
	std::array<uint8_t, 64> depth; // 8×8, wiersz po wierszu od góry
	std::array<uint8_t, 16> extra; // 🔶 4 × u32
	std::array<uint8_t, 9> tail;   // 🔶 4 B (często narożniki siatki) + 5 B flag
};
static_assert(sizeof(BlockRecord) == 89, "rekord BlockDB ma 89 bajtów");

class BlockDb {
public:
	static constexpr uint32_t kBlocks = 118688;

	// Wczytuje rekordy z BlockDB0.BDB (najmniejszy plik). Rzuca ParseError.
	void load(const Vfs& vfs);
	void loadData(const std::vector<uint8_t>& data);
	bool loaded() const { return !records_.empty(); }
	size_t size() const { return records_.size(); }
	const BlockRecord* record(uint32_t id) const { return id < records_.size() ? &records_[id] : nullptr; }

private:
	std::vector<BlockRecord> records_;
};

} // namespace ow
