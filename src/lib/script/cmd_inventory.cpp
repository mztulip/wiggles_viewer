// Ekwipunek i zawieranie (inv_*, is_contained, global_inv_rem, is_wearing_pannier). Prawa za Zimennik/Diggles-Unity
// (MIT), Runtime/Tcl/Engine/DigglesTclEngine.cs (InvAdd FUN_0069bd30, InvRem FUN_0069c0f0, InvGetSize
// FUN_005cde10 tam cytowane); 🔶 o ile nie sprawdzone w laboratorium:
// - inv_add chowa przedmiot (niewidoczny, pozycja {0 0 0}, poza światem dla obj_query) i zwraca indeks albo −1
//   (przedmiot w cudzym ekwipunku, brak miejsca);
// - inv_rem wyjmuje przedmiot po indeksie albo odwołaniu, pokazuje go i stawia w podanym punkcie, domyślnie w
//   miejscu właściciela; zwraca odwołanie;
// - pojemność: krasnal 6 miejsc + kosze i taczki w ekwipunku (Holzkiepe 4, Grosse_Holzkiepe 8, Schubkarren 6,
//   Hamsterkarren 6), inne obiekty 99.
#include "lib/script/commands.h"
#include "lib/vfs.h"

#include <tcl.h>

#include <algorithm>
#include <cstdlib>

