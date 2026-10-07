// Zaznaczenie gracza i kamera: selection, is_selected, get/set_selectedobject, get_view, set_view, set_viewpos,
// set_view_begin. Prawa zaznaczenia za Zimennik/Diggles-Unity (MIT), Runtime/Tcl/Engine/SelectionSet.cs
// (FUN_005e32b0, FUN_005e3350 tam cytowane); 🔶 o ile nie sprawdzone w laboratorium.
#include "lib/script/commands.h"

#include <tcl.h>

#include <algorithm>
#include <cstdlib>

namespace ow::script
{

bool Engine::canSelect(int id)
{
	ScriptObject* o = object(id);
	if (!o || !o->alive || o->owner != 0) return false; // tylko obiekty gracza (gracz lokalny 0)
	if (auto it = o->props.find("selectable"); it != o->props.end() && !truthy(it->second)) return false; // jawne set_selectable 0
	if (selection_.empty()) return true;
	// Do niepustego zaznaczenia dołączają tylko krasnale i dzieci.
	return o->cls && (o->cls->type == "gnome" || o->cls->type == "baby");
}

bool Engine::selectInclude(int id)
{
	bool in = std::count(selection_.begin(), selection_.end(), id) > 0;
	if (in || !canSelect(id)) return in;
	selection_.push_back(id);
	return true;
}

void Engine::selectOnly(int id)
{
	selection_.clear();
	if (id > 0) selection_.push_back(id);
}

void Engine::clickSelect(int id, bool additive)
{ // kliknięcie: bez obiektu czyści; bez Shift albo gdy obiekt nie może dołączyć: najpierw czyści
	if (id <= 0)
	{
		selection_.clear();
		return;
	}
	if (!canSelect(id) || !additive) selection_.clear();
	selectInclude(id);
}

namespace
{

int cmdSelection(Engine& e, Tcl_Interp* in, Args& a)
{
	std::string sub = a.size() > 1 ? a[1] : "", r;
	int id = a.size() > 2 ? e.resolveObj(in, a[2]) : 0;
	auto& s = e.selection_;
	if (sub == "clear") s.clear();
	else if (sub == "check") r = std::count(s.begin(), s.end(), id) ? "1" : "0";
	else if (sub == "caninclude") r = e.canSelect(id) ? "1" : "0";
	else if (sub == "include") e.selectInclude(id);
	else if (sub == "exclude") s.erase(std::remove(s.begin(), s.end(), id), s.end());
	setResult(in, r);
	return TCL_OK;
}

int cmdIsSelected(Engine& e, Tcl_Interp* in, Args& a)
{
	int id = a.size() > 1 ? e.resolveObj(in, a[1]) : 0;
	setResult(in, id && std::count(e.selection_.begin(), e.selection_.end(), id) ? "1" : "0");
	return TCL_OK;
}

// get_selectedobject: pierwszy zaznaczony (0 = brak); set_selectedobject [obj]: zaznaczenie tylko jego
// (bez sprawdzania praw, jak programowe zaznaczenie w grze), bez argumentu czyści.
int cmdSelectedObject(Engine& e, Tcl_Interp* in, Args& a)
{
	if (a[0] == "get_selectedobject") setResult(in, std::to_string(e.selection_.empty() ? 0 : e.selection_.front()));
	else
	{
		int id = a.size() > 1 ? e.resolveObj(in, a[1]) : 0;
		if (!id && a.size() > 1) id = std::atoi(a[1].c_str());
		e.selectOnly(id);
		setResult(in, "");
	}
	return TCL_OK;
}

// get_view [przewidywany]: "x y zoom dx dy" bieżącej kamery gry (bez kamery: 0 0 1 0 0).
int cmdGetView(Engine& e, Tcl_Interp* in, Args&)
{
	float v[5] = {0, 0, 1, 0, 0};
	if (e.host_) e.host_->getView(v);
	setResult(in, fmt(v[0]) + " " + fmt(v[1]) + " " + fmt(v[2]) + " " + fmt(v[3]) + " " + fmt(v[4]));
	return TCL_OK;
}

// set_view x y zoom [dx dy [move freeze speed set]]: kamera gry (🔶 move/speed: na razie od razu w celu).
// set_viewpos x y z zoom dx dy …: to samo z głębokością (głębokość pomijamy).
int cmdSetView(Engine& e, Tcl_Interp* in, Args& a)
{
	float v[5] = {0, 0, 1, 0, 0};
	if (e.host_) e.host_->getView(v);
	size_t base = a[0] == "set_viewpos" ? 1 : 0; // set_viewpos ma dodatkowe z po y
	auto arg = [&](size_t i, float def) { return a.size() > i ? std::strtof(a[i].c_str(), nullptr) : def; };
	v[0] = arg(1, v[0]), v[1] = arg(2, v[1]), v[2] = arg(3 + base, v[2]);
	v[3] = arg(4 + base, 0.f), v[4] = arg(5 + base, 0.f);
	bool move = a.size() > 6 + base && truthy(a[6 + base]);
	if (e.host_) e.host_->setView(v, move);
	setResult(in, "");
	return TCL_OK;
}

// set_view_begin y: górna granica widoku (wiersz); zapamiętana, gra jeszcze z niej nie korzysta (🔶).
int cmdSetViewBegin(Engine& e, Tcl_Interp* in, Args& a)
{
	if (a.size() > 1) e.viewBegin = std::atoi(a[1].c_str());
	setResult(in, "");
	return TCL_OK;
}

std::vector<CmdEntry> build()
{
	return {
	    {"selection", cmdSelection},
	    {"is_selected", cmdIsSelected},
	    {"get_selectedobject", cmdSelectedObject},
	    {"set_selectedobject", cmdSelectedObject},
	    {"get_view", cmdGetView},
	    {"set_view", cmdSetView},
	    {"set_viewpos", cmdSetView},
	    {"set_view_begin", cmdSetViewBegin},
	};
}

} // namespace

const std::vector<CmdEntry>& uiCommands()
{
	static const std::vector<CmdEntry> v = build();
	return v;
}

} // namespace ow::script
