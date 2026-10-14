// Fabuła kampanii po stronie StoryMgr: wydarzenia (sm_add_event, sm_set_event, sm_get_event), komunikaty
// wyzwalaczy (sm_send_message) i wywołanie zwrotne StoryMgr (sm_register). Prawa za Zimennik/Diggles-Unity
// (MIT), Runtime/Tcl/Engine/DigglesTclEngine.cs (opis FUN_0065a820, FUN_005f8cf0); rozdział 21 przewodnika.
// Mapa fabuły i nacisk kopania (sm_create_map, sm_set_digcount, sm_map_set/get, sm_mark_temparea, sm_set_temp,
// strefy, sm_scan, DigNotify). 🔶 Generator (sm_get_level_info, lg_*) jeszcze nie.
#include "lib/script/commands.h"
#include "lib/map_world.h"
#include "lib/vfs.h"

#include <tcl.h>

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdlib>

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

// sm_scan: pole fabuły zatrzaśniętego przebicia "cx cy" i oznaczenie go jako oddanego; brak (albo już oddany) 0.
// Wagi nie zeruje (zrobi to dopiero udany lg_get_level), więc nieudana generacja ponawia się przy kolejnym kopaniu.
int cmdSmScan(Engine& e, Tcl_Interp* in, Args&)
{
	auto& s = e.story_;
	if (!s.latchDir || !s.latchX || !s.latchY || s.latchConsumed)
	{
		setResult(in, "0");
		return TCL_OK;
	}
	s.latchConsumed = true;
	setResult(in, std::to_string(s.cell(s.latchX)) + " " + std::to_string(s.cell(s.latchY)));
	return TCL_OK;
}

int intArg(const Args& a, size_t i, int def)
{
	return i < a.size() ? int(std::lround(std::atof(a[i].c_str()))) : def;
}

// sm_create_map W H: mapa fabuły (W/4 × H/4 pól), wszystko puste; nowa gra: zatrzask, nacisk, próg 5 i rejestry od zera.
int cmdSmCreateMap(Engine& e, Tcl_Interp* in, Args& a)
{
	auto& s = e.story_;
	bool replaying = s.replaying;
	s = Engine::StoryWorld{};
	s.replaying = replaying;
	s.mapW = s.cell(intArg(a, 1, 0)), s.mapH = s.cell(intArg(a, 2, 0));
	s.map.assign(size_t(std::max(s.mapW, 0)) * size_t(std::max(s.mapH, 0)), 0);
	setResult(in, "");
	return TCL_OK;
}

// sm_set_digcount N: próg nacisku kopania (kampania: 32); sm_dig_message 0|1: wywołanie zwrotne przy kopaniu.
int cmdSmDigSettings(Engine& e, Tcl_Interp* in, Args& a)
{
	if (a[0] == "sm_set_digcount") e.story_.threshold = intArg(a, 1, 5);
	else e.story_.digMessage = a.size() > 1 && truthy(a[1]);
	setResult(in, "");
	return TCL_OK;
}

// sm_map_set cx cy [cw ch]: prostokąt pól fabuły zajęty; sm_map_get cx cy: 1 zajęte (też poza mapą), 0 wolne.
int cmdSmMap(Engine& e, Tcl_Interp* in, Args& a)
{
	if (a[0] == "sm_map_set")
	{
		e.story_.markRect(intArg(a, 1, 0), intArg(a, 2, 0), intArg(a, 3, 1), intArg(a, 4, 1));
		setResult(in, "");
	}
	else
		setResult(in, std::to_string(e.story_.get(intArg(a, 1, 0), intArg(a, 2, 0))));
	return TCL_OK;
}

// sm_mark_temparea x y szablon: pola fabuły pod szablonem (rozmiar ze skryptu szablonu, zaokrąglony w górę do
// pól 4 × 4; nieznany 16 × 16) zajęte. 🔶 Za Diggles-Unity (bez sprawdzania granic mapy).
int cmdSmMarkTempArea(Engine& e, Tcl_Interp* in, Args& a)
{
	setResult(in, "");
	if (a.size() < 4) return TCL_OK;
	int w = 0, h = 0;
	if (!templateSize(e.vfs_, a[3], w, h) || w <= 0 || h <= 0) w = h = 16;
	auto& s = e.story_;
	int cx = s.cell(intArg(a, 1, 0)), cy = s.cell(intArg(a, 2, 0));
	int cw = (w + 3) / 4, ch = (h + 3) / 4;
	for (int y = cy; y < cy + ch; ++y)
		for (int x = cx; x < cx + cw; ++x)
			if (x >= 0 && y >= 0 && x < s.mapW && y < s.mapH) s.map[size_t(y) * size_t(s.mapW) + size_t(x)] = 1;
	return TCL_OK;
}

