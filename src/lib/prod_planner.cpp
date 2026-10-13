#include "lib/prod_planner.h"

#include "lib/script/engine.h"
#include "lib/world_sim.h"

#include <tcl.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace ow
{

namespace
{
// Stałe gry (Diggles-Unity ProdDispatcher.cs, odczytane z Diggles.exe .rdata; tam adresy).
constexpr double kInterval = 0.5;     // przegląd co 0,5 s, gracze po kolei (gra: 1,0 s, Diggles-Unity: 0,5)
constexpr int kAssignCooldown = 2;    // tury do ponownego przydziału krasnala
constexpr int kFailBreathe = 50;      // 5 s wstrzymania krasnala po porażce
constexpr int kReopenTicks = 20;      // 2 s, zanim nieudane zadanie wróci
constexpr float kReopenRadius = 0.8f; // to samo zadanie = w promieniu 0,8
constexpr int kScanBudget = 2000;     // pól prostokąta znaczników na przegląd
constexpr int kScanSkip = 8, kMerge = 20, kDiggerRefresh = 5; // odległości Manhattan
constexpr float kAnnounceTimer = 10, kInitialTimer = 1, kNeedExisting = 7.5f, kJobZ = 14;
constexpr float kDigMaxCost = 120, kCrowdNear = 15, kCrowdFar = 30, kCrowdPenalty = 1.5f;

int manhattan(int dx, int dy)
{
	return std::abs(dx) + std::abs(dy);
}

// Ważony kwadrat odległości gry: oś pionowa (wspinanie) ·5, głębokość ·0,25.
float dist2(const float a[3], const float b[3])
{
	float dx = a[0] - b[0], dy = (a[1] - b[1]) * 5.f, dz = (a[2] - b[2]) * 0.25f;
	return dx * dx + dy * dy + dz * dz;
}

const char* objVar(script::ScriptObject* o, const char* name)
{
	return o && o->interp ? Tcl_GetVar(o->interp, name, TCL_GLOBAL_ONLY) : nullptr;
}
} // namespace

void ProdPlanner::reset()
{
	accum_ = 0, cursor_ = 0, dispatched = 0;
	for (auto& s : idle_) s.clear();
	for (auto& r : reqs_) r.clear();
	scanX_.fill(0), scanY_.fill(0);
	busy_.clear(), breathe_.clear(), reopen_.clear();
}

bool ProdPlanner::pooled(int id) const
{
	for (const auto& s : idle_)
		if (s.count(id)) return true;
	return false;
}

void ProdPlanner::gnomeIdle(int id, bool idle)
{
	script::ScriptObject* o = w_.scripts ? w_.scripts->object(id) : nullptr;
	if (!o) return;
	auto& pool = idle_[size_t(clampPlayer(o->owner))];
	if (!idle)
	{ // praca: z puli, koniec wstrzymania po porażce
		pool.erase(id);
		breathe_.erase(id);
		return;
	}
	if (busy_.count(id)) return;
	pool.insert(id);
}

void ProdPlanner::releaseGnome(int id)
{ // gnome_failed_work / gnome_stopped_work: wstrzymanie 5 s, zadanie wraca po 2 s, rezerwacja zdjęta
	breathe_[id] = kFailBreathe;
	auto& locks = w_.scripts->placeLocks_;
	if (auto it = locks.find(id); it != locks.end())
	{
		reopen_.push_back({{it->second.first[0], it->second.first[1], it->second.first[2]}, kReopenTicks});
		locks.erase(it);
	}
}

void ProdPlanner::announceDig(int tx, int ty)
{ // gnome_announce_dig: prośby w promieniu Manhattan < 20 zamknięte na 10 przeglądów (nikt drugi tu nie kopie)
	for (auto& reqs : reqs_)
		for (auto& r : reqs)
			if (manhattan(r.x - tx, r.y - ty) < kMerge) r.timer = kAnnounceTimer;
}

void ProdPlanner::logicTick()
{
	for (auto it = breathe_.begin(); it != breathe_.end();) it = --it->second <= 0 ? breathe_.erase(it) : std::next(it);
	for (auto it = busy_.begin(); it != busy_.end();) it = --it->second <= 0 ? busy_.erase(it) : std::next(it);
	reopen_.erase(std::remove_if(reopen_.begin(), reopen_.end(), [](Reopen& r) { return --r.ticks <= 0; }), reopen_.end());
}

void ProdPlanner::tick(double dt)
{
	if (!w_.scripts) return;
	accum_ += dt;
	if (accum_ < kInterval) return;
	accum_ = 0;
	cursor_ = (cursor_ + 1) % kPlayers;
	sweep(cursor_);
}

bool ProdPlanner::workTime(int id) const
{ // okno pracy krasnala w godzinach doby (godzina = 150 s, doba 12 h)
	script::ScriptObject* o = w_.scripts->object(id);
	if (!o) return false;
	double phase = std::fmod(w_.scripts->time() / script::kHourSec, script::kDayHours);
	double rel = phase - o->workStart;
	if (rel < 0) rel += script::kDayHours;
	return rel < o->workLen;
}

bool ProdPlanner::autoSchedule(int id) const
{
	script::ScriptObject* o = w_.scripts->object(id);
	auto it = o ? o->props.find("prodautoschedule") : decltype(o->props.end()){};
	return o && (it == o->props.end() || it->second != "0");
}

bool ProdPlanner::jobOnCooldown(const float p[3]) const
{
	for (const auto& r : reopen_)
		if (std::hypot(r.pos[0] - p[0], r.pos[1] - p[1]) <= kReopenRadius) return true;
	return false;
}

// Krasnal kopie: zajęty (poza pulą) i current_worktask = dig (evt_task_dig / evt_autoprod_dig).
bool ProdPlanner::activeDigger(int id, int player) const
{
	if (idle_[size_t(player)].count(id)) return false;
	const char* wt = objVar(w_.scripts->object(id), "current_worktask");
	return wt && std::string(wt) == "dig";
}

// Punkt kopania krasnala: current_digpos z jego skryptu, inaczej jego pozycja.
bool ProdPlanner::digPos(int id, float out[3]) const
{
	script::ScriptObject* o = w_.scripts->object(id);
	if (const char* s = objVar(o, "current_digpos"); s && *s && std::sscanf(s, "%f %f %f", &out[0], &out[1], &out[2]) >= 2) return true;
	return w_.getPos(id, out);
}

// Prośby kopania (lejek gry): A. kopiący odświeża prośbę przy sobie albo zakłada nową; B. odliczanie, prośby bez
// znacznika znikają, czekające się starzeją; C. przegląd prostokąta znaczników po 2000 pól, nowa prośba tylko dalej niż
// 20 pól od istniejącej (cały zaznaczony obszar 10×10 = jedna prośba = jeden kopiący).
void ProdPlanner::maintainDigRequests(int player)
{
	auto& reqs = reqs_[size_t(player)];
	DigTerrain& T = w_.digTerrain;
	int bx0, by0, bx1, by1;
	if (player != 0 || !T.ready())
	{ // jedna siatka znaczników: gracz lokalny (🔶 jak w Diggles-Unity)
		reqs.clear();
		return;
	}
	if (!T.bounds(player, bx0, by0, bx1, by1)) return;
	for (auto& [id, o] : w_.scripts->objects_)
	{
		if (!o->alive || o->owner != player || !activeDigger(id, player)) continue;
		float dp[3];
		if (!digPos(id, dp)) continue;
		int dx = int(std::lround(dp[0])), dy = int(std::lround(dp[1]));
		bool near = false;
		for (auto& r : reqs)
			if (manhattan(r.x - dx, r.y - dy) < kDiggerRefresh)
			{
				r.timer = kAnnounceTimer, near = true;
				break;
			}
		if (!near) reqs.push_back({dx, dy, kInitialTimer, 0, false});
	}
	for (size_t k = reqs.size(); k-- > 0;)
	{
		auto& r = reqs[k];
		r.timer -= 1;
		if (r.timer > 0) continue;
		if (T.nibble(r.x, r.y, player) == 0) reqs.erase(reqs.begin() + long(k));
		else r.age += 1;
	}
	auto nearReq = [&](int x, int y, int m) {
		for (auto& r : reqs)
			if (manhattan(r.x - x, r.y - y) < m) return true;
		return false;
	};
	int& sx = scanX_[size_t(player)];
	int& sy = scanY_[size_t(player)];
	for (int n = 0; n < kScanBudget; ++n)
	{
		++sx;
		if (sx < bx0 || sx > bx1) ++sy, sx = bx0;
		if (sy < by0 || sy > by1) sy = by0;
		if (nearReq(sx, sy, kScanSkip) || T.nibble(sx, sy, player) == 0 || nearReq(sx, sy, kMerge)) continue;
		reqs.push_back({sx, sy, kInitialTimer, 0, false});
	}
}

// Zadania kopania z próśb: prośba zamknięta (licznik > 0) nie daje zadania; potrzeba = wiek · 0,5 + 1 (7,5, gdy już było
// zadanie). Pozycja zadania = krawędź zaznaczonego obszaru (get_digedge od pola prośby), z = 14.
void ProdPlanner::collectDigJobs(int player, std::vector<Job>& jobs)
{
	if (player != 0) return;
	for (auto& r : reqs_[size_t(player)])
	{
		r.timer -= 1;
		if (r.timer > 0)
		{
			r.hadJob = false;
			continue;
		}
		int ex, ey;
		if (!w_.digEdgeTile(r.x, r.y, r.x, r.y, ex, ey)) continue;
		Job j;
		j.kind = 0, j.tx = r.x, j.ty = r.y, j.need = r.hadJob ? kNeedExisting : r.age * 0.5f + 1;
		j.pos[0] = float(ex), j.pos[1] = float(ey), j.pos[2] = kJobZ;
		r.hadJob = true;
		jobs.push_back(j);
	}
}

// Koszt dopasowania: ważona odległość; kopanie: powyżej 120 odpada, a inny kopiący blisko zadania mnoży koszt ·1,5
// (bliżej niż √15) i jeszcze ·1,5 (bliżej niż √30): krasnale rozchodzą się do różnych frontów.
float ProdPlanner::matchCost(int id, const Job& j, int player) const
{
	float g[3];
	if (!w_.getPos(id, g)) return INFINITY;
	float d = dist2(g, j.pos);
	if (j.kind != 0) return d;
	if (d > kDigMaxCost) return INFINITY;
	for (auto& [oid, o] : w_.scripts->objects_)
	{
		if (oid == id || !o->alive || o->owner != player || !activeDigger(oid, player)) continue;
		float dp[3];
		if (!digPos(oid, dp)) continue;
		float h2 = (dp[0] - j.pos[0]) * (dp[0] - j.pos[0]) + (dp[1] - j.pos[1]) * (dp[1] - j.pos[1]);
		if (h2 < kCrowdNear) d *= kCrowdPenalty;
		if (h2 < kCrowdFar) d *= kCrowdPenalty;
	}
	return d;
}

// Rezerwacja miejsca zadania na 6 s (placelock), żeby dwóch krasnali nie szło w to samo miejsce.
bool ProdPlanner::reserve(const Job& j, int id)
{
	auto& locks = w_.scripts->placeLocks_;
	double now = w_.scripts->time();
	for (auto& [lid, l] : locks)
		if (lid != id && l.second >= float(now) && std::hypot(l.first[0] - j.pos[0], l.first[1] - j.pos[1]) < 0.8f) return false;
	locks[id] = {{j.pos[0], j.pos[1], j.pos[2]}, float(now + 6.0)};
	return true;
}

void ProdPlanner::raise(int id, const Job& j)
{
	script::ScriptObject* o = w_.scripts->object(id);
	if (!o) return;
	char cmd[160];
	std::snprintf(cmd, sizeof cmd, "set_event this evt_autoprod_dig -pos1 {%.3g %.3g %.3g}", j.pos[0], j.pos[1], j.pos[2]);
	if (o->interp) w_.scripts->runIn(*o, cmd, "planista");
	if (std::getenv("OW_TRACE_PLAN")) std::fprintf(stderr, "[plan %.1f] %d: %s (prośba %d %d)\n", w_.scripts->time(), id, cmd, j.tx, j.ty);
	++dispatched;
}

void ProdPlanner::sweep(int player)
{
	std::vector<Job> jobs;
	collectDigJobs(player, jobs);
	maintainDigRequests(player);
	auto& pool = idle_[size_t(player)];
	for (auto it = pool.begin(); it != pool.end();)
	{
		script::ScriptObject* o = w_.scripts->object(*it);
		bool ok = o && o->alive && o->owner == player && !busy_.count(*it) && workTime(*it) && autoSchedule(*it);
		it = ok ? std::next(it) : pool.erase(it);
	}
	if (pool.empty() || jobs.empty()) return;
	std::stable_sort(jobs.begin(), jobs.end(), [](const Job& a, const Job& b) { return a.need > b.need; });
	for (const Job& j : jobs)
	{
		if (jobOnCooldown(j.pos)) continue;
		int best = 0;
		float bestD = INFINITY;
		for (int id : pool)
		{
			if (breathe_.count(id)) continue;
			float d = matchCost(id, j, player);
			if (d < bestD) bestD = d, best = id;
		}
		if (!best || !reserve(j, best)) continue;
		busy_[best] = kAssignCooldown;
		pool.erase(best);
		if (j.kind == 0) announceDig(j.tx, j.ty); // zadany obszar zamknięty dla drugiego kopiącego
		raise(best, j);
		break; // jedno zadanie na przegląd (gra tak samo)
	}
}

} // namespace ow
