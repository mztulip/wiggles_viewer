#include "lib/walk_plan.h"

#include "lib/nav_world.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <deque>

namespace ow
{
namespace
{

// Zaokrąglenie do najbliższej, połówki do parzystej (jak ROUND w grze i Mathf.RoundToInt).
int rnd(float v)
{
	return int(std::nearbyint(v));
}

struct P3
{
	float x, y, z;
};

bool isWallRecord(const NavWorld& nav, int sx, int sy)
{ // klasyfikacja punktu drogi (FUN_00423730): bez bitu podłogi = ściana
	return !nav.floor(sx, sy);
}

// ---- FloorPathMap: głębokość chodu wzdłuż odcinka podłogi ----
// Siatka: kolumna = punkt odcinka co 1/8 bloku, wiersz = głębokość · 4 (64 wiersze, 0..15,75). Komórka:
// bity maski (poza pasem) i 24-bitowy koszt zalewania.
constexpr uint32_t kTooLow = 0x10000000, kTooHigh = 0x08000000, kMask = 0xff000000, kCost = 0x00ffffff;
constexpr uint32_t kSeed = 0x00fffff0;
// Pięć przebiegów sąsiadów z kosztami (koszt maleje od celu, droga wspina się po największym):
// skoczek, przekątne, osie, skoczek, dwa pola po osi.
const int kPassCost[5] = {1280, 512, 256, 1280, 1024};
const signed char kPassOff[5][4][2] = {
    {{-1, -2}, {2, -1}, {1, 2}, {-2, 1}},
    {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}},
    {{-1, 0}, {0, -1}, {1, 0}, {0, 1}},
    {{-1, 2}, {-2, -1}, {1, -2}, {2, 1}},
    {{0, 2}, {-2, 0}, {0, -2}, {2, 0}},
};

// Punkt co 1/8 bloku długości łamanej (reszta przechodzi na następny odcinek), z = 15; na końcu cel.
std::vector<P3> densify(const std::vector<std::pair<float, float>>& ctl, const P3& goal)
{
	std::vector<P3> pts;
	float rem = 0;
	for (size_t i = 0; i + 1 < ctl.size(); ++i)
	{
		float x0 = ctl[i].first, y0 = ctl[i].second, x1 = ctl[i + 1].first, y1 = ctl[i + 1].second;
		float len = std::hypot(x1 - x0, y1 - y0);
		if (len <= 1e-5f) continue;
		float t = rem;
		while (t < len)
		{
			float k = t / len;
			pts.push_back({x0 + (x1 - x0) * k, y0 + (y1 - y0) * k, 15.f});
			t += 0.125f;
		}
		rem = t - len;
	}
	pts.push_back({goal.x, goal.y, 15.f});
	return pts;
}

// y punktu na najbliższą krawędź podłogi: wiersz, w którym wysokość rośnie o więcej niż 2 względem wiersza
// wyżej (d = 0..2 w dół, d/2 w górę).
void lipSnap(const NavWorld& nav, std::vector<P3>& pts)
{
	for (auto& p : pts)
	{
		int sx = int(p.x * 8.f), sy = int(p.y * 8.f);
		for (int d = 0; d < 3; ++d)
		{
			if (nav.detail(sx, sy + d) - nav.detail(sx, sy + d - 1) > 2)
			{
				p.y = float(sy + d) * 0.125f;
				break;
			}
			int up = sy - (d >> 1);
			if (nav.detail(sx, up) - nav.detail(sx, up - 1) > 2)
			{
				p.y = float(up) * 0.125f;
				break;
			}
		}
	}
}

void stampBands(const NavWorld& nav, const std::vector<P3>& pts, std::vector<uint32_t>& grid)
{
	for (size_t i = 0; i < pts.size(); ++i)
	{
		int sx = rnd(pts[i].x * 8.f), sym1 = rnd(pts[i].y * 8.f) - 1;
		int lo = 1000, hi = 0;
		if (sx >= 0 && sym1 >= 0 && (sx >> 3) < nav.width() && (sym1 >> 3) < nav.height())
			lo = nav.detail(sx, sym1), hi = nav.detail(sx, sym1 + 2);
		for (int r = 0; r < 64; ++r)
		{
			if (r < lo * 4 + 1) grid[i * 64 + size_t(r)] |= kTooLow;
			else if (r >= hi * 4 - 1)
				grid[i * 64 + size_t(r)] |= kTooHigh;
		}
	}
}

int nearestPoint(const std::vector<P3>& pts, const P3& p)
{
	int best = 0;
	float bd = 1e30f;
	for (size_t i = 0; i < pts.size(); ++i)
	{
		float dx = pts[i].x - p.x, dy = pts[i].y - p.y, d = dx * dx + dy * dy;
		if (d < bd)
		{
			bd = d, best = int(i);
			if (d < 0.01f) break;
		}
	}
	return best;
}

bool freeCell(uint32_t c)
{
	return (c & kMask) == 0 && (c & kCost) == 0;
}

// Najbliższa wolna komórka w oknie ±16 kolumn i wierszy (FUN_0041bbd0).
bool snapHoriz(const std::vector<uint32_t>& grid, int n, int& col, int& row)
{
	int cc = std::clamp(col, 0, n - 1), cr = std::clamp(row, 0, 63);
	if (freeCell(grid[size_t(cc) * 64 + size_t(cr)]))
	{
		col = cc, row = cr;
		return true;
	}
	int c0 = std::max(0, col - 16), c1 = std::min(n - 1, col + 16), r0 = std::max(0, row - 16), r1 = std::min(63, row + 16);
	int bc = -1, br = -1, bd = 1 << 30;
	for (int c = c0; c <= c1; ++c)
		for (int r = r0; r <= r1; ++r)
		{
			if (!freeCell(grid[size_t(c) * 64 + size_t(r)])) continue;
			int d = (c - col) * (c - col) + (r - row) * (r - row);
			if (d < bd) bd = d, bc = c, br = r;
		}
	if (bc < 0) return false;
	col = bc, row = br;
	return true;
}

// Cel poza zasięgiem okna (np. głębokość krawędzi przy ścianie poza pasem): kolumna celu, najbliższy
// dozwolony wiersz (gra przypina komórkę i idzie dalej).
bool pinNearestRow(const std::vector<uint32_t>& grid, int col, int& row)
{
	int best = -1, bd = 1 << 30;
	for (int r = 0; r < 64; ++r)
	{
		if (!freeCell(grid[size_t(col) * 64 + size_t(r)])) continue;
		int d = (r - row) * (r - row);
		if (d < bd) bd = d, best = r;
	}
	if (best < 0) return false;
	row = best;
	return true;
}

bool losClear(const std::vector<uint32_t>& grid, int n, int c0, int r0, int c1, int r1)
{
	float dist = std::sqrt(float((c1 - c0) * (c1 - c0) + (r1 - r0) * (r1 - r0)));
	int steps = int(dist) + 1;
	for (int i = 0; i <= steps; ++i)
	{
		float t = float(i) / float(steps);
		int c = int(float(c0) + float(c1 - c0) * t), r = int(float(r0) + float(r1 - r0) * t);
		if (c < 0 || c >= n || r < 0 || r > 63) return false;
		if (grid[size_t(c) * 64 + size_t(r)] & kMask) return false;
	}
	return true;
}

void flood(std::vector<uint32_t>& grid, int n, int sc, int sr)
{
	sc = std::clamp(sc, 0, n - 1), sr = std::clamp(sr, 0, 63);
	uint32_t& s = grid[size_t(sc) * 64 + size_t(sr)];
	s = (s & kMask) | kSeed;
	std::deque<std::pair<int, int>> q{{sc, sr}};
	int guard = n * 64 * 8;
	while (!q.empty() && guard-- > 0)
	{
		auto [c, r] = q.front();
		q.pop_front();
		uint32_t cost = grid[size_t(c) * 64 + size_t(r)] & kCost;
		for (int pass = 0; pass < 5; ++pass)
		{
			long nc = long(cost) - kPassCost[pass];
			if (nc <= 0) continue;
			for (int k = 0; k < 4; ++k)
			{
				int cc = c + kPassOff[pass][k][0], rr = r + kPassOff[pass][k][1];
				if (cc < 0 || cc >= n || rr < 0 || rr > 63) continue;
				uint32_t& cell = grid[size_t(cc) * 64 + size_t(rr)];
				if ((cell & kMask) || uint32_t(nc) <= (cell & kCost)) continue;
				cell = (cell & kMask) | uint32_t(nc);
				q.push_back({cc, rr});
			}
		}
	}
}

bool backtrace(const std::vector<uint32_t>& grid, int n, int col, int row, int sc, int sr, std::vector<std::pair<int, int>>& cells)
{
	for (int guard = n * 64; guard > 0; --guard)
	{
		cells.push_back({col, row});
		if (col == sc && row == sr) return true;
		uint32_t best = grid[size_t(col) * 64 + size_t(row)] & kCost;
		int bc = -1, br = -1;
		for (int pass = 0; pass < 5; ++pass)
			for (int k = 0; k < 4; ++k)
			{
				int cc = col + kPassOff[pass][k][0], rr = row + kPassOff[pass][k][1];
				if (cc < 0 || cc >= n || rr < 0 || rr > 63) continue;
				uint32_t c = grid[size_t(cc) * 64 + size_t(rr)] & kCost;
				if (c > best) best = c, bc = cc, br = rr;
			}
		if (bc < 0) return false;
		col = bc, row = br;
	}
	return false;
}

// Głębokość wzdłuż odcinka podłogi (FloorPathMap.Build, ostatni odcinek biegu: okno ±16 przy celu, trzy punkty
// przy wolnej linii prostej). Wynik: start … cel; pusty = brak drogi w głębokości.
std::vector<P3> floorPathMap(const NavWorld& nav, const std::vector<std::pair<float, float>>& ctl, const P3& start, const P3& goal, std::string* why)
{
	std::vector<P3> pts = densify(ctl, goal);
	lipSnap(nav, pts);
	int n = int(pts.size());
	std::vector<uint32_t> grid(size_t(n) * 64, 0);
	stampBands(nav, pts, grid);
	int startCol = nearestPoint(pts, start), goalColRaw = nearestPoint(pts, goal);
	int startRow = std::clamp(rnd(start.z * 4.f), 0, 63), goalRowRaw = std::clamp(rnd(goal.z * 4.f), 0, 63);
	if (!snapHoriz(grid, n, startCol, startRow))
	{
		if (why) *why = "floorpathmap: start poza pasem";
		return {};
	}
	int goalCol = goalColRaw, goalRow = goalRowRaw;
	if (!snapHoriz(grid, n, goalCol, goalRow))
	{
		goalCol = std::clamp(goalColRaw, 0, n - 1), goalRow = goalRowRaw;
		if (!pinNearestRow(grid, goalCol, goalRow))
		{
			if (why) *why = "floorpathmap: cel bez pasa";
			return {};
		}
	}
	P3 landing{goal.x, goal.y, float(goalRow) * 0.25f};
	if (losClear(grid, n, goalCol, goalRow, startCol, startRow))
	{ // linia prosta wolna: start, punkt celu w kolumnie celu na głębokości celu, cel
		std::vector<P3> los{start};
		P3 mid{pts[size_t(goalCol)].x, pts[size_t(goalCol)].y, landing.z};
		auto far = [](const P3& a, const P3& b) { return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z) > 1e-6f; };
		if (far(mid, start) && far(mid, landing)) los.push_back(mid);
		los.push_back(landing);
		return los;
	}
	flood(grid, n, goalCol, goalRow);
	if (!(grid[size_t(startCol) * 64 + size_t(startRow)] & kCost))
	{
		if (why) *why = "floorpathmap: zalewanie nie doszło do startu";
		return {};
	}
	std::vector<std::pair<int, int>> cells;
	if (!backtrace(grid, n, startCol, startRow, goalCol, goalRow, cells))
	{
		if (why) *why = "floorpathmap: powrót utknął";
		return {};
	}
	std::vector<P3> wps{start};
	for (auto [c, r] : cells) wps.push_back({pts[size_t(c)].x, pts[size_t(c)].y, float(r) * 0.25f});
	wps.push_back(landing);
	for (size_t i = 1; i + 1 < wps.size(); ++i)
	{ // punkty wewnętrzne: w dół, dopóki wiersz niżej jest podłogą (ostatni wiersz podłogi)
		int sx = rnd(wps[i].x * 8.f);
		for (int g = 0; nav.floor(sx, rnd(wps[i].y * 8.f) + 1) && g < 64; ++g) wps[i].y += 0.125f;
	}
	return wps;
}

