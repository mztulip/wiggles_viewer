#include "lib/prod_planner.h"

#include "lib/script/engine.h"
#include "lib/script/commands.h"
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
constexpr float kRequestNeed = 7.5f;                   // zlecenia pakowania i budowy
constexpr int kHarvestCooldown = 150;                  // 15 s między zbiorami (5 s przegląd + 10 s po wysłaniu)
constexpr float kHarvestNeedScale = 0.2f;              // potrzeba zbioru = niedobór · 0,2
constexpr float kPrioFinite = 0.0005f, kPrioInfinite = 0.0003f; // wzrost priorytetu zamówienia na turę logiki
constexpr float kInventionNeedScale = 10;               // potrzeba wynalazku = priorytet · 10
constexpr int kInfiniteOrder = 10;                      // zamówienie „bez końca”
constexpr float kAbsorbXZ = 4, kAbsorbY = 2;           // przedmiot tuż przy budynku: budynek bierze go sam

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

// Zasób, który daje brakujący materiał (gra: rejestr wytwarzania FUN_00432690; tabela jak w Diggles-Unity: grzyb).
const char* produceOf(const std::string& cls)
{
	return cls == "Pilzstamm" || cls == "Pilzhut" ? "Pilz" : nullptr;
}

std::string fmtPos(const float p[3])
{
	char b[96];
	std::snprintf(b, sizeof b, "{%.3g %.3g %.3g}", p[0], p[1], p[2]);
	return b;
}
} // namespace

void ProdPlanner::reset()
{
	accum_ = 0, cursor_ = 0, dispatched = 0, logicTick_ = 0;
	for (auto& s : idle_) s.clear();
	for (auto& r : reqs_) r.clear();
	scanX_.fill(0), scanY_.fill(0), harvestCooldown_.fill(0);
	busy_.clear(), breathe_.clear(), reopen_.clear(), claim_.clear(), plans_.clear(), slotPrio_.clear(), materials_.clear();
	sentKind.clear();
}

bool ProdPlanner::pooled(int id) const
{
	for (const auto& s : idle_)
		if (s.count(id)) return true;
	return false;
}

int ProdPlanner::claimOf(int place) const
{
	auto it = claim_.find(place);
	return it == claim_.end() ? 0 : it->second;
}

bool ProdPlanner::flag(int id, const char* prop, bool def) const
{
	script::ScriptObject* o = w_.scripts->object(id);
	if (!o) return def;
	auto it = o->props.find(prop);
	return it == o->props.end() ? def : script::truthy(it->second);
}

void ProdPlanner::releaseClaim(int place)
{ // budynek wolny: postęp i zlecenia rozpakowania i budowy znikają (pakowanie zostaje: 🔶 jak w Diggles-Unity)
	claim_.erase(place);
	if (script::ScriptObject* o = w_.scripts->object(place))
		o->props["prod_cur"] = "0", o->props["prod_max"] = "0", o->props["prod_unpack"] = "0", o->props["prod_buildup"] = "0";
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
	// Bezczynność bez planu kończy pracę przy budynku; z planem to sygnał do następnego kroku planu. Plan, który
	// właśnie się skończył, też od razu zwalnia budynek (inaczej zwolnienie przy następnej bezczynności zerowałoby
	// zlecenie wydane w międzyczasie, np. rozpakowanie po pakowaniu).
	if (auto p = plans_.find(id); p != plans_.end())
	{
		advancePlan(id, p->second);
		if (plans_.count(id)) return;
	}
	for (auto it = claim_.begin(); it != claim_.end();)
	{
		int place = it->first;
		++it;
		if (claim_[place] == id) releaseClaim(place);
	}
}

