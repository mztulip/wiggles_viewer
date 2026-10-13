// Właściwości obiektów i graczy: flagi set_X/get_X, płeć, typ, pozycja i obrót po osiach, prędkość, rasa i
// dyplomacja graczy. Prawa (wartości domyślne, kody) za Zimennik/Diggles-Unity (MIT),
// Runtime/Tcl/Engine/DigglesTclEngine.cs i DigglesTclContext.cs; 🔶 o ile nie sprawdzone w laboratorium.
#include "lib/script/commands.h"

#include <tcl.h>

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

// Flagi i wartości obiektu bez skutków poza samym odczytem (get_X zwraca to, co zapisało set_X, inaczej
// wartość domyślną). Skutki, które mamy w scenie, idą osobnymi komendami (set_visibility, set_pos …).
struct Prop
{
	const char* name;
	const char* def;
	bool boolean; // zapis jako 0/1 (wartość logiczna Tcl)
};
const Prop kProps[] = {
    {"physic", "0", true},
    {"hoverable", "0", true},
    {"selectable", "0", true},
    {"storable", "0", true},
    {"autolight", "0", true},
    {"viewinfog", "0", true},
    {"lock", "0", false},
    {"cloaked", "0", true},
    {"invulnerable", "0", true},
    {"undeletable", "0", true},
    {"objectcollision", "0", true},
    {"collision", "0", true},
    {"climbability", "0", false},
    {"forceipol", "0", true},
    {"instore", "0", true},
    {"attackinprogress", "0", true},
    {"logactions", "0", true},
    {"processactions", "1", true},
    {"obj_logicskip", "0", false},
    {"obj_logicfogskip", "0", false},
    {"snaptowall", "0", true},
    {"placesnapmode", "0", false},
    {"shield_class", "0", false},
    {"weapon_class", "0", false},
    {"rendermaterial", "", false},
    {"objvariation", "0", false},
    {"buildupstate", "0", false},
    {"user_groups", "", false},
    {"objinfo", "", false},
    {"boxed", "0", true},
    {"sequenceactive", "0", true},
    {"alternateanimdb", "0", true},
    {"activegameplay", "0", true},
    {"diedinfight", "0", true},
};

const Prop* findProp(const std::string& cmd)
{
	std::string n = cmd.substr(4); // po get_ / set_
	for (const auto& p : kProps)
		if (n == p.name) return &p;
	return nullptr;
}

int cmdProp(Engine& e, Tcl_Interp* in, Args& a)
{
	const Prop* p = findProp(a[0]);
	ScriptObject* o = obj(e, in, a);
	if (a[0][0] == 's')
	{
		if (o && a.size() > 2) o->props[p->name] = p->boolean ? (truthy(a[2]) ? "1" : "0") : a[2];
		// Zbudowany i spakowany wykluczają się: ustawienie jednego zdejmuje drugi (FUN_0067e1e0, FUN_0067dc80).
		// Spakowanie kończy też zlecenie pakowania (prod_pack 0; Diggles-Unity DigglesTclEntity.cs PackRequested).
		if (o && a.size() > 2 && truthy(a[2]) && std::string(p->name) == "buildupstate") o->props["boxed"] = "0";
		if (o && a.size() > 2 && truthy(a[2]) && std::string(p->name) == "boxed") o->props["buildupstate"] = "0", o->props["prod_pack"] = "0";
		setResult(in, "");
	}
	else
	{
		auto it = o ? o->props.find(p->name) : decltype(o->props.end()){};
		setResult(in, o && it != o->props.end() ? it->second : p->def);
	}
	return TCL_OK;
}

// set_objgender obj male|female|neutral, get_objgender obj (domyślnie neutral).
int cmdGender(Engine& e, Tcl_Interp* in, Args& a)
{
	ScriptObject* o = obj(e, in, a);
	if (a[0] == "set_objgender")
	{
		if (o && a.size() > 2) o->gender = a[2] == "male" || a[2] == "female" ? a[2] : "neutral";
		setResult(in, "");
	}
	else setResult(in, o ? o->gender : "neutral");
	return TCL_OK;
}