// Wierzchołki łamanej sterującej między punktami głębokości (z interpolowane wzdłuż drogi): bez nich ruch
// między rzadkimi punktami szedłby cięciwą przez załamania podłogi.
std::vector<P3> executableRoute(const std::vector<P3>& depth, const std::vector<std::pair<float, float>>& ctl)
{
	std::vector<P3> out;
	auto add = [&](const P3& p) {
		if (!out.empty())
		{
			const P3& b = out.back();
			if ((b.x - p.x) * (b.x - p.x) + (b.y - p.y) * (b.y - p.y) + (b.z - p.z) * (b.z - p.z) <= 1e-8f)
			{
				out.back() = p;
				return;
			}
		}
		out.push_back(p);
	};
	if (depth.size() <= 1 || ctl.size() < 2) return depth;
	std::vector<float> cum(ctl.size(), 0.f);
	for (size_t i = 1; i < ctl.size(); ++i)
		cum[i] = cum[i - 1] + std::hypot(ctl[i].first - ctl[i - 1].first, ctl[i].second - ctl[i - 1].second);
	float total = cum.back();
	if (total <= 1e-5f) return depth;
	auto project = [&](const P3& p, float minProg) {
		float bestProg = minProg, bd = 1e30f;
		for (size_t i = 0; i + 1 < ctl.size(); ++i)
		{
			float dx = ctl[i + 1].first - ctl[i].first, dy = ctl[i + 1].second - ctl[i].second, len = cum[i + 1] - cum[i];
			if (len <= 1e-5f || cum[i + 1] < minProg) continue;
			float minT = std::clamp((minProg - cum[i]) / len, 0.f, 1.f);
			float t = std::clamp(((p.x - ctl[i].first) * dx + (p.y - ctl[i].second) * dy) / (dx * dx + dy * dy), minT, 1.f);
			float px = ctl[i].first + dx * t - p.x, py = ctl[i].second + dy * t - p.y, d = px * px + py * py;
			if (d >= bd) continue;
			bd = d, bestProg = cum[i] + len * t;
		}
		return bestProg;
	};
	std::vector<float> prog(depth.size(), 0.f);
	for (size_t i = 1; i + 1 < depth.size(); ++i) prog[i] = project(depth[i], prog[i - 1]);
	prog.back() = total;
	add(depth[0]);
	size_t ci = 1;
	for (size_t a = 1; a < depth.size(); ++a)
	{
		float from = prog[a - 1], to = prog[a];
		while (ci + 1 < ctl.size() && cum[ci] <= from + 1e-5f) ++ci;
		while (ci + 1 < ctl.size() && cum[ci] < to - 1e-5f)
		{
			float t = to > from + 1e-5f ? (cum[ci] - from) / (to - from) : 0.f;
			add({ctl[ci].first, ctl[ci].second, depth[a - 1].z + (depth[a].z - depth[a - 1].z) * t});
			++ci;
		}
		add(depth[a]);
	}
	return out;
}