void ProdPlanner::releaseGnome(int id)
{ // gnome_failed_work / gnome_stopped_work: wstrzymanie 5 s, zadanie wraca po 2 s, rezerwacja i budynki zwolnione
	breathe_[id] = kFailBreathe;
	auto& locks = w_.scripts->placeLocks_;
	if (auto it = locks.find(id); it != locks.end())
	{
		reopen_.push_back({{it->second.first[0], it->second.first[1], it->second.first[2]}, kReopenTicks});
		locks.erase(it);
	}
	for (int c = 0; c < kPlayers; ++c)
		if (harvestCooldown_[size_t(c)] > kReopenTicks) harvestCooldown_[size_t(c)] = kReopenTicks;
	std::vector<int> mine;
	for (auto& [place, g] : claim_)
		if (g == id) mine.push_back(place);
	for (int place : mine)
	{
		Reopen r{{0, 0, 0}, kReopenTicks};
		if (w_.getPos(place, r.pos)) reopen_.push_back(r);
		releaseClaim(place);
	}
	if (auto p = plans_.find(id); p != plans_.end()) dropPlan(id, p->second.stage < 2);
}

void ProdPlanner::announceDig(int tx, int ty)
{ // gnome_announce_dig: prośby w promieniu Manhattan < 20 zamknięte na 10 przeglądów (nikt drugi tu nie kopie)
	for (auto& reqs : reqs_)
		for (auto& r : reqs)
			if (manhattan(r.x - tx, r.y - ty) < kMerge) r.timer = kAnnounceTimer;
}

