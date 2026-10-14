#include "lib/level_gen.h"

#include "lib/vfs.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <sstream>

namespace ow
{

namespace
{

std::optional<std::vector<uint8_t>> readAny(const Vfs& vfs, std::initializer_list<std::string> paths)
{
	for (const auto& p : paths)
		if (auto d = vfs.read(p)) return d;
	return std::nullopt;
}

// Kwantyzacja współrzędnej drzwi z pikseli .ent (8 na pole) do 4 pól (FUN_005826d0:219-231).
int quant(int px)
{
	int s = px >= 0 ? 1 : -1;
	return ((px + s * 7) >> 3) & ~3;
}

std::string lower(std::string s)
{
	return Vfs::lower(std::move(s));
}

} // namespace

std::string LevelGen::trimTcl(const std::string& s)
{
	std::string t;
	for (char c : s)
		if (c != '{' && c != '}' && c != '"' && c != ' ' && c != '\t' && c != '\r' && c != '\n') t += c;
	size_t slash = t.find_last_of("/\\");
	if (slash != std::string::npos) t = t.substr(slash + 1);
	if (!t.empty() && (t.size() < 4 || lower(t.substr(t.size() - 4)) != ".tcl")) t += ".tcl";
	return t;
}

LevelGen::Piece* LevelGen::resolve(const std::string& tcl, bool create)
{
	std::string n = trimTcl(tcl);
	if (n.empty()) return nullptr;
	auto it = pieces_.find(lower(n));
	if (it == pieces_.end())
	{
		if (!create) return nullptr;
		it = pieces_.emplace(lower(n), Piece{}).first;
		it->second.tcl = n;
	}
	if (!it->second.loaded) loadMeta(it->second);
	return &it->second;
}

// Metadane szablonu (FUN_005826d0, Diggles-Unity LoadMeta): baza .pmp z map_template (albo z nagłówka
// split_template, który podaje też wymiary), obiekty klas filtra z wierszy `new`, wymiary z .pmp, drzwi z .ent.
void LevelGen::loadMeta(Piece& p)
{
	p.loaded = true;
	auto text = readAny(vfs_, {"Data/Templates/tcl/" + p.tcl, "Data/Templates/" + p.tcl});
	if (text)
	{
		std::istringstream in(std::string(text->begin(), text->end()));
		std::string line;
		while (std::getline(in, line))
		{
			if (size_t si = line.find("split_template"); si != std::string::npos && !p.split)
			{
				std::istringstream ws(line.substr(si));
				std::string cmd, name, pmp;
				int w = 0, h = 0;
				ws >> cmd >> name >> pmp;
				if (ws >> w >> h) p.w = w, p.h = h;
				if (pmp.size() > 4 && lower(pmp.substr(pmp.size() - 4)) == ".pmp" && p.pmpBase.empty()) p.pmpBase = pmp.substr(0, pmp.size() - 4);
				p.split = true;
				continue;
			}
			if (size_t mi = line.find("map_template"); mi != std::string::npos && p.pmpBase.empty())
			{
				std::istringstream ws(line.substr(mi + 12));
				std::string f;
				ws >> f;
				if (f.size() > 4 && lower(f.substr(f.size() - 4)) == ".pmp")
				{
					size_t sl = f.find_last_of("/\\");
					p.pmpBase = f.substr(sl == std::string::npos ? 0 : sl + 1, f.size() - 4 - (sl == std::string::npos ? 0 : sl + 1));
				}
			}
			if (size_t ni = line.find("new "); ni != std::string::npos)
			{
				std::string rest = line.substr(ni + 4);
				size_t e = rest.find_first_of(" \t[]\"{}");
				std::string cls = rest.substr(0, e);
				for (const auto& fc : filterClasses_)
					if (lower(fc) == lower(cls))
					{
						p.objCounts[fc]++;
						p.objectBearing = true;
						break;
					}
			}
		}
	}
	if (p.pmpBase.empty()) return;
	if (!p.split)
		if (auto d = readAny(vfs_, {"Data/Templates/pmp/" + p.pmpBase + ".pmp", "Data/Templates/" + p.pmpBase + ".pmp"}); d && d->size() >= 8)
		{
			p.w = int((*d)[0] | (*d)[1] << 8 | (*d)[2] << 16 | (*d)[3] << 24);
			p.h = int((*d)[4] | (*d)[5] << 8 | (*d)[6] << 16 | (*d)[7] << 24);
		}
	if (auto d = readAny(vfs_, {"Data/Templates/ent/" + p.pmpBase + ".ent", "Data/Templates/" + p.pmpBase + ".ent"}))
	{
		std::istringstream in(std::string(d->begin(), d->end()));
		std::string line;
		while (std::getline(in, line))
		{
			std::istringstream ws(line);
			std::string cmd;
			int type = 0, xp = 0, yp = 0;
			if (!(ws >> cmd >> type >> xp >> yp) || cmd != "map_entry" || type != 1) continue;
			int qx = quant(xp), qy = quant(yp);
			int rx = ((xp % 32) + 32) % 32, ry = ((yp % 32) + 32) % 32;
			if (ry < rx)
			{
				if (rx + ry < 32) qx += 2; // krawędź górna albo dolna, środek w x
				else qx += 4, qy += 2;     // krawędź prawa
			}
			else
			{
				if (rx + ry < 32) qy += 2; // krawędź lewa albo prawa, środek w y
				else qx += 2, qy += 4;     // krawędź dolna
			}
			int dir = qy == 0 ? 1 : qx == 0 ? 0
			    : qx == p.w                 ? 2
			    : qy == p.h                 ? 3
			                                : -1;
			if (dir < 0) continue; // drzwi nie na krawędzi: pominięte
			p.edges.push_back({qx, qy, dir});
		}
	}
	p.openSides = 0;
	for (const auto& e : p.edges) p.openSides |= e.dir == 1 ? 1 : e.dir == 2 ? 2
		: e.dir == 3                                                         ? 4
		                                                                     : 8;
}

void LevelGen::addFilterClass(const std::string& cls)
{
	for (const auto& c : filterClasses_)
		if (lower(c) == lower(cls)) return;
	filterClasses_.push_back(cls);
}

void LevelGen::setTemplateGroup(const std::string& name, const std::vector<std::string>& tcls)
{
	if (!groups_.count(lower(name))) groupOrder_.push_back(lower(name));
	auto& g = groups_[lower(name)];
	for (const auto& t : tcls)
	{
		std::string n = trimTcl(t);
		if (n.empty()) continue;
		if (std::find(g.tcls.begin(), g.tcls.end(), n) == g.tcls.end()) g.tcls.push_back(n);
		if (!pieces_.count(lower(n))) pieces_[lower(n)].tcl = n;
	}
}

void LevelGen::setTemplateGroupProps(const std::string& name, int levelCount)
{
	for (const auto& t : groups_[lower(name)].tcls)
		if (auto it = pieces_.find(lower(t)); it != pieces_.end()) it->second.levelCap = levelCount;
}

void LevelGen::setTemplateProps(const std::string& tcl, int levelCount, int gameCount, int tunnel)
{
	std::string n = trimTcl(tcl);
	if (n.empty()) return;
	Piece& p = pieces_[lower(n)];
	p.tcl = n;
	if (levelCount != INT_MIN) p.levelCap = levelCount;
	if (gameCount != INT_MIN) p.gameCap = gameCount;
	if (tunnel != INT_MIN) p.tunnelFlag = tunnel;
}

void LevelGen::setTemplateGroupValue(const std::string& name, float v)
{
	auto it = groups_.find(lower(name));
	if (it == groups_.end()) return;
	for (const auto& t : it->second.tcls)
		if (auto pt = pieces_.find(lower(t)); pt != pieces_.end()) pt->second.value = v;
}

void LevelGen::setTemplateValue(const std::string& tcl, float v)
{
	if (auto it = pieces_.find(lower(trimTcl(tcl))); it != pieces_.end()) it->second.value = v;
}

void LevelGen::poolClear()
{ // FUN_0064e4f0 → FUN_00583650: pula, elementy wymuszone, obszar i stan ziarna od zera
	pool_.clear();
	forced_.clear();
	ax0_ = ay0_ = ax1_ = ay1_ = 0;
	seedState_ = -1;
}

void LevelGen::poolAddTemplateSets(const std::string& spec)
{ // "Urwald.Std": grupy o tej nazwie i jej podgrupy (Urwald.Std.Gng, …), w kolejności rejestracji grup
	std::istringstream ws(spec);
	std::string set;
	while (ws >> set)
	{
		std::string s = lower(set);
		for (const auto& name : groupOrder_)
			if (name == s || name.rfind(s + ".", 0) == 0)
				for (const auto& t : groups_[name].tcls)
					if (std::find(pool_.begin(), pool_.end(), t) == pool_.end()) pool_.push_back(t);
	}
}

void LevelGen::poolSetFilter(const std::string& group)
{ // lg_tp_setfilter: USUWA członków grupy z puli (FUN_0064f830)
	std::string g = group;
	g.erase(0, g.find_first_not_of(" \t"));
	g.erase(g.find_last_not_of(" \t") + 1);
	auto it = groups_.find(lower(g));
	if (it == groups_.end()) return;
	std::set<std::string> drop;
	for (const auto& t : it->second.tcls) drop.insert(lower(t));
	pool_.erase(std::remove_if(pool_.begin(), pool_.end(), [&](const std::string& t) { return drop.count(lower(t)) > 0; }), pool_.end());
}

void LevelGen::poolObjFilter(const std::vector<std::string>& classes)
{ // lg_tp_objfilter {Troll Zwerg}: bez szablonów z obiektami tych klas
	std::set<std::string> drop;
	for (const auto& c : classes) drop.insert(lower(c));
	if (drop.empty()) return;
	pool_.erase(std::remove_if(pool_.begin(), pool_.end(),
	                [&](const std::string& t) {
		                Piece* p = resolve(t);
		                if (!p) return false;
		                for (const auto& [cls, n] : p->objCounts)
			                if (n > 0 && drop.count(lower(cls))) return true;
		                return false;
	                }),
	    pool_.end());
}

std::string LevelGen::pickStartPiece(int side, bool downThrough)
{
	std::vector<std::string> pick;
	for (const auto& t : pool_)
	{
		Piece* p = resolve(t);
		if (!p || p->w != 4 || p->h != 4) continue;
		if (downThrough)
		{
			if (p->edges.size() != 2) continue;
			int d0 = p->edges[0].dir, d1 = p->edges[1].dir;
			if (!((d0 == 0 && d1 == 2) || (d0 == 2 && d1 == 0))) continue;
		}
		else if (p->edges.size() != 1 || p->edges[0].dir != side)
			continue;
		pick.push_back(t);
	}
	if (pick.empty() || !rnd) return "";
	return pick[size_t((rnd() & 0x7fff) * int(pick.size())) >> 15];
}

bool LevelGen::probeForcedPose(const std::string& tcl, int wx, int wy)
{
	Piece* p = resolve(tcl, true);
	if (!p || p->w <= 0 || p->h <= 0 || ((wx | wy) & 3)) return false;
	if (mapW > 0 && mapH > 0 && (wx < 8 || wy < 8 || wx + p->w >= mapW - 8 || wy + p->h >= mapH - 8)) return false;
	if (tileTemplated)
		for (int y = wy; y < wy + p->h; ++y)
			for (int x = wx; x < wx + p->w; ++x)
				if (tileTemplated(x, y)) return false;
	return true;
}

bool LevelGen::tempSize(const std::string& tcl, int& w, int& h)
{
	Piece* p = resolve(tcl, true);
	if (!p || p->w <= 0 || p->h <= 0) return false;
	w = p->w, h = p->h;
	return true;
}

// lg_start (FUN_00589790): elementy wymuszone, potem jeden klaster dokowany od czoła frontu (story.tcl: ratioA 0).
bool LevelGen::start()
{
	level_.clear(), levelPieces_.clear(), placed_.clear(), frontier_.clear(), objPieces_.clear(), objPlaced_.clear();
	for (auto& [k, p] : pieces_) p.placedThisLevel = 0;
	aw_ = std::max(0, ax1_ - ax0_), ah_ = std::max(0, ay1_ - ay0_);
	lastFail.clear();
	if (aw_ < 4 || ah_ < 4 || pool_.empty())
	{
		lastFail = "obszar " + std::to_string(aw_) + "×" + std::to_string(ah_) + ", pula " + std::to_string(pool_.size());
		return false;
	}
	grid_.assign(size_t(aw_) * size_t(ah_), 0);
	filled_ = 0, seedState_ = -1;
	bool anyForced = false;
	for (const auto& f : forced_)
	{
		Piece* p = resolve(f.tcl, true);
		if (!p || p->w <= 0 || p->h <= 0)
		{
			lastFail = "wymuszony " + f.tcl + ": nieznany";
			return fail();
		}
		if (!forcePlace(*p, f.gx, f.gy))
		{
			lastFail = "wymuszony " + f.tcl + " nie mieści się w (" + std::to_string(f.gx) + ", " + std::to_string(f.gy) + ")";
			return fail();
		}
		anyForced = true;
	}
	bool ok = true;
	std::vector<std::pair<long long, Piece*>> costs;
	do
	{
		if (!seedOrAdvance(anyForced))
		{
			lastFail = "ziarno";
			return fail();
		}
		ok = true;
		for (int guard = 0; !frontier_.empty() && guard < 8192; ++guard)
		{
			Door d = frontier_[0]; // zawsze czoło frontu
			costs.clear();
			for (const auto& t : pool_)
				if (Piece* c = resolve(t); c && c->w > 0 && c->h > 0) costs.push_back({cost(*c, d), c});
			std::stable_sort(costs.begin(), costs.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
			ok = false;
			for (auto& [k, c] : costs)
			{
				for (const auto& e : c->edges)
				{
					if (((e.dir - d.dir) & 3) != 2) continue; // tylko krawędź naprzeciw drzwi
					if (tryPlace(*c, d.x - e.lx, d.y - e.ly, 4))
					{
						ok = true;
						break;
					}
				}
				if (ok) break;
			}
			if (!ok) break; // czoła nie da się zadokować
		}
	} while (float(filled_) / float(aw_ * ah_) < ratioA_ && ok);
	if (!ok)
	{
		lastFail = "zamknięcie: " + std::to_string(frontier_.size()) + " otwartych drzwi, " + std::to_string(placed_.size()) + " elementów";
		return fail();
	}
	for (const auto& pl : placed_)
	{
		level_.push_back({pl.p->tcl, ax0_ + pl.gx, ay0_ + pl.gy, pl.p->w, pl.p->h});
		levelPieces_.push_back(pl.p);
	}
	return !placed_.empty();
}

bool LevelGen::fail()
{
	placed_.clear(), level_.clear(), levelPieces_.clear();
	return false;
}

void LevelGen::commitSessionEmission()
{
	for (Piece* p : levelPieces_)
		if (p->objectBearing) ++p->emittedThisSession;
}

// FUN_005862c0: z elementem wymuszonym ziarno już jest; inaczej pierwszy element puli 4 × 4 z dwojgiem drzwi
// w ≤ 16 losowych miejscach wyrównanych do 4 (co najmniej 4 pola od brzegu obszaru).
bool LevelGen::seedOrAdvance(bool anyForced)
{
	if (seedState_ == -1 && anyForced)
	{
		seedState_ = -2;
		return true;
	}
	Piece* seed = nullptr;
	for (const auto& t : pool_)
		if (Piece* p = resolve(t); p && p->w == 4 && p->h == 4 && p->edges.size() == 2)
		{
			seed = p;
			break;
		}
	if (!seed && !pool_.empty()) seed = resolve(pool_[0]);
	if (!seed || seed->w <= 0 || seed->h <= 0 || !rnd) return false;
	int spanX = aw_ - seed->w - 4, spanY = ah_ - seed->h - 4;
	if (spanX <= 0 || spanY <= 0) return false;
	int tries = std::max(1, std::min(16, ((spanX >> 2) * spanY) >> 2));
	for (int t = 0; t < tries; ++t)
	{
		int x = (((rnd() & 0x7fff) % spanX) & ~3) + 4;
		int y = (((rnd() & 0x7fff) % spanY) & ~3) + 4;
		if (tryPlace(*seed, x, y, 0))
		{
			seedState_ = -2;
			return true;
		}
	}
	return false;
}

// FUN_005842c0. Siatka: 0 wolne, 2 zajęte (wnętrze i marginesy), 0x80 rezerwacja na czas walidacji.
bool LevelGen::tryPlace(Piece& p, int gx, int gy, int margin)
{
	if ((gx | gy) & 3) return false;
	margin &= ~3;
	if (gx < 0 || gy < 0 || gx + p.w > aw_ || gy + p.h > ah_) return false;
	for (int y = gy; y < gy + p.h; ++y)
		for (int x = gx; x < gx + p.w; ++x)
			if (grid_[size_t(y) * aw_ + x]) return false;
	mergeEdges(p, gx, gy);
	int f = p.openSides;
	int wy0 = (f & 1) ? gy : std::max(0, gy - margin), wy1 = (f & 4) ? gy + p.h : std::min(ah_, gy + p.h + margin);
	int wx0 = (f & 8) ? gx : std::max(0, gx - margin), wx1 = (f & 2) ? gx + p.w : std::min(aw_, gx + p.w + margin);
	for (int y = wy0; y < wy1; ++y)
		for (int x = wx0; x < wx1; ++x) grid_[size_t(y) * aw_ + x] |= 0x80;
	if (validateFrontier())
	{
		for (int y = wy0; y < wy1; ++y)
			for (int x = wx0; x < wx1; ++x) grid_[size_t(y) * aw_ + x] = 2;
		filled_ += p.w * p.h;
		for (const auto& [cls, n] : p.objCounts) objPlaced_[cls] += float(n);
		++p.placedThisLevel;
		placed_.push_back({&p, gx, gy});
		if (p.objectBearing) objPieces_.push_back({gx, gy, float(p.w * p.h)});
		return true;
	}
	for (int y = wy0; y < wy1; ++y)
		for (int x = wx0; x < wx1; ++x) grid_[size_t(y) * aw_ + x] &= 0x7f;
	mergeEdges(p, gx, gy); // cofnięcie przełączenia
	return false;
}

// FUN_005848b0: element wymuszony, okno przycięte do obszaru, ≥ 8 pól od brzegów mapy, bez pól pod szablonem.
bool LevelGen::forcePlace(Piece& p, int gx, int gy)
{
	if ((gx | gy) & 3) return false;
	int wx = ax0_ + gx, wy = ay0_ + gy;
	if (mapW > 0 && mapH > 0 && (wx < 8 || wy < 8 || wx + p.w >= mapW - 8 || wy + p.h >= mapH - 8)) return false;
	if (tileTemplated)
		for (int y = wy; y < wy + p.h; ++y)
			for (int x = wx; x < wx + p.w; ++x)
				if (tileTemplated(x, y)) return false;
	int cx0 = std::max(0, gx), cy0 = std::max(0, gy), cx1 = std::min(aw_, gx + p.w), cy1 = std::min(ah_, gy + p.h);
	for (int y = cy0; y < cy1; ++y)
		for (int x = cx0; x < cx1; ++x)
			if (grid_[size_t(y) * aw_ + x]) return false;
	mergeEdges(p, gx, gy);
	for (int y = cy0; y < cy1; ++y)
		for (int x = cx0; x < cx1; ++x) grid_[size_t(y) * aw_ + x] = 2;
	if (!validateFrontier())
	{
		for (int y = cy0; y < cy1; ++y)
			for (int x = cx0; x < cx1; ++x) grid_[size_t(y) * aw_ + x] = 0;
		mergeEdges(p, gx, gy);
		return false;
	}
	filled_ += (cx1 - cx0) * (cy1 - cy0);
	placed_.push_back({&p, gx, gy});
	return true;
}

// FUN_00583f60: drzwi w tym samym polu co otwarte drzwi frontu zamykają je (para), inne dochodzą do frontu.
void LevelGen::mergeEdges(const Piece& p, int dx, int dy)
{
	for (const auto& e : p.edges)
	{
		int x = e.lx + dx, y = e.ly + dy;
		auto it = std::find_if(frontier_.begin(), frontier_.end(), [&](const Door& d) { return d.x == x && d.y == y; });
		if (it != frontier_.end())
		{
			*it = frontier_.back();
			frontier_.pop_back();
		}
		else
			frontier_.push_back({x, y, e.dir});
	}
}

// FUN_00584120: każde otwarte drzwi ≥ 2 pola od brzegu obszaru i z co najmniej jednym wolnym z 4 pól wokół.
bool LevelGen::validateFrontier() const
{
	for (const auto& d : frontier_)
	{
		if (d.x < 2 || d.y < 2 || d.x > aw_ - 2 || d.y > ah_ - 2) return false;
		auto g = [&](int x, int y) { return grid_[size_t(y) * aw_ + x]; };
		if (g(d.x - 1, d.y - 1) && g(d.x, d.y - 1) && g(d.x - 1, d.y) && g(d.x, d.y)) return false;
	}
	return true;
}

// FUN_00586710: koszt dopasowania elementu do drzwi (mniej = lepiej). Stałe z .rdata gry za Diggles-Unity.
long long LevelGen::cost(const Piece& p, const Door& d)
{
	const long long kMax = INT_MAX;
	for (const auto& cls : filterClasses_)
	{ // limity klas obiektów
		auto c = objCons_.find(cls);
		if (c == objCons_.end()) continue;
		float placed = objPlaced_.count(cls) ? objPlaced_[cls] : 0.f;
		int add = p.objCounts.count(cls) ? p.objCounts.at(cls) : 0;
		if (placed + float(add) > c->second.max) return kMax;
	}
	if (p.objectBearing)
	{ // wariant z obiektami raz na sesję i z odstępem od innych
		if (p.emittedThisSession > 0) return kMax;
		for (const auto& o : objPieces_)
			if (float(std::abs(o.x - d.x)) * float(std::abs(o.y - d.y)) < o.r * 1.5f) return kMax;
	}
	float total = float(aw_ * ah_);
	float progress = ratioOpt_ == 0.f ? 1.f : (float(filled_) / total) / ratioOpt_;
	if (p.w == 4 && p.h == 4) return 0x7ffffff9LL + long(p.edges.size()); // ziarna na koniec
	int area = p.w * p.h;
	float over = 1.f;
	if (area && ratioMax_ != 0.f)
	{
		over = (float(filled_ + area) / total) / ratioMax_;
		if (over > 1.f) return kMax;
	}
	if (p.levelCap <= p.placedThisLevel) return kMax;
	float sizeTerm = float(std::abs(20 - p.h - p.w)) * (2.f - progress);
	int edges = int(p.edges.size());
	float band = 0.f;
	if (progress >= 0.f && progress < 0.3f) band = std::fabs(3.f - float(edges));
	else if (progress >= 0.3f && progress < 0.6f) band = std::fabs(2.5f - float(edges));
	else if (progress >= 0.6f && progress < 0.8f) band = std::fabs(2.f - float(edges));
	else if (progress >= 0.8f) band = std::fabs(1.f - float(edges)) * 10.f; // pod koniec zamykające
	float sum = 0.f;
	int n = 0;
	for (const auto& cls : filterClasses_)
	{
		auto c = objCons_.find(cls);
		if (c == objCons_.end() || c->second.stride == -1.f) continue;
		++n;
		if (c->second.stride == 0.f)
		{
			sum += 1.3f;
			continue;
		}
		float placed = objPlaced_.count(cls) ? objPlaced_[cls] : 0.f;
		int add = p.objCounts.count(cls) ? p.objCounts.at(cls) : 0;
		float a1 = progress != 0.f ? std::fabs(c->second.stride - placed / progress) : 0.f;
		float a2 = std::fabs(c->second.stride - (placed + float(add)) / over);
		sum += (a2 - a1) / c->second.stride;
	}
	float objPressure = n == 0 ? 1.f : std::fabs(sum / float(n) + 1.f);
	auto g = [&](int x, int y) { return grid_[size_t(y) * aw_ + x]; };
	if (p.tunnelFlag == 1)
	{
		if (d.dir != 0 && d.dir != 2) return kMax;
		if (d.y - 4 < 0 || d.y + 4 >= ah_ || d.x < 0 || d.x >= aw_) return kMax;
		if (g(d.x, d.y - 4) || g(d.x, d.y + 4)) return 0x7ffffffe;
	}
	else if (p.tunnelFlag == 2)
	{
		if (d.dir != 1 && d.dir != 3) return kMax;
		if (d.x - 4 < 0 || d.x + 4 >= aw_ || d.y < 0 || d.y >= ah_) return kMax;
		if (g(d.x - 4, d.y) || g(d.x + 4, d.y)) return 0x7ffffffe;
	}
	float capTerm = float(p.objectBearing ? std::min(p.gameCap, 5) : 5) * 200.f;
	float repeat = float(p.placedThisLevel) * 250.f;
	float jitter = float(rnd ? (rnd() & 0x7fff) : 0) * (1.f / 32768.f) * (1.f - progress);
	double c = (2.0 * sizeTerm + band * 100.0 + capTerm + repeat + jitter + 0.5) * p.value * 100.0 * objPressure;
	if (c >= double(INT_MAX)) return INT_MAX - 8;
	return std::llround(c);
}

} // namespace ow
