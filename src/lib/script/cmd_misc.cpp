// Komendy „misc” bez sceny: wektory, listy, czas czytania tekstu, statystyki gracza, tematy rozmów krasnali
// (talkissue, smalltalk), rodzina (partner_info). Prawa za Zimennik/Diggles-Unity (MIT),
// Runtime/Tcl/Engine/DigglesTclEngine.cs (adresy FUN_… tam cytowane); 🔶 o ile nie sprawdzone w laboratorium.
#include "lib/script/commands.h"

#include <tcl.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace ow::script
{

namespace
{

ScriptObject* obj(Engine& e, Tcl_Interp* in, const Args& a, size_t i)
{
	return a.size() > i ? e.object(e.resolveObj(in, a[i])) : nullptr;
}

std::string joinList(const std::vector<std::string>& v)
{
	Tcl_Obj* l = Tcl_NewListObj(0, nullptr);
	Tcl_IncrRefCount(l);
	for (const auto& s : v) Tcl_ListObjAppendElement(nullptr, l, Tcl_NewStringObj(s.data(), int(s.size())));
	std::string r = Tcl_GetString(l);
	Tcl_DecrRefCount(l);
	return r;
}

// vector_abs v: długość wektora.
int cmdVectorAbs(Engine&, Tcl_Interp* in, Args& a)
{
	float v[3] = {0, 0, 0};
	if (a.size() > 1) parseVec(in, a[1], v);
	setResult(in, fmt(std::sqrt(double(v[0]) * v[0] + double(v[1]) * v[1] + double(v[2]) * v[2])));
	return TCL_OK;
}

// vector_random x y z: losowy wektor, każda składowa równomiernie w [−x, x] itd.
int cmdVectorRandom(Engine&, Tcl_Interp* in, Args& a)
{
	float v[3] = {0, 0, 0};
	for (size_t k = 0; k < 3 && k + 1 < a.size(); ++k)
	{
		double r = double(std::rand()) / (double(RAND_MAX) + 1.0);
		v[k] = float((2.0 * r - 1.0) * std::atof(a[k + 1].c_str()));
	}
	setResult(in, vecStr(v));
	return TCL_OK;
}

// lrem zmienna indeks: usuwa element listy w zmiennej wołającego (na miejscu), zwraca nową listę.
int cmdLrem(Engine&, Tcl_Interp* in, Args& a)
{
	if (a.size() < 3) return TCL_OK;
	const char* v = Tcl_GetVar(in, a[1].c_str(), TCL_LEAVE_ERR_MSG);
	if (!v) return TCL_ERROR;
	auto items = listItems(in, v);
	long i = std::atol(a[2].c_str());
	if (a[2] == "end") i = long(items.size()) - 1;
	if (i >= 0 && i < long(items.size())) items.erase(items.begin() + i);
	std::string r = joinList(items);
	Tcl_SetVar(in, a[1].c_str(), r.c_str(), 0);
	setResult(in, r);
	return TCL_OK;
}

// calc_text_time tekst: czas czytania w sekundach, 0,06 s na znak, co najmniej 0,5 (🔶 stała z Diggles-Unity).
int cmdCalcTextTime(Engine&, Tcl_Interp* in, Args& a)
{
	size_t chars = 0;
	if (a.size() > 1)
		for (unsigned char c : a[1]) chars += (c & 0xC0) != 0x80; // znaki UTF-8, nie bajty
	setResult(in, fmt(std::max(0.5, double(chars) * 0.06)));
	return TCL_OK;
}

// tasklist_list obj: cała kolejka zadań jako lista.
int cmdTasklistList(Engine& e, Tcl_Interp* in, Args& a)
{
	ScriptObject* o = obj(e, in, a, 1);
	setResult(in, o ? joinList(o->tasks) : "");
	return TCL_OK;
}

// state_getenablecnt obj: licznik state_enable/state_disable (stan działa przy > 0).
int cmdStateEnableCnt(Engine& e, Tcl_Interp* in, Args& a)
{
	ScriptObject* o = obj(e, in, a, 1);
	setResult(in, std::to_string(o ? o->stateCounter : 0));
	return TCL_OK;
}

int cmdLocalPlayer(Engine&, Tcl_Interp* in, Args&)
{
	setResult(in, "0"); // gra jednoosobowa: gracz 0
	return TCL_OK;
}

// is_storymgr: 1 w kampanii (świat z menedżerem fabuły), 0 w menu i grze swobodnej.
int cmdIsStoryMgr(Engine& e, Tcl_Interp* in, Args&)
{
	setResult(in, e.storyMgr ? "1" : "0");
	return TCL_OK;
}

bool isClass(const ScriptObject& o, const char* name)
{
	return o.cls && o.cls->name == name;
}

// gamestats co gracz [klasa]: liczniki kolonii gracza (numgnomes, numfemale, nummale, numbabies, numpregnant,
// numitems klasa) i atrybuty gracza (attribsum|attribmax|attribavg nazwa). 🔶 numbuiltprodclasses: 0, dopóki
// nie mamy stanu budowy.
int cmdGamestats(Engine& e, Tcl_Interp* in, Args& a)
{
	std::string sub = a.size() > 1 ? a[1] : "";
	int own = a.size() > 2 ? std::atoi(a[2].c_str()) : 0;
	auto count = [&](auto pred) {
		int n = 0;
		for (auto& [id, o] : e.objects_)
			if (o->alive && o->owner == own && pred(*o)) ++n;
		return n;
	};
	std::string r = "0";
	if (sub == "numgnomes") r = std::to_string(count([](const ScriptObject& o) { return isClass(o, "Zwerg"); }));
	else if (sub == "numfemale")
		r = std::to_string(count([](const ScriptObject& o) { return isClass(o, "Zwerg") && o.gender == "female"; }));
	else if (sub == "nummale")
		r = std::to_string(count([](const ScriptObject& o) { return isClass(o, "Zwerg") && o.gender == "male"; }));
	else if (sub == "numbabies") r = std::to_string(count([](const ScriptObject& o) { return isClass(o, "Baby"); }));
	else if (sub == "numpregnant")
		r = std::to_string(count([](const ScriptObject& o) {
			auto it = o.props.find("partner_preg");
			return it != o.props.end() && truthy(it->second);
		}));
	else if (sub == "numitems")
	{
		std::string cls = a.size() > 3 ? a[3] : "";
		r = std::to_string(count([&](const ScriptObject& o) { return o.cls && o.cls->name == cls; }));
	}
	else if (sub == "attribsum" || sub == "attribmax" || sub == "attribavg")
	{
		auto ow = e.ownerAttribs_.find(own);
		std::string name = a.size() > 3 ? a[3] : "";
		r = ow != e.ownerAttribs_.end() && ow->second.count(name) ? fmt(ow->second.at(name)) : "0";
	}
	setResult(in, r);
	return TCL_OK;
}

// talkissue obj add|set|get|sum|list|reduce|delete …: tematy do rozmowy krasnala (nazwa → waga 0..50).
// add: dodaje (ujemna zmiana poniżej 3 usuwa temat); reduce f: mnoży tylko tematy, których nazwa nie
// zaczyna się cyfrą (tematy fabularne są odporne).
int cmdTalkissue(Engine& e, Tcl_Interp* in, Args& a)
{
	ScriptObject* o = obj(e, in, a, 1);
	if (!o || a.size() < 3)
	{
		setResult(in, "0");
		return TCL_OK;
	}
	auto& led = o->talkIssues;
	auto clamp = [](double v) { return std::clamp(v, 0.0, 50.0); };
	auto f1 = [](double v) {
		std::string s = fmt(v);
		return s.find_first_of(".e") == std::string::npos ? s + ".0" : s;
	};
	const std::string& c = a[2];
	std::string r;
	if (c == "add" && a.size() > 4)
	{
		double v = std::atof(a[4].c_str()), nv = clamp((led.count(a[3]) ? led[a[3]] : 0.0) + v);
		if (v < 0 && nv < 3) led.erase(a[3]);
		else led[a[3]] = nv;
	}
	else if (c == "set" && a.size() > 4) led[a[3]] = clamp(std::atof(a[4].c_str()));
	else if (c == "get") r = a.size() > 3 && led.count(a[3]) ? f1(led[a[3]]) : "0.0";
	else if (c == "sum")
	{
		double s = 0;
		for (auto& [k, v] : led) s += v;
		r = f1(s);
	}
	else if (c == "list")
	{
		std::vector<std::string> keys;
		for (auto& [k, v] : led) keys.push_back(k);
		r = joinList(keys);
	}
	else if (c == "reduce" && a.size() > 3)
	{
		double f = std::atof(a[3].c_str());
		for (auto& [k, v] : led)
			if (!k.empty() && k[0] > '9') v *= f;
	}
	else if (c == "delete" && a.size() > 3) led.erase(a[3]);
	setResult(in, r);
	return TCL_OK;
}

// smalltalk add klucz tekst / smalltalk get klucz: teksty pogawędek wg tematu (get: lista tekstów).
int cmdSmalltalk(Engine& e, Tcl_Interp* in, Args& a)
{
	std::string r;
	if (a.size() > 3 && a[1] == "add") e.smalltalk_[a[2]].push_back(a[3]);
	else if (a.size() > 2 && a[1] == "get" && e.smalltalk_.count(a[2])) r = joinList(e.smalltalk_[a[2]]);
	setResult(in, r);
	return TCL_OK;
}

// partner_info setpartner|getpartner|setfather|getfather|setmother|getmother|setpregnancy|getpregnancy|
// addchild|getchildren|transfer obj [wartość]: rodzina krasnala (rozmnażanie).
int cmdPartnerInfo(Engine& e, Tcl_Interp* in, Args& a)
{
	ScriptObject* o = obj(e, in, a, 2);
	std::string sub = a.size() > 1 ? a[1] : "", r;
	if (o)
	{
		auto& p = o->props;
		if (sub.rfind("set", 0) == 0 && a.size() > 3)
		{
			std::string k = sub.substr(3);
			p["partner_" + (k == "pregnancy" ? std::string("preg") : k)] = a[3];
		}
		else if (sub.rfind("get", 0) == 0 && sub != "getchildren")
		{
			std::string k = sub.substr(3);
			k = "partner_" + (k == "pregnancy" ? std::string("preg") : k);
			r = p.count(k) ? p[k] : (sub == "getpregnancy" ? "0" : "");
		}
		else if (sub == "addchild" && a.size() > 3)
		{
			auto l = listItems(in, p["partner_children"]);
			l.push_back(a[3]);
			p["partner_children"] = joinList(l);
		}
		else if (sub == "getchildren") r = p.count("partner_children") ? p["partner_children"] : "";
		else if (sub == "transfer" && a.size() > 3)
			if (ScriptObject* d = obj(e, in, a, 3))
				for (auto& [k, v] : p)
					if (k.rfind("partner_", 0) == 0) d->props[k] = v;
	}
	setResult(in, r);
	return TCL_OK;
}

std::vector<CmdEntry> build()
{
	return {
	    {"vector_abs", cmdVectorAbs},
	    {"vector_random", cmdVectorRandom},
	    {"lrem", cmdLrem},
	    {"calc_text_time", cmdCalcTextTime},
	    {"tasklist_list", cmdTasklistList},
	    {"state_getenablecnt", cmdStateEnableCnt},
	    {"get_local_player", cmdLocalPlayer},
	    {"is_storymgr", cmdIsStoryMgr},
	    {"gamestats", cmdGamestats},
	    {"talkissue", cmdTalkissue},
	    {"smalltalk", cmdSmalltalk},
	    {"partner_info", cmdPartnerInfo},
	};
}

} // namespace

const std::vector<CmdEntry>& miscCommands()
{
	static const std::vector<CmdEntry> v = build();
	return v;
}

} // namespace ow::script
