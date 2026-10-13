// Kopanie ze skryptów (z_dignwalk.tcl): get_material, dig_mark, is_dig_marked, dig_next, dig_apply, dig_resetid,
// inc_playerdigmarkid. Prawa za Zimennik/Diggles-Unity (MIT), Runtime/Tcl/Engine/DigglesTclEngine.cs i
// Docs/DIGGING_MECHANIC.md; świat dostarcza Host (WorldSim: lib/dig_terrain). 🔶 o ile nie sprawdzone w laboratorium.
#include "lib/script/commands.h"

#include <tcl.h>

#include <cmath>
#include <cstdlib>

namespace ow::script
{

namespace
{

// get_material poz: materiał pola (0 skała, 1 piasek, 2 ziemia, 3 otwarte); bez świata 1.
int cmdGetMaterial(Engine& e, Tcl_Interp* in, Args& a)
{
	float p[3];
	setResult(in, std::to_string(e.host_ && a.size() > 1 && parseVec(in, a[1], p) ? e.host_->material(p) : 1));
	return TCL_OK;
}

// dig_mark gracz x y tryb [głębokość]: tryb 0 jaskinia (głębokość 0..3: pełna … płytka), 1 tunel, 2 zdjęcie.
int cmdDigMark(Engine& e, Tcl_Interp* in, Args& a)
{
	if (e.host_ && a.size() > 4)
	{
		int owner = std::atoi(a[1].c_str()), mode = std::atoi(a[4].c_str());
		int x = int(std::lround(std::atof(a[2].c_str()))), y = int(std::lround(std::atof(a[3].c_str())));
		int cnt = a.size() > 5 ? std::max(0, std::min(3, std::atoi(a[5].c_str()))) : 0;
		if (owner >= 0 && owner < 7) e.host_->digMark(owner, x, y, mode, cnt);
	}
	setResult(in, "");
	return TCL_OK;
}

// is_dig_marked x1 y1 x2 y2: czy w prostokącie [x1, x2) × [y1, y2) jest znacznik gracza 0.
int cmdIsDigMarked(Engine& e, Tcl_Interp* in, Args& a)
{
	bool r = false;
	if (e.host_ && a.size() > 4)
		r = e.host_->isDigMarked(std::atoi(a[1].c_str()), std::atoi(a[2].c_str()), std::atoi(a[3].c_str()), std::atoi(a[4].c_str()));
	setResult(in, r ? "1" : "0");
	return TCL_OK;
}

// dig_next poz obj [próba] [tylko_id]: kolejny węzeł do kopania; brak = „-1 -1 -1”.
int cmdDigNext(Engine& e, Tcl_Interp* in, Args& a)
{
	float p[3], out[3];
	ScriptObject* o = a.size() > 2 ? e.object(e.resolveObj(in, a[2])) : nullptr;
	int tryN = a.size() > 3 ? std::max(0, std::atoi(a[3].c_str())) : 0;
	bool matchId = a.size() > 4 && truthy(a[4]);
	bool ok = e.host_ && a.size() > 2 && parseVec(in, a[1], p) &&
	    e.host_->digNext(p, o && o->owner >= 0 ? o->owner : 0, o ? o->digSetId : -1, tryN, matchId, out);
	setResult(in, ok ? vecStr(out) : "-1 -1 -1");
	return TCL_OK;
}

// dig_apply poz obj: jeden krok wycinania węzła round(x + 0,5), round(y + 0,5); 1 = coś wycięto. Id znacznika
// trafia do kopiącego (dig_next woli najpierw dokończyć tę samą żyłę).
int cmdDigApply(Engine& e, Tcl_Interp* in, Args& a)
{
	float p[3];
	ScriptObject* o = a.size() > 2 ? e.object(e.resolveObj(in, a[2])) : e.currentObject(in);
	bool ok = false;
	if (e.host_ && a.size() > 1 && parseVec(in, a[1], p))
	{
		int id = 0;
		int nx = int(std::floor(p[0] + 0.5f)), ny = int(std::floor(p[1] + 0.5f));
		ok = e.host_->digApply(nx, ny, o && o->owner >= 0 ? o->owner : 0, id);
		if (ok && o) o->digSetId = id;
	}
	setResult(in, ok ? "1" : "0");
	return TCL_OK;
}

// get_angle_num4 poz cpoz: kierunek od poz do cpoz w płaszczyźnie x/y, skwantowany do 0..3 (1 = +x, 2 = +y, 3 = −x,
// 0 = −y; obcięcie do zera jak cvttss2si w grze, za Diggles-Unity FUN_00646b10). dig_check patrzy nim, z której
// strony węzła sprawdzać wolne pola.
int cmdAngleNum4(Engine&, Tcl_Interp* in, Args& a)
{
	float p[3], c[3];
	int r = 0;
	if (a.size() > 2 && parseVec(in, a[1], p) && parseVec(in, a[2], c))
	{
		float ang = std::atan2(c[1] - p[1], c[0] - p[0]);
		int eax = int((ang - 2.3561945f + 31.415928f) / -1.5707964f);
		r = (63 - eax) & 3;
	}
	setResult(in, std::to_string(r));
	return TCL_OK;
}

int cmdDigResetId(Engine& e, Tcl_Interp* in, Args& a)
{
	if (ScriptObject* o = a.size() > 1 ? e.object(e.resolveObj(in, a[1])) : nullptr) o->digSetId = -1;
	setResult(in, "");
	return TCL_OK;
}

int cmdIncDigMarkId(Engine& e, Tcl_Interp* in, Args& a)
{
	if (e.host_) e.host_->incDigMarkId(a.size() > 1 ? std::atoi(a[1].c_str()) : 0);
	setResult(in, "");
	return TCL_OK;
}

std::vector<CmdEntry> build()
{
	return {
	    {"get_material", cmdGetMaterial},
	    {"dig_mark", cmdDigMark},
	    {"is_dig_marked", cmdIsDigMarked},
	    {"dig_next", cmdDigNext},
	    {"dig_apply", cmdDigApply},
	    {"dig_resetid", cmdDigResetId},
	    {"inc_playerdigmarkid", cmdIncDigMarkId},
	    {"get_angle_num4", cmdAngleNum4},
	};
}

} // namespace

const std::vector<CmdEntry>& digCommands()
{
	static const std::vector<CmdEntry> v = build();
	return v;
}

} // namespace ow::script
