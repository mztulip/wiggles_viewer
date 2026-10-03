#include "lib/map_world.h"

#include "lib/vfs.h"

#include <algorithm>
#include <climits>
#include <sstream>

namespace ow {

bool isTransparentTile(const Pmp& tpl, int x, int y) {
	for (int dy = 0; dy <= 1; ++dy)
		for (int dx = 0; dx <= 1; ++dx) {
			int xx = x + dx, yy = y + dy;
			if (xx >= int(tpl.width) || yy >= int(tpl.height)) continue;
			if (tpl.type(xx, yy) & 0x7F) return false;
		}
	return true;
}

Pmp makeWorldGrid(int width, int height) {
	Pmp w;
	w.width = uint32_t(width);
	w.height = uint32_t(height);
	size_t n = size_t(width) * height;
	w.types.assign(n, 0);
	w.blocks.assign(n, 0);
	w.layer2.assign(n, 4);
	return w;
}

void blitTemplate(Pmp& world, L2m& worldLayer, const Pmp& tpl, const L2m* tplLayer, int ox, int oy) {
	for (int py = 0; py < int(tpl.height); ++py)
		for (int px = 0; px < int(tpl.width); ++px) {
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
			for (int px = 0; px < int(tplLayer->width); ++px) {
				int x = ox + px, y = oy + py;
				if (x < 0 || y < 0 || x >= int(world.width) || y >= int(world.height)) continue;
				uint16_t v = tplLayer->values[size_t(py) * tplLayer->width + px];
				if (v) worldLayer.values[size_t(y) * worldLayer.width + x] = v;
			}
}

std::optional<std::vector<uint8_t>> readTemplateData(const Vfs& vfs, const std::string& name) {
	if (auto d = vfs.read("Data/Templates/" + name)) return d;
	if (auto d = vfs.read(name)) return d;
	size_t slash = name.find_last_of("/\\");
	if (slash != std::string::npos) return vfs.read("Data/Templates/" + name.substr(slash + 1));
	return std::nullopt;
}

namespace {

std::string fileName(const std::string& p) {
	size_t s = p.find_last_of("/\\");
	return s == std::string::npos ? p : p.substr(s + 1);
}

void parseInto(const Vfs& vfs, const std::string& gamePath, Scenario& sc, int depth) {
	if (depth > 4) return;
	auto data = vfs.read(gamePath);
	if (!data) return;
	std::istringstream in(std::string(data->begin(), data->end()));
	std::string line, pending;
	while (std::getline(in, line)) {
		if (!line.empty() && line.back() == '\r') line.pop_back();
		std::istringstream ws(line);
		std::string cmd;
		ws >> cmd;
		if (cmd.empty() || cmd[0] == '#' || cmd.rfind("//", 0) == 0 || cmd.rfind(";#", 0) == 0) continue;
		if (cmd == "map") {
			std::string sub;
			int w = 0, h = 0;
			if (ws >> sub >> w >> h && sub == "create") sc.width = w, sc.height = h;
		} else if (cmd == "call") {
			std::string target;
			ws >> target;
			std::string low = Vfs::lower(target);
			if (low.find("templates/") != std::string::npos) pending = fileName(target);
			else if (low.find("gamesave/") != std::string::npos) parseInto(vfs, "Data/Gamesave/" + fileName(target), sc, depth + 1);
		} else if (cmd == "set_view" && !sc.view) {
			GameView v;
			if (ws >> v.x >> v.y >> v.zoom >> v.dx >> v.dy) sc.view = v;
		} else if (cmd == "MapTemplateSet" && !pending.empty()) {
			ScenarioPlacement p;
			p.script = pending;
			if (ws >> p.x >> p.y) sc.placements.push_back(p);
		} else if (line.find("load_level") != std::string::npos || line.find("load_sandbox") != std::string::npos)
			sc.usesGenerator = true;
	}
}

} // namespace

Scenario parseScenario(const Vfs& vfs, const std::string& gamePath) {
	Scenario sc;
	parseInto(vfs, gamePath, sc, 0);
	return sc;
}

BuiltWorld buildWorld(const Vfs& vfs, const Scenario& sc, int margin) {
	BuiltWorld out;
	struct Loaded {
		ScenarioPlacement place;
		TemplateScript script;
		Pmp pmp;
		std::optional<L2m> l2m;
	};
	std::vector<Loaded> loaded;
	int x0 = INT_MAX, y0 = INT_MAX, x1 = INT_MIN, y1 = INT_MIN;
	for (const auto& p : sc.placements) {
		try {
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
		} catch (const std::exception& e) {
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
	if (sc.view) {
		out.view = sc.view;
		out.view->x -= float(x0);
		out.view->y -= float(y0);
	}
	out.grid = makeWorldGrid(x1 - x0, y1 - y0);
	out.layer.width = out.grid.width;
	out.layer.height = out.grid.height;
	out.layer.values.assign(size_t(out.grid.width) * out.grid.height, 0);
	for (const auto& l : loaded) {
		int ox = l.place.x - x0, oy = l.place.y - y0;
		blitTemplate(out.grid, out.layer, l.pmp, l.l2m ? &*l.l2m : nullptr, ox, oy);
		for (auto o : l.script.objects) {
			o.pos[0] += float(ox);
			o.pos[1] += float(oy);
			out.objects.push_back(o);
		}
	}
	return out;
}

} // namespace ow
