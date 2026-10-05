#include "lib/map_world.h"

#include "lib/blockdb.h"
#include "lib/save_game.h"
#include "lib/vfs.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <climits>
#include <sstream>

namespace ow
{

bool isTransparentTile(const Pmp& tpl, int x, int y)
{
	for (int dy = 0; dy <= 1; ++dy)
		for (int dx = 0; dx <= 1; ++dx)
		{
			int xx = x + dx, yy = y + dy;
			if (xx >= int(tpl.width) || yy >= int(tpl.height)) continue;
			if (tpl.type(xx, yy) & 0x7F) return false;
		}
	return true;
}

Pmp makeWorldGrid(int width, int height)
{
	Pmp w;
	w.width = uint32_t(width);
	w.height = uint32_t(height);
	size_t n = size_t(width) * height;
	w.types.assign(n, 0);
	w.blocks.assign(n, 0);
	w.layer2.assign(n, 4);
	return w;
}

void blitTemplate(Pmp& world, L2m& worldLayer, const Pmp& tpl, const L2m* tplLayer, int ox, int oy)
{
	for (int py = 0; py < int(tpl.height); ++py)
		for (int px = 0; px < int(tpl.width); ++px)
		{
			int x = ox + px, y = oy + py;
			if (x < 0 || y < 0 || x >= int(world.width) || y >= int(world.height)) continue;
			if (isTransparentTile(tpl, px, py)) continue;
			size_t d = size_t(y) * world.width + x, s = size_t(py) * tpl.width + px;
			world.blocks[d] = tpl.blocks[s];
			world.types[d] = tpl.types[s];
			world.layer2[d] = tpl.layer2[s];
		}
	if (tplLayer && !tplLayer->values.empty() && worldLayer.width == world.width)
		for (int py = 0; py < int(tplLayer->height); ++py)
			for (int px = 0; px < int(tplLayer->width); ++px)
			{
				int x = ox + px, y = oy + py;
				if (x < 0 || y < 0 || x >= int(world.width) || y >= int(world.height)) continue;
				uint16_t v = tplLayer->values[size_t(py) * tplLayer->width + px];
				if (v) worldLayer.values[size_t(y) * worldLayer.width + x] = v;
			}
}

std::optional<std::vector<uint8_t>> readTemplateData(const Vfs& vfs, const std::string& name)
{
	if (auto d = vfs.read("Data/Templates/" + name)) return d;
	if (auto d = vfs.read(name)) return d;
	size_t slash = name.find_last_of("/\\");
	if (slash != std::string::npos) return vfs.read("Data/Templates/" + name.substr(slash + 1));
	return std::nullopt;
}

namespace
{

std::string fileName(const std::string& p)
{
	size_t s = p.find_last_of("/\\");
	return s == std::string::npos ? p : p.substr(s + 1);
}

void parseInto(const Vfs& vfs, const std::string& gamePath, Scenario& sc, int depth)
{
	if (depth > 4) return;
	auto data = vfs.read(gamePath);
	if (!data) return;
	std::istringstream in(std::string(data->begin(), data->end()));
	std::string line, pending;
	while (std::getline(in, line))
	{
		if (!line.empty() && line.back() == '\r') line.pop_back();
		std::istringstream ws(line);
		std::string cmd;
		ws >> cmd;
		if (cmd.empty() || cmd[0] == '#' || cmd.rfind("//", 0) == 0 || cmd.rfind(";#", 0) == 0) continue;
		if (cmd == "map")
		{
			std::string sub;
			int w = 0, h = 0;
			if (ws >> sub >> w >> h && sub == "create") sc.width = w, sc.height = h;
		}
		else if (cmd == "call")
		{
			std::string target;
			ws >> target;
			std::string low = Vfs::lower(target);
			if (low.find("templates/") != std::string::npos) pending = fileName(target);
			else if (low.find("gamesave/") != std::string::npos) parseInto(vfs, "Data/Gamesave/" + fileName(target), sc, depth + 1);
		}
		else if (cmd == "set_view" && !sc.view)
		{
			GameView v;
			if (ws >> v.x >> v.y >> v.zoom >> v.dx >> v.dy) sc.view = v;
		}
		else if (cmd == "set_light_begin")
		{
			int v;
			if (ws >> v) sc.lightBegin = v;
		}
		else if (cmd == "set_ground_begin")
		{
			int v;
			if (ws >> v) sc.groundBegin = v;
		}
		else if (cmd == "set_fow_begin")
		{
			int v;
			if (ws >> v) sc.fowBegin = v;
		}
		else if (cmd == "MapTemplateSet" && !pending.empty())
		{
			ScenarioPlacement p;
			p.script = pending;
			if (ws >> p.x >> p.y) sc.placements.push_back(p);
		}
		else if (cmd == "set_pos" && (line.find("[qnew ") != std::string::npos || line.find("[new ") != std::string::npos))
		{
			// set_pos [qnew Zwerg] {28.25 50.4 8.0}
			size_t a = line.find("new ") + 4, b = line.find_first_of(" ]", a);
			size_t lb = line.find('{', b), rb = line.find('}', lb == std::string::npos ? b : lb);
			if (b != std::string::npos && lb != std::string::npos && rb != std::string::npos)
			{
				TemplateObject o;
				o.className = line.substr(a, b - a);
				std::istringstream ps(line.substr(lb + 1, rb - lb - 1));
				if (ps >> o.pos[0] >> o.pos[1] >> o.pos[2]) sc.objects.push_back(o);
			}
		}
		else if (line.find("load_level") != std::string::npos || line.find("load_sandbox") != std::string::npos)
			sc.usesGenerator = true;
	}
}

} // namespace

void applySkyLight(Pmp& grid, int originY, int lightBegin, const BlockDb* db, int* litCount)
{
	const int W = int(grid.width), H = int(grid.height);
	constexpr int kScan = 6;      // 24 kroki po 2 drobne pola
	constexpr uint32_t kAir = 59; // blok otwartego powietrza
	constexpr int kOpenFill = 13; // drobne pole o wysokości < 0xD jest otwarte
	auto open = [&](uint32_t block) {
		if (block == kAir) return true;
		const BlockRecord* r = db ? db->record(block) : nullptr;
		if (!r) return false;
		for (uint8_t d : r->depth)
			if (d < kOpenFill) return true;
		return false;
	};
	int lit = 0;
	for (int y = 0; y < H && originY + y < lightBegin; ++y)
		for (int x = 0; x < W; ++x)
		{
			bool sky = false;
			for (int k = 0; k <= kScan && !sky; ++k)
			{
				int wy = originY + y - k; // wiersz w świecie
				if (wy < 0) sky = true;   // skan wyszedł ponad świat
				else if (y - k >= 0) sky = open(grid.blocks[size_t(y - k) * W + x]);
				// poza przycięciem (nad nim) jest domyślny świat: lita skała
			}
			if (sky)
			{
				grid.layer2[size_t(y) * W + x] = uint8_t(kDaylight);
				++lit;
			}
		}
	if (litCount) *litCount = lit;
}

Scenario parseScenario(const Vfs& vfs, const std::string& gamePath)
{
	Scenario sc;
	parseInto(vfs, gamePath, sc, 0);
	return sc;
}

BuiltWorld buildWorld(const Vfs& vfs, const Scenario& sc, int margin, const BlockDb* db)
{
	BuiltWorld out;
	struct Loaded
	{
		ScenarioPlacement place;
		TemplateScript script;
		Pmp pmp;
		std::optional<L2m> l2m;
	};
	std::vector<Loaded> loaded;
	int x0 = INT_MAX, y0 = INT_MAX, x1 = INT_MIN, y1 = INT_MIN;
	for (const auto& p : sc.placements)
	{
		try
		{
			auto text = vfs.read("Data/Templates/" + p.script);
			if (!text) throw std::runtime_error("brak skryptu");
			Loaded l{p, parseTemplateScript(std::string(text->begin(), text->end())), {}, {}};
			auto pd = readTemplateData(vfs, l.script.pmpFile);
			if (!pd) throw std::runtime_error("brak " + l.script.pmpFile);
			l.pmp = parsePmp(*pd);
			if (!l.script.l2mFile.empty())
				if (auto ld = readTemplateData(vfs, l.script.l2mFile)) l.l2m = parseL2m(*ld);
			x0 = std::min(x0, p.x);
			y0 = std::min(y0, p.y);
			x1 = std::max(x1, p.x + int(l.pmp.width));
			y1 = std::max(y1, p.y + int(l.pmp.height));
			loaded.push_back(std::move(l));
			out.placedScripts.push_back(p.script);
		}
		catch (const std::exception& e)
		{
			out.errors.push_back(p.script + ": " + e.what());
		}
	}
	if (loaded.empty()) return out;
	// Przycięcie do użytego obszaru (świat gry bywa 900×900 bloków).
	x0 -= margin;
	y0 -= margin;
	x1 += margin;
	y1 += margin;
	if (sc.width > 0) x0 = std::max(x0, 0), x1 = std::min(x1, sc.width);
	if (sc.height > 0) y0 = std::max(y0, 0), y1 = std::min(y1, sc.height);
	out.originX = x0;
	out.originY = y0;
	if (sc.view)
	{
		out.view = sc.view;
		out.view->x -= float(x0);
		out.view->y -= float(y0);
	}
	out.grid = makeWorldGrid(x1 - x0, y1 - y0);
	out.layer.width = out.grid.width;
	out.layer.height = out.grid.height;
	out.layer.values.assign(size_t(out.grid.width) * out.grid.height, 0);
	for (const auto& l : loaded)
	{
		int ox = l.place.x - x0, oy = l.place.y - y0;
		blitTemplate(out.grid, out.layer, l.pmp, l.l2m ? &*l.l2m : nullptr, ox, oy);
		for (auto o : l.script.objects)
		{
			o.pos[0] += float(ox);
			o.pos[1] += float(oy);
			out.objects.push_back(o);
		}
	}
	for (auto o : sc.objects)
	{
		o.pos[0] -= float(x0);
		o.pos[1] -= float(y0);
		out.objects.push_back(o);
	}
	applySkyLight(out.grid, y0, sc.lightBegin, db, &out.skyTiles);
	return out;
}

BuiltWorld buildWorldFromSave(const SaveGame& save, const BlockDb* db, Scenario& sc)
{
	BuiltWorld out;
	sc = Scenario{};
	sc.width = save.width;
	sc.height = save.height;
	sc.lightBegin = save.lightBegin;
	if (save.groundBegin != -10000 && save.groundBegin != save.lightBegin) sc.groundBegin = save.groundBegin;
	out.grid = makeWorldGrid(save.width, save.height);
	out.layer.width = out.grid.width;
	out.layer.height = out.grid.height;
	out.layer.values.assign(save.tiles.size(), 0);
	static const uint8_t kType[4] = {4, 3, 2, 5};
	for (size_t i = 0; i < save.tiles.size(); ++i)
	{
		const SaveTile& t = save.tiles[i];
		out.grid.blocks[i] = t.block;
		out.grid.types[i] = uint8_t(kType[t.material & 3] | (t.flags & 0x80));
		out.layer.values[i] = t.cell;
		if (t.fluidType && t.fluidFill > 0.001f)
		{
			if (out.fluidType.empty()) out.fluidType.assign(save.tiles.size(), 0), out.fluidFill.assign(save.tiles.size(), 0.f);
			out.fluidType[i] = t.fluidType;
			out.fluidFill[i] = t.fluidFill;
		}
	}
	for (const SaveObject& o : save.objects)
	{
		TemplateObject t;
		t.className = o.className;
		t.name = std::to_string(o.id);
		t.owner = o.owner;
		t.linkParent = o.linkParent;
		t.linkPoint = o.linkPoint;
		t.tclVars = o.tclVars;
		t.instanceName = o.instanceName;
		for (int k = 0; k < 3; ++k)
		{
			t.pos[k] = o.pos[k]; // zapis trzyma z w skali skryptu (jak argument new)
			t.rot[k] = o.rot[k];
		}
		out.objects.push_back(t);
	}
	// Dekoracje spoza listy obiektów: „nasiona” z kubełków zawsze; obiekty zastępcze tylko wtedy, gdy
	// obiekt, którego są kopią, nie istnieje (inaczej rysowalibyśmy go dwa razy).
	std::unordered_set<int> ids;
	for (const SaveObject& o : save.objects) ids.insert(o.id);
	for (const SaveDeco& d : save.decos)
	{
		if (d.proxy && ids.count(d.link)) continue;
		TemplateObject t;
		t.className = d.className;
		t.name = d.proxy ? "kopia" : "dekoracja";
		for (int k = 0; k < 3; ++k)
		{
			t.pos[k] = d.pos[k];
			t.rot[k] = std::isfinite(d.rot[k]) ? d.rot[k] : 0.f;
		}
		out.objects.push_back(t);
	}
	GameView v;
	v.x = save.viewX;
	v.y = save.viewY;
	v.zoom = save.viewZoom;
	v.dx = save.viewDx;
	v.dy = save.viewDy;
	out.view = v;
	sc.view = v;
	out.placedScripts.push_back("zapis");
	applySkyLight(out.grid, 0, save.lightBegin, db, &out.skyTiles);
	return out;
}

} // namespace ow
