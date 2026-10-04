// Nawigacja postaci po polach świata (pierwsze przybliżenie, 🔶; docs/silnik/14_gra.md).
//
// Pole jest otwarte, gdy w dolnej połowie jego bloku (BlockDB, siatka 8×8 wysokości) co najmniej
// połowa drobnych pól ma wysokość < 13, czyli jest wykopana. Postać:
//  - stoi na otwartym polu z zamkniętym polem pod spodem,
//  - chodzi w bok i po skosie (±1 wiersz) między polami, na których da się stać,
//  - wspina się w pionie po otwartych polach przy ścianie (lita skała z boku) i spada w dół.
// Gra liczy ruch na drobnej siatce i maskach .pmp (Diggles-Unity ORIGINAL_PATHFINDING_RE.md); to
// jest uproszczenie do czasu przeniesienia tamtych praw (TODO).
#pragma once

#include <cstdint>
#include <vector>

namespace ow {

class BlockDb;
struct Pmp;

class NavGrid {
public:
	void build(const Pmp& grid, const BlockDb& db);
	void updateTile(const Pmp& grid, const BlockDb& db, int x, int y);
	int width() const { return w_; }
	int height() const { return h_; }
	bool open(int x, int y) const { return x >= 0 && y >= 0 && x < w_ && y < h_ && open_[size_t(y) * w_ + x]; }
	bool standable(int x, int y) const { return open(x, y) && y + 1 < h_ && !open(x, y + 1); }
	bool climbable(int x, int y) const { return open(x, y) && (!open(x - 1, y) || !open(x + 1, y)); }
	// Droga z pola (sx, sy) do (tx, ty), łącznie z oboma końcami; puste = brak drogi.
	std::vector<std::pair<int, int>> path(int sx, int sy, int tx, int ty, int maxNodes = 200000) const;

private:
	static bool openBlock(const BlockDb& db, uint32_t block);
	int w_ = 0, h_ = 0;
	std::vector<uint8_t> open_;
};

} // namespace ow