void ProdPlanner::logicTick()
{
	++logicTick_;
	for (auto it = breathe_.begin(); it != breathe_.end();) it = --it->second <= 0 ? breathe_.erase(it) : std::next(it);
	for (auto it = busy_.begin(); it != busy_.end();) it = --it->second <= 0 ? busy_.erase(it) : std::next(it);
	reopen_.erase(std::remove_if(reopen_.begin(), reopen_.end(), [](Reopen& r) { return --r.ticks <= 0; }), reopen_.end());
	for (auto& c : harvestCooldown_)
		if (c > 0) --c;
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

bool ProdPlanner::jobOnCooldown(const float p[3]) const
{
	for (const auto& r : reopen_)
		if (std::hypot(r.pos[0] - p[0], r.pos[1] - p[1]) <= kReopenRadius) return true;
	return false;
}

bool ProdPlanner::planTargets(int item) const
{
	for (const auto& [g, p] : plans_)
		if (p.item == item) return true;
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

// ---------------------------------------------------------------------------------------------------------------
// Kopanie (prośby i zadania, docs/silnik/20_planista.md §20.3)

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
		j.kind = Dig, j.tx = r.x, j.ty = r.y, j.need = r.hadJob ? kNeedExisting : r.age * 0.5f + 1;
		j.pos[0] = float(ex), j.pos[1] = float(ey), j.pos[2] = kJobZ;
		r.hadJob = true;
		jobs.push_back(j);
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Budynki: zlecenia, wynalazki, produkcja, dowóz, zbiory

// Priorytet zamówienia (gra: rekord slotu +0x14, co turę logiki budynku): 0 przy braku zamówienia, rośnie o 0,0005
// (0,0003 dla „bez końca”) na turę do 1. Liczymy leniwie od ostatniego odczytu.
float ProdPlanner::slotPriority(int place, const std::string& item, int count)
{
	auto key = std::make_pair(place, item);
	if (count <= 0)
	{
		slotPrio_.erase(key);
		return 0;
	}
	float rate = count == kInfiniteOrder ? kPrioInfinite : kPrioFinite;
	auto it = slotPrio_.find(key);
	if (it == slotPrio_.end()) it = slotPrio_.emplace(key, std::make_pair(rate, logicTick_)).first;
	auto& [v, t] = it->second;
	v = std::min(1.f, v + rate * float(logicTick_ - t));
	t = logicTick_;
	return v;
}

double ProdPlanner::ownerAttrib(int player, const std::string& name)
{
	std::string r;
	return w_.scripts->eval("get_owner_attrib " + std::to_string(player) + " " + name, &r) ? std::atof(r.c_str()) : 0;
}

// Materiały przedmiotu: metoda budynku prod_item_materials (dane techtreetunes), pamiętane na klasę.
std::vector<std::string> ProdPlanner::materialsFor(int place, const std::string& item)
{
	script::ScriptObject* o = w_.scripts->object(place);
	std::string key = (o && o->cls ? o->cls->name : "") + "\n" + item;
	if (auto it = materials_.find(key); it != materials_.end()) return it->second;
	std::vector<std::string> m;
	std::string r;
	if (w_.scripts->eval("call_method " + std::to_string(place) + " prod_item_materials {" + item + "}", &r))
		for (const auto& s : script::listItems(w_.scripts->main_, r))
			if (!s.empty() && s != "0") m.push_back(s);
	return materials_[key] = m;
}

// Materiały minus to, co budynek ma w ekwipunku (każdy przedmiot pokrywa jedną pozycję).
std::vector<std::string> ProdPlanner::missingMaterials(int place, const std::string& item)
{
	auto miss = materialsFor(place, item);
	if (script::ScriptObject* o = w_.scripts->object(place))
		for (int id : o->inventory)
			if (script::ScriptObject* it = w_.scripts->object(id); it && it->cls)
				if (auto f = std::find(miss.begin(), miss.end(), it->cls->name); f != miss.end()) miss.erase(f);
	return miss;
}

// Wymagania wynalazku: metoda prod_item_attribs (pary atrybut wartość, np. {exp_Nahrung 0.039}); metoda instancji, bo
// czyta zmienne globalne tttinvent_* z techtreetunes, których nie ma przy wywołaniu statycznym.
std::vector<std::pair<std::string, float>> ProdPlanner::inventRequirements(int place, const std::string& item)
{
	std::vector<std::pair<std::string, float>> req;
	script::ScriptObject* o = w_.scripts->object(place);
	std::string r;
	if (!o || !o->cls || !w_.scripts->eval("call_method " + std::to_string(place) + " prod_item_attribs {" + item + "}", &r)) return req;
	std::vector<std::string> tok;
	for (const auto& el : script::listItems(w_.scripts->main_, r))
		for (const auto& t : script::listItems(w_.scripts->main_, el)) tok.push_back(t);
	for (size_t i = 0; i + 1 < tok.size(); i += 2) req.push_back({tok[i], float(std::atof(tok[i + 1].c_str()))});
	return req;
}

bool ProdPlanner::qualifies(int gnome, const std::vector<std::pair<std::string, float>>& req)
{
	for (const auto& [attr, val] : req)
		if (w_.scripts->attrib(gnome, attr) < val) return false;
	return true;
}

// Budynek może dawać zadania produkcji: zbudowany, ma zamówienia, nie jest skrzynią, automatyczny, włączony, przyjmuje
// zadania (prod_schedule) i nikt przy nim nie pracuje (jeden krasnal na budynek). 🔶 Bez energii.
bool ProdPlanner::placeAvailable(int place)
{
	script::ScriptObject* o = w_.scripts->object(place);
	if (!o || o->prodSlots.empty() || !flag(place, "buildupstate", false) || flag(place, "boxed", false)) return false;
	if (!flag(place, "prodautoschedule", true) || !flag(place, "prod_enabled", true) || !flag(place, "prod_schedule", true)) return false;
	int c = claimOf(place);
	return !c || !w_.scripts->object(c);
}

void ProdPlanner::collectInventionJobs(int place, int player, std::vector<Job>& jobs)
{ // wynalazek: zamówiony, jeszcze nie wynaleziony (Bp<przedmiot> właściciela), ktoś z krasnali gracza się nadaje
	script::ScriptObject* o = w_.scripts->object(place);
	float pos[3];
	if (!o || !w_.getPos(place, pos)) return;
	for (const auto& [item, cnt] : o->prodSlots)
	{
		float prio = slotPriority(place, item, int(cnt));
		if (cnt <= 0 || ownerAttrib(player, "Bp" + item) > 0.5) continue;
		auto req = inventRequirements(place, item);
		bool any = req.empty();
		for (auto& [id, g] : w_.scripts->objects_)
			if (!any && g->alive && g->owner == player && g->cls && (g->cls->type == "gnome" || g->cls->type == "baby"))
				any = qualifies(id, req);
		if (!any) continue;
		Job j;
		j.kind = Production, j.target = place, j.cls = "Bp" + item, j.need = prio * kInventionNeedScale;
		std::copy(pos, pos + 3, j.pos);
		jobs.push_back(j);
	}
}

// Najbliższe źródło materiału: wolne w świecie albo na półce magazynu, gracza albo niczyje, nie zablokowane, nie w drodze.
int ProdPlanner::haulSource(int place, const std::string& cls, int player, const std::set<int>& exclude)
{
	float pp[3];
	if (!w_.getPos(place, pp)) return 0;
	int best = 0;
	float bestD = INFINITY;
	for (auto& [id, it] : w_.scripts->objects_)
	{
		if (!it->alive || !it->cls || it->cls->name != cls) continue;
		if (it->containedIn >= 0 && !flag(id, "instore", false)) continue;
		if (it->owner != player && it->owner >= 0) continue;
		if (flag(id, "lock", false) || busy_.count(id) || planTargets(id) || exclude.count(id)) continue;
		float p[3];
		if (!w_.getPos(id, p)) continue;
		float d = dist2(p, pp);
		if (d < bestD) bestD = d, best = id;
	}
	return best;
}

void ProdPlanner::collectProductionJobs(int place, int player, std::vector<Job>& jobs)
{
	script::ScriptObject* o = w_.scripts->object(place);
	float pos[3];
	if (!o || !w_.getPos(place, pos)) return;
	for (const auto& [item, cnt] : o->prodSlots)
	{
		if (cnt <= 0 || ownerAttrib(player, "Bp" + item) <= 0.5) continue; // tylko wynalezione
		float need = float(cnt) + 2; // 🔶 jak w Diggles-Unity (gra: priorytet · 1000)
		auto miss = flag(place, "prod_materialneed", true) ? missingMaterials(place, item) : std::vector<std::string>{};
		if (miss.empty())
		{
			o->props["prod_item"] = item;
			Job j;
			j.kind = Production, j.target = place, j.cls = item, j.need = need;
			std::copy(pos, pos + 3, j.pos);
			jobs.push_back(j);
			return; // jeden pracujący na budynek
		}
		std::set<int> used;
		for (const auto& cls : miss)
		{
			int src = haulSource(place, cls, player, used);
			if (!src) continue; // brak źródła: zbiory (collectHarvestJobs) albo czekanie
			float sp[3];
			w_.getPos(src, sp);
			if (std::fabs(sp[0] - pos[0]) < kAbsorbXZ && std::fabs(sp[1] - pos[1]) < kAbsorbY && std::fabs(sp[2] - pos[2]) < kAbsorbXZ)
			{ // przedmiot tuż przy budynku: budynek bierze go sam (evt_prodplace_transferprod)
				busy_[src] = kAssignCooldown;
				w_.scripts->eval("set_event " + std::to_string(place) + " evt_prodplace_transferprod -target " + std::to_string(place) +
				    " -subject1 " + std::to_string(src));
				continue;
			}
			used.insert(src);
			Job j;
			j.kind = Haul, j.target = place, j.cls = cls, j.item = src, j.need = need;
			std::copy(pos, pos + 3, j.pos);
			jobs.push_back(j);
		}
		return; // jedno zamówienie budynku na przegląd
	}
}

// Zlecenia budynków po kolei (jedno zadanie na budynek): wynalazki, pakowanie, budowa, rozpakowanie, produkcja.
void ProdPlanner::collectPlaceJobs(int player, std::vector<Job>& jobs)
{
	std::vector<int> places;
	for (auto& [id, o] : w_.scripts->objects_)
		if (o->alive && o->owner == player && o->containedIn < 0 && o->cls && o->cls->type != "gnome" && o->cls->type != "baby") places.push_back(id);
	for (int id : places)
	{
		int c = claimOf(id);
		if ((c && w_.scripts->object(c)) || planTargets(id)) continue; // ktoś już przy nim pracuje
		bool avail = placeAvailable(id);
		if (avail) collectInventionJobs(id, player, jobs);
		float pos[3];
		if (!w_.getPos(id, pos)) continue;
		Job j;
		j.target = id, j.need = kRequestNeed;
		std::copy(pos, pos + 3, j.pos);
		if (flag(id, "prod_pack", false))
		{
			j.kind = Pack;
			jobs.push_back(j);
			continue;
		}
		if (flag(id, "prod_buildup", false))
		{
			j.kind = Unpack;
			jobs.push_back(j);
			continue;
		}
		if (flag(id, "prod_unpack", false))
		{
			j.kind = Unpack;
			script::ScriptObject* o = w_.scripts->object(id);
			if (auto it = o->props.find("order_pos"); it != o->props.end()) std::sscanf(it->second.c_str(), "%f %f %f", &j.pos[0], &j.pos[1], &j.pos[2]);
			j.asBox = flag(id, "order_asbox", false);
			jobs.push_back(j);
			continue;
		}
		if (avail) collectProductionJobs(id, player, jobs);
	}
}

// Zbiory: brakującego materiału nie ma nigdzie, a da go zasób (grzyb → kapelusz, trzon): najbliższy wolny zasób, jedno
// zadanie na przegląd, potem 15 s przerwy.
void ProdPlanner::collectHarvestJobs(int player, std::vector<Job>& jobs)
{
	if (harvestCooldown_[size_t(player)] > 0) return;
	for (auto& [pid, e] : w_.scripts->objects_)
	{
		if (!e->alive || e->owner != player || e->containedIn >= 0 || e->prodSlots.empty() || !flag(pid, "buildupstate", false)) continue;
		if (flag(pid, "boxed", false) || !flag(pid, "prodautoschedule", true) || !flag(pid, "prod_enabled", true) ||
		    !flag(pid, "prod_materialneed", true))
			continue;
		float pp[3];
		if (!w_.getPos(pid, pp)) continue;
		for (const auto& [item, cnt] : e->prodSlots)
		{
			if (cnt <= 0 || ownerAttrib(player, "Bp" + item) <= 0.5) continue;
			std::map<std::string, int> deficit;
			for (const auto& cls : missingMaterials(pid, item)) ++deficit[cls];
			for (const auto& [cls, n] : deficit)
			{
				bool covered = false;
				for (const auto& [g, p] : plans_)
					covered = covered || (p.kind == Haul && p.dest == pid && p.cls == cls);
				for (auto& [iid, it] : w_.scripts->objects_)
					covered = covered || (it->alive && it->cls && it->cls->name == cls && (it->containedIn < 0 || flag(iid, "instore", false)) && (it->owner == player || it->owner < 0) && !flag(iid, "lock", false));
				const char* res = produceOf(cls);
				if (covered || !res) continue;
				int best = 0;
				float bestD = INFINITY, bp[3] = {0, 0, 0};
				for (auto& [rid, r] : w_.scripts->objects_)
				{
					float rp[3];
					if (!r->alive || r->containedIn >= 0 || !r->cls || r->cls->name != res || (r->owner != player && r->owner != -1) ||
					    flag(rid, "lock", false) || busy_.count(rid) || !w_.getPos(rid, rp))
						continue;
					float d = dist2(rp, pp);
					if (d < bestD) bestD = d, best = rid, std::copy(rp, rp + 3, bp);
				}
				if (!best) continue;
				Job j;
				j.kind = Harvest, j.target = best, j.cls = res, j.need = float(n) * kHarvestNeedScale;
				std::copy(bp, bp + 3, j.pos);
				jobs.push_back(j);
				return;
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Dopasowanie, przydział, plany

bool ProdPlanner::match(int id, const Job& j)
{ // wynalazek: tylko krasnal spełniający wymagania; reszta: każdy
	if (j.kind != Production || j.cls.rfind("Bp", 0) != 0) return true;
	return qualifies(id, inventRequirements(j.target, j.cls.substr(2)));
}

// Koszt dopasowania: ważona odległość; kopanie: powyżej 120 odpada, a inny kopiący blisko zadania mnoży koszt ·1,5
// (bliżej niż √15) i jeszcze ·1,5 (bliżej niż √30): krasnale rozchodzą się do różnych frontów.
float ProdPlanner::matchCost(int id, const Job& j, int player) const
{
	float g[3];
	if (!w_.getPos(id, g)) return INFINITY;
	float d = dist2(g, j.pos);
	if (j.kind != Dig) return d;
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

bool ProdPlanner::emit(int id, const std::string& cmd, const char* kind)
{
	script::ScriptObject* o = w_.scripts->object(id);
	if (!o || !o->interp) return false;
	if (std::getenv("OW_TRACE_PLAN")) std::fprintf(stderr, "[plan %.1f] %d: %s\n", w_.scripts->time(), id, cmd.c_str());
	w_.scripts->runIn(*o, cmd, "planista");
	if (kind) ++sentKind[kind], ++dispatched;
	return true;
}

void ProdPlanner::assign(int id, const Job& j, int player)
{
	busy_[id] = kAssignCooldown;
	breathe_.erase(id);
	idle_[size_t(player)].erase(id);
	std::string t = std::to_string(j.target);
	switch (j.kind)
	{
	case Dig:
		announceDig(j.tx, j.ty); // obszar zamknięty dla drugiego kopiącego
		emit(id, "set_event this evt_autoprod_dig -pos1 " + fmtPos(j.pos), "dig");
		break;
	case Harvest:
		harvestCooldown_[size_t(player)] = kHarvestCooldown;
		emit(id, "set_event this evt_autoprod_harvest -subject1 " + t + " -text1 " + j.cls, "harvest");
		break;
	case Production:
		claim_[j.target] = id;
		if (script::ScriptObject* p = w_.scripts->object(j.target))
			p->props["prod_item"] = j.cls.rfind("Bp", 0) == 0 ? j.cls.substr(2) : j.cls, p->props["prod_cur"] = "0", p->props["prod_max"] = "0";
		emit(id, "set_event this evt_autoprod_workat -subject1 " + t + " -text1 " + j.cls, j.cls.rfind("Bp", 0) == 0 ? "invent" : "workat");
		break;
	case Pack:
	{ // plan: czapka (pakowanie) → evt_autoprod_pack
		claim_[j.target] = id;
		if (script::ScriptObject* p = w_.scripts->object(j.target)) p->props["prod_buildup"] = "0", p->props["prod_unpack"] = "0";
		Plan p;
		p.kind = Pack, p.item = j.target;
		plans_[id] = p;
		emit(id, "set_event this evt_change_muetze -text1 pack", "pack");
		break;
	}
	case Unpack:
	{
		claim_[j.target] = id;
		script::ScriptObject* c = w_.scripts->object(j.target);
		std::string cls = c && c->cls ? c->cls->name : "";
		if (c) c->props["prod_buildup"] = "0", c->props["prod_unpack"] = "0";
		if (flag(j.target, "boxed", false))
		{ // skrzynię trzeba najpierw przynieść: czapka → podniesienie → rozpakowanie na miejscu zlecenia
			Plan p;
			p.kind = Unpack, p.item = j.target, p.cls = cls, p.asBox = j.asBox;
			std::copy(j.pos, j.pos + 3, p.orderPos);
			plans_[id] = p;
			emit(id, "set_event this evt_change_muetze -text1 unpack", "unpack");
		}
		else // budynek już stoi: dokończenie budowy albo naprawa
			emit(id, "set_event this evt_autoprod_unpack -subject1 " + t + " -text1 {" + cls + "} -pos1 " + fmtPos(j.pos) + " -num1 " + (j.asBox ? "1" : "0"), "unpack");
		break;
	}
	case Haul:
	{ // plan: czapka transportowa → podniesienie przedmiotu → przekazanie do budynku
		claim_[j.target] = id;
		busy_[j.item] = kAssignCooldown;
		Plan p;
		p.kind = Haul, p.item = j.item, p.dest = j.target, p.cls = j.cls;
		plans_[id] = p;
		emit(id, "set_event this evt_change_muetze -text1 transport", "haul");
		break;
	}
	}
}

void ProdPlanner::dropPlan(int id, bool rearm)
{
	auto it = plans_.find(id);
	if (it == plans_.end()) return;
	Plan p = it->second;
	plans_.erase(it);
	for (int place : {p.dest, p.item})
		if (place && claimOf(place) == id) releaseClaim(place);
	if (rearm && p.kind == Unpack)
		if (script::ScriptObject* c = w_.scripts->object(p.item))
		{ // nieudane przyniesienie skrzyni: zlecenie wraca
			c->props["prod_unpack"] = "1";
			c->props["order_pos"] = std::to_string(p.orderPos[0]) + " " + std::to_string(p.orderPos[1]) + " " + std::to_string(p.orderPos[2]);
			c->props["order_asbox"] = p.asBox ? "1" : "0";
		}
}

// Krok planu na zgłoszenie bezczynności krasnala (po zakończeniu poprzedniego kroku).
void ProdPlanner::advancePlan(int id, Plan& p)
{
	script::ScriptObject* it = w_.scripts->object(p.item);
	auto free = [&](script::ScriptObject* o) { return o && o->alive && (o->containedIn < 0 || o->containedIn == id || flag(o->id, "instore", false)); };
	std::string item = std::to_string(p.item);
	if (p.kind == Pack)
	{
		if (p.stage == 0 && it && flag(p.item, "prod_pack", false))
		{
			p.stage = 1, busy_[id] = kAssignCooldown;
			emit(id, "set_event this evt_autoprod_pack -subject1 " + item, nullptr);
			return;
		}
		plans_.erase(id); // pakowanie prowadzi skrypt; budynek zwalnia następna bezczynność albo porażka
		return;
	}
	if (p.stage == 0 && free(it))
	{ // podniesienie (ze świata albo z półki magazynu)
		p.stage = 1, busy_[id] = kAssignCooldown;
		emit(id, std::string("set_event this ") + (flag(p.item, "instore", false) ? "evt_autoprod_pickup_store" : "evt_autoprod_pickup") + " -subject1 " + item + " -text1 {" + p.cls + "}",
		    nullptr);
		return;
	}
	if (p.stage == 1 && it && it->containedIn == id)
	{
		p.stage = 2, busy_[id] = kAssignCooldown;
		if (p.kind == Unpack)
			emit(id, "set_event this evt_autoprod_unpack -subject1 " + item + " -text1 {" + p.cls + "} -pos1 " + fmtPos(p.orderPos) + " -num1 " + (p.asBox ? "1" : "0"),
			    nullptr);
		else if (w_.scripts->object(p.dest))
			emit(id, "set_event this evt_autoprod_transferprod -subject1 " + std::to_string(p.dest) + " -subject2 " + item + " -text1 {" + p.cls + "}",
			    nullptr);
		return;
	}
	if (p.stage == 1)
	{ // przedmiot nie trafił do rąk (podniesienie się nie udało): jak porażka, żeby nie przydzielać go w kółko
		breathe_[id] = kFailBreathe;
		Reopen r{{0, 0, 0}, kReopenTicks};
		if (w_.getPos(p.item, r.pos)) reopen_.push_back(r);
	}
	dropPlan(id, p.kind == Unpack && p.stage < 2);
}

void ProdPlanner::sweep(int player)
{
	// Plany martwych krasnali albo przedmiotów znikają; budynki trzymane przez nieistniejących krasnali są wolne.
	for (auto it = plans_.begin(); it != plans_.end();)
	{
		int g = it->first;
		bool alive = w_.scripts->object(g) && w_.scripts->object(it->second.item);
		++it;
		if (!alive) dropPlan(g, true);
	}
	for (auto it = claim_.begin(); it != claim_.end();)
	{
		int place = it->first, g = it->second;
		++it;
		if (!w_.scripts->object(g)) releaseClaim(place);
	}
	std::vector<Job> jobs;
	collectDigJobs(player, jobs);
	collectHarvestJobs(player, jobs);
	collectPlaceJobs(player, jobs);
	maintainDigRequests(player);
	auto& pool = idle_[size_t(player)];
	for (auto it = pool.begin(); it != pool.end();)
	{
		script::ScriptObject* o = w_.scripts->object(*it);
		bool ok = o && o->alive && o->owner == player && !busy_.count(*it) && workTime(*it) && flag(*it, "prodautoschedule", true);
		it = ok ? std::next(it) : pool.erase(it);
	}
	if (pool.empty() || jobs.empty()) return;
	std::stable_sort(jobs.begin(), jobs.end(), [](const Job& a, const Job& b) { return a.need > b.need; });
	for (const Job& j : jobs)
	{
		if (jobOnCooldown(j.pos)) continue;
		if (float ip[3]; j.kind == Haul && w_.getPos(j.item, ip) && jobOnCooldown(ip)) continue; // nieudane podniesienie
		int best = 0;
		float bestD = INFINITY;
		for (int id : pool)
		{
			if (breathe_.count(id) || plans_.count(id) || !match(id, j)) continue;
			float d = matchCost(id, j, player);
			if (d < bestD) bestD = d, best = id;
		}
		if (!best || !reserve(j, best)) continue;
		assign(best, j, player);
		break; // jedno zadanie na przegląd (gra tak samo)
	}
}

} // namespace ow