// sm_set_temp {{szablon x y} …}: szablony unikalne użyte (dla wyboru w sm_get_level_info, krok 4 wzrostu).
int cmdSmSetTemp(Engine& e, Tcl_Interp* in, Args& a)
{
	for (size_t k = 1; k < a.size(); ++k)
		for (const auto& entry : listItems(in, a[k]))
		{
			auto t = listItems(in, entry);
			if (t.size() < 3) continue;
			std::string n = t[0];
			if (n.size() > 4 && Vfs::lower(n.substr(n.size() - 4)) == ".tcl") n.resize(n.size() - 4);
			e.story_.placed.emplace_back(n, std::atoi(t[1].c_str()), std::atoi(t[2].c_str()));
		}
	// Rejestr sm_add_temp: szablon użyty; grupa (sm_def_temp_group) użyta, gdy użyte są wszystkie jej części.
	auto used = [&](const std::string& name) {
		for (const auto& [n, x, y] : e.story_.placed)
			if (Vfs::lower(n) == Vfs::lower(name)) return true;
		return false;
	};
	for (auto& t : e.story_.temps)
	{
		if (t.placed) continue;
		if (used(t.name)) t.placed = true;
		else if (auto g = e.story_.tempGroups.find(t.name); g != e.story_.tempGroups.end())
		{
			bool all = true, any = false;
			for (const auto& row : listItems(in, g->second))
			{
				auto f = listItems(in, row);
				if (f.size() < 3 || f[0] == "mat") continue;
				any = true;
				all = all && used(f[2]);
			}
			t.placed = any && all;
		}
	}
	setResult(in, "");
	return TCL_OK;
}

// Strefy świata (Urwald, Metall, Kristall, Lava): sm_add_zone nazwa; sm_set_zone nr y (górna granica y strefy);
// sm_force_zone nr (najniższa strefa przy wyborze); sm_get_zone y: nazwa strefy, w której leży y.
int cmdSmZone(Engine& e, Tcl_Interp* in, Args& a)
{
	auto& s = e.story_;
	setResult(in, "");
	if (a[0] == "sm_add_zone" && a.size() > 1)
	{
		for (const auto& z : s.zones)
			if (z.name == a[1]) return TCL_OK;
		s.zones.push_back({a[1], -1});
	}
	else if (a[0] == "sm_set_zone")
	{
		int i = intArg(a, 1, -1);
		if (i >= 0 && i < int(s.zones.size())) s.zones[size_t(i)].bound = intArg(a, 2, -1);
	}
	else if (a[0] == "sm_force_zone")
		s.zoneFloor = intArg(a, 1, 0);
	else if (a[0] == "sm_get_zone")
	{
		int y = intArg(a, 1, 0), zi = 0;
		for (size_t k = 0; k < s.zones.size(); ++k)
		{ // strefa k: (granica k−1, granica k], −1 = bez końca
			int lo = k > 0 ? s.zones[k - 1].bound : 0, hi = s.zones[k].bound < 0 ? INT_MAX : s.zones[k].bound;
			zi = int(k);
			if (lo <= y && y <= hi) break;
		}
		zi = std::max(zi, s.zoneFloor);
		setResult(in, zi < int(s.zones.size()) ? s.zones[size_t(zi)].name : "Urwald");
	}
	return TCL_OK;
}

// sm_draw_stone -border|-funnel: pasy granitu maluje składanie świata (map_world.h drawStoneOption); tu, przy
// powtórce przebiegu wstępnego, tylko znaczy ich prostokąty na mapie fabuły (gra: FUN_005fc9d0:139-141) i 9
// górnych wierszy mapy fabuły (-border, FUN_0065e090:943). 🔶 -exlimit, -utmat, -matlist: krok 4–5 wzrostu.
int cmdSmDrawStone(Engine& e, Tcl_Interp* in, Args& a)
{
	auto& s = e.story_;
	setResult(in, "");
	if (a.size() < 2 || s.mapW <= 0) return TCL_OK;
	int W = s.mapW * 4, H = s.mapH * 4;
	auto band = [&](int x0, int y0, int x1, int y1) {
		x0 = std::max(x0, 0), y0 = std::max(y0, 0), x1 = std::min(x1, W), y1 = std::min(y1, H);
		s.markRect(x0 >> 2, y0 >> 2, (x1 - x0) >> 2, (y1 - y0) >> 2);
	};
	if (a[1] == "-border")
	{
		band(0, 0, W, 32), band(0, 0, 32, H + 4), band(W - 32, 0, W, H + 4);
		s.markRect(0, 0, s.mapW, 9);
	}
	else if (a[1] == "-funnel")
		for (int y = 16, bw = s.mapW * 2 - 20; bw > 0; y += 4, bw -= 8) band(W - bw, y, W, y + 6), band(0, y, bw, y + 6);
	return TCL_OK;
}