P3 airClamp(const NavWorld& nav, P3 p)
{
	p.z = std::max(p.z, airDepth(nav, p.x, p.y));
	return p;
}

} // namespace

int supportDepth(const NavWorld& nav, int sx, int sy)
{
	int h = nav.detail(sx, sy - 1);
	if (h != 15) return h;
	int hx = std::min(nav.detail(sx - 1, sy - 1), nav.detail(sx + 1, sy - 1));
	int hy = std::min(nav.detail(sx, sy - 2), nav.detail(sx, sy));
	return std::min(hx, hy);
}

float floorLineDepth(const NavWorld& nav, int sx, int sy)
{
	return std::min(float(supportDepth(nav, sx, sy)) + 1.f, 14.5f);
}

bool floorBand(const NavWorld& nav, int sx, int sy, float& z0, float& z1)
{
	int lo = nav.detail(sx, sy - 1) * 4 + 1, hi = nav.detail(sx, sy + 1) * 4 - 1; // wiersze [lo, hi)
	hi = std::min(hi, 64);
	if (lo >= hi) return false;
	z0 = float(lo) * 0.25f, z1 = float(hi - 1) * 0.25f;
	return true;
}

bool hasFloorSupport(const NavWorld& nav, float x, float y)
{
	int sx = rnd(x * 8.f), sy = rnd(y * 8.f);
	return nav.floor(sx, sy + 1) || nav.floor(sx, sy) || nav.floor(sx, sy - 1);
}

