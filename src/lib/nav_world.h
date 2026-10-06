// Dane ruchu postaci jak w grze: płaszczyzna podpól 8×8 na pole i graf pól dla wyszukiwania drogi.
// Prawa za Zimennik/Diggles-Unity (MIT): Docs/ORIGINAL_PATHFINDING_RE.md §1 i Runtime/Navigation/NavWorld.cs
// (odczyt działającej gry: EFFPASSWALK FUN_005a6b40, EFFBLOCK FUN_005a6350, get_hmap FUN_004093f0).
// Opis: docs/silnik/17_ruch.md.
//
// Świat to widok z boku („mrowisko”): x w prawo, y w dół (grawitacja, wspinanie), a wysokość z siatki 8×8
// bloku BlockDB (0..15) to głębokość w ekran (oś kopania). Podłoga to linia grubości jednego podpola
// w płaszczyźnie (x, y): granica między wolną przestrzenią nad nią a materiałem pod nią; postać chodzi po
// takich liniach i swobodnie zmienia głębokość.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ow
{

class BlockDb;
struct Pmp;

class NavWorld
{
public:
	static constexpr uint8_t kWalk = 0x01;  // postać może stać w podpolu
	static constexpr uint8_t kFloor = 0x02; // podpole jest podłogą (bez tego bitu: ściana do wspinania)
	static constexpr uint8_t kExitPairMask = 0x3F, kExitFloor = 0x40;

	// Całe pole od nowa: podpola z wysokości bloków (EFFPASSWALK), potem graf pól (EFFBLOCK). grid i db muszą
	// żyć dłużej niż NavWorld (grid zmienia się przy kopaniu: potem revalidate).
	void build(const Pmp& grid, const BlockDb& db);
	// Po zmianie bloków w prostokącie podpól [sx0, sx1] × [sy0, sy1]: jak gra po kopaniu (prostokąt poszerzony
	// o zasięg reguły: 8 w bok, 2 w górę, 16 w dół), podpola, potem graf pól.
	void revalidate(int sx0, int sy0, int sx1, int sy1);
	void revalidateTile(int tx, int ty) { revalidate(tx * 8, ty * 8, tx * 8 + 7, ty * 8 + 7); }

	int width() const { return w_; }
	int height() const { return h_; }
	int subW() const { return w_ * 8; }
	int subH() const { return h_ * 8; }

	uint8_t sub(int sx, int sy) const; // bity kWalk | kFloor; poza światem 0
	bool walkable(int sx, int sy) const { return (sub(sx, sy) & kWalk) != 0; }
	bool floor(int sx, int sy) const { return (sub(sx, sy) & kFloor) != 0; }
	bool climbWall(int sx, int sy) const { return (sub(sx, sy) & (kWalk | kFloor)) == kWalk; }
	// Wysokość 0..15 podpola z bieżącego bloku (get_hmap); poza światem 15 (lita skała). z świata = h · 0,5 + 7.
	int detail(int sx, int sy) const;

	// Graf pól (EFFBLOCK): koszt pola −1 (zablokowane), 20 (podłoga) albo 50 (tylko ściana); punkt wyjścia pola
	// (podpole x = bity 0..2, y = bity 3..5) i bit 0x40 „pole z podłogą”.
	int baseCost(int tx, int ty) const;
	uint8_t exitPair(int tx, int ty) const;
	bool tileFloor(int tx, int ty) const { return (exitPair(tx, ty) & kExitFloor) != 0; }
	int digByte(int tx, int ty) const; // wysokość ze środka bloku (bajt 0x24 rekordu): koszt różnic wysokości

	// Wyszukiwanie drogi Calc1 (FUN_0059d640, za Diggles-Unity Calc1Pathfinder): punkty w podpolach. Początek
	// i cel dociągane do najbliższego podpola do chodzenia (początek: pierścienie do 32 wzdłuż osi, najpierw +y;
	// cel: wachlarz do 12, głównie w dół), potem zsuwane w dół po podłodze. Szukanie po polach (FIFO, sąsiedzi
	// +y, −y, +x, −x), postać bez wspinania wchodzi tylko na pola z podłogą (bit 0x40). Wynik: początek, punkt
	// wyjścia każdego kolejnego pola, cel; pusty = brak drogi.
	std::vector<std::pair<int, int>> findPath(int sx, int sy, int gx, int gy, bool canClimb) const;

	size_t walkableCount() const;
	// Podgląd tekstowy podpól pól [tx0, tx1] × [ty0, ty1]: '.' zablokowane, '|' ściana, '_' podłoga.
	std::string ascii(int tx0, int ty0, int tx1, int ty1) const;

private:
	uint8_t deriveSub(int sx, int sy) const; // EFFPASSWALK
	void deriveTile(int tx, int ty);         // EFFBLOCK
	const Pmp* grid_ = nullptr;
	const BlockDb* db_ = nullptr;
	int w_ = 0, h_ = 0;
	void refreshDetail(int tx0, int ty0, int tx1, int ty1); // det_ z bieżących bloków
	std::vector<uint8_t> det_;     // wysokości podpól, wiersz po wierszu świata (subW · subH)
	std::vector<uint8_t> sub_;     // [pole · 64 + wiersz · 8 + kolumna]
	std::vector<int8_t> cost_;     // na pole
	std::vector<uint8_t> exit_;    // na pole
	bool pathWalkable(int sx, int sy) const { return baseCost(sx >> 3, sy >> 3) != -1 && walkable(sx, sy); }
	mutable std::vector<int> dist_, pred_, stamp_; // bufory findPath
	mutable int gen_ = 0;
};

} // namespace ow