// lg_sort_level x y lista: lista {szablon x y} posortowana rosnąco po kwadracie odległości lewego górnego rogu
// od (x, y), przy równych w kolejności listy (fill_temp_buffer: najbliższe nakładane najpierw). 🔶 Za Diggles-Unity
// LevelGenerator.SortLevel.
int cmdLgSortLevel(Engine&, Tcl_Interp* in, Args& a)
{
	long sx = a.size() > 1 ? std::atol(a[1].c_str()) : 0, sy = a.size() > 2 ? std::atol(a[2].c_str()) : 0;
	struct Item
	{
		std::string text;
		long d;
	};
	std::vector<Item> items;
	if (a.size() > 3)
		for (const auto& t : listItems(in, a[3]))
		{
			auto w = listItems(in, t);
			if (w.size() < 3) continue;
			long dx = std::atol(w[1].c_str()) - sx, dy = std::atol(w[2].c_str()) - sy;
			items.push_back({t, dx * dx + dy * dy});
		}
	std::stable_sort(items.begin(), items.end(), [](const Item& p, const Item& q) { return p.d < q.d; });
	Tcl_Obj* l = Tcl_NewListObj(0, nullptr);
	for (const auto& it : items) Tcl_ListObjAppendElement(nullptr, l, Tcl_NewStringObj(it.text.data(), int(it.text.size())));
	Tcl_SetObjResult(in, l);
	return TCL_OK;
}

} // namespace

void Engine::StoryWorld::markRect(int cx, int cy, int cw, int ch)
{
	if (mapW > 0 && (cx < 0 || cy < 0 || cx + cw > mapW || cy + ch > mapH)) return;
	for (int y = cy; y < cy + ch; ++y)
		for (int x = cx; x < cx + cw; ++x) map[size_t(y) * size_t(mapW) + size_t(x)] = 1;
}

// Nacisk kopania (gra: CStoryMgr::DigNotify = FUN_005fd060, za Diggles-Unity StoryDigNotify, WORLDGEN_ORIGINAL_RE
// A1): węzeł ≥ 8 pól od brzegów mapy; wywołanie zwrotne "dig shut -1" tylko przy sm_dig_message; zatrzask kasowany,
// pole fabuły węzła zajęte; potem, gdy węzeł ma znacznik tunelu (półbajt z bitem 3 zgaszonym), pierwszy z sąsiadów
// o 4 pola w kolejności +x, +y, −x, −y, którego pole fabuły jest wolne i który też ma znacznik tunelu, dodaje wagę
// 2, 4, 2, 1; waga ponad próg zatrzaskuje {kierunek, sąsiad}. Najwyżej jeden przyrost na kopnięcie.
void Engine::storyDigNotify(int x, int y, int owner)
{
	auto& s = story_;
	if (s.mapW <= 0 || x < 8 || y < 8 || x >= s.mapW * 4 - 8 || y >= s.mapH * 4 - 8) return;
	if (s.digMessage && !storyCallbackProc_.empty())
		if (ScriptObject* cb = object(storyCallbackObj_); cb && cb->interp) runIn(*cb, storyCallbackProc_ + " dig shut -1", "sm_dig");
	s.latchDir = s.latchX = s.latchY = 0, s.latchConsumed = false;
	s.markRect(s.cell(x), s.cell(y), 1, 1);
	auto gate = [&](int nx, int ny) {
		int n = host_ ? host_->digNibble(nx, ny, owner) : 0;
		return (n & 8) == 0 && (n & 0xF) != 0;
	};
	if (!gate(x, y)) return;
	struct Probe
	{
		int dx, dy, dir, weight;
	};
	for (const Probe& p : {Probe{4, 0, 8, 2}, Probe{0, 4, 2, 4}, Probe{-4, 0, 4, 2}, Probe{0, -4, 1, 1}})
	{
		int nx = x + p.dx, ny = y + p.dy;
		if (s.get(s.cell(nx), s.cell(ny)) == 1 || !gate(nx, ny)) continue;
		s.weight += p.weight;
		if (s.weight > s.threshold) s.latchX = nx, s.latchY = ny, s.latchDir = p.dir, s.latchConsumed = false;
		return;
	}
}

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
	    {"lg_sort_level", cmdLgSortLevel},
	    {"sm_create_map", cmdSmCreateMap},
	    {"sm_set_digcount", cmdSmDigSettings},
	    {"sm_dig_message", cmdSmDigSettings},
	    {"sm_map_set", cmdSmMap},
	    {"sm_map_get", cmdSmMap},
	    {"sm_mark_temparea", cmdSmMarkTempArea},
	    {"sm_set_temp", cmdSmSetTemp},
	    {"sm_add_zone", cmdSmZone},
	    {"sm_set_zone", cmdSmZone},
	    {"sm_force_zone", cmdSmZone},
	    {"sm_get_zone", cmdSmZone},
	    {"sm_draw_stone", cmdSmDrawStone},
	};
	return v;
}

} // namespace ow::script
