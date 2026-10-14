// Generator świata po stronie skryptów: komendy lg_* (rejestr szablonów, pula, wzrost klastra; lib/level_gen),
// szablony unikalne StoryMgr (sm_add_temp, sm_def_temp_group), wybór obszaru (sm_get_level_info) i element startowy
// przy przebiciu (lg_set_starttemplatepro). Prawa za Zimennik/Diggles-Unity (MIT), DigglesTclEngine.cs
// (CmdSmGetLevelInfo, CmdLgSetStartTemplatePro, FreeRectFit) i Docs/WORLDGEN_ORIGINAL_RE.md; rozdział 21.8.
// 🔶 Bez korytarza łączącego szablon unikalny z elementem startowym (router FUN_00587480) i bez dziedzińców `mat`.
#include "lib/script/commands.h"
#include "lib/vfs.h"

#include <tcl.h>

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace ow::script
{

LevelGen& Engine::levelGen()
{
	if (!levelGen_)
	{
		levelGen_ = std::make_unique<LevelGen>(vfs_);
		levelGen_->rnd = [this]() { return storyRand(); };
		levelGen_->tileTemplated = [this](int x, int y) { return host_ && host_->tileTemplated(x, y); };
	}
	if (host_) host_->mapSize(levelGen_->mapW, levelGen_->mapH);
	return *levelGen_;
}

int Engine::storyRand()
{
	if (!storyRng_)
	{
		const char* seed = std::getenv("OW_LEVEL_SEED");
		storyRng_ = std::make_unique<std::mt19937>(seed ? unsigned(std::strtoul(seed, nullptr, 10)) + 1u : std::random_device{}());
	}
	return int((*storyRng_)() & 0x7fff);
}

namespace
{

bool trace()
{
	static const bool t = std::getenv("OW_TRACE_WORLDGEN") != nullptr;
	return t;
}

int intArg(const Args& a, size_t i, int def)
{
	return i < a.size() ? int(std::lround(std::atof(a[i].c_str()))) : def;
}

int cell(int v)
{
	return Engine::StoryWorld::cell(v);
}

// Nazwa szablonu unikalnego bez nawiasów i bez .tcl.
std::string tempName(std::string s)
{
	s.erase(std::remove_if(s.begin(), s.end(), [](char c) { return c == '{' || c == '}' || c == '"' || c == ' '; }), s.end());
	if (s.size() > 4 && Vfs::lower(s.substr(s.size() - 4)) == ".tcl") s.resize(s.size() - 4);
	return s;
}

struct GroupEntry
{
	std::string tcl;
	int dx, dy;
};

// sm_def_temp_group: wiersze {dx dy szablon}; wiersze `mat` (dziedzińce) pomijane.
std::vector<GroupEntry> groupEntries(Tcl_Interp* in, const std::string& spec)
{
	std::vector<GroupEntry> out;
	for (const auto& row : listItems(in, spec))
	{
		auto f = listItems(in, row);
		if (f.size() < 3 || f[0] == "mat") continue;
		char* e1 = nullptr;
		char* e2 = nullptr;
		long dx = std::strtol(f[0].c_str(), &e1, 10), dy = std::strtol(f[1].c_str(), &e2, 10);
		if (*e1 || *e2) continue;
		out.push_back({f[2] + ".tcl", int(dx), int(dy)});
	}
	return out;
}

// Rozmiar szablonu unikalnego w polach: grupa = obwiednia jej części, inaczej wymiary szablonu.
bool tempSize(Engine& e, Tcl_Interp* in, const std::string& name, int& w, int& h)
{
	auto& s = e.story_;
	w = h = 0;
	if (auto g = s.tempGroups.find(tempName(name)); g != s.tempGroups.end())
	{
		for (const auto& en : groupEntries(in, g->second))
		{
			int tw = 0, th = 0;
			if (!e.levelGen().tempSize(en.tcl, tw, th)) return false;
			w = std::max(w, en.dx + tw), h = std::max(h, en.dy + th);
		}
		return w > 0 && h > 0;
	}
	return e.levelGen().tempSize(tempName(name) + ".tcl", w, h);
}

// FUN_005fa9a0: prostokąt (w, h) rośnie do celu w surowym wolnym prostokącie i zostaje ustawiony tak, by zawierał
// (px, py): tryb 4 dociąga wskazówkę, z szablonem unikalnym losowo, bez niego na środku.
bool freeRectFit(Engine& e, int minX, int minY, int maxX, int maxY, int& px, int& py, int& w, int& h, int mode, bool hasUtemp)
{
	auto next = [&](int n) { return n > 0 ? e.storyRand() % n : 0; };
	int rw = maxX - minX, rh = maxY - minY;
	if (rw <= 0 || rh <= 0) return false;
	int targetW = std::min(rw, std::max(16, w + 8)), targetH = std::min(rh, std::max(8, h));
	while (w < targetW && w < 24) ++w;
	if (w < targetW) w += next(targetW - w);
	while (h < targetH && h < 8) ++h;
	if (h < targetH) h += next(targetH - h);
	int xMin = std::max(minX, px - w + 1), xMax = std::min(px, maxX - w);
	int yMin = std::max(minY, py - h + 1), yMax = std::min(py, maxY - h);
	if (xMax < xMin) xMax = xMin;
	if (yMax < yMin) yMax = yMin;
	if (mode == 4) px = std::clamp(px, xMin, xMax), py = std::clamp(py, yMin, yMax);
	else if (hasUtemp)
		px = xMin + next(xMax - xMin + 1), py = yMin + next(yMax - yMin + 1);
	else
		px = (xMax - xMin) / 2 + xMin, py = (yMax - yMin) / 2 + yMin;
	return true;
}

int zoneIndex(const Engine::StoryWorld& s, int y)
{
	if (s.zones.empty()) return -1;
	int zi = 0;
	for (size_t k = 0; k < s.zones.size(); ++k)
	{
		int lo = k > 0 ? s.zones[k - 1].bound : 0, hi = s.zones[k].bound < 0 ? INT_MAX : s.zones[k].bound;
		zi = int(k);
		if (lo <= y && y <= hi) break;
	}
	return std::max(zi, s.zoneFloor);
}

int zoneBase(const Engine::StoryWorld& s, int y)
{
	int zi = zoneIndex(s, y);
	return zi > 0 && zi - 1 < int(s.zones.size()) ? std::max(0, s.zones[size_t(zi) - 1].bound) : 0;
}

// --- rejestr i pula (lg_*) ---
int cmdLgConfig(Engine& e, Tcl_Interp* in, Args& a)
{
	LevelGen& g = e.levelGen();
	const std::string& c = a[0];
	setResult(in, "");
	auto opt = [&](const char* name) {
		for (size_t k = 2; k + 1 < a.size(); ++k)
			if (a[k] == name) return int(std::lround(std::atof(a[k + 1].c_str())));
		return INT_MIN;
	};
	if (c == "lg_addfilterclass")
	{
		for (size_t k = 1; k < a.size(); ++k)
			for (const auto& cls : listItems(in, a[k])) g.addFilterClass(cls);
	}
	else if (c == "lg_set_templategroup" && a.size() > 2)
		g.setTemplateGroup(a[1], listItems(in, a[2]));
	else if (c == "lg_set_templategroupprops" && a.size() > 1)
	{
		if (int lc = opt("-levelcount"); lc != INT_MIN) g.setTemplateGroupProps(a[1], lc);
	}
	else if (c == "lg_set_templateprops" && a.size() > 1)
		g.setTemplateProps(a[1], opt("-levelcount"), opt("-gamecount"), opt("-tunnel"));
	else if (c == "lg_set_templategroupvalue" && a.size() > 2)
		g.setTemplateGroupValue(a[1], float(std::atof(a[2].c_str())));
	else if (c == "lg_set_templatevalue" && a.size() > 2)
		g.setTemplateValue(a[1], float(std::atof(a[2].c_str())));
	else if (c == "lg_get_temp_size")
	{
		int w = 0, h = 0;
		if (a.size() > 1) g.tempSize(a[1], w, h);
		setResult(in, std::to_string(w) + " " + std::to_string(h));
	}
	else if (c == "lg_tp_clear")
		g.poolClear();
	else if (c == "lg_tp_addtemplatesets" && a.size() > 1)
		g.poolAddTemplateSets(a[1]);
	else if (c == "lg_tp_setfilter" && a.size() > 1)
		g.poolSetFilter(a[1]);
	else if (c == "lg_tp_objfilter" && a.size() > 1)
		g.poolObjFilter(listItems(in, a[1]));
	else if (c == "lg_set_objcount" && a.size() > 4)
		g.setObjCount(a[1], float(std::atof(a[2].c_str())), float(std::atof(a[3].c_str())), float(std::atof(a[4].c_str())));
	else if (c == "lg_set_area" && a.size() > 4)
		g.setArea(intArg(a, 1, 0), intArg(a, 2, 0), intArg(a, 3, 0), intArg(a, 4, 0));
	else if (c == "lg_set_templateratio" && a.size() > 3)
		g.setTemplateRatio(float(std::atof(a[1].c_str())), float(std::atof(a[2].c_str())), float(std::atof(a[3].c_str())));
	return TCL_OK; // lg_set_leveltype, lg_mark_area, lg_tp_log: bez skutku
}

// lg_start: wzrost klastra (lib/level_gen) → 1 udany, 0 nie.
int cmdLgStart(Engine& e, Tcl_Interp* in, Args&)
{
	LevelGen& g = e.levelGen();
	bool ok = g.start();
	if (trace())
		std::fprintf(stderr, "[świat] lg_start %s: %s (obszar %d,%d %d×%d pól fabuły, szablon unikalny '%s')\n", ok ? "udany" : "nieudany",
		    ok ? (std::to_string(g.level().size()) + " elementów").c_str() : g.lastFail.c_str(), e.story_.rectX, e.story_.rectY,
		    e.story_.rectW, e.story_.rectH, e.story_.utemp.c_str());
	setResult(in, ok ? "1" : "0");
	return TCL_OK;
}

// lg_get_level: {szablon x y} …; pola fabuły pod elementami zajęte, ograniczenia obiektów od zera, a gdy coś wyszło,
// nacisk kopania od zera (następne przebicie wymaga nowego kopania) i warianty z obiektami zużyte (FUN_00652d10).
int cmdLgGetLevel(Engine& e, Tcl_Interp* in, Args&)
{
	LevelGen& g = e.levelGen();
	Tcl_Obj* l = Tcl_NewListObj(0, nullptr);
	for (const auto& p : g.level())
	{
		Tcl_Obj* t = Tcl_NewListObj(0, nullptr);
		Tcl_ListObjAppendElement(nullptr, t, Tcl_NewStringObj(p.tcl.c_str(), -1));
		Tcl_ListObjAppendElement(nullptr, t, Tcl_NewIntObj(p.x));
		Tcl_ListObjAppendElement(nullptr, t, Tcl_NewIntObj(p.y));
		Tcl_ListObjAppendElement(nullptr, l, t);
		e.story_.markRect(cell(p.x), cell(p.y), (p.w + 3) / 4, (p.h + 3) / 4);
	}
	g.clearObjCounts();
	if (!g.level().empty())
	{
		e.story_.weight = 0;
		g.commitSessionEmission();
	}
	Tcl_SetObjResult(in, l);
	return TCL_OK;
}

// sm_add_temp nazwa [lo hi] [-opcje]: szablon unikalny strefy dodanej ostatnio; okno głębokości lo..hi (pola świata
// względem początku strefy). Opcje: -mindist szablon wartość, -middist wartość, -nonrelevant prawdopodobieństwo,
// -digconnect a [b]; pozostałe (-exclusive, -zoneborder, -stoneidx, -connect, -checkalign, -events) pomijane 🔶.
int cmdSmAddTemp(Engine& e, Tcl_Interp* in, Args& a)
{
	setResult(in, "");
	if (a.size() < 2) return TCL_OK;
	auto& s = e.story_;
	Engine::StoryWorld::Temp t;
	t.name = tempName(a[1]);
	t.zone = int(s.zones.size()) - 1;
	size_t k = 2;
	auto isInt = [&](size_t i) {
		if (i >= a.size()) return false;
		char* end = nullptr;
		std::strtol(a[i].c_str(), &end, 10);
		return !a[i].empty() && !*end;
	};
	if (isInt(k)) t.lo = std::atoi(a[k++].c_str());
	if (isInt(k)) t.hi = std::atoi(a[k++].c_str());
	for (; k < a.size(); ++k)
	{
		const std::string& o = a[k];
		if (o == "-mindist")
		{
			if (++k < a.size()) t.minDistRef = tempName(a[k]);
			if (++k < a.size()) t.minDistVal = float(std::atof(a[k].c_str()));
		}
		else if (o == "-middist" && ++k < a.size())
			t.midDist = float(std::atof(a[k].c_str()));
		else if (o == "-nonrelevant" && ++k < a.size())
			t.nonRelevant = float(std::atof(a[k].c_str()));
		else if (o == "-digconnect" && ++k < a.size())
		{
			t.digConnect = a[k];
			if (k + 1 < a.size() && a[k + 1].rfind("-", 0) != 0) t.digConnect += " " + a[++k];
		}
		else if (o == "-exclusive" || o == "-zoneborder" || o == "-stoneidx" || o == "-connect" || o == "-checkalign")
			++k;
	}
	s.temps.erase(std::remove_if(s.temps.begin(), s.temps.end(), [&](const auto& x) { return x.name == t.name; }), s.temps.end());
	s.temps.push_back(t);
	return TCL_OK;
}

int cmdSmDefTempGroup(Engine& e, Tcl_Interp* in, Args& a)
{
	if (a.size() > 2) e.story_.tempGroups[tempName(a[1])] = a[2];
	setResult(in, "");
	return TCL_OK;
}

// sm_get_level_info cx cy (FUN_005fac90): (1) surowy wolny prostokąt rozszerzany od pola przebicia paskami wolnych pól
// fabuły (lewo, góra, dół, prawo); (2) wybór szablonu unikalnego: nieużyty, ze strefy pola, głębokość ≥ lo, -mindist
// (wzorzec już użyty), -middist (nie w środkowej kolumnie mapy), kostka: waga (-nonrelevant albo (y−lo+1)/(hi−lo))
// ≥ los, potem dopasowanie do prostokąta, inaczej następny; (3) wynik: dopasowanie wokół pola przebicia.
// Zwraca "x y w h szablon" w polach fabuły.
int cmdSmGetLevelInfo(Engine& e, Tcl_Interp* in, Args& a)
{
	auto& s = e.story_;
	int cx = intArg(a, 1, 0), cy = intArg(a, 2, 0);
	int rx = cx, ry = cy, rw = 1, rh = 1;
	bool ok0 = s.get(cx, cy) != 1;
	bool gl = ok0, gu = ok0, gd = ok0, gr = ok0;
	auto stripFree = [&](int sx, int sy, int sw, int sh) {
		for (int y = 0; y < sh; ++y)
			for (int x = 0; x < sw; ++x)
				if (s.get(sx + x, sy + y) == 1) return false;
		return true;
	};
	for (int guard = 0; (gl || gu || gd || gr) && guard < 4096; ++guard)
	{
		if (gl) stripFree(rx - 1, ry, 1, rh) ? (--rx, ++rw) : (gl = false, 0);
		if (gu) stripFree(rx, ry - 1, rw, 1) ? (--ry, ++rh) : (gu = false, 0);
		if (gd) stripFree(rx, ry + rh, rw, 1) ? ++rh : (gd = false, 0);
		if (gr) stripFree(rx + rw, ry, 1, rh) ? ++rw : (gr = false, 0);
	}
	s.utemp.clear();
	int wy = cy * 4, zi = zoneIndex(s, wy), zb = zoneBase(s, wy);
	int seedW = 1, seedH = 1;
	if (ok0)
		for (const auto& t : s.temps)
		{
			if (t.placed || t.zone != zi) continue;
			int lo = std::min(t.lo, t.hi + 1), hi = std::max(t.lo, t.hi + 1);
			if (wy < lo + zb) continue;
			if (t.minDistVal > 0 && !t.minDistRef.empty())
			{
				bool refPlaced = false;
				for (const auto& r : s.temps) refPlaced |= r.name == t.minDistRef && r.placed;
				if (!refPlaced) continue;
			}
			if (t.midDist > 0 && float(std::abs(cx - s.mapW / 2) * 4) < t.midDist) continue;
			float weight = t.nonRelevant > 0 ? t.nonRelevant : float(wy - lo - zb + 1) / float(std::max(1, hi - lo));
			if (weight < float(e.storyRand()) / 32768.f) continue; // kostka przed dopasowaniem
			int tw = 0, th = 0;
			if (!tempSize(e, in, t.name, tw, th)) continue;
			int sW = cell(tw + 8) + 1, sH = cell(th + 8) + 1;
			if (sW > rw || sH > rh) continue; // nie mieści się: następny kandydat
			s.utemp = t.name, seedW = sW, seedH = sH;
			break;
		}
	s.rawX = rx, s.rawY = ry, s.rawW = rw, s.rawH = rh, s.scanX = cx, s.scanY = cy;
	int x = cx, y = cy, w = seedW, h = seedH;
	if (ok0) freeRectFit(e, rx, ry, rx + rw, ry + rh, x, y, w, h, 1, !s.utemp.empty());
	s.rectX = x, s.rectY = y, s.rectW = w, s.rectH = h;
	if (trace())
		std::fprintf(stderr, "[świat] sm_get_level_info %d %d: wolne %d,%d %d×%d → %d,%d %d×%d, szablon unikalny '%s'\n", cx, cy, rx, ry, rw, rh, x, y, w,
		    h, s.utemp.c_str());
	setResult(in, std::to_string(x) + " " + std::to_string(y) + " " + std::to_string(w) + " " + std::to_string(h) + " " + (s.utemp.empty() ? "{}" : s.utemp));
	return TCL_OK;
}

// lg_set_starttemplatepro (FUN_00650f90): zakotwiczenie generacji przy przebiciu. Krok wstępny o 4 pola w kierunku
// kopania (element startowy nigdy nie nachodzi na tunel gracza), do 4 prób: znacznik tunelu w środku pola zatrzasku,
// świeży element startowy w stronę kopania, miejsce (probeForcedPose); porażka → "nodigmark". Z szablonem unikalnym
// prostokąt ustawiony na nowo tak, by przebicie było przy krawędzi od strony kopania (FUN_005fc0a0), a szablon
// w ≤ 4 losowych miejscach daleko od przebicia. Przy brzegu obszaru element z jednymi drzwiami (FUN_00650f90:261-273).
int cmdLgSetStartTemplatePro(Engine& e, Tcl_Interp* in, Args&)
{
	auto& s = e.story_;
	LevelGen& g = e.levelGen();
	auto rectStr = [&]() { return std::to_string(s.rectX) + " " + std::to_string(s.rectY) + " " + std::to_string(s.rectW) + " " + std::to_string(s.rectH); };
	g.clearForced();
	int side = -1, stepX = 0, stepY = 0;
	bool downThrough = false;
	switch (s.latchDir)
	{
	case 1: side = 1, stepY = -4; break;
	case 2: side = 3, downThrough = true, stepY = 4; break;
	case 4: side = 0, stepX = -4; break;
	case 8: side = 2, stepX = 4; break;
	}
	bool anchored = false;
	int ax = 0, ay = 0;
	std::string piece;
	int lx = s.latchX & ~3, ly = s.latchY & ~3;
	if (side >= 0 && s.latchX && s.latchY)
	{
		int wx = lx + stepX, wy = ly + stepY;
		for (int k = 0; k < 4 && !anchored; ++k)
		{
			int n = e.host_ ? e.host_->digNibble(lx + 2, ly + 2, 0) : 3;
			if ((n & 8) || !(n & 0xF)) break; // pole zatrzasku bez znacznika tunelu
			std::string cand = g.pickStartPiece(side, downThrough);
			if (cand.empty()) break;
			if (g.probeForcedPose(cand, wx, wy))
			{
				anchored = true, ax = wx, ay = wy, piece = cand;
				break;
			}
			wx += stepX, wy += stepY;
		}
	}
	if (!anchored)
	{
		if (trace()) std::fprintf(stderr, "[świat] lg_set_starttemplatepro: nodigmark (zatrzask %d %d,%d)\n", s.latchDir, s.latchX, s.latchY);
		s.utemp.clear();
		setResult(in, rectStr() + " nodigmark");
		return TCL_OK;
	}
	if (!s.utemp.empty() && s.rawW > 0 && s.rawH > 0)
	{ // FUN_005fc0a0: prostokąt pod szablon unikalny + droga od przebicia, przebicie przy krawędzi od strony kopania
		int utW = 0, utH = 0;
		if (tempSize(e, in, s.utemp, utW, utH))
		{
			int dist = std::abs(ay - ly) + 4 + std::abs(ax - lx);
			int minX = s.rawX, minY = s.rawY, maxX = s.rawX + s.rawW, maxY = s.rawY + s.rawH;
			switch (s.latchDir)
			{
			case 1: maxY = std::min(maxY, s.scanY); break;
			case 2:
				if (int d = s.scanY - minY; d > 0) maxY -= d, minY = s.scanY;
				break;
			case 4: maxX = std::min(maxX, s.scanX); break;
			case 8:
				if (int d = s.scanX - minX; d > 0) maxX -= d, minX = s.scanX;
				break;
			}
			bool vertical = s.latchDir == 1 || s.latchDir == 2;
			int W = cell(utW + (vertical ? 0 : dist) + 8) + 1, H = cell(utH + (vertical ? dist : 0) + 8) + 1;
			int px = s.scanX, py = s.scanY, mode = 1;
			if (vertical)
			{ // kolumna drzwi łączących szablonu (🔶 początek części -digconnect grupy, jak Diggles-Unity)
				int doorX = 0;
				if (auto gr = s.tempGroups.find(s.utemp); gr != s.tempGroups.end())
				{
					auto subs = groupEntries(in, gr->second);
					int ai = 0;
					for (const auto& t : s.temps)
						if (t.name == s.utemp && !t.digConnect.empty()) ai = std::atoi(t.digConnect.c_str());
					if (ai < 0 || ai >= int(subs.size())) ai = 0;
					if (!subs.empty()) doorX = subs[size_t(ai)].dx;
				}
				int utWc = cell(utW), utDir = doorX < 5 ? 3 : utW <= doorX ? 2
				                                                           : 1;
				int hint = utDir == 1 ? s.scanX - cell(doorX) : utDir == 2 ? s.scanX - utWc - 2
				                                                           : s.scanX + 2;
				hint = std::max(std::min(hint, maxX - utWc), minX);
				px = std::min(hint, s.scanX), mode = 4;
			}
			if (freeRectFit(e, minX, minY, maxX, maxY, px, py, W, H, mode, true)) s.rectX = px, s.rectY = py, s.rectW = W, s.rectH = H;
			else
				s.rectX = s.scanX, s.rectY = s.scanY, s.rectW = s.rectH = 1, s.utemp.clear();
		}
	}
	if (s.rectW > 0 && s.rectH > 0) g.setArea(s.rectX * 4, s.rectY * 4, (s.rectX + s.rectW) * 4, (s.rectY + s.rectH) * 4);
	{ // przy brzegu obszaru: element z jednymi drzwiami w stronę kopania (w dół: drzwi w dół)
		int walkedX = stepX ? std::abs(ax - lx) + 4 : 0;
		int relX = (lx - s.rectX * 4) + walkedX;
		if (s.rectW > 0 && (relX < 5 || relX >= s.rectW * 4 - 4))
			if (std::string sw = g.pickStartPiece(downThrough ? 3 : side, false); !sw.empty()) piece = sw;
	}
	g.addForced(piece, ax - s.rectX * 4, ay - s.rectY * 4);
	if (!s.utemp.empty())
	{ // szablon unikalny daleko od przebicia: ≤ 4 losowe miejsca w obszarze bez pasa od strony kopania
		std::vector<GroupEntry> entries;
		if (auto gr = s.tempGroups.find(s.utemp); gr != s.tempGroups.end()) entries = groupEntries(in, gr->second);
		else
			entries.push_back({s.utemp + ".tcl", 0, 0});
		int extW = 0, extH = 0;
		for (const auto& en : entries)
		{
			int tw = 0, th = 0;
			if (!g.tempSize(en.tcl, tw, th))
			{
				extW = -1;
				break;
			}
			extW = std::max(extW, en.dx + tw), extH = std::max(extH, en.dy + th);
		}
		bool placedU = false;
		if (!entries.empty() && extW > 0)
		{
			int minX = 4, maxX = s.rectW * 4 - extW - 4, minY = 4, maxY = s.rectH * 4 - extH - 4;
			int wD = stepX ? std::abs(ax - lx) + 4 : 0, hD = stepY ? std::abs(ay - ly) + 4 : 0;
			switch (s.latchDir)
			{
			case 1: maxY -= 4 + hD; break;
			case 2: minY += hD + 4; break;
			case 4: maxX -= 4 + wD; break;
			case 8: minX = wD + 8; break;
			}
			minX = (minX + 3) & ~3, minY = (minY + 3) & ~3;
			for (int attempt = 0; attempt < 4 && minX <= maxX && minY <= maxY && !placedU; ++attempt)
			{
				int px = minX + (e.storyRand() % ((maxX - minX) / 4 + 1)) * 4, py = minY + (e.storyRand() % ((maxY - minY) / 4 + 1)) * 4;
				bool ok = true;
				for (const auto& en : entries) ok = ok && g.probeForcedPose(en.tcl, s.rectX * 4 + px + en.dx, s.rectY * 4 + py + en.dy);
				if (!ok) continue;
				for (const auto& en : entries) g.addForced(en.tcl, px + en.dx, py + en.dy);
				placedU = true;
			}
		}
		if (trace()) std::fprintf(stderr, "[świat] szablon unikalny '%s' %s\n", s.utemp.c_str(), placedU ? "w obszarze" : "nie mieści się");
	}
	if (trace())
		std::fprintf(stderr, "[świat] zakotwiczenie: kierunek %d, zatrzask %d,%d → %s w (%d, %d), obszar %d,%d %d×%d\n", s.latchDir, s.latchX, s.latchY,
		    piece.c_str(), ax, ay, s.rectX, s.rectY, s.rectW, s.rectH);
	setResult(in, rectStr() + " " + (s.utemp.empty() ? "{}" : s.utemp));
	return TCL_OK;
}

} // namespace

const std::vector<CmdEntry>& levelGenCommands()
{
	static const std::vector<CmdEntry> v = {
	    {"lg_addfilterclass", cmdLgConfig},
	    {"lg_set_templategroup", cmdLgConfig},
	    {"lg_set_templategroupprops", cmdLgConfig},
	    {"lg_set_templateprops", cmdLgConfig},
	    {"lg_set_templategroupvalue", cmdLgConfig},
	    {"lg_set_templatevalue", cmdLgConfig},
	    {"lg_get_temp_size", cmdLgConfig},
	    {"lg_tp_clear", cmdLgConfig},
	    {"lg_tp_addtemplatesets", cmdLgConfig},
	    {"lg_tp_setfilter", cmdLgConfig},
	    {"lg_tp_objfilter", cmdLgConfig},
	    {"lg_set_objcount", cmdLgConfig},
	    {"lg_set_area", cmdLgConfig},
	    {"lg_set_templateratio", cmdLgConfig},
	    {"lg_set_leveltype", cmdLgConfig},
	    {"lg_mark_area", cmdLgConfig},
	    {"lg_tp_log", cmdLgConfig},
	    {"lg_start", cmdLgStart},
	    {"lg_get_level", cmdLgGetLevel},
	    {"lg_set_starttemplatepro", cmdLgSetStartTemplatePro},
	    {"sm_add_temp", cmdSmAddTemp},
	    {"sm_def_temp_group", cmdSmDefTempGroup},
	    {"sm_get_level_info", cmdSmGetLevelInfo},
	};
	return v;
}

} // namespace ow::script
