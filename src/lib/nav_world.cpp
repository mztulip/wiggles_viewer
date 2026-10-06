#include "lib/nav_world.h"

#include "lib/blockdb.h"
#include "lib/map_template.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <deque>

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

std::vector<std::pair<int, int>> NavWorld::findPath(int sx, int sy, int gx, int gy, bool canClimb) const
{
	std::vector<std::pair<int, int>> out;
	if (!grid_) return out;
	if (!pathWalkable(sx, sy)) // początek: osie, kolejno +y, +x, −x, −y; bez trafienia zostaje
		for (int r = 1; r <= 32; ++r)
		{
			if (pathWalkable(sx, sy + r))
			{
				sy += r;
				break;
			}
			if (pathWalkable(sx + r, sy))
			{
				sx += r;
				break;
			}
			if (pathWalkable(sx - r, sy))
			{
				sx -= r;
				break;
			}
			if (pathWalkable(sx, sy - r))
			{
				sy -= r;
				break;
			}
		}
	if (!pathWalkable(gx, gy)) // cel: wachlarz ośmiu prób, głównie w dół
	{
		bool found = false;
		for (int r = 1; r <= 12 && !found; ++r)
		{
			const int probes[8][2] = {{0, r}, {0, -(r >> 2)}, {r >> 1, 0}, {-(r >> 1), 0}, {r / 3, r >> 1}, {-(r / 3), r >> 1}, {r / 3, -(r / 5)}, {-(r / 3), -(r / 5)}};
			for (const auto& p : probes)
				if (pathWalkable(gx + p[0], gy + p[1]))
				{
					gx += p[0], gy += p[1], found = true;
					break;
				}
		}
		if (!found) return out;
	}
	while (floor(sx, sy + 1)) ++sy; // zsunięcie na dolny rząd półki
	while (floor(gx, gy + 1)) ++gy;
	int st = (sy >> 3) * w_ + (sx >> 3), gt = (gy >> 3) * w_ + (gx >> 3);
	if (cost_[size_t(st)] == -1 || cost_[size_t(gt)] == -1) return out;
	if (st == gt && sx == gx && sy == gy) return {{sx, sy}};

	size_t n = size_t(w_) * h_;
	if (dist_.size() != n) dist_.assign(n, 0), pred_.assign(n, -1), stamp_.assign(n, 0), gen_ = 0;
	++gen_;
	std::deque<int> q;
	dist_[size_t(st)] = cost_[size_t(st)] + 1, pred_[size_t(st)] = -1, stamp_[size_t(st)] = gen_;
	q.push_back(st);
	constexpr int kCliff = 0x114, kStep = 0x10; // przejście podłoga ↔ ściana; stopień wysokości (wariant v1)
	int enqueued = 1, head = 0, maxNodes = 200000;
	bool goalSeen = false;
	while (!q.empty() && head < maxNodes)
	{
		int cur = q.front();
		q.pop_front();
		++head;
		int cx = cur % w_, cy = cur / w_, dcur = dist_[size_t(cur)];
		bool curFloor = (exit_[size_t(cur)] & kExitFloor) != 0;
		int digCur = digByte(cx, cy);
		const int nb[4][2] = {{cx, cy + 1}, {cx, cy - 1}, {cx + 1, cy}, {cx - 1, cy}};
		for (const auto& p : nb)
		{
			int tx = p[0], ty = p[1];
			if (tx < 0 || ty < 0 || tx >= w_ || ty >= h_) continue;
			int t = ty * w_ + tx, base = cost_[size_t(t)];
			if (base == -1) continue;
			bool tFloor = (exit_[size_t(t)] & kExitFloor) != 0;
			if (!canClimb && !tFloor) continue;
			int c = curFloor != tFloor ? base + kCliff - kStep * std::min(digCur, digByte(tx, ty))
			    : !curFloor            ? base + 1 + kStep * std::abs(digCur - digByte(tx, ty))
			                           : base + 1;
			int nd = dcur + c;
			if (stamp_[size_t(t)] == gen_ && nd >= dist_[size_t(t)]) continue;
			dist_[size_t(t)] = nd, pred_[size_t(t)] = cur, stamp_[size_t(t)] = gen_;
			q.push_back(t);
			++enqueued;
			if (t == gt && !goalSeen)
			{ // po pierwszym dojściu do celu: jeszcze najwyżej połowa tego, co dotąd
				goalSeen = true;
				maxNodes = std::min(maxNodes, int(std::lround(enqueued * 1.5)));
			}
		}
	}
	if (stamp_[size_t(gt)] != gen_) return out;
	std::vector<int> tiles;
	for (int t = gt; t != -1; t = pred_[size_t(t)])
	{
		tiles.push_back(t);
		if (tiles.size() > n) return out;
	}
	std::reverse(tiles.begin(), tiles.end());
	out.push_back({sx, sy});
	for (size_t i = 1; i < tiles.size(); ++i)
	{
		int t = tiles[i], e = exit_[size_t(t)];
		out.push_back({(t % w_) * 8 + (e & 7), (t / w_) * 8 + ((e >> 3) & 7)});
	}
	if (out.size() > 1) out.back() = {gx, gy};
	else out.push_back({gx, gy});
	return out;
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
