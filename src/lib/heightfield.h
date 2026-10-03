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
	static constexpr float kFrontZ = 2900.f;     // przednia płaszczyzna (z = 14,5 bloku, jak w siatkach BlockDB i VRML gry)

	int width = 0, height = 0; // w próbkach
	std::vector<uint8_t> value;
	std::vector<uint32_t> blockId; // ID bloku dla każdej próbki (do tekstur i podglądu)
	std::vector<uint8_t> ground;   // rodzaj gruntu próbki: 0 = A (skała), 1 = B (ziemia), 2 = C (piasek)

	uint8_t at(int x, int y) const {
		x = x < 0 ? 0 : (x >= width ? width - 1 : x);
		y = y < 0 ? 0 : (y >= height ? height - 1 : y);
		return value[size_t(y) * width + x];
	}
	// Współrzędna z w jednostkach modelu (oś do widza): z = (7 + 0,5·v) bloku, czyli 15 -> 2900, 0 -> 1400.
	static float depthZ(uint8_t v) { return kFrontZ - (15.f - float(v)) * kLevelUnits; }
};

// Rodzaj gruntu z typu pola .PMP (młodszy półbajt; ✅ ze zrzutów gry): 2 -> C, 3 -> B, 4 -> A;
// 0 ma tę samą klasę co 2 (domyślna klasa świata z map create) -> C; 5 (otwór do tła) -> A.
int groundFromPmpType(uint8_t type);

// Siatka to świat po nałożeniu szablonów (map_world.h). Nieznane ID bloków dają litą skałę.
Heightfield buildHeightfield(const Pmp& world, const BlockDb& db);

} // namespace ow
