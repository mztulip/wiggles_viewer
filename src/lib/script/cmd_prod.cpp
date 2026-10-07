// Stan produkcji budynków (get/set_prod_*, sloty zamówień, postęp info_*_prod, miejsca gości prod_guest,
// stanowiska krasnala). Prawa i wartości domyślne za Zimennik/Diggles-Unity (MIT), Runtime/Tcl/Engine/DigglesTclEngine.cs
// (FUN_… gry tam cytowane); 🔶 o ile nie sprawdzone w laboratorium. Planisty zadań (kto co robi) jeszcze nie ma:
// te komendy tylko przechowują stan, który czytają skrypty (genericprod.tcl, z_work_*.tcl).
#include "lib/script/commands.h"

#include <tcl.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace ow::script
{

namespace
{

ScriptObject* obj(Engine& e, Tcl_Interp* in, const Args& a, size_t i = 1)
{
	return a.size() > i ? e.object(e.resolveObj(in, a[i])) : nullptr;
}

// Flagi produkcji: nazwa (po get_prod_/set_prod_), wartość domyślna.
struct Flag
{
	const char* name;
	const char* def;
};
const Flag kFlags[] = {
    {"buildup", "0"},       // zlecona budowa (jednorazowe)
    {"enabled", "1"},       // budynek działa (np. źródło energii włączone)
    {"materialneed", "1"},  // potrzebuje materiałów (farma: 0)
    {"schedule", "1"},      // przyjmuje zadania produkcji
    {"switchmode", "0"},    // zamówienia jako przełącznik wł./wył.
    {"exclusivemode", "0"}, // jedno aktywne zamówienie naraz
    {"pack", "0"},          // zlecone pakowanie (rozbiórka do skrzyni)
    {"unpack", "0"},        // zlecone rozpakowanie
    {"directevents", "0"},
    {"toolneed", "0"},
};

int cmdFlag(Engine& e, Tcl_Interp* in, Args& a)
{
	std::string n = a[0].substr(9); // po get_prod_ / set_prod_
	const Flag* f = nullptr;
	for (const auto& k : kFlags)
		if (n == k.name) f = &k;
	ScriptObject* o = obj(e, in, a);
	std::string key = "prod_" + n;
	if (a[0][0] == 's')
	{
		if (o && a.size() > 2) o->props[key] = truthy(a[2]) ? "1" : "0";
		setResult(in, "");
	}
	else setResult(in, o && o->props.count(key) ? o->props[key] : f->def);
	return TCL_OK;
}

// set_prod_ownerstrength / get_prod_ownerstrength: siła właściciela przy przejmowaniu (lina), domyślnie 1, bez
// przycinania (skrypt sam pilnuje 1,0).
int cmdOwnerStrength(Engine& e, Tcl_Interp* in, Args& a)
{
	ScriptObject* o = obj(e, in, a);
	if (a[0][0] == 's')
	{
		if (o && a.size() > 2) o->props["prod_ownerstrength"] = fmt(std::atof(a[2].c_str()));
		setResult(in, "");
	}
	else setResult(in, o && o->props.count("prod_ownerstrength") ? o->props["prod_ownerstrength"] : "1");
	return TCL_OK;
}

// set_prod_slot_cnt bud przedmiot n / get_prod_slot_cnt bud przedmiot: zamówiona liczba (10 = bez końca).
int cmdSlotCnt(Engine& e, Tcl_Interp* in, Args& a)
{
	ScriptObject* o = obj(e, in, a);
	if (a[0][0] == 's')
	{
		if (o && a.size() > 3) o->prodSlots[a[2]] = std::atof(a[3].c_str());
		setResult(in, "");
	}
	else
	{
		auto it = o && a.size() > 2 ? o->prodSlots.find(a[2]) : decltype(o->prodSlots.end()){};
		setResult(in, o && a.size() > 2 && it != o->prodSlots.end() ? fmt(it->second) : "0");
	}
	return TCL_OK;
}

// Katalog przedmiotów budynku: metoda statyczna klasy prod_items (tttitems_<Klasa> z techtreetunes.tcl).
std::vector<std::string> catalog(Engine& e, ScriptObject* o)
{
	std::string r;
	if (o && o->cls && e.eval("call_method_static " + o->cls->name + " prod_items", &r)) return listItems(nullptr, r);
	return {};
}

int cmdSlotList(Engine& e, Tcl_Interp* in, Args& a)
{
	std::string r;
	if (ScriptObject* o = obj(e, in, a); o && o->cls) e.eval("call_method_static " + o->cls->name + " prod_items", &r);
	setResult(in, r);
	return TCL_OK;
}

// get_prod_slot_invented bud przedmiot: gracz-właściciel ma atrybut Bp<przedmiot> ≠ 0.
int cmdSlotInvented(Engine& e, Tcl_Interp* in, Args& a)
{
	ScriptObject* o = obj(e, in, a);
	std::string r;
	bool ok = o && a.size() > 2 && e.eval("get_owner_attrib " + std::to_string(o->owner) + " Bp" + a[2], &r);
	setResult(in, ok && std::atof(r.c_str()) != 0 ? "1" : "0");
	return TCL_OK;
}

// get_prod_slot_inventable bud przedmiot: przedmiot jest w katalogu budynku (🔶 bez epok drzewa technologii).
int cmdSlotInventable(Engine& e, Tcl_Interp* in, Args& a)
{
	ScriptObject* o = obj(e, in, a);
	auto c = catalog(e, o);
	setResult(in, a.size() > 2 && std::find(c.begin(), c.end(), a[2]) != c.end() ? "1" : "0");
	return TCL_OK;
}

// get_prod_slot_buildable: 🔶 zawsze 1, dopóki nie ma planisty z zasobami świata (czerwone ikony).
int cmdSlotBuildable(Engine&, Tcl_Interp* in, Args&)
{
	setResult(in, "1");
	return TCL_OK;
}

// info_start_prod bud przedmiot / info_progress_prod bud bieżący maks / info_end_prod bud: postęp produkcji;
// info_end_prod zmniejsza zamówienie bieżącego przedmiotu (current_itemtype budynku), chyba że to 10 (bez końca).
int cmdInfoProd(Engine& e, Tcl_Interp* in, Args& a)
{
	ScriptObject* o = obj(e, in, a);
	if (o && a[0] == "info_start_prod" && a.size() > 2)
	{
		std::string item = a[2].rfind("Bp", 0) == 0 ? a[2].substr(2) : a[2];
		o->props["prod_item"] = item, o->props["prod_cur"] = "0", o->props["prod_max"] = "0";
	}
	else if (o && a[0] == "info_progress_prod" && a.size() > 3)
		o->props["prod_cur"] = std::to_string(std::lround(std::atof(a[2].c_str()))),
		o->props["prod_max"] = std::to_string(std::lround(std::atof(a[3].c_str())));
	else if (o && a[0] == "info_end_prod")
	{
		const char* item = o->interp ? Tcl_GetVar(o->interp, "current_itemtype", TCL_GLOBAL_ONLY) : nullptr;
		if (item && *item)
			if (auto it = o->prodSlots.find(item); it != o->prodSlots.end() && it->second > 0 && it->second < 10) it->second -= 1;
		o->props["prod_item"] = "", o->props["prod_cur"] = "0", o->props["prod_max"] = "0";
	}
	setResult(in, "");
	return TCL_OK;
}

// set_prodautoschedule obj [0|1] / get_prodautoschedule obj: automatyczne przydzielanie pracy, domyślnie włączone
// (bez argumentu albo nieczytelny: wyłączone).
int cmdAutoSchedule(Engine& e, Tcl_Interp* in, Args& a)
{
	ScriptObject* o = obj(e, in, a);
	if (a[0][0] == 's')
	{
		if (o) o->props["prodautoschedule"] = a.size() > 2 && truthy(a[2]) ? "1" : "0";
		setResult(in, "");
	}
	else setResult(in, o ? (o->props.count("prodautoschedule") ? o->props["prodautoschedule"] : "1") : "0");
	return TCL_OK;
}

int cmdAllocLock(Engine& e, Tcl_Interp* in, Args& a)
{
	ScriptObject* o = obj(e, in, a);
	if (a[0][0] == 's')
	{
		if (o && a.size() > 2) o->props["prodalloclock"] = truthy(a[2]) ? "1" : "0";
		setResult(in, "");
	}
	else setResult(in, o && o->props.count("prodalloclock") ? o->props["prodalloclock"] : "0");
	return TCL_OK;
}

// prod_gnome_last_workplace krasnal bud / prod_gnome_get_last_workplace krasnal: ostatnie stanowisko.
// prod_gnome_preferred_workplace krasnal [bud]: ulubione stanowisko; ten sam budynek drugi raz wyłącza, brak czyści.
int cmdWorkplace(Engine& e, Tcl_Interp* in, Args& a)
{
	ScriptObject* g = obj(e, in, a);
	std::string r;
	if (g)
	{
		if (a[0] == "prod_gnome_last_workplace" && a.size() > 2) g->props["last_workplace"] = a[2];
		else if (a[0] == "prod_gnome_get_last_workplace") r = g->props.count("last_workplace") ? g->props["last_workplace"] : "0";
		else if (a[0] == "prod_gnome_preferred_workplace")
		{
			int t = a.size() == 3 && e.object(e.resolveObj(in, a[2])) ? e.resolveObj(in, a[2]) : -1;
			int cur = g->props.count("preferred_workplace") ? std::atoi(g->props["preferred_workplace"].c_str()) : -1;
			g->props["preferred_workplace"] = std::to_string(cur == t ? -1 : t);
		}
		else if (a[0] == "prod_gnome_get_preferred_workplace")
		{
			int cur = g->props.count("preferred_workplace") ? std::atoi(g->props["preferred_workplace"].c_str()) : -1;
			r = std::to_string(cur > 0 && e.object(cur) && e.object(cur)->alive ? cur : 0);
		}
	}
	else if (a[0].find("get") != std::string::npos) r = "0";
	setResult(in, r);
	return TCL_OK;
}

// prod_guest podkomenda bud …: 16 miejsc dla gości (stołówka, łóżka): linki do punktów (addlink, setlink, getlink),
// goście (guestfree, guestget, guestset, guestremove) i ich zamówienia (addorder, remorder, resetorder, getorder,
// nextorder).
int cmdGuest(Engine& e, Tcl_Interp* in, Args& a)
{
	std::string sub = a.size() > 1 ? a[1] : "";
	ScriptObject* b = obj(e, in, a, 2);
	if (!b)
	{
		setResult(in, "0");
		return TCL_OK;
	}
	auto& L = b->guestLinks;
	auto& G = b->guests;
	auto& O = b->guestOrders;
	int s = a.size() > 3 ? std::atoi(a[3].c_str()) : 0;
	bool ok = s >= 0 && s < 16;
	std::string r;
	if (sub == "addlink")
	{
		for (int k = 0; k < 16; ++k)
			if (L[size_t(k)] == -1)
			{
				L[size_t(k)] = s;
				break;
			}
	}
	else if (sub == "setlink")
	{
		if (ok) L[size_t(s)] = a.size() > 4 ? std::atoi(a[4].c_str()) : -1;
	}
	else if (sub == "getlink") r = ok ? std::to_string(L[size_t(s)]) : "-1";
	else if (sub == "guestfree")
	{
		r = "-1";
		for (int k = 0; k < 16; ++k)
		{
			if (L[size_t(k)] == -1) continue;
			if (G[size_t(k)] && !e.object(G[size_t(k)])) G[size_t(k)] = 0, O[size_t(k)] = 0; // gość już nie istnieje
			if (!G[size_t(k)])
			{
				r = std::to_string(k);
				break;
			}
		}
	}
	else if (sub == "guestget") r = ok ? std::to_string(G[size_t(s)]) : "0";
	else if (sub == "guestset")
	{
		int g = a.size() > 4 ? e.resolveObj(in, a[4]) : 0;
		if (!g && a.size() > 4) g = std::atoi(a[4].c_str());
		if (ok)
		{
			G[size_t(s)] = g;
			if (!g) O[size_t(s)] = 0;
		}
	}
	else if (sub == "guestremove")
	{
		int g = a.size() > 3 ? e.resolveObj(in, a[3]) : 0;
		r = "1";
		for (int k = 0; k < 16; ++k)
			if (g && G[size_t(k)] == g)
			{
				G[size_t(k)] = 0, O[size_t(k)] = 0, r = std::to_string(k);
				break;
			}
	}
	else if (sub == "addorder")
	{
		if (ok) O[size_t(s)] += 1;
	}
	else if (sub == "remorder")
	{
		if (ok) O[size_t(s)] = std::max(0.0, O[size_t(s)] - 1);
	}
	else if (sub == "resetorder")
	{
		if (ok) O[size_t(s)] = 0;
	}
	else if (sub == "getorder") r = ok ? std::to_string(int(O[size_t(s)])) : "0";
	else if (sub == "nextorder")
	{
		int best = -1;
		double bv = 0;
		for (int k = 0; k < 16; ++k)
			if (G[size_t(k)] && O[size_t(k)] > bv) bv = O[size_t(k)], best = k;
		r = std::to_string(best);
	}
	else r = "0";
	setResult(in, r);
	return TCL_OK;
}

int cmdOne(Engine&, Tcl_Interp* in, Args&)
{ // prod_valid, prod_assignworker: 🔶 jak w Diggles-Unity zawsze 1
	setResult(in, "1");
	return TCL_OK;
}

std::vector<CmdEntry> build()
{
	std::vector<CmdEntry> v;
	static std::vector<std::string> names;
	names.reserve(2 * std::size(kFlags));
	for (const auto& f : kFlags) names.push_back(std::string("get_prod_") + f.name), names.push_back(std::string("set_prod_") + f.name);
	for (const auto& n : names) v.push_back({n.c_str(), cmdFlag});
	for (const char* n : {"get_prod_ownerstrength", "set_prod_ownerstrength"}) v.push_back({n, cmdOwnerStrength});
	for (const char* n : {"get_prod_slot_cnt", "set_prod_slot_cnt"}) v.push_back({n, cmdSlotCnt});
	v.push_back({"get_prod_slot_list", cmdSlotList});
	v.push_back({"get_prod_slot_invented", cmdSlotInvented});
	v.push_back({"get_prod_slot_inventable", cmdSlotInventable});
	v.push_back({"get_prod_slot_buildable", cmdSlotBuildable});
	for (const char* n : {"info_start_prod", "info_progress_prod", "info_end_prod"}) v.push_back({n, cmdInfoProd});
	for (const char* n : {"get_prodautoschedule", "set_prodautoschedule"}) v.push_back({n, cmdAutoSchedule});
	for (const char* n : {"get_prodalloclock", "set_prodalloclock"}) v.push_back({n, cmdAllocLock});
	for (const char* n : {"prod_gnome_last_workplace", "prod_gnome_get_last_workplace", "prod_gnome_preferred_workplace",
	         "prod_gnome_get_preferred_workplace"})
		v.push_back({n, cmdWorkplace});
	v.push_back({"prod_guest", cmdGuest});
	for (const char* n : {"prod_valid", "prod_assignworker"}) v.push_back({n, cmdOne});
	return v;
}

} // namespace

const std::vector<CmdEntry>& prodCommands()
{
	static const std::vector<CmdEntry> v = build();
	return v;
}

} // namespace ow::script
