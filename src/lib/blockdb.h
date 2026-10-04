// Data/BlockDB0/1/2.BDB: baza 118688 bloków terenu (opis: docs/formaty/blockdb.md).
//
// Każdy blok ma:
//  - rekord 89 B: siatka 8×8 wysokości (0..15; 15 = lita skała, z = 7 + 0,5·v bloku),
//    4 × u32 sąsiedzi do zszywania, narożniki i flagi,
//  - 3 podsiatki (góra, środek, dół) zbudowane z globalnej palety wierzchołków (x, y, z, u, v)
//    i normalnych z SPHERE.DAT.
// Rekordy 89 B są identyczne we wszystkich trzech plikach; pliki różnią się szczegółowością
// siatek (gra wybiera plik ustawieniem LandScapeQuality 0..2).
//
// Układ siatek pochodzi z projektu Zimennik/Diggles-Unity (MIT,
// https://github.com/Zimennik/Diggles-Unity, Docs/RENDERING_PIPELINE_COMPLETE.md §2.1);
// zweryfikowaliśmy go na plikach gry (suma sekcji = rozmiar pliku dla BDB0 i BDB2).
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace ow {

class Vfs;

struct BlockRecord {
	std::array<uint8_t, 64> depth; // 8×8 wysokości, wiersz po wierszu od góry
	std::array<uint8_t, 16> extra; // 4 × u32: sąsiedzi NW, NE, SW, SE do zszywania (0xFFFFFFFF = brak)
	std::array<uint8_t, 9> tail;   // 4 B wysokości narożników + bajt wariantu + flagi (🔶)
};
static_assert(sizeof(BlockRecord) == 89, "rekord BlockDB ma 89 bajtów");

// Deskryptor płynu bloku: ostatnie 4 bajty rekordu (u32 LE, przesunięcie 85). Za Zimennik/Diggles-Unity
// (MIT, Docs/FLUID_SYSTEM_RE.md); ✅ sprawdzone na danych: blok 0 = 15/15 (nieprawidłowy), blok 59 = 960, 8/0.
struct FluidShape {
	int capacity = 0;     // bity 0..9: pojemność w jednostkach objętości gry
	int upper = 15;       // bity 10..13: granica (ósme części pola od góry), poniżej której stoi płyn
	int lower = 15;       // bity 14..17: dolna granica powierzchni
	int vertical = 0;     // bity 18..24: przepuszczalność w pionie
	int horizontal = 0;   // bity 25..31: przepuszczalność w poziomie
	bool valid() const { return upper <= 8 && lower <= 8 && lower <= upper; }
};

struct BdbPaletteEntry {
	float x, y, z; // x, y w obrębie bloku (0..1, y w dół), z = wysokość w blokach (7..14,5)
	float u, v;    // 🔶 współrzędne tekstury w jednostkach atlasu (zakres ~0..7,5)
};
static_assert(sizeof(BdbPaletteEntry) == 20, "wpis palety ma 20 bajtów");

// Widok jednej podsiatki bloku: wierzchołek i -> palette[paletteIndex(i)], normalna sphere[normalIdx[i]].
struct BdbSubMesh {
	const uint8_t* paletteRaw = nullptr; // vertCount × u16 LE (może być niewyrównane)
	const uint8_t* normalIdx = nullptr;
	uint16_t vertCount = 0;
	const uint16_t* indices = nullptr; // trójkąty, indeksy 0..vertCount-1
	uint32_t indexCount = 0;
	uint16_t paletteIndex(uint16_t i) const { return uint16_t(paletteRaw[i * 2] | (paletteRaw[i * 2 + 1] << 8)); }
};

class BlockDb {
public:
	static constexpr uint32_t kBlocks = 118688;

	// quality 0..2 -> Data/BlockDB<quality>.BDB. Rzuca ParseError.
	void load(const Vfs& vfs, int quality = 2);
	void loadData(const std::vector<uint8_t>& data);
	bool loaded() const { return !records_.empty(); }
	size_t size() const { return records_.size(); }
	const BlockRecord* record(uint32_t id) const { return id < records_.size() ? &records_[id] : nullptr; }
	FluidShape fluid(uint32_t id) const {
		FluidShape f;
		if (id >= records_.size()) return f;
		const auto& t = records_[id].tail;
		uint32_t v = uint32_t(t[5]) | uint32_t(t[6]) << 8 | uint32_t(t[7]) << 16 | uint32_t(t[8]) << 24;
		f.capacity = int(v & 1023), f.upper = int(v >> 10 & 15), f.lower = int(v >> 14 & 15);
		f.vertical = int(v >> 18 & 127), f.horizontal = int(v >> 25 & 127);
		return f;
	}
	int variant(uint32_t id) const { return id < records_.size() ? records_[id].tail[4] : 0; }
	// sub 0..2; false, gdy podsiatka pusta albo ID poza zakresem.
	bool subMesh(uint32_t block, int sub, BdbSubMesh& out) const;
	const BdbPaletteEntry& palette(uint16_t i) const { return palette_[i]; }
	size_t paletteSize() const { return palette_.size(); }

private:
	struct Cell {
		uint16_t vertCount;
		uint32_t descriptor;
		uint32_t bulkOffset;
	};
	std::vector<BlockRecord> records_;
	std::vector<Cell> cells_; // 3 na blok
	std::vector<uint16_t> indexPool_;
	std::vector<std::array<uint32_t, 2>> descriptors_; // (offset w puli indeksów, liczba indeksów)
	std::vector<BdbPaletteEntry> palette_;
	std::vector<uint8_t> bulk_; // na podsiatkę: vertCount × u16 indeks palety, potem vertCount × u8 normalna
};

} // namespace ow