// auto_choose_gender obj: płeć wyrównująca krasnale gracza (więcej mężczyzn → kobieta), zwraca ją.
int cmdAutoGender(Engine& e, Tcl_Interp* in, Args& a)
{
	ScriptObject* self = obj(e, in, a);
	int owner = self ? self->owner : 0, balance = 0;
	for (auto& [id, o] : e.objects_)
	{
		if (o.get() == self || !o->alive || o->owner != owner || !o->cls || o->cls->type != "gnome") continue;
		balance += o->gender == "male" ? -1 : o->gender == "female" ? 1
		                                                            : 0;
	}
	std::string g = balance >= 0 ? "male" : "female";
	if (self) self->gender = g;
	setResult(in, g);
	return TCL_OK;
}

// get_objtype obj: typ klasy (none, dummy, gnome, monster, material, …).
int cmdObjType(Engine& e, Tcl_Interp* in, Args& a)
{
	ScriptObject* o = obj(e, in, a);
	setResult(in, o && o->cls ? o->cls->type : "");
	return TCL_OK;
}

// set_posx|y|z obj v: jedna współrzędna pozycji.
int cmdSetPosAxis(Engine& e, Tcl_Interp* in, Args& a)
{
	int id = a.size() > 2 ? e.resolveObj(in, a[1]) : 0;
	float p[3];
	if (id && e.host_ && e.host_->getPos(id, p))
	{
		p[a[0].back() - 'x'] = std::strtof(a[2].c_str(), nullptr);
		e.host_->setPos(id, p);
	}
	setResult(in, "");
	return TCL_OK;
}

// get_posbottom / set_posbottom: pozycja spodu przedmiotu (materiały, narzędzia). 🔶 Przesunięcie spodu
// (najwyższy punkt siatki w y) jeszcze nie liczone: spód = punkt obiektu.
int cmdPosBottom(Engine& e, Tcl_Interp* in, Args& a)
{
	int id = a.size() > 1 ? e.resolveObj(in, a[1]) : 0;
	float p[3] = {0, 0, 0};
	if (a[0] == "set_posbottom")
	{
		if (id && a.size() > 2 && parseVec(in, a[2], p) && e.host_) e.host_->setPos(id, p);
		setResult(in, "");
	}
	else
	{
		if (id && e.host_) e.host_->getPos(id, p);
		setResult(in, vecStr(p));
	}
	return TCL_OK;
}

// Obrót: oś Y idzie do sceny (get_roty/set_roty w rdzeniu), osie X i Z tylko zapamiętane (🔶 nie rysowane).
int cmdRotAxis(Engine& e, Tcl_Interp* in, Args& a)
{
	ScriptObject* o = obj(e, in, a);
	std::string key = std::string("rot") + a[0].back();
	if (a[0][0] == 's')
	{
		if (o && a.size() > 2) o->props[key] = fmt(std::atof(a[2].c_str()));
		setResult(in, "");
	}
	else setResult(in, o && o->props.count(key) ? o->props[key] : "0");
	return TCL_OK;
}

// get_rot obj → "x y z"; set_rot obj {x y z} albo set_rot obj x y z.
int cmdRotVec(Engine& e, Tcl_Interp* in, Args& a)
{
	ScriptObject* o = obj(e, in, a);
	int id = o ? o->id : 0;
	if (a[0] == "set_rot")
	{
		float r[3] = {0, 0, 0};
		bool ok = a.size() == 3 ? parseVec(in, a[2], r) : a.size() >= 5;
		if (a.size() >= 5)
			for (int k = 0; k < 3; ++k) r[k] = std::strtof(a[size_t(2 + k)].c_str(), nullptr);
		if (o && ok)
		{
			o->props["rotx"] = fmt(r[0]), o->props["rotz"] = fmt(r[2]);
			if (e.host_) e.host_->setRotY(id, r[1]);
		}
		setResult(in, "");
	}
	else
	{
		float r[3] = {0, 0, 0};
		if (o)
		{
			r[0] = o->props.count("rotx") ? std::strtof(o->props["rotx"].c_str(), nullptr) : 0.f;
			r[2] = o->props.count("rotz") ? std::strtof(o->props["rotz"].c_str(), nullptr) : 0.f;
			if (e.host_) r[1] = e.host_->getRotY(id);
		}
		setResult(in, vecStr(r));
	}
	return TCL_OK;
}

// set_vel / get_vel obj: prędkość obiektu (wektor, domyślnie 0). 🔶 Fizyka punktowa jeszcze jej nie używa.
int cmdVel(Engine& e, Tcl_Interp* in, Args& a)
{
	ScriptObject* o = obj(e, in, a);
	if (a[0] == "set_vel")
	{
		float v[3];
		if (o && a.size() > 2 && parseVec(in, a[2], v)) o->props["vel"] = vecStr(v);
		setResult(in, "");
	}
	else setResult(in, o && o->props.count("vel") ? o->props["vel"] : "0 0 0");
	return TCL_OK;
}