float airDepth(const NavWorld& nav, float x, float y)
{
	int sx = rnd(x * 8.f), sy = rnd(y * 8.f);
	return float(std::min(nav.detail(sx, sy), nav.detail(sx, sy - 1)));
}

std::vector<WalkPoint> planWalk(const NavWorld& nav, const float start[3], const float goal[3], bool canClimb, bool keepGoal, std::string* why)
{
	std::vector<WalkPoint> wps;
	auto subs = nav.findPath(rnd(start[0] * 8.f), rnd(start[1] * 8.f), rnd(goal[0] * 8.f), rnd(goal[1] * 8.f), canClimb);
	if (subs.empty())
	{
		if (why) *why = "calc1: brak drogi";
		return wps;
	}
	if (!keepGoal)
	{ // cel przesunięty na samą ścianę (nie kliknięty na ścianie): w dół na pierwszą podłogę pod nim
		auto& g = subs.back();
		int rx = rnd(goal[0] * 8.f), ry = rnd(goal[1] * 8.f);
		bool wallGoal = nav.climbWall(rx, ry);
		if (!wallGoal && nav.climbWall(g.first, g.second))
			for (int d = 1; d <= 16; ++d)
			{
				if (nav.floor(g.first, g.second + d))
				{
					g.second += d;
					break;
				}
				if (!nav.walkable(g.first, g.second + d)) break;
			}
	}
	// Dwa punkty podłogi z uskokiem ≥ 3 podpola między nimi (droga po polach pomija ścianę półki): wstawka
	// podpól ściany z kolumny jednego z nich.
	for (int i = int(subs.size()) - 2; i >= 0; --i)
	{
		auto [ax, ay] = subs[size_t(i)];
		auto [bx, by] = subs[size_t(i) + 1];
		if (isWallRecord(nav, ax, ay) || isWallRecord(nav, bx, by)) continue;
		int dy = by - ay;
		if (std::abs(dy) < 3 || std::abs(dy) > 64 || std::abs(bx - ax) > 2) continue;
		int step = dy > 0 ? 1 : -1;
		std::vector<int> cols{ax};
		if (bx != ax) cols.push_back(bx);
		for (int col : cols)
		{
			std::vector<std::pair<int, int>> face;
			for (int y = ay + step; y != by; y += step)
				if (nav.climbWall(col, y)) face.push_back({col, y});
			if (face.empty()) continue;
			subs.insert(subs.begin() + i + 1, face.begin(), face.end());
			break;
		}
	}

	P3 cur{float(subs[0].first) * 0.125f, float(subs[0].second) * 0.125f, std::clamp(start[2], 0.f, 15.75f)};
	wps.push_back({start[0], start[1], start[2], isWallRecord(nav, subs[0].first, subs[0].second)});
	for (size_t i = 0; i < subs.size();)
	{
		bool wallRun = isWallRecord(nav, subs[i].first, subs[i].second);
		size_t j = i;
		while (j < subs.size() && isWallRecord(nav, subs[j].first, subs[j].second) == wallRun) ++j;
		if (wallRun)
		{ // ściana: podpola drogi na głębokości podparcia
			for (size_t k = i; k < j; ++k)
			{
				auto [sx, sy] = subs[k];
				cur = {float(sx) * 0.125f, float(sy) * 0.125f, float(supportDepth(nav, sx, sy))};
				wps.push_back({cur.x, cur.y, cur.z, true});
			}
			if (j < subs.size())
			{ // wspinanie zajmuje pierwsze podpole podłogi dalej (zejście ze ściany) z głębokością ściany
				auto [bx, by] = subs[j];
				P3 b = airClamp(nav, {float(bx) * 0.125f, float(by) * 0.125f, cur.z});
				wps.push_back({b.x, b.y, b.z, isWallRecord(nav, bx, by)});
				cur = {b.x, b.y, floorLineDepth(nav, bx, by)};
			}
		}
		else
		{
			std::vector<std::pair<float, float>> ctl{{cur.x, cur.y}};
			for (size_t k = i; k < j; ++k) ctl.push_back({float(subs[k].first) * 0.125f, float(subs[k].second) * 0.125f});
			P3 runGoal;
			if (j == subs.size())
			{ // ostatni bieg: podpole celu na głębokości celu
				runGoal = {float(subs.back().first) * 0.125f, float(subs.back().second) * 0.125f, std::clamp(goal[2], 0.f, 15.75f)};
			}
			else
			{ // bieg kończy się przy ścianie: przy jej powierzchni (min wysokości dwóch pierwszych podpól ściany + 1)
				auto [lx, ly] = subs[j - 1];
				auto [w1x, w1y] = subs[j];
				auto [w2x, w2y] = subs[std::min(j + 1, subs.size() - 1)];
				int hw = std::min(nav.detail(w1x, w1y), nav.detail(w2x, w2y));
				runGoal = {float(lx) * 0.125f, float(ly) * 0.125f, std::min(15.f, float(hw) + 1.f)};
			}
			auto route = floorPathMap(nav, ctl, cur, runGoal, why);
			if (route.empty())
			{
				wps.clear();
				return wps;
			}
			route = executableRoute(route, ctl);
			for (size_t k = 1; k < route.size(); ++k)
			{
				P3 e = airClamp(nav, route[k]);
				wps.push_back({e.x, e.y, e.z, isWallRecord(nav, rnd(e.x * 8.f), rnd(e.y * 8.f))});
			}
			cur = airClamp(nav, route.back());
		}
		i = j;
	}
	if (wps.size() <= 1)
	{
		if (why) *why = "trasa z jednego punktu";
		wps.clear();
	}
	return wps;
}

} // namespace ow
