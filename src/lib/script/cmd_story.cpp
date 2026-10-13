// Fabuła kampanii po stronie StoryMgr: wydarzenia (sm_add_event, sm_set_event, sm_get_event), komunikaty
// wyzwalaczy (sm_send_message) i wywołanie zwrotne StoryMgr (sm_register). Prawa za Zimennik/Diggles-Unity
// (MIT), Runtime/Tcl/Engine/DigglesTclEngine.cs (opis FUN_0065a820, FUN_005f8cf0); rozdział 21 przewodnika.
// 🔶 Generator świata (sm_scan, sm_get_level_info, lg_*) jeszcze nie: sm_scan zwraca 0 (świat nie rośnie).
#include "lib/script/commands.h"

#include <tcl.h>

#include <algorithm>

namespace ow::script
{

namespace
{

// sm_add_event nazwa [klasa_wyzwalacza] [słowo]: rejestruje wydarzenie (wartość 0), zwraca jego numer.
// Słowo domyślnie "activate". Ponowna rejestracja: gra zgłasza błąd („Story Event redefinition”, skrypty
// wołają ją w catch); tu numer istniejącego wydarzenia bez zmiany wyzwalacza.
int cmdSmAddEvent(Engine& e, Tcl_Interp* in, Args& a)
{
	if (a.size() < 2) return TCL_ERROR;
	auto& ev = e.storyEvents_;
	auto it = std::find_if(ev.begin(), ev.end(), [&](const Engine::StoryEvent& s) { return s.name == a[1]; });
	if (it == ev.end())
	{
		ev.push_back({a[1], a.size() > 2 ? a[2] : "", a.size() > 3 ? a[3] : "activate"});
		e.storyValues_.emplace(a[1], "0");
		it = ev.end() - 1;
	}
	setResult(in, std::to_string(it - ev.begin()));
	return TCL_OK;
}

// sm_set_event nazwa [wartość]: domyślnie 1.
int cmdSmSetEvent(Engine& e, Tcl_Interp* in, Args& a)
{
	if (a.size() > 1) e.storyValues_[a[1]] = a.size() > 2 ? a[2] : "1";
	setResult(in, "");
	return TCL_OK;
}

// sm_get_event nazwa: wartość wydarzenia, nieznane 0.
int cmdSmGetEvent(Engine& e, Tcl_Interp* in, Args& a)
{
	auto it = a.size() > 1 ? e.storyValues_.find(a[1]) : e.storyValues_.end();
	setResult(in, it == e.storyValues_.end() ? "0" : it->second);
	return TCL_OK;
}

// sm_register obiekt procedura: wywołanie zwrotne StoryMgr ("callback" w STORY.TCL).
int cmdSmRegister(Engine& e, Tcl_Interp* in, Args& a)
{
	if (a.size() > 2)
	{
		e.storyCallbackObj_ = e.resolveObj(in, a[1]);
		e.storyCallbackProc_ = a[2];
	}
	setResult(in, "");
	return TCL_OK;
}

// sm_send_message obiekt słowo: (1) każde jeszcze nieustawione wydarzenie, którego klasa wyzwalacza to klasa
// nadawcy, a słowo to słowo komunikatu, dostaje 1; (2) wywołanie zwrotne "<procedura> <klasa> <słowo> <id>"
// w interpreterze obiektu z sm_register (STORY.TCL: newsticker, wydarzenia QL_* z listy zadań).
int cmdSmSendMessage(Engine& e, Tcl_Interp* in, Args& a)
{
	setResult(in, "");
	if (a.size() < 3) return TCL_OK;
	int id = e.resolveObj(in, a[1]);
	ScriptObject* s = e.object(id);
	if (!s || !s->cls) return TCL_OK;
	const std::string& cls = s->cls->name;
	for (const auto& ev : e.storyEvents_)
		if (!ev.trigger.empty() && ev.trigger == cls && ev.keyword == a[2] && !truthy(e.storyValues_[ev.name])) e.storyValues_[ev.name] = "1";
	if (ScriptObject* cb = e.object(e.storyCallbackObj_); cb && cb->interp && !e.storyCallbackProc_.empty())
	{
		Tcl_Obj* l = Tcl_NewListObj(0, nullptr);
		for (const std::string& w : {e.storyCallbackProc_, cls, a[2], std::to_string(id)})
			Tcl_ListObjAppendElement(nullptr, l, Tcl_NewStringObj(w.data(), int(w.size())));
		Tcl_IncrRefCount(l);
		std::string code = Tcl_GetString(l);
		Tcl_DecrRefCount(l);
		e.runIn(*cb, code, "sm_send_message");
	}
	return TCL_OK;
}

// sm_get_resolution: pole mapy fabuły = 4 × 4 pola świata (konstruktor StoryMgr, Diggles-Unity WORLDGEN_ORIGINAL_RE A0).
int cmdSmResolution(Engine&, Tcl_Interp* in, Args&)
{
	setResult(in, "4");
	return TCL_OK;
}

// sm_scan: pozycja przebicia kopania do dobudowania świata; 0 = brak. 🔶 Bez generatora zawsze 0.
int cmdSmScan(Engine&, Tcl_Interp* in, Args&)
{
	setResult(in, "0");
	return TCL_OK;
}

} // namespace

const std::vector<CmdEntry>& storyCommands()
{
	static const std::vector<CmdEntry> v = {
	    {"sm_add_event", cmdSmAddEvent},
	    {"sm_set_event", cmdSmSetEvent},
	    {"sm_get_event", cmdSmGetEvent},
	    {"sm_register", cmdSmRegister},
	    {"sm_send_message", cmdSmSendMessage},
	    {"sm_get_resolution", cmdSmResolution},
	    {"sm_scan", cmdSmScan},
	};
	return v;
}

} // namespace ow::script