// set_ownerrace gracz rasa / get_ownerrace gracz (gracz 0..7, inaczej −1).
int cmdOwnerRace(Engine& e, Tcl_Interp* in, Args& a)
{
	int own = a.size() > 1 ? std::atoi(a[1].c_str()) : -1;
	bool valid = own >= 0 && own < 8;
	if (a[0] == "set_ownerrace")
	{
		if (valid && a.size() > 2) e.ownerRace_[size_t(own)] = std::atoi(a[2].c_str());
		setResult(in, "");
	}
	else setResult(in, std::to_string(valid ? e.ownerRace_[size_t(own)] : -1));
	return TCL_OK;
}

// get_objrace obj: rasa właściciela obiektu (−1 bez właściciela).
int cmdObjRace(Engine& e, Tcl_Interp* in, Args& a)
{
	ScriptObject* o = obj(e, in, a);
	int own = o ? o->owner : -1;
	setResult(in, o ? std::to_string(own >= 0 && own < 8 ? e.ownerRace_[size_t(own)] : -1) : "0");
	return TCL_OK;
}

// Dyplomacja: tabela 8×8 kodów own 0, ally 1, friend 2, neutral 3, enemy 4 (na starcie: sam ze sobą own,
// reszta neutral). get_diplomacy g1 g2: świat (−1) z każdym „enemy”. set_diplomacy g1 g2 stan zapisuje
// w jedną stronę; świata, pary samej ze sobą i stanu own nie zmienia.
int cmdDiplomacy(Engine& e, Tcl_Interp* in, Args& a)
{
	static const char* kNames[] = {"own", "ally", "friend", "neutral", "enemy"};
	int o1 = a.size() > 1 ? std::atoi(a[1].c_str()) : 0, o2 = a.size() > 2 ? std::atoi(a[2].c_str()) : 0;
	bool valid = o1 >= 0 && o1 < 8 && o2 >= 0 && o2 < 8;
	if (a[0] == "set_diplomacy")
	{
		if (valid && o1 != o2 && a.size() > 3)
			for (int c = 1; c < 5; ++c)
				if (a[3] == kNames[c]) e.diplomacy_[size_t(o1)][size_t(o2)] = c;
		setResult(in, "");
	}
	else if (o1 == -1 || o2 == -1) setResult(in, "enemy");
	else setResult(in, valid ? kNames[e.diplomacy_[size_t(o1)][size_t(o2)]] : "");
	return TCL_OK;
}

std::vector<CmdEntry> build()
{
	std::vector<CmdEntry> v;
	static std::vector<std::string> names; // wpisy trzymają wskaźniki na nazwy
	names.reserve(2 * std::size(kProps));
	for (const auto& p : kProps)
	{
		names.push_back(std::string("get_") + p.name);
		names.push_back(std::string("set_") + p.name);
	}
	for (const auto& n : names) v.push_back({n.c_str(), cmdProp});
	for (const char* n : {"set_objgender", "get_objgender"}) v.push_back({n, cmdGender});
	v.push_back({"auto_choose_gender", cmdAutoGender});
	v.push_back({"get_objtype", cmdObjType});
	for (const char* n : {"set_posx", "set_posy", "set_posz"}) v.push_back({n, cmdSetPosAxis});
	for (const char* n : {"get_posbottom", "set_posbottom"}) v.push_back({n, cmdPosBottom});
	for (const char* n : {"get_rotx", "get_rotz", "set_rotx", "set_rotz"}) v.push_back({n, cmdRotAxis});
	for (const char* n : {"get_rot", "set_rot"}) v.push_back({n, cmdRotVec});
	for (const char* n : {"get_vel", "set_vel"}) v.push_back({n, cmdVel});
	for (const char* n : {"get_ownerrace", "set_ownerrace"}) v.push_back({n, cmdOwnerRace});
	v.push_back({"get_objrace", cmdObjRace});
	for (const char* n : {"get_diplomacy", "set_diplomacy"}) v.push_back({n, cmdDiplomacy});
	return v;
}

} // namespace

const std::vector<CmdEntry>& objectPropCommands()
{
	static const std::vector<CmdEntry> v = build();
	return v;
}

} // namespace ow::script
