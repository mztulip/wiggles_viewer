// Wyzwalacze obiektów (trigger …) i tryb przerywnika (set_sequence). Prawa za Zimennik/Diggles-Unity (MIT),
// Runtime/Tcl/Engine/TriggerService.cs (odczyt gry: Trigger.cpp, FUN_0060e920 komenda, FUN_0060df40 skan,
// FUN_0060da90 wartości domyślne); 🔶 o ile nie sprawdzone w laboratorium. Opis: docs/silnik/11_obiekty_tcl_i_petla.md §11.4a.
#include "lib/script/commands.h"
#include "lib/vfs.h"

#include <tcl.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace ow::script
{

// Maska słowa właściciela (any, own, other, world, player, enemy, friend, ally, neutral, allplayers, numer gracza):
// bity 0..7 gracze, 0x10000 świat, 0x20000 „ten sam gracz”, 0x40000 „inny gracz”. Słowa relacji biorą wiersz
// tabeli dyplomacji gracza pytającego.
static uint32_t ownerWordMask(Engine& e, const std::string& w, int self)
{
	auto rel = [&](int r) {
		if (self < 0 || self > 7) return 0xFFFFFFFFu;
		uint32_t m = 0;
		for (int i = 0; i < 8; ++i)
			if (e.diplomacy_[size_t(self)][size_t(i)] == r) m |= 1u << i;
		return m;
	};
	bool own = self >= 0 && self < 8;
	if (w == "world") return 0x10000;
	if (w == "player") return 0x20000;
	if (w == "enemy") return rel(4) | 0x10000;
	if (w == "friend") return rel(2);
	if (w == "ally") return rel(1);
	if (w == "neutral") return rel(3);
	if (w == "any") return 0xFFFFFFFF;
	if (w == "allplayers") return 0xFF;
	if (w == "own") return own ? 1u << self : 0u;
	if (w == "other") return own ? ((0xFFu & ~(1u << self)) | 0x10000u) : 0xFFu;
	char* end = nullptr;
	long n = std::strtol(w.c_str(), &end, 10);
	if (end && !*end && !w.empty()) return n == -1 ? 0x10000u : n >= 0 && n < 32 ? 1u << n
		                                                                         : 0u;
	return 0;
}

bool Engine::ownerMatches(int owner, const std::string& spec, int selfOwner, Tcl_Interp* in)
{
	auto words = listItems(in, spec);
	if (words.empty()) return true;
	uint32_t mask = 0;
	for (const auto& w : words) mask |= ownerWordMask(*this, w, selfOwner);
	uint32_t bits = owner < 0 ? 0x10000u : ((owner == selfOwner ? 0x20000u : 0x40000u) | (owner < 32 ? 1u << owner : 0u));
	return (bits & mask) != 0;
}

namespace
{

enum Kind
{
	Dead,
	SingleTimer,
	RepeatTimer,
	AnyObject,
	UniqueObject,
	Callback
};
enum Exec
{
	Never = 1,
	OnActivate,
	OnInactivate,
	WhenActive,
	WhenInactive,
	Always
};

TriggerRec* findRec(Engine& e, int owner)
{
	for (auto& r : e.triggers_)
		if (r.kind != Dead && r.owner == owner) return &r;
	return nullptr;
}

int fail(Tcl_Interp* in, const char* msg)
{
	setResult(in, msg);
	return TCL_ERROR;
}

int cmdTrigger(Engine& e, Tcl_Interp* in, Args& a)
{
	if (a.size() < 3) return fail(in, "wrong # args");
	const std::string& verb = a[1];
	int owner = e.resolveObj(in, a[2]);
	if (verb == "create")
	{
		if (a.size() != 5) return fail(in, "wrong # args");
		static const char* kNames[] = {"", "single_timer", "repeat_timer", "any_object", "unique_object", "callback"};
		int kind = 0;
		for (int k = 1; k <= 5; ++k)
			if (a[3] == kNames[k]) kind = k;
		if (!kind) return fail(in, "invalid trigger type");
		if (TriggerRec* old = findRec(e, owner)) old->kind = Dead; // obiekt ma najwyżej jeden wyzwalacz
		TriggerRec r;
		r.kind = kind, r.owner = owner, r.fire = a[4];
		r.lastCheck = e.time() - 4.9; // pierwsze sprawdzenie od razu
		size_t slot = 0;
		while (slot < e.triggers_.size() && e.triggers_[slot].kind != Dead) ++slot; // pierwsze wolne miejsce
		if (slot == e.triggers_.size()) e.triggers_.push_back(r);
		else e.triggers_[slot] = r;
		setResult(in, std::to_string(slot));
		return TCL_OK;
	}
	if (verb == "delete")
	{
		if (TriggerRec* r = findRec(e, owner)) r->kind = Dead;
		setResult(in, "");
		return TCL_OK;
	}
	TriggerRec* r = findRec(e, owner);
	if (!r) return fail(in, "object has no trigger");
	std::string v = a.size() > 3 ? a[3] : "", res;
	double d = std::atof(v.c_str());
	if (verb == "set_execution")
	{ // „when_inavtive”: literówka gry, zachowana
		static const std::pair<const char*, int> kModes[] = {{"never", Never}, {"on_activate", OnActivate}, {"on_inactivate", OnInactivate},
		    {"when_active", WhenActive}, {"when_inavtive", WhenInactive}, {"always", Always}};
		int m = 0;
		for (auto& [n, k] : kModes)
			if (v == n) m = k;
		if (!m) return fail(in, "invalid trigger execution");
		r->exec = m;
	}
	else if (verb == "set_timer")
	{
		if (r->checkTimer > 1.0) r->checkTimer = 1.0;
		r->checkIdle = r->checkTimer;
		r->fireTime = e.time() + d, r->interval = d;
	}
	else if (verb == "set_checktimer") r->checkTimer = d;
	else if (verb == "set_target_range") r->range = float(d);
	else if (verb == "set_target_ref")
	{
		int t = e.resolveObj(in, v);
		if (!e.object(t)) return fail(in, "invalid object");
		r->target = t;
	}
	else if (verb == "set_target_type") r->types = listItems(in, v);
	else if (verb == "set_target_class")
	{
		if (!e.findClass(v)) return fail(in, "invalid class");
		r->cls = Vfs::lower(v);
	}
	else if (verb == "set_target_count") r->count = int(d);
	else if (verb == "set_target_owner") r->ownerSpec = v, r->hasOwnerSpec = true;
	else if (verb == "set_callback") r->callback = v;
	else if (verb == "set_active") r->active = true;
	else if (verb == "set_inactive") r->active = false;
	else if (verb == "get_actuators")
	{
		Tcl_Obj* l = Tcl_NewListObj(0, nullptr);
		for (int id : r->actuators) Tcl_ListObjAppendElement(in, l, Tcl_NewIntObj(id));
		Tcl_SetObjResult(in, l);
		return TCL_OK;
	}
	else return fail(in, "unknown sub-command");
	setResult(in, res);
	return TCL_OK;
}

// set_sequence 0|1: przerywnik (wstrzymuje wyzwalacze). 🔶 Pozostałe skutki przerywnika jeszcze nie.
int cmdSetSequence(Engine& e, Tcl_Interp* in, Args& a)
{
	e.sequenceActive = a.size() > 1 && truthy(a[1]);
	setResult(in, "");
	return TCL_OK;
}

std::vector<CmdEntry> build()
{
	return {{"trigger", cmdTrigger}, {"set_sequence", cmdSetSequence}};
}

} // namespace

const std::vector<CmdEntry>& triggerCommands()
{
	static const std::vector<CmdEntry> v = build();
	return v;
}

// Wykonanie skryptu wyzwalacza w interpreterze jego obiektu; obiekt usunięty = wyzwalacz martwy.
static void fire(Engine& e, TriggerRec& r)
{
	ScriptObject* o = e.object(r.owner);
	if (!o || !o->alive)
	{
		r.kind = Dead;
		return;
	}
	if (o->interp) e.runIn(*o, r.fire, "trigger");
	else e.eval(r.fire);
}

static void edge(Engine& e, TriggerRec& r, bool on)
{
	if (r.active == on) return;
	r.active = on;
	if (r.exec == (on ? OnActivate : OnInactivate)) fire(e, r);
}

// Skan co turę logiki (FUN_0060df40): każdy rekord sprawdzany co swój interwał (domyślnie 3 s, 0,5 s, gdy w pobliżu
// są kandydaci); w przerywniku nic.
void Engine::scanTriggers()
{
	if (sequenceActive) return;
	double now = time_;
	size_t n = triggers_.size(); // zapamiętane przed pętlą: wyzwalacze utworzone w trakcie czekają do następnej tury
	for (size_t i = 0; i < n; ++i)
	{
		TriggerRec* r = &triggers_[i];
		if (r->kind == Dead || now - r->lastCheck < r->checkTimer) continue;
		r->lastCheck = now;
		float op[3] = {0, 0, 0};
		ScriptObject* o = object(r->owner);
		bool ownerOk = o && o->alive && host_ && host_->getPos(r->owner, op);
		switch (r->kind)
		{
		case SingleTimer:
			if (r->fireTime <= now && !r->active) edge(*this, *r, true);
			break;
		case RepeatTimer:
			if (now < r->fireTime) edge(*this, *r, false);
			else
			{
				edge(*this, *r, true);
				r->fireTime += r->interval;
			}
			break;
		case AnyObject:
		{
			if (!ownerOk)
			{
				r->kind = Dead;
				break;
			}
			std::vector<int> cand;
			for (auto& [id, ob] : objects_)
			{
				if (id == r->owner || !ob->alive || ob->containedIn >= 0 || !ob->cls) continue;
				if (!r->cls.empty() && Vfs::lower(ob->cls->name) != r->cls) continue;
				if (!r->types.empty() && std::find(r->types.begin(), r->types.end(), ob->cls->type) == r->types.end()) continue;
				if (r->hasOwnerSpec && !ownerMatches(ob->owner, r->ownerSpec, o->owner, main_)) continue;
				cand.push_back(id);
			}
			bool found = false;
			if (r->count <= int(cand.size()))
			{
				r->actuators.clear();
				for (int id : cand)
				{
					float p[3];
					if (!host_->getPos(id, p)) continue;
					float dx = p[0] - op[0], dy = p[1] - op[1], dz = p[2] - op[2];
					if (dx * dx + dy * dy + dz * dz <= r->range * r->range) r->actuators.push_back(id), found = true;
				}
			}
			if (found) edge(*this, *r, true);
			else r->checkTimer = cand.empty() ? r->checkIdle : r->checkNear;
			break;
		}
		case UniqueObject:
		{
			float p[3];
			if (!ownerOk || !object(r->target) || !host_->getPos(r->target, p))
			{
				r->kind = Dead;
				break;
			}
			float dx = p[0] - op[0], dy = p[1] - op[1], dz = p[2] - op[2], d2 = dx * dx + dy * dy + dz * dz;
			if (d2 > r->range * r->range)
				r->checkTimer = d2 <= (r->range + 5.f) * (r->range + 5.f) ? r->checkNear : r->checkIdle;
			else
			{
				r->actuators = {r->target};
				edge(*this, *r, true);
			}
			break;
		}
		case Callback:
		{
			ScriptObject* t = object(r->target != -1 ? r->target : r->owner);
			if (!t || !t->alive)
			{
				r->kind = Dead;
				break;
			}
			if (r->callback.empty()) break;
			Tcl_Interp* ti = t->interp ? t->interp : main_;
			int b = 0;
			if (Tcl_EvalEx(ti, r->callback.c_str(), -1, TCL_EVAL_GLOBAL) == TCL_OK &&
			    Tcl_GetBooleanFromObj(ti, Tcl_GetObjResult(ti), &b) == TCL_OK)
				edge(*this, *r, b != 0);
			break;
		}
		default:
			break;
		}
		r = &triggers_[i]; // skrypty mogły powiększyć tablicę
		if (r->kind == Dead) continue;
		if (r->exec == Always) fire(*this, *r);
		r = &triggers_[i];
		if (r->kind == Dead) continue;
		if (r->active && r->exec == WhenActive) fire(*this, *r);
		else if (!r->active && r->exec == WhenInactive) fire(*this, *r);
	}
	while (!triggers_.empty() && triggers_.back().kind == Dead) triggers_.pop_back();
}

} // namespace ow::script
