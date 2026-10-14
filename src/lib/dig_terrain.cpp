#include "lib/dig_terrain.h"

#include "lib/blockdb.h"
#include "lib/map_template.h"

#include <algorithm>
#include <cmath>

namespace ow
{

namespace
{

constexpr int kNW = 0, kNE = 1, kSW = 2, kSE = 3;

uint32_t neighbour(const BlockRecord& r, int corner)
{
	const uint8_t* p = &r.extra[size_t(corner) * 4];
	return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

int gate(const BlockRecord& r, int corner)
{
	return r.tail[size_t(corner)];
}

} // namespace

void DigTerrain::init(const Pmp& pmp)
{
	w_ = int(pmp.width), h_ = int(pmp.height);
	size_t n = size_t(w_) * size_t(h_);
	state_.assign(n, 0);
	id_.assign(n, 0);
	material_.assign(n, 2);
	playerDigMarkId = 0;
	hasBounds_.fill(false);
	refreshMaterial(pmp, 0, 0, w_, h_);
}

void DigTerrain::refreshMaterial(const Pmp& pmp, int x0, int y0, int x1, int y1)
{
	if (int(pmp.width) != w_ || int(pmp.height) != h_) return;
	x0 = std::max(x0, 0), y0 = std::max(y0, 0), x1 = std::min(x1, w_), y1 = std::min(y1, h_);
	auto raw = [&](int x, int y) { return uint8_t(pmp.types[size_t(y) * size_t(w_) + size_t(x)] & 0x7F); };
	for (int y = y0; y < y1; ++y)
		for (int x = x0; x < x1; ++x)
		{
			uint8_t v = raw(x, y);
			if (v == 0 && y + 1 < h_) v = raw(x, y + 1); // pierwszy niezerowy: niżej, z prawej, z prawej niżej
			if (v == 0 && x + 1 < w_) v = raw(x + 1, y);
			if (v == 0 && x + 1 < w_ && y + 1 < h_) v = raw(x + 1, y + 1);
			material_[size_t(y) * size_t(w_) + size_t(x)] = v == 2 ? 2 : v == 3 ? 1
			    : v == 4                                                        ? 0
			    : v == 5                                                        ? 3
			                                                                    : 2;
		}
}

int DigTerrain::material(int tx, int ty) const
{
	return in(tx, ty) ? material_[size_t(ty) * size_t(w_) + size_t(tx)] : 0;
}

void DigTerrain::setMark(int tx, int ty, int owner, int cnt, bool dummy)
{
	if (!in(tx, ty) || owner < 0 || owner >= 8) return;
	size_t i = size_t(ty) * size_t(w_) + size_t(tx);
	int s = owner * 4;
	uint32_t nib = dummy ? 3u : uint32_t((cnt & 3) | 8);
	state_[i] = (state_[i] & ~(0xFu << s)) | nib << s;
	id_[i] = (id_[i] & ~(0xFu << s)) | uint32_t(playerDigMarkId & 15) << s;
	auto& b = bounds_[size_t(owner)];
	if (!hasBounds_[size_t(owner)]) b = {tx, ty, tx, ty}, hasBounds_[size_t(owner)] = true;
	else b = {std::min(b[0], tx), std::min(b[1], ty), std::max(b[2], tx), std::max(b[3], ty)};
}

bool DigTerrain::bounds(int owner, int& x0, int& y0, int& x1, int& y1) const
{
	if (owner < 0 || owner >= 8 || !hasBounds_[size_t(owner)]) return false;
	const auto& b = bounds_[size_t(owner)];
	x0 = b[0], y0 = b[1], x1 = b[2], y1 = b[3];
	return true;
}

void DigTerrain::clearMark(int tx, int ty, int owner)
{
	if (in(tx, ty) && owner >= 0 && owner < 8) state_[size_t(ty) * size_t(w_) + size_t(tx)] &= ~(0xFu << (owner * 4));
}

int DigTerrain::nibble(int tx, int ty, int owner) const
{
	return in(tx, ty) && owner >= 0 && owner < 8 ? int(state_[size_t(ty) * size_t(w_) + size_t(tx)] >> (owner * 4) & 0xF) : 0;
}

bool DigTerrain::marked(int tx, int ty, int owner) const
{
	return (nibble(tx, ty, owner) & 0xB) != 0;
}

int DigTerrain::markId(int tx, int ty, int owner) const
{
	return in(tx, ty) && owner >= 0 && owner < 8 ? int(id_[size_t(ty) * size_t(w_) + size_t(tx)] >> (owner * 4) & 0xF) : 0;
}

int DigTerrain::decodeTarget(int v)
{
	if ((v & 0xF) == 0) return 15;
	if ((v & 8) == 0) return 11;
	return (v & 3) ? (v & 3) * 4 - 1 : 0;
}

int DigTerrain::rimAtNode(const Pmp& pmp, const BlockDb& db, int nx, int ny) const
{
	const int cells[4][3] = {{nx - 1, ny - 1, kSE}, {nx, ny - 1, kSW}, {nx - 1, ny, kNE}, {nx, ny, kNW}};
	bool followed = false;
	int m = -1;
	for (const auto& c : cells)
	{
		if (!in(c[0], c[1])) continue;
		uint32_t blk = pmp.blocks[size_t(c[1]) * size_t(w_) + size_t(c[0])];
		const BlockRecord* r = db.record(blk);
		if (!r) continue;
		uint32_t cand = neighbour(*r, c[2]);
		const BlockRecord* rc = cand != blk ? db.record(cand) : nullptr; // 0xFFFFFFFF albo poza bazą: brak przejścia
		int g = rc ? gate(*rc, c[2]) : gate(*r, c[2]);
		followed = followed || rc;
		m = std::max(m, g);
	}
	return followed ? m : -1;
}

// Przejście narożnika pola grafem BlockDB, tylko gdy bramka bloku docelowego równa się krawędzi węzła (szwy).
bool DigTerrain::stepCorner(Pmp& pmp, const BlockDb& db, int tx, int ty, int corner, int rim, std::vector<std::pair<int, int>>& changed)
{
	if (!in(tx, ty)) return false;
	uint32_t& blk = pmp.blocks[size_t(ty) * size_t(w_) + size_t(tx)];
	const BlockRecord* r = db.record(blk);
	if (!r) return false;
	uint32_t cand = neighbour(*r, corner);
	const BlockRecord* rc = cand != blk ? db.record(cand) : nullptr;
	if (!rc || gate(*rc, corner) != rim) return false;
	blk = cand;
	changed.push_back({tx, ty});
	return true;
}

bool DigTerrain::carveStep(Pmp& pmp, const BlockDb& db, int nx, int ny, int owner, int& dugId, std::vector<std::pair<int, int>>& changed)
{
	dugId = 0;
	if (owner < 0 || owner >= 8 || nx < 1 || ny < 1 || nx >= w_ - 1 || ny >= h_ - 1) return false;
	// Warunek wstępny: materiał pola 1 albo 2 (skała i otwarte odmawiają i zdejmują znacznik).
	if (!diggable(nx, ny))
	{
		clearMark(nx, ny, owner);
		return false;
	}
	dugId = markId(nx, ny, owner);
	int target = decodeTarget(nibble(nx, ny, owner)), rim = rimAtNode(pmp, db, nx, ny);
	if (rim < 0 || target > rim)
	{ // już wykopane do celu: „nic tu do kopania”
		clearMark(nx, ny, owner);
		return false;
	}
	size_t before = changed.size();
	stepCorner(pmp, db, nx - 1, ny - 1, kSE, rim, changed);
	stepCorner(pmp, db, nx, ny - 1, kSW, rim, changed);
	stepCorner(pmp, db, nx - 1, ny, kNE, rim, changed);
	stepCorner(pmp, db, nx, ny, kNW, rim, changed);
	bool any = changed.size() > before;
	int newRim = rimAtNode(pmp, db, nx, ny);
	// Zamiatanie: znaczniki wszystkich graczy, których cel nowa krawędź już przekroczyła, są wykonane.
	size_t i = size_t(ny) * size_t(w_) + size_t(nx);
	for (int o = 0; o < 8; ++o)
	{
		int v = int(state_[i] >> (o * 4) & 0xF);
		if (v && newRim < decodeTarget(v)) state_[i] &= ~(0xFu << (o * 4));
	}
	// Zastój (żaden narożnik się nie zmienił, krawędź stoi): zdjąć znacznik, inaczej dig_next podawałby ten węzeł
	// w nieskończoność (🔶 za Diggles-Unity; w grze robi to warunek wstępny ekspozycji, którego nie mamy).
	if (!any && newRim == rim)
	{
		clearMark(nx, ny, owner);
		return false;
	}
	return any;
}

bool DigTerrain::next(const Pmp& pmp, const BlockDb& db, const float pos[3], int owner, int gnomeId, int tryN, bool matchId, float out[3]) const
{
	int px = int(pos[0]), py = int(pos[1]);
	int x0 = std::max(2, px - 16), x1 = std::min(w_ - 2, px + 16), y0 = std::max(2, py - 17), y1 = std::min(h_ - 2, py + 15);
	struct Cand
	{
		float cost;
		int ord, x, y, rim;
	};
	std::vector<Cand> c;
	for (int y = y0; y < y1; ++y)
		for (int x = x0; x < x1; ++x)
		{
			if (!marked(x, y, owner) || !diggable(x, y)) continue;
			int rim = rimAtNode(pmp, db, x, y);
			if (rim < 0) continue;
			int id = markId(x, y, owner);
			if (matchId && id != gnomeId) continue;
			float dx = float(x) - pos[0], dy = float(y) - (pos[1] - 0.75f), dz = (float(rim) + 4.f) - (pos[2] + 6.f);
			float cost = dx * dx + dy * dy + dz * dz * 0.25f + (id != gnomeId ? 8.f : 0.f); // najpierw dokończyć swoją żyłę
			c.push_back({cost, int(c.size()), x, y, rim});
		}
	if (int(c.size()) <= tryN) return false;
	std::stable_sort(c.begin(), c.end(), [](const Cand& a, const Cand& b) { return a.cost < b.cost; });
	const Cand& p = c[size_t(tryN)];
	out[0] = float(p.x), out[1] = float(p.y), out[2] = std::clamp(float(p.rim) + 6.f, 1.f, 14.f);
	return true;
}

} // namespace ow
