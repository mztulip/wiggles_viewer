#include "lib/nav.h"

#include "lib/blockdb.h"
#include "lib/map_template.h"

#include <algorithm>
#include <cstdlib>
#include <queue>

namespace ow {

bool NavGrid::openBlock(const BlockDb& db, uint32_t block) {
	if (block == 59) return true;
	const BlockRecord* r = db.record(block);
	if (!r) return false;
	int open = 0;
	for (int y = 4; y < 8; ++y)
		for (int x = 0; x < 8; ++x) open += r->depth[size_t(y * 8 + x)] < 13;
	return open >= 16;
}

void NavGrid::build(const Pmp& grid, const BlockDb& db) {
	w_ = int(grid.width);
	h_ = int(grid.height);
	open_.assign(size_t(w_) * h_, 0);
	for (int y = 0; y < h_; ++y)
		for (int x = 0; x < w_; ++x) open_[size_t(y) * w_ + x] = openBlock(db, grid.blocks[size_t(y) * w_ + x]);
}

void NavGrid::updateTile(const Pmp& grid, const BlockDb& db, int x, int y) {
	if (x < 0 || y < 0 || x >= w_ || y >= h_) return;
	open_[size_t(y) * w_ + x] = openBlock(db, grid.blocks[size_t(y) * w_ + x]);
}

std::vector<std::pair<int, int>> NavGrid::path(int sx, int sy, int tx, int ty, int maxNodes) const {
	std::vector<std::pair<int, int>> out;
	if (!open(sx, sy) || !open(tx, ty)) return out;
	auto id = [&](int x, int y) { return y * w_ + x; };
	std::vector<int> cost(size_t(w_) * h_, -1), from(size_t(w_) * h_, -1);
	using Q = std::pair<int, int>; // (koszt + heurystyka, id)
	std::priority_queue<Q, std::vector<Q>, std::greater<Q>> q;
	cost[size_t(id(sx, sy))] = 0;
	q.push({0, id(sx, sy)});
	int visited = 0;
	while (!q.empty() && visited++ < maxNodes) {
		int cur = q.top().second;
		q.pop();
		int x = cur % w_, y = cur / w_;
		if (x == tx && y == ty) break;
		int c = cost[size_t(cur)];
		auto relax = [&](int nx, int ny, int step) {
			if (!open(nx, ny)) return;
			int n = id(nx, ny), nc = c + step;
			if (cost[size_t(n)] >= 0 && cost[size_t(n)] <= nc) return;
			cost[size_t(n)] = nc;
			from[size_t(n)] = cur;
			q.push({nc + 10 * (std::abs(nx - tx) + std::abs(ny - ty)), n});
		};
		bool stand = standable(x, y), climb = climbable(x, y);
		if (!stand && !climb) { // w powietrzu: tylko spadanie
			relax(x, y + 1, 5);
			continue;
		}
		for (int d : {-1, 1}) {
			if (stand && standable(x + d, y)) relax(x + d, y, 10);
			if (stand && standable(x + d, y - 1) && open(x, y - 1)) relax(x + d, y - 1, 14); // stopień w górę
			if (stand && standable(x + d, y + 1) && open(x + d, y)) relax(x + d, y + 1, 14); // stopień w dół
			if (open(x + d, y) && !standable(x + d, y)) relax(x + d, y, 12);                // zejście z krawędzi
		}
		if (climb || stand) {
			if (climbable(x, y - 1) || standable(x, y - 1)) relax(x, y - 1, 20); // wspinanie w górę
			if (open(x, y + 1)) relax(x, y + 1, 15);                              // w dół
		}
	}
	int t = id(tx, ty);
	if (cost[size_t(t)] < 0) return out;
	for (int n = t; n >= 0; n = from[size_t(n)]) {
		out.push_back({n % w_, n / w_});
		if (n == id(sx, sy)) break;
	}
	std::reverse(out.begin(), out.end());
	return out;
}

} // namespace ow
