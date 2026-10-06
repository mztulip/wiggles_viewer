#include "lib/nav_world.h"

#include "lib/blockdb.h"
#include "lib/map_template.h"

#include <algorithm>
#include <cstdlib>

namespace ow
{
namespace
{

// „Kapsuła” postaci nad stopą (dx, dy w podpolach): wszystkie te podpola muszą mieć wysokość < 14, żeby
// w podpolu dało się stać (FUN_005a6b40:116-419, za Diggles-Unity NavWorld.OpenBand).
const signed char kOpenBand[31][2] = {
    {0, -8},
    {-7, -15},
    {7, -15},
    {0, -15},
    {0, -1},
    {-7, -8},
    {7, -8},
    {-4, -3},
    {4, -3},
    {-7, -5},
    {7, -5},
    {-7, -12},
    {7, -12},
    {-4, -15},
    {4, -15},
    {0, -12},
    {0, -4},
    {-4, -8},
    {4, -8},
    {-3, -13},
    {3, -13},
    {-5, -11},
    {5, -11},
    {-2, -10},
    {2, -10},
    {-2, -6},
    {2, -6},
    {-5, -5},
    {5, -5},
    {-2, -2},
    {2, -2},
};
constexpr int kOpenThreshold = 14;

} // namespace

void NavWorld::build(const Pmp& grid, const BlockDb& db)
{
	grid_ = &grid, db_ = &db;
	w_ = int(grid.width), h_ = int(grid.height);
	sub_.assign(size_t(w_) * h_ * 64, 0);
	cost_.assign(size_t(w_) * h_, -1);
	exit_.assign(size_t(w_) * h_, 0x24);
	det_.assign(size_t(subW()) * subH(), 15);
	refreshDetail(0, 0, w_ - 1, h_ - 1);
	// 🔶 Gra po wczytaniu świata przelicza podpola w całym świecie poza marginesem 3 pól (z boku i u dołu)
	// i 2 pól (u góry), a w marginesie zostaje płaszczyzna z .pmp. Szablon oglądany osobno jest u nas całym
	// światem, więc liczymy wszędzie (poza światem wysokość 15, czyli skała).
	revalidate(0, 0, subW() - 1, subH() - 1);
}

int NavWorld::detail(int sx, int sy) const
{
	if (sx < 0 || sy < 0 || sx >= subW() || sy >= subH()) return 15;
	return det_[size_t(sy) * size_t(subW()) + size_t(sx)];
}

void NavWorld::refreshDetail(int tx0, int ty0, int tx1, int ty1)
{
	for (int ty = std::max(0, ty0); ty <= std::min(h_ - 1, ty1); ++ty)
		for (int tx = std::max(0, tx0); tx <= std::min(w_ - 1, tx1); ++tx)
		{
			const BlockRecord* r = db_->record(grid_->blocks[size_t(ty) * w_ + size_t(tx)]);
			for (int i = 0; i < 64; ++i)
				det_[size_t(ty * 8 + (i >> 3)) * size_t(subW()) + size_t(tx * 8 + (i & 7))] = r ? r->depth[size_t(i)] : 15;
		}
}

uint8_t NavWorld::sub(int sx, int sy) const
{
	if (sx < 0 || sy < 0 || sx >= subW() || sy >= subH()) return 0;
	return sub_[(size_t(sy >> 3) * w_ + size_t(sx >> 3)) * 64 + size_t(((sy & 7) << 3) | (sx & 7))];
}

uint8_t NavWorld::deriveSub(int sx, int sy) const
{
	// Wymuszona ściana: bit 0x80 typu pola (tile+0x18) w polu ((sx + 4) >> 3, (sy + 4) >> 3).
	int px = (sx + 4) >> 3, py = (sy + 4) >> 3;
	if (px < 0 || py < 0 || px >= w_ || py >= h_) return 0;
	if (grid_->types[size_t(py) * w_ + size_t(px)] & 0x80) return 0;
	for (const auto& o : kOpenBand)
		if (detail(sx + o[0], sy + o[1]) >= kOpenThreshold) return 0;
	// Podłoga (3) albo ściana (1): skok wysokości o więcej niż 2 (pierwszy rząd materiału pod wolną przestrzenią
	// albo ostatni wolny rząd nad podporą).
	int here = detail(sx, sy), below = detail(sx, sy + 1);
	if (detail(sx, sy - 1) + 2 < here && detail(sx, sy - 11) + 2 < here) return 3;
	if (here + 2 < below) return detail(sx, sy - 10) + 2 < below ? 3 : 1;
	return 1;
}

void NavWorld::deriveTile(int tx, int ty)
{
	size_t t = size_t(ty) * w_ + size_t(tx);
	const uint8_t* s = &sub_[t * 64];
	int best = 0, bestPair = 0x24;
	bool none = true, all = true;
	for (int i = 0; i < 64; ++i)
	{
		if (!(s[i] & kWalk))
		{
			all = false;
			continue;
		}
		none = false;
		// podłoga przed ścianą, potem najbliżej środka pola (4, 4); pierwsze ściśle największe
		int score = ((s[i] & kFloor) ? 0x100 : 0) - std::abs(4 - (i >> 3)) - std::abs(4 - (i & 7)) + 8;
		if (score > best) best = score, bestPair = i;
	}
	if (none)
	{
		cost_[t] = -1;
		return;
	}
	// Cztery pola 2×2 z wymuszoną ścianą: zablokowane.
	auto forced = [&](int x, int y) { return x < w_ && y < h_ && (grid_->types[size_t(y) * w_ + size_t(x)] & 0x80); };
	if (tx + 1 < w_ && ty + 1 < h_ && forced(tx, ty) && forced(tx + 1, ty) && forced(tx, ty + 1) && forced(tx + 1, ty + 1))
	{
		cost_[t] = -1;
		return;
	}
	uint8_t pair;
	if (all)
	{ // całe pole wolne: punkt wyjścia zsuwa się w dół po kolejnych podpolach podłogi
		if ((best >> 8) == 0) pair = 0x24;
		else
		{
			pair = uint8_t(bestPair);
			while (pair < 0x38 && (s[pair + 8] & kFloor)) pair = uint8_t(pair + 8);
			pair |= kExitFloor;
		}
	}
	else
		pair = uint8_t(((best >> 2) & kExitFloor) | bestPair);
	exit_[t] = pair;
	cost_[t] = int8_t((best >> 8) ? 20 : 50);
}

void NavWorld::revalidate(int sx0, int sy0, int sx1, int sy1)
{
	if (!grid_) return;
	refreshDetail(sx0 >> 3, sy0 >> 3, sx1 >> 3, sy1 >> 3); // bloki zmienione w prostokącie (kopanie)
	// zasięg reguły podpola: kapsuła sięga 15 w górę i 7 w bok, więc zmiana wysokości wpływa na podpola do 16
	// niżej, 2 wyżej i 8 w bok (Diggles-Unity NavWorld.RevalidateRect)
	int x0 = std::max(0, sx0 - 8), y0 = std::max(0, sy0 - 2), x1 = std::min(subW() - 1, sx1 + 8), y1 = std::min(subH() - 1, sy1 + 16);
	for (int sy = y0; sy <= y1; ++sy)
		for (int sx = x0; sx <= x1; ++sx)
			sub_[(size_t(sy >> 3) * w_ + size_t(sx >> 3)) * 64 + size_t(((sy & 7) << 3) | (sx & 7))] = deriveSub(sx, sy);
	for (int ty = y0 >> 3; ty <= y1 >> 3; ++ty)
		for (int tx = x0 >> 3; tx <= x1 >> 3; ++tx) deriveTile(tx, ty);
}

int NavWorld::baseCost(int tx, int ty) const
{
	return tx < 0 || ty < 0 || tx >= w_ || ty >= h_ ? -1 : cost_[size_t(ty) * w_ + size_t(tx)];
}

uint8_t NavWorld::exitPair(int tx, int ty) const
{
	return tx < 0 || ty < 0 || tx >= w_ || ty >= h_ ? 0x24 : exit_[size_t(ty) * w_ + size_t(tx)];
}

int NavWorld::digByte(int tx, int ty) const
{
	if (!grid_ || tx < 0 || ty < 0 || tx >= w_ || ty >= h_) return 0;
	const BlockRecord* r = db_->record(grid_->blocks[size_t(ty) * w_ + size_t(tx)]);
	return r ? int(r->depth[0x24]) : 0;
}

size_t NavWorld::walkableCount() const
{
	return size_t(std::count_if(sub_.begin(), sub_.end(), [](uint8_t b) { return (b & kWalk) != 0; }));
}

std::string NavWorld::ascii(int tx0, int ty0, int tx1, int ty1) const
{
	std::string out;
	for (int sy = std::max(0, ty0) * 8; sy <= std::min(h_ - 1, ty1) * 8 + 7; ++sy)
	{
		for (int sx = std::max(0, tx0) * 8; sx <= std::min(w_ - 1, tx1) * 8 + 7; ++sx)
		{
			uint8_t b = sub(sx, sy);
			out += (b & kWalk) ? ((b & kFloor) ? '_' : '|') : '.';
		}
		out += '\n';
	}
	return out;
}

} // namespace ow
