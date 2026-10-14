// Kopanie terenu jak w grze (docs/silnik/19_kopanie.md): znaczniki kopania graczy, materiał pól i wycinanie węzłów
// siatki przez graf przejść bloków BlockDB. Prawa za Zimennik/Diggles-Unity (MIT), Docs/DIGGING_MECHANIC.md i
// Runtime/World/DiggableTerrain.cs (adresy FUN_… gry tam cytowane); 🔶 o ile nie sprawdzone w laboratorium.
#pragma once

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

namespace ow
{

struct Pmp;
class BlockDb;

class DigTerrain
{
public:
	static constexpr uint32_t kFloorBlock = 59; // płaskie dno (wszystkie narożniki 0)

	// Rozmiar i materiał pól z mapy; znaczniki od zera.
	void init(const Pmp& pmp);
	bool ready() const { return w_ > 0; }
	// Materiał pól prostokąta [x0, x1) × [y0, y1) odczytany na nowo z mapy (po nałożeniu szablonu w trakcie gry);
	// znaczniki zostają (gra: nałożenie nie rusza półbajtów tile+0x08).
	void refreshMaterial(const Pmp& pmp, int x0, int y0, int x1, int y1);

	// Materiał pola (gra: tile+0x18 & 3): 0 skała nie do kopania, 1 piasek, 2 ziemia, 3 otwarte. Z bajtu typu
	// .pmp: 2 → 2, 3 → 1, 4 → 0, 5 → 3; 0 i 1 biorą pierwszy niezerowy bajt pola niżej, z prawej, z prawej niżej,
	// a bez niego 2 (wartość domyślna map create).
	int material(int tx, int ty) const;
	bool diggable(int tx, int ty) const
	{
		int m = material(tx, ty);
		return m == 1 || m == 2;
	}

	// Znaczniki (gra: tile+0x08, półbajt gracza 0..7). Tryb 1 (tunel, „dummy”): półbajt 3, cel 11 (jeden krok
	// wycinania); tryb 0 (jaskinia): półbajt cnt|8, cel {0, 3, 7, 11} dla cnt 0..3. Id znacznika (tile+0x0c) =
	// bieżący playerDigMarkId & 15.
	void setMark(int tx, int ty, int owner, int cnt, bool dummy);
	void clearMark(int tx, int ty, int owner);
	bool marked(int tx, int ty, int owner) const; // półbajt & 0xB ≠ 0
	int nibble(int tx, int ty, int owner) const;
	int markId(int tx, int ty, int owner) const;
	static int decodeTarget(int nibble); // 0 → 15 (brak), dummy → 11, cnt|8 → cnt ? 4·cnt − 1 : 0
	int playerDigMarkId = 0;
	// Prostokąt pól, na których gracz kiedykolwiek malował znaczniki (gra: rośnie przy malowaniu, nie maleje).
	bool bounds(int owner, int& x0, int& y0, int& x1, int& y1) const;

	// Krawędź węzła (nx, ny) = wspólny narożnik pól (nx−1, ny−1) SE, (nx, ny−1) SW, (nx−1, ny) NE, (nx, ny) NW:
	// największa bramka po przejściu każdego narożnika grafem BlockDB; −1, gdy żaden narożnik nie ma przejścia.
	int rimAtNode(const Pmp& pmp, const BlockDb& db, int nx, int ny) const;
	// Znacznik malowany tylko, gdy krawędź węzła sięga jego celu (inaczej jest już wykopane).
	bool markRimGate(const Pmp& pmp, const BlockDb& db, int tx, int ty, int paintNibble) const
	{
		return rimAtNode(pmp, db, tx, ty) >= decodeTarget(paintNibble);
	}

	// Jeden krok wycinania (dig_apply): obniża węzeł o jeden poziom krawędzi. Zmienione pola trafiają do changed
	// (nowe ID bloków są już w pmp.blocks). dugId: id znacznika pola. false = nic nie wycięto (znacznik zdjęty).
	bool carveStep(Pmp& pmp, const BlockDb& db, int nx, int ny, int owner, int& dugId, std::vector<std::pair<int, int>>& changed);

	// dig_next: n-ty (tryN) najtańszy zaznaczony węzeł w oknie wokół pos; false = brak (wartownik −1 −1 −1).
	bool next(const Pmp& pmp, const BlockDb& db, const float pos[3], int owner, int gnomeId, int tryN, bool matchId, float out[3]) const;

private:
	int w_ = 0, h_ = 0;
	std::vector<uint8_t> material_;
	std::vector<uint32_t> state_, id_; // półbajty 8 graczy
	std::array<std::array<int, 4>, 8> bounds_{};
	std::array<bool, 8> hasBounds_{};
	bool in(int x, int y) const { return x >= 0 && y >= 0 && x < w_ && y < h_; }
	bool stepCorner(Pmp& pmp, const BlockDb& db, int tx, int ty, int corner, int rim, std::vector<std::pair<int, int>>& changed);
};

} // namespace ow
