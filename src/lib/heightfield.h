// Mapa wysokości terenu zbudowana z bloków szablonu (.PMP + BlockDB).
// 8 próbek na blok; próbka = wartość 0..15 z BlockDB (15 = lita skała).
#pragma once

#include <cstdint>
#include <vector>

namespace ow {

struct Pmp;
class BlockDb;

struct Heightfield {
	static constexpr int kSamplesPerBlock = 8;
	static constexpr float kSampleUnits = 25.f;  // 200 jednostek na blok / 8
	static constexpr float kLevelUnits = 100.f;  // jeden poziom głębokości
	static constexpr float kFrontZ = 3000.f;     // przednia płaszczyzna (z = 15 bloków)

	int width = 0, height = 0; // w próbkach
	std::vector<uint8_t> value;
	std::vector<uint32_t> blockId; // ID bloku dla każdej próbki (do tekstur i podglądu)

	uint8_t at(int x, int y) const {
		x = x < 0 ? 0 : (x >= width ? width - 1 : x);
		y = y < 0 ? 0 : (y >= height ? height - 1 : y);
		return value[size_t(y) * width + x];
	}
	// Współrzędna z w jednostkach modelu (oś do widza): 15 -> 3000, 0 -> 1500.
	static float depthZ(uint8_t v) { return kFrontZ - (15.f - float(v)) * kLevelUnits; }
};

// Nieznane ID bloków dają litą skałę.
Heightfield buildHeightfield(const Pmp& pmp, const BlockDb& db);

} // namespace ow