namespace ow::script
{

namespace
{

constexpr int kGnomeSlots = 6;

ScriptObject* obj(Engine& e, Tcl_Interp* in, const Args& a, size_t i)
{
	return a.size() > i ? e.object(e.resolveObj(in, a[i])) : nullptr;
}

int slotBonus(const ScriptObject* it)
{
	if (!it || !it->cls) return 0;
	const std::string& n = it->cls->name;
	return n == "Holzkiepe" ? 4 : n == "Grosse_Holzkiepe" ? 8
	    : n == "Schubkarren" || n == "Hamsterkarren"      ? 6
	                                                      : 0;
}

int capacity(Engine& e, const ScriptObject& o)
{
	if (!o.cls || o.cls->type != "gnome") return 99;
	int c = kGnomeSlots;
	for (int id : o.inventory) c += slotBonus(e.object(id));
	return c;
}

void show(Engine& e, ScriptObject& it, bool visible)
{
	it.visible = visible;
	if (e.host_) e.host_->setVisible(it.id, visible);
}

int cmdInvAdd(Engine& e, Tcl_Interp* in, Args& a)
{
	ScriptObject *o = obj(e, in, a, 1), *it = obj(e, in, a, 2);
	int r = -1;
	if (o && it && it != o)
	{
		auto at = std::find(o->inventory.begin(), o->inventory.end(), it->id);
		if (at != o->inventory.end()) r = int(at - o->inventory.begin());
		else if ((it->containedIn < 0 || it->containedIn == o->id) && int(o->inventory.size()) < capacity(e, *o))
		{
			o->inventory.push_back(it->id);
			it->containedIn = o->id;
			const float zero[3] = {0, 0, 0};
			if (e.host_) e.host_->setPos(it->id, zero);
			show(e, *it, false);
			r = int(o->inventory.size()) - 1;
		}
	}
	setResult(in, std::to_string(r));
	return TCL_OK;
}

// Zwolnienie przedmiotu z ekwipunku o (bez zmiany pozycji).
void release(Engine& e, ScriptObject& o, size_t idx)
{
	int id = o.inventory[idx];
	o.inventory.erase(o.inventory.begin() + long(idx));
	if (ScriptObject* it = e.object(id))
	{
		it->containedIn = -1;
		it->props.erase("instore");
		show(e, *it, true);
	}
}

int cmdInvRem(Engine& e, Tcl_Interp* in, Args& a)
{
	ScriptObject* o = obj(e, in, a, 1);
	std::string r;
	if (o && a.size() > 2)
	{
		long idx = -1;
		int ref = e.resolveObj(in, a[2]);
		auto at = std::find(o->inventory.begin(), o->inventory.end(), ref);
		if (ref && at != o->inventory.end()) idx = long(at - o->inventory.begin());
		else
		{
			char* end = nullptr;
			long n = std::strtol(a[2].c_str(), &end, 10);
			if (end && !*end && n >= 0 && n < long(o->inventory.size())) idx = n;
		}
		if (idx >= 0)
		{
			int id = o->inventory[size_t(idx)];
			release(e, *o, size_t(idx));
			float p[3] = {0, 0, 0};
			bool given = a.size() > 3 && parseVec(in, a[3], p);
			if ((given || (e.host_ && e.host_->getPos(o->id, p))) && e.host_) e.host_->setPos(id, p);
			r = std::to_string(id);
		}
	}
	setResult(in, r);
	return TCL_OK;
}

int cmdInvCheck(Engine& e, Tcl_Interp* in, Args& a)
{ // 🔶 jak w Diggles-Unity: tylko wolne miejsce, rodzaj przedmiotu pomijany
	ScriptObject* o = obj(e, in, a, 1);
	setResult(in, o && int(o->inventory.size()) < capacity(e, *o) ? "1" : "0");
	return TCL_OK;
}

int cmdInvCnt(Engine& e, Tcl_Interp* in, Args& a)
{
	ScriptObject* o = obj(e, in, a, 1);
	setResult(in, std::to_string(o ? o->inventory.size() : 0));
	return TCL_OK;
}

int cmdInvGetSize(Engine& e, Tcl_Interp* in, Args& a)
{
	ScriptObject* o = obj(e, in, a, 1);
	setResult(in, std::to_string(o ? capacity(e, *o) : 0));
	return TCL_OK;
}

// inv_find obj klasa: indeks pierwszego przedmiotu klasy (bez rozróżniania wielkości liter) albo −1.
int cmdInvFind(Engine& e, Tcl_Interp* in, Args& a)
{
	ScriptObject* o = obj(e, in, a, 1);
	int r = -1;
	if (o && a.size() > 2)
	{
		std::string want = Vfs::lower(a[2]);
		for (size_t k = 0; k < o->inventory.size() && r < 0; ++k)
		{
			ScriptObject* it = e.object(o->inventory[k]);
			if (it && it->cls && Vfs::lower(it->cls->name) == want) r = int(k);
		}
	}
	setResult(in, std::to_string(r));
	return TCL_OK;
}

int cmdInvFindObj(Engine& e, Tcl_Interp* in, Args& a)
{
	ScriptObject* o = obj(e, in, a, 1);
	int ref = a.size() > 2 ? e.resolveObj(in, a[2]) : 0, r = -1;
	if (o && ref)
	{
		auto at = std::find(o->inventory.begin(), o->inventory.end(), ref);
		if (at != o->inventory.end()) r = int(at - o->inventory.begin());
	}
	setResult(in, std::to_string(r));
	return TCL_OK;
}

int cmdInvGet(Engine& e, Tcl_Interp* in, Args& a)
{
	ScriptObject* o = obj(e, in, a, 1);
	long i = a.size() > 2 ? std::atol(a[2].c_str()) : -1;
	setResult(in, o && i >= 0 && i < long(o->inventory.size()) ? std::to_string(o->inventory[size_t(i)]) : "0");
	return TCL_OK;
}

int cmdInvList(Engine& e, Tcl_Interp* in, Args& a)
{
	ScriptObject* o = obj(e, in, a, 1);
	std::string r;
	if (o)
		for (int id : o->inventory) r += (r.empty() ? "" : " ") + std::to_string(id);
	setResult(in, r);
	return TCL_OK;
}

int cmdIsContained(Engine& e, Tcl_Interp* in, Args& a)
{
	ScriptObject* o = obj(e, in, a, 1);
	setResult(in, o && o->containedIn >= 0 ? "1" : "0");
	return TCL_OK;
}

// global_inv_rem lista: wyjmuje obiekty z każdego ekwipunku (pozycja bez zmian).
int cmdGlobalInvRem(Engine& e, Tcl_Interp* in, Args& a)
{
	if (a.size() > 1)
		for (const auto& w : listItems(in, a[1]))
			if (ScriptObject* it = e.object(e.resolveObj(in, w)); it && it->containedIn >= 0)
				if (ScriptObject* o = e.object(it->containedIn))
				{
					auto at = std::find(o->inventory.begin(), o->inventory.end(), it->id);
					if (at != o->inventory.end()) release(e, *o, size_t(at - o->inventory.begin()));
				}
	setResult(in, "");
	return TCL_OK;
}

// is_wearing_pannier obj: niesie kosz albo taczkę i ma więcej niż 6 przedmiotów (kosz jest w użyciu).
int cmdWearingPannier(Engine& e, Tcl_Interp* in, Args& a)
{
	ScriptObject* o = obj(e, in, a, 1);
	bool worn = false;
	if (o)
		for (int id : o->inventory)
		{
			ScriptObject* it = e.object(id);
			worn = worn || (it && it->cls && it->cls->type == "transport");
		}
	setResult(in, worn && o && int(o->inventory.size()) > kGnomeSlots ? "1" : "0");
	return TCL_OK;
}

std::vector<CmdEntry> build()
{
	return {
	    {"inv_add", cmdInvAdd},
	    {"inv_rem", cmdInvRem},
	    {"inv_check", cmdInvCheck},
	    {"inv_cnt", cmdInvCnt},
	    {"inv_cnt_raw", cmdInvCnt},
	    {"inv_getsize", cmdInvGetSize},
	    {"inv_find", cmdInvFind},
	    {"inv_find_obj", cmdInvFindObj},
	    {"inv_find_raw", cmdInvFindObj},
	    {"inv_get", cmdInvGet},
	    {"inv_get_raw", cmdInvGet},
	    {"inv_list", cmdInvList},
	    {"is_contained", cmdIsContained},
	    {"global_inv_rem", cmdGlobalInvRem},
	    {"is_wearing_pannier", cmdWearingPannier},
	};
}

} // namespace

const std::vector<CmdEntry>& inventoryCommands()
{
	static const std::vector<CmdEntry> v = build();
	return v;
}

} // namespace ow::script
