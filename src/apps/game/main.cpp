// openwiggles: gra (pierwsza grywalna wersja, docs/silnik/14_gra.md).
//
// Użycie:
//   openwiggles [--game <katalog gry>] [--scenario NAZWA | --save PLIK.sav | --template SZABLON]
//               [--renderer vulkan|opengl] [--size SZERxWYS] [--lang KOD] [--locale KOD] [--screenshot plik.png]
//               [--view X Y ZOOM DX DY] [--warmup SEKUNDY] [--load plik.ows] [--save-to plik.ows]
// F5 / F9: szybki zapis i wczytanie najnowszego zapisu (~/.local/share/openwiggles/saves/, .sav jak w grze albo .ows).
// Bez --game: zmienna WIGGLES_DIR, potem OW_GAME, potem ~/.wine/drive_c/Program Files (x86)/Innonics/Wiggles.
// --overlay KATALOG: nakładka w układzie modów (Data/ i Data_<język> przykrywają pliki gry), można podać kilka razy.
// --no-border-mask: bez czarnej ramki ekranu z Data/GUI/bordermask.tga (w grze rejestr Performance\BorderMask, domyślnie 1).
// --locale: język tekstów gry (lmsg, MESSAGES.TXT); domyślnie język interfejsu, jeśli dane go mają, inaczej język wydania.
// Bez mapy: najnowszy zapis z Data/Gamesave, a gdy go nie ma, szablon kris_unq_060 z jednym krasnalem.
//
// Sterowanie: LPM zaznacza krasnala, PPM na wolnym polu = idź, PPM na skale = kop.
// WASD / strzałki / środkowy przycisk myszy przesuwają kamerę, kółko przybliża, N = nowy krasnal
// przy kursorze, Esc wychodzi.
//
// Scena i rysowanie: MapScene (lib/map_scene.h). 🔶 Tymczasowo: ruch po siatce pól (lib/nav.h), kopanie
// zamienia blok na pusty (59). Zachowania krasnali z Tcl (Scripts/classes/zwerg) jeszcze nie działają.
#include "lib/blockdb.h"
#include "lib/render/border_mask.h"
#include "lib/render/model_renderer.h"
#include "lib/render/texture_cache.h"
#include "lib/i18n.h"
#include "lib/map_template.h"
#include "lib/model3db.h"
#include "lib/nav.h"
#include "lib/png_write.h"
#include "lib/render/rhi/rhi.h"
#include "lib/script/engine.h"
#include "lib/sphere.h"
#include "lib/vfs.h"
#include "ow_version.h"
#include "lib/map_scene.h"
#include "lib/world_sim.h"

#include <SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <filesystem>
#include <set>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

using namespace ow;

namespace
{

constexpr float kBlock = MapScene::kBlock;
constexpr float kPi = kSimPi; // stałe ruchu krasnali: lib/world_sim.h

struct Args
{
	std::string game, scenario, save, templ, screenshot, lang, locale;
	std::vector<std::string> overlays;
	rhi::Backend renderer = rhi::Backend::Vulkan;
	bool rendererForced = false;
	int width = 1280, height = 800;
	bool scripts = true;          // --no-scripts: bez silnika Tcl
	bool scriptGnomes = true;     // --player-gnomes: krasnale tylko na rozkaz gracza, bez skryptów
	bool reveal = false;          // --reveal: bez nieodkrytego terenu
	bool borderMask = true;       // --no-border-mask: bez ramki ekranu
	float warmup = 0;            // --warmup S: przewiń symulację o S sekund (zrzuty, testy)
	std::string load, saveTo;     // --load plik.ows, --save-to plik.ows (zapis po --warmup; testy)
	std::optional<GameView> view; // --view x y zoom dx dy
};

bool parseArgs(int argc, char** argv, Args& a)
{
	for (int i = 1; i < argc; ++i)
	{
		std::string k = argv[i];
		auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
		if (k == "--game") a.game = next();
		else if (k == "--scenario") a.scenario = next();
		else if (k == "--save") a.save = next();
		else if (k == "--template") a.templ = next();
		else if (k == "--screenshot") a.screenshot = next();
		else if (k == "--lang") a.lang = next();
		else if (k == "--locale") a.locale = next();
		else if (k == "--overlay") a.overlays.push_back(next());
		else if (k == "--size") std::sscanf(next(), "%dx%d", &a.width, &a.height);
		else if (k == "--warmup") a.warmup = float(std::atof(next()));
		else if (k == "--no-scripts") a.scripts = false;
		else if (k == "--no-border-mask") a.borderMask = false;
		else if (k == "--player-gnomes") a.scriptGnomes = false;
		else if (k == "--reveal") a.reveal = true;
		else if (k == "--load") a.load = next();
		else if (k == "--save-to") a.saveTo = next();
		else if (k == "--view")
		{
			GameView v;
			v.x = float(std::atof(next()));
			v.y = float(std::atof(next()));
			v.zoom = float(std::atof(next()));
			v.dx = float(std::atof(next()));
			v.dy = float(std::atof(next()));
			a.view = v;
		}
		else if (k == "--renderer")
		{
			if (!rhi::parseBackend(next(), a.renderer)) return false;
			a.rendererForced = true;
		}
		else
		{
			std::fprintf(stderr, "nieznana opcja %s\n", k.c_str());
			return false;
		}
	}
	if (a.game.empty())
	{
		if (const char* e = std::getenv("WIGGLES_DIR"); e && *e) a.game = e;
		else if (const char* e = std::getenv("OW_GAME")) a.game = e;
		else if (const char* h = std::getenv("HOME")) a.game = std::string(h) + "/.wine/drive_c/Program Files (x86)/Innonics/Wiggles";
	}
	return true;
}

// Przycięcie kątów kamery gry (docs/silnik/06_kamera.md §6.3).
float angleLimit(float R)
{
	return R > 15.6f + 1e-3f ? 2.23f / (R - 15.6f) : 1e9f;
}

// Kamera gry (set_view x y zoom dx dy): macierz rzutowania·widoku i macierz świata (gra -> OpenGL).
void gameCamera(const GameView& g, float aspect, glm::mat4& viewProj, glm::mat4& world)
{
	float Rb = std::clamp(g.zoom, kGameZoomMin, kGameZoomMax) * 16.f - 10.f;
	float R = std::max(0.5f, Rb) * kBlock;
	float lim = angleLimit(Rb), dx = std::clamp(g.dx, -lim, lim), dy = std::clamp(g.dy, -lim, lim);
	float cdx = std::cos(dx), sdx = std::sin(dx), cdy = std::cos(dy), sdy = -std::sin(dy);
	glm::vec3 eye(R * cdx * sdy, -R * sdx, R * cdx * cdy);
	glm::vec3 up(sdx * sdy, cdx, sdx * cdy);
	float tanV = std::tan(0.3f * aspect) / aspect;
	glm::mat4 proj = glm::perspective(2.f * std::atan(tanV), aspect, 0.5f * kBlock, 180.f * kBlock);
	viewProj = proj * glm::lookAt(eye, glm::vec3(0.f), up);
	world = glm::translate(glm::scale(glm::mat4(1.f), glm::vec3(1.f, -1.f, 1.f)), -glm::vec3(g.x, g.y, 14.f) * kBlock);
}

using Unit = SimUnit;


// Źródło mapy gry (do ponownego wczytania przy wczytywaniu zapisu .ows).
struct Source
{
	std::string kind; // save, savepath, template, scenario
	std::string name;
};

// Prosty zapis binarny: liczby little-endian, napisy z długością u32.
struct OwsWriter
{
	std::string buf;
	void u32(uint32_t v) { buf.append(reinterpret_cast<const char*>(&v), 4); }
	void i32(int32_t v) { u32(uint32_t(v)); }
	void f32(float v) { buf.append(reinterpret_cast<const char*>(&v), 4); }
	void f64(double v) { buf.append(reinterpret_cast<const char*>(&v), 8); }
	void str(const std::string& s) { u32(uint32_t(s.size())), buf += s; }
};
struct OwsReader
{
	std::string buf;
	size_t p = 0;
	bool ok = true;
	template <class T>
	T get()
	{
		T v{};
		if (p + sizeof(T) > buf.size())
		{
			ok = false;
			return v;
		}
		std::memcpy(&v, buf.data() + p, sizeof(T));
		p += sizeof(T);
		return v;
	}
	uint32_t u32() { return get<uint32_t>(); }
	int32_t i32() { return get<int32_t>(); }
	float f32() { return get<float>(); }
	double f64() { return get<double>(); }
	std::string str()
	{
		uint32_t n = u32();
		if (!ok || p + n > buf.size())
		{
			ok = false;
			return {};
		}
		std::string s = buf.substr(p, n);
		p += n;
		return s;
	}
};

// Gra = symulacja świata (lib/world_sim.h: skrypty, krasnale, kopanie) + gracz (zaznaczenie, rozkazy, menu, zapis).
struct Game : WorldSim
{
	Game() : WorldSim(maps) {} // maps powstaje po klasie bazowej; WorldSim tylko zapamiętuje referencję
	Vfs vfs;
	Source source;
	int baseObjects = 0;              // obiekty mapy źródłowej (dalej: utworzone w grze)
	std::vector<uint32_t> baseBlocks; // bloki i typy po wczytaniu źródła (różnice idą do zapisu)
	std::vector<uint8_t> baseTypes;
	int dragX0 = -1, dragY0 = -1;          // początek przeciągnięcia PPM (pole)
	bool rightDrag = false;
	bool paused = false;
	float speed = 1.f; // tempo gry: 1, 2, 4 (klawisze 1–3)
	std::unique_ptr<rhi::Device> device;
	std::unique_ptr<TextureCache> textures;
	std::vector<Vec3f> sphere;
	MapScene maps;
	BorderMask border; // ramka ekranu (docs/silnik/09_klatka.md §9.4)
	ModelDrawOptions draw;
	GameView cam;
	int hoverX = -1, hoverY = -1;
	bool showHelp = true;
	bool showMenu = false;  // menu wyboru gry (start bez źródła, Esc w grze)
	bool loaded = false;    // jest wczytany świat
	bool quit = false;
	Source pending;         // gra wybrana w menu, wczytywana po narysowaniu klatki „Wczytywanie…”
	int pendingFrames = 0;
	struct MenuEntry
	{
		Source src;
		std::string label;
	};
	std::vector<MenuEntry> menuOwn, menuGame, menuScen; // listy menu (odświeżane przy otwarciu)

	const Pmp* grid() const { return maps.grid(); }

	void initWorld()
	{
		initUnits();
		if (const auto& v = maps.view()) cam = *v;
		else if (const Pmp* g = grid()) cam = GameView{float(g->width) * 0.5f, float(g->height) * 0.5f, 1.38f, 0.f, 0.f};
		if (gnomeCount() == 0 && grid())
		{ // mapa bez krasnali: jeden przy środku kamery
			spawnUnit(cam.x, cam.y);
			cam.x = units.back().x, cam.y = units.back().y - 1.f;
		}
		status = gnomeCount() == 0 ? tr("Brak krasnali na mapie.") : "";
	}

	// Rozkaz: idź na pole albo kop pole skały.
	void order(int tx, int ty)
	{
		if (selected < 0 || selected >= int(units.size())) return;
		Unit& u = units[size_t(selected)];
		if (nav.open(tx, ty))
		{
			int gx = tx, gy = ty;
			for (int d = 0; d < 6 && !nav.standable(gx, gy) && !nav.climbable(gx, gy); ++d) ++gy; // cel w powietrzu: na ziemię
			auto p = nav.path(u.tx, u.ty, gx, gy);
			if (p.empty())
			{
				status = tr("Nie ma drogi.");
				return;
			}
			u.path = std::move(p), u.step = 0, u.state = Unit::Walk, u.digX = -1;
			u.goalX = u.goalZ = -1;
			status.clear();
			return;
		}
		// Kopanie: stanowisko obok pola (z boku, nad nim albo pod nim przy ścianie), najkrótsza droga.
		std::vector<std::pair<int, int>> best;
		const int cand[4][2] = {{tx - 1, ty}, {tx + 1, ty}, {tx, ty - 1}, {tx, ty + 1}};
		for (auto& c : cand)
		{
			if (!(nav.standable(c[0], c[1]) || nav.climbable(c[0], c[1]))) continue;
			auto p = nav.path(u.tx, u.ty, c[0], c[1]);
			if (!p.empty() && (best.empty() || p.size() < best.size())) best = std::move(p);
		}
		if (best.empty())
		{
			status = tr("Nie da się dojść do tej skały.");
			return;
		}
		u.path = std::move(best), u.step = 0, u.state = Unit::Walk, u.digX = tx, u.digY = ty;
		u.goalX = u.goalZ = -1;
		status.clear();
	}


	// Wczytanie mapy źródłowej i start świata (krasnale, skrypty). Używane przy starcie i przy wczytaniu zapisu.
	bool loadSource(const Source& s)
	{
		bool ok = false;
		if (s.kind == "savepath") ok = maps.loadSavePath(s.name);
		else if (s.kind == "template") ok = maps.loadTemplate(s.name);
		else ok = maps.loadScenario(s.name); // scenariusz .tcl albo zapis .sav z Data/Gamesave
		source = s;
		loaded = ok;
		initWorld();
		startScripts();
		baseObjects = maps.objectCount();
		if (const Pmp* g = grid()) baseBlocks = g->blocks, baseTypes = g->types;
		return ok;
	}

	static std::string saveDir()
	{
		const char* xdg = std::getenv("XDG_DATA_HOME");
		const char* home = std::getenv("HOME");
		return (xdg && *xdg ? std::string(xdg) : std::string(home ? home : ".") + "/.local/share") + "/openwiggles/saves";
	}

	// Zapis zgodny z grą (.sav, łata na wczytanym zapisie; rozdział 14.7). false = brak bazy (gra z szablonu).
	bool saveSav(const std::string& path, std::string& err)
	{
		const SaveGame* base = maps.baseSave();
		const Pmp* g = grid();
		if (!base || !g)
		{
			err = tr("gra nie pochodzi z zapisu .sav");
			return false;
		}
		SavePatch p;
		p.gameTime = base->gameTime + (scripts ? scripts->time() : 0.0);
		float v[5] = {cam.x, cam.y, cam.zoom, cam.dx, cam.dy};
		std::copy(v, v + 5, p.view);
		const auto& ft = maps.fluidTypes();
		const auto& ff = maps.fluidFills();
		const auto& fog = maps.fogBytes();
		const L2m* l2 = maps.layer();
		p.tiles.resize(g->blocks.size());
		for (size_t i = 0; i < p.tiles.size(); ++i)
			p.tiles[i] = {g->blocks[i], g->types[i], uint16_t(l2 && i < l2->values.size() ? l2->values[i] : 0),
			    uint8_t(i < ft.size() ? ft[i] : 0), i < ff.size() ? ff[i] : 0.f,
			    uint8_t(i < fog.size() ? fog[i] : base->tiles[i].fog[0])};
		for (int i = 0; i < baseObjects && i < maps.objectCount(); ++i)
		{
			char* end = nullptr;
			long id = std::strtol(maps.objectName(i).c_str(), &end, 10);
			if (maps.objectName(i).empty() || *end) continue;
			SavePatch::Object o;
			std::copy(maps.objectPos(i), maps.objectPos(i) + 3, o.pos);
			std::copy(maps.objectRot(i), maps.objectRot(i) + 3, o.rot);
			p.objects[int(id)] = o;
		}
		std::error_code ec;
		std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
		return writeSavePatch(*base, p, path, err);
	}

	// Szybki zapis: .sav, gdy gra pochodzi z zapisu gry, inaczej własny .ows (🔶 do zastąpienia).
	std::string quickSave()
	{
		char name[64];
		std::time_t t = std::time(nullptr);
		std::strftime(name, sizeof name, "quick_Quicksave_%Y%m%d_%H%M.sav", std::localtime(&t));
		std::string err;
		if (maps.baseSave())
		{
			std::string path = saveDir() + "/" + name;
			return saveSav(path, err) ? std::string(tr("Zapisano: ")) + path : std::string(tr("Błąd zapisu: ")) + err;
		}
		std::string path = saveDir() + "/quick.ows";
		return saveGame(path) ? std::string(tr("Zapisano (format tymczasowy .ows): ")) + path : std::string(tr("Błąd zapisu: ")) + path;
	}

	// Zapis .ows (rozdział 14.7): źródło + zmiany (bloki, płyny, obiekty, stan skryptów, kamera, kopanie).
	bool saveGame(const std::string& path)
	{
		const Pmp* g = grid();
		if (!g) return false;
		OwsWriter w;
		w.str("OWSAVE"), w.u32(1);
		w.str(source.kind), w.str(source.name);
		w.f64(scripts ? scripts->time() : 0.0);
		w.f32(cam.x), w.f32(cam.y), w.f32(cam.zoom), w.f32(cam.dx), w.f32(cam.dy);
		std::vector<size_t> changed;
		for (size_t i = 0; i < g->blocks.size() && i < baseBlocks.size(); ++i)
			if (g->blocks[i] != baseBlocks[i] || g->types[i] != baseTypes[i]) changed.push_back(i);
		w.u32(uint32_t(changed.size()));
		for (size_t i : changed) w.u32(uint32_t(i)), w.u32(g->blocks[i]), w.u32(g->types[i]);
		const auto& ft = maps.fluidTypes();
		const auto& ff = maps.fluidFills();
		std::vector<size_t> wet;
		for (size_t i = 0; i < ft.size() && i < ff.size(); ++i)
			if (ft[i] || ff[i] > 0) wet.push_back(i);
		w.u32(uint32_t(wet.size()));
		for (size_t i : wet) w.u32(uint32_t(i)), w.u32(ft[i]), w.f32(ff[i]);
		w.u32(uint32_t(maps.objectCount())), w.u32(uint32_t(baseObjects));
		for (int i = 0; i < maps.objectCount(); ++i)
		{
			const float* p = maps.objectPos(i);
			const float* r = maps.objectRot(i);
			w.str(maps.objectClass(i)), w.i32(idOf(i));
			for (int k = 0; k < 3; ++k) w.f32(p[k]);
			for (int k = 0; k < 3; ++k) w.f32(r[k]);
			script::ScriptObject* o = scripts ? scripts->object(idOf(i)) : nullptr;
			w.u32(o ? 1 : 0);
			if (o)
			{
				w.str(o->name), w.i32(o->owner), w.str(o->state), w.i32(o->stateCounter);
				w.str(scripts->captureVars(*o));
				w.u32(uint32_t(o->attribs.size()));
				for (auto& [k, v] : o->attribs) w.str(k), w.f64(v);
			}
		}
		w.u32(uint32_t(digMarks.size()));
		for (auto& [x, y] : digMarks) w.i32(x), w.i32(y);
		std::error_code ec;
		std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
		std::ofstream f(path, std::ios::binary);
		f.write(w.buf.data(), std::streamsize(w.buf.size()));
		return bool(f);
	}

	// Najnowszy zapis (.sav albo .ows) w katalogu zapisów; pusty, gdy brak.
	static std::string newestSave()
	{
		std::string best;
		std::filesystem::file_time_type bt{};
		std::error_code ec;
		for (auto& e : std::filesystem::directory_iterator(saveDir(), ec))
		{
			std::string ext = e.path().extension().string();
			if (ext != ".sav" && ext != ".ows") continue;
			auto t = e.last_write_time(ec);
			if (best.empty() || t > bt) best = e.path().string(), bt = t;
		}
		return best;
	}

	bool loadGame(const std::string& path)
	{
		if (path.size() > 4 && path.compare(path.size() - 4, 4, ".sav") == 0) return loadSource({"savepath", path});
		OwsReader r;
		{
			std::ifstream f(path, std::ios::binary);
			if (!f) return false;
			r.buf.assign(std::istreambuf_iterator<char>(f), {});
		}
		if (r.str() != "OWSAVE" || r.u32() != 1) return false;
		Source s;
		s.kind = r.str(), s.name = r.str();
		double t = r.f64();
		GameView c;
		c.x = r.f32(), c.y = r.f32(), c.zoom = r.f32(), c.dx = r.f32(), c.dy = r.f32();
		if (!r.ok || !loadSource(s)) return false;
		cam = c;
		Pmp* g = const_cast<Pmp*>(grid());
		for (uint32_t n = r.u32(), k = 0; k < n && r.ok; ++k)
		{
			uint32_t i = r.u32(), b = r.u32(), ty = r.u32();
			if (g && i < g->blocks.size()) maps.setBlock(int(i % g->width), int(i / g->width), b, uint8_t(ty));
		}
		maps.rebuildTerrain();
		if (g) nav.build(*g, maps.blockDb());
		{
			std::vector<uint8_t> ft(g ? g->blocks.size() : 0, 0);
			std::vector<float> ff(ft.size(), 0.f);
			for (uint32_t n = r.u32(), k = 0; k < n && r.ok; ++k)
			{
				uint32_t i = r.u32(), ty = r.u32();
				float f = r.f32();
				if (i < ft.size()) ft[i] = uint8_t(ty), ff[i] = f;
			}
			maps.setFluidState(ft, ff);
		}
		uint32_t count = r.u32(), base = r.u32();
		(void)base;
		struct Pending
		{
			int index;
			std::string vars, state;
			int counter;
		};
		std::vector<Pending> pending;
		for (uint32_t i = 0; i < count && r.ok; ++i)
		{
			std::string cls = r.str();
			int id = r.i32();
			float p[3], rot[3];
			for (float& v : p) v = r.f32();
			for (float& v : rot) v = r.f32();
			int idx = int(i);
			if (idx >= maps.objectCount())
			{ // obiekt utworzony w grze
				idx = maps.addObject(cls, p[0], p[1], p[2]);
				if (idx >= int(idxToId.size())) idxToId.resize(size_t(idx) + 1, 0);
				idxToId[size_t(idx)] = id, idToIdx[id] = idx;
				nextScriptId = std::max(nextScriptId, id + 1);
				if (Vfs::lower(cls) == "zwerg") addUnit(idx);
				if (scripts) scripts->attach(id, cls, "");
			}
			maps.setObjectPose(idx, p[0], p[1], p[2], rot[1]);
			if (Unit* u = unitOf(idx))
			{
				u->x = p[0], u->y = p[1], u->z = p[2], u->facing = rot[1];
				u->path.clear(), u->state = Unit::Idle, u->onArrive = nullptr;
				snapToFloor(*u);
			}
			if (r.u32())
			{
				std::string name = r.str();
				int owner = r.i32();
				std::string state = r.str();
				int counter = r.i32();
				std::string vars = r.str();
				script::ScriptObject* o = scripts ? scripts->object(id) : nullptr;
				for (uint32_t n = r.u32(), k = 0; k < n && r.ok; ++k)
				{
					std::string an = r.str();
					double av = r.f64();
					if (o) o->attribs[an] = av;
				}
				if (o)
				{
					o->name = name, o->owner = owner;
					pending.push_back({idx, vars, state, counter});
				}
			}
		}
		// Stan skryptów na końcu (zmienne mogą wskazywać obiekty utworzone później).
		for (auto& pnd : pending)
			if (script::ScriptObject* o = scripts->object(idOf(pnd.index)))
			{
				if (!pnd.vars.empty()) scripts->runIn(*o, pnd.vars, "wczytanie zapisu");
				o->state = pnd.state, o->stateCounter = pnd.counter, o->walking = false, o->actionUntil = -1;
			}
		if (scripts) scripts->setTime(t);
		digMarks.clear();
		for (uint32_t n = r.u32(), k = 0; k < n && r.ok; ++k)
		{
			int x = r.i32(), y = r.i32();
			digMarks.insert({x, y});
		}
		return r.ok;
	}


	void update(float dt)
	{
		dt = std::min(dt, 0.1f);
		maps.update(dt);
		simUpdate(dt);
	}

	// Pole pod kursorem: promień kamery przecięty z płaszczyzną głębokości krasnali w trafionym miejscu terenu
	// (wolne pole: głębokość chodu przed tylną ścianą; skała: przednia płaszczyzna), bez trafienia z = kGnomeZ.
	// Promień spod kursora w układzie sceny (przed macierzą world, jednostki modelu): p0 + t·(p1 − p0).
	void cursorRay(int mx, int my, int w, int h, glm::vec3& p0, glm::vec3& p1)
	{
		glm::mat4 vp, world;
		gameCamera(cam, float(w) / float(std::max(h, 1)), vp, world);
		glm::mat4 inv = glm::inverse(vp * world);
		float nx = 2.f * float(mx) / float(w) - 1.f, ny = 1.f - 2.f * float(my) / float(h);
		glm::vec4 a = inv * glm::vec4(nx, ny, -1.f, 1.f), b = inv * glm::vec4(nx, ny, 1.f, 1.f);
		p0 = glm::vec3(a) / a.w, p1 = glm::vec3(b) / b.w;
	}
	bool pickTile(int mx, int my, int w, int h, int& tx, int& ty, float& fx, float& fy)
	{
		glm::vec3 p0, p1;
		cursorRay(mx, my, w, h, p0, p1);
		float zs = kGnomeZ;
		int htx, hty;
		if (maps.pickTerrain(p0, p1 - p0, htx, hty)) zs = nav.open(htx, hty) ? walkZ(float(htx) + 0.5f, hty) : 15.f;
		float zPlane = (zs * 0.5f + 7.f) * kBlock;
		if (std::abs(p1.z - p0.z) < 1e-4f) return false;
		float t = (zPlane - p0.z) / (p1.z - p0.z);
		glm::vec3 hit = p0 + (p1 - p0) * t;
		fx = hit.x / kBlock, fy = hit.y / kBlock;
		tx = int(std::floor(fx)), ty = int(std::floor(fy));
		return true;
	}

	void click(int button, int mx, int my, int w, int h)
	{
		int tx, ty;
		float fx, fy;
		if (!pickTile(mx, my, w, h, tx, ty, fx, fy)) return;
		if (button == SDL_BUTTON_LEFT)
		{
			// Najpierw trafienie w model (trójkąty bieżącej klatki, jak w wigview); czapka wybiera swojego krasnala.
			glm::vec3 p0, p1;
			cursorRay(mx, my, w, h, p0, p1);
			int hit = maps.pickObject(p0, p1 - p0);
			for (int k = 0; k < 3 && hit >= 0 && maps.linkParent(hit) >= 0; ++k) hit = maps.linkParent(hit);
			for (size_t i = 0; i < units.size() && hit >= 0; ++i)
				if (units[i].obj == hit && units[i].gnome)
				{
					selected = int(i);
					return;
				}
			int best = -1;
			float bd = 1.2f;
			for (size_t i = 0; i < units.size(); ++i)
			{
				if (!units[i].gnome) continue; // wukery nie słuchają gracza
				float d = std::hypot(units[i].x - fx, (units[i].y - 0.6f) - fy);
				if (d < bd) bd = d, best = int(i);
			}
			if (best >= 0) selected = best;
		}
		else if (button == SDL_BUTTON_RIGHT)
		{
			rightDrag = true;
			dragX0 = tx, dragY0 = ty;
		}
	}

	void release(int mx, int my, int w, int h)
	{
		if (!rightDrag) return;
		rightDrag = false;
		int tx, ty;
		float fx, fy;
		if (!pickTile(mx, my, w, h, tx, ty, fx, fy)) return;
		if (tx == dragX0 && ty == dragY0)
		{
			if (selected >= 0 && selected < int(units.size())) units[size_t(selected)].digJob = false;
			order(tx, ty);
		}
		else
			markDig(dragX0, dragY0, tx, ty);
	}

	// Znaczniki kopania i prostokąt przeciągania: rzut narożników pola (przednia płaszczyzna) na ekran.
	void drawMarks(int w, int h)
	{
		glm::mat4 vp, world;
		gameCamera(cam, float(w) / float(std::max(h, 1)), vp, world);
		glm::mat4 m = vp * world;
		ImDrawList* dl = ImGui::GetBackgroundDrawList();
		float sx = ImGui::GetIO().DisplaySize.x / float(w), sy = ImGui::GetIO().DisplaySize.y / float(h);
		auto proj = [&](float x, float y, ImVec2& out) {
			glm::vec4 c = m * glm::vec4(x * kBlock, y * kBlock, 14.5f * kBlock, 1.f);
			if (c.w <= 0) return false;
			out = ImVec2((c.x / c.w * 0.5f + 0.5f) * float(w) * sx, (0.5f - c.y / c.w * 0.5f) * float(h) * sy);
			return true;
		};
		auto quad = [&](int x0, int y0, int x1, int y1, ImU32 fill, ImU32 line) {
			ImVec2 a, b;
			if (!proj(float(x0), float(y0), a) || !proj(float(x1 + 1), float(y1 + 1), b)) return;
			if (fill) dl->AddRectFilled(a, b, fill);
			dl->AddRect(a, b, line);
		};
		for (auto& [x, y] : digMarks) quad(x, y, x, y, IM_COL32(255, 200, 40, 50), IM_COL32(255, 200, 40, 160));
		if (selected >= 0 && selected < int(units.size()))
		{ // znacznik nad zaznaczonym krasnalem
			const Unit& u = units[size_t(selected)];
			glm::vec4 c = m * glm::vec4(u.x * kBlock, (u.y - 1.5f) * kBlock, (u.z * 0.5f + 7.f) * kBlock, 1.f);
			if (c.w > 0)
			{
				ImVec2 p((c.x / c.w * 0.5f + 0.5f) * float(w) * sx, (0.5f - c.y / c.w * 0.5f) * float(h) * sy);
				dl->AddTriangleFilled(ImVec2(p.x - 8, p.y - 12), ImVec2(p.x + 8, p.y - 12), p, IM_COL32(255, 220, 60, 230));
			}
		}
		if (rightDrag && hoverX >= 0)
			quad(std::min(dragX0, hoverX), std::min(dragY0, hoverY), std::max(dragX0, hoverX), std::max(dragY0, hoverY), 0,
			    IM_COL32(255, 255, 255, 200));
	}

	void render(int w, int h)
	{
		glm::mat4 vp, world;
		gameCamera(cam, float(w) / float(std::max(h, 1)), vp, world);
		glm::vec3 target = glm::vec3(cam.x, cam.y, 14.f) * kBlock;
		maps.render(vp, world, draw, target.z, target);
	}

	// Listy menu: nasze zapisy (najnowsze pierwsze), zapisy gry z Data/Gamesave, scenariusze.
	void refreshMenu()
	{
		menuOwn.clear(), menuGame.clear(), menuScen.clear();
		std::vector<std::pair<std::filesystem::file_time_type, std::string>> own;
		std::error_code ec;
		for (auto& e : std::filesystem::directory_iterator(saveDir(), ec))
		{
			std::string ext = e.path().extension().string();
			if (ext == ".sav" || ext == ".ows") own.push_back({e.last_write_time(ec), e.path().string()});
		}
		std::sort(own.begin(), own.end(), [](auto& a, auto& b) { return a.first > b.first; });
		for (auto& [t, path] : own) menuOwn.push_back({{"file", path}, std::filesystem::path(path).filename().string()});
		auto files = vfs.list("Data/Gamesave");
		std::sort(files.begin(), files.end());
		for (const auto& f : files)
		{
			std::string ext = f.size() > 4 ? Vfs::lower(f.substr(f.size() - 4)) : "";
			if (ext == ".sav") menuGame.push_back({{"scenario", f}, f});
			else if (ext == ".tcl" && Vfs::lower(f).rfind("multi_", 0) != 0) menuScen.push_back({{"scenario", f}, f});
		}
	}

	// Menu wyboru gry (🔶 zastępuje menu oryginału): wybór zapisu albo scenariusza.
	void menu()
	{
		ImVec2 ds = ImGui::GetIO().DisplaySize;
		ImGui::SetNextWindowPos(ImVec2(ds.x * 0.5f, ds.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(std::min(560.f, ds.x - 32), std::min(520.f, ds.y - 32)), ImGuiCond_Always);
		ImGui::Begin(tr("Wybierz grę"), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
		if (pendingFrames > 0)
		{
			ImGui::Text(tr("Wczytywanie: %s…"), std::filesystem::path(pending.name).filename().string().c_str());
			ImGui::End();
			return;
		}
		auto list = [&](const char* title, const std::vector<MenuEntry>& v) {
			if (v.empty() || !ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen)) return;
			ImGui::PushID(title);
			for (size_t i = 0; i < v.size(); ++i)
				if (ImGui::Selectable((v[i].label + "##" + std::to_string(i)).c_str()))
					pending = v[i].src, pendingFrames = 2;
			ImGui::PopID();
		};
		ImGui::BeginChild("lists", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));
		list(tr("Twoje zapisy"), menuOwn);
		list(tr("Zapisy gry (Data/Gamesave)"), menuGame);
		list(tr("Scenariusze"), menuScen);
		ImGui::EndChild();
		if (loaded && ImGui::Button(tr("Wróć do gry (Esc)"))) showMenu = false;
		if (loaded) ImGui::SameLine();
		if (ImGui::Button(tr("Wyjście"))) quit = true;
		ImGui::End();
	}

	// Wczytanie gry wybranej w menu: najpierw klatka z napisem „Wczytywanie…”, potem wczytanie.
	void menuStep()
	{
		if (pendingFrames <= 0 || --pendingFrames > 0) return;
		bool ok = pending.kind == "file" ? loadGame(pending.name) : loadSource(pending);
		status = ok ? std::string(tr("Wczytano: ")) + pending.name : std::string(tr("Nie udało się wczytać: ")) + pending.name + " " + maps.error();
		if (ok) showMenu = false;
	}

	void hud()
	{
		if (showMenu)
		{
			menu();
			return;
		}
		ImGui::SetNextWindowPos(ImVec2(8, 8), ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.6f);
		ImGui::Begin("hud", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
		ImGui::Text("OpenWiggles %s", OW_VERSION);
		ImGui::Text(tr("Krasnale: %d"), gnomeCount());
		if (selected >= 0 && selected < int(units.size()))
		{
			const Unit& u = units[size_t(selected)];
			const char* st = u.state == Unit::Walk ? tr("idzie") : u.state == Unit::Dig ? tr("kopie")
			                                                                            : tr("czeka");
			ImGui::Text(tr("Zaznaczony: krasnal %d (%s), pole %d, %d"), selected + 1, st, u.tx, u.ty);
		}
		if (hoverX >= 0) ImGui::TextDisabled(tr("Pole pod kursorem: %d, %d (%s)"), hoverX, hoverY, nav.open(hoverX, hoverY) ? tr("wolne") : tr("skała"));
		if (!status.empty()) ImGui::TextColored(ImVec4(1, 0.7f, 0.3f, 1), "%s", status.c_str());
		ImGui::Text(paused ? tr("PAUZA (spacja)") : tr("Tempo: ×%.0f (1–3, spacja = pauza)"), double(speed));
		if (showHelp)
		{
			ImGui::Separator();
			ImGui::TextDisabled("%s", tr("LPM: zaznacz krasnala   PPM: idź / kop skałę"));
			ImGui::TextDisabled("%s", tr("Przeciągnij PPM: zaznacz skałę do kopania (zaznaczony krasnal kopie)"));
			ImGui::TextDisabled("%s", tr("WASD, strzałki, środkowy przycisk: kamera   kółko: przybliżenie"));
			ImGui::TextDisabled("%s", tr("Kursor przy krawędzi okna: przesuwanie kamery"));
			ImGui::TextDisabled("%s", tr("N: nowy krasnal przy kursorze   F1: pomoc   Esc: menu"));
			ImGui::TextDisabled("%s", tr("F5: szybki zapis   F9: szybkie wczytanie"));
		}
		ImGui::End();
		gnomePanel();
	}

	// Panel krasnali: imię, zajęcie, potrzeby (atrybuty skryptów); kliknięcie = zaznaczenie i kamera.
	void gnomePanel()
	{
		ImVec2 ds = ImGui::GetIO().DisplaySize;
		ImGui::SetNextWindowPos(ImVec2(ds.x - 8, 8), ImGuiCond_Always, ImVec2(1, 0));
		ImGui::SetNextWindowBgAlpha(0.6f);
		ImGui::Begin("krasnale", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
		static const char* kNeeds[4][2] = {{"atr_Hitpoints", N_("zdrowie")}, {"atr_Nutrition", N_("jedzenie")},
		    {"atr_Alertness", N_("sen")}, {"atr_Mood", N_("nastrój")}};
		for (size_t i = 0; i < units.size(); ++i)
		{
			const Unit& u = units[i];
			if (!u.gnome) continue;
			ImGui::PushID(int(i));
			std::string name;
			const script::ScriptObject* so = scripts ? scripts->object(idOf(u.obj)) : nullptr;
			if (so && !so->name.empty()) name = so->name;
			if (name.empty()) name = std::string(tr("krasnal")) + " " + std::to_string(i + 1);
			if (ImGui::Selectable(name.c_str(), int(i) == selected, 0, ImVec2(ImGui::GetFontSize() * 9, 0)))
			{
				selected = int(i);
				cam.x = u.x, cam.y = u.y - 1.f;
			}
			if (so)
			{
				ImGui::SameLine();
				ImGui::TextDisabled("%s", so->state.c_str());
				for (auto& n : kNeeds)
				{
					float v = float(scripts->attrib(so->id, n[0]));
					ImGui::SameLine();
					ImGui::PushStyleColor(ImGuiCol_PlotHistogram, v < 0.3f ? ImVec4(0.9f, 0.3f, 0.2f, 1) : ImVec4(0.3f, 0.8f, 0.3f, 1));
					ImGui::ProgressBar(v, ImVec2(ImGui::GetFontSize() * 2.5f, ImGui::GetFontSize() * 0.6f), "");
					ImGui::PopStyleColor();
					ImGui::SetItemTooltip("%s: %.0f%%", tr(n[1]), double(v * 100.f));
				}
			}
			ImGui::PopID();
		}
		ImGui::End();
	}
};

// --- ScriptHost ---
} // namespace

int main(int argc, char** argv)
{
	Args args;
	if (!parseArgs(argc, argv, args)) return 2;
	i18n::loadBuiltin();
	i18n::setLanguage(args.lang.empty() ? i18n::systemLanguage() : args.lang);

	auto game = std::make_unique<Game>();
	Game& g = *game;
	g.vfs.addRoot(args.game);
	for (const auto& o : args.overlays)
		if (!g.vfs.addOverlay(o, i18n::language())) std::fprintf(stderr, "nakładka %s: brak katalogu Data ani Data_%s\n", o.c_str(), i18n::language().c_str());
	if (!g.vfs.exists("Data/3db"))
	{
		std::fprintf(stderr, "Nie znaleziono Data/3db w '%s'. Podaj --game <katalog gry>.\n", args.game.c_str());
		return 1;
	}
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
		return 1;
	}
	SDL_Window* win = nullptr;
	std::string err;
	{
		std::vector<rhi::Backend> order{args.renderer};
		rhi::Backend other = args.renderer == rhi::Backend::Vulkan ? rhi::Backend::OpenGL : rhi::Backend::Vulkan;
		if (!args.rendererForced && rhi::backendCompiled(other)) order.push_back(other);
		for (rhi::Backend b : order)
		{
			if (!rhi::backendCompiled(b)) continue;
			rhi::prepareWindow(b);
			win = SDL_CreateWindow("OpenWiggles " OW_VERSION, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, args.width,
			    args.height, rhi::windowFlags(b) | SDL_WINDOW_RESIZABLE);
			if (!win) continue;
			g.device = rhi::createDevice(b, win, true, err, !args.screenshot.empty());
			if (g.device) break;
			std::fprintf(stderr, "%s: %s\n", rhi::backendName(b), err.c_str());
			SDL_DestroyWindow(win);
			win = nullptr;
		}
		if (!g.device)
		{
			std::fprintf(stderr, "Nie udało się uruchomić żadnego backendu renderowania\n");
			return 1;
		}
	}
	rhi::Device& dev = *g.device;
	g.textures = std::make_unique<TextureCache>(g.vfs);
	g.textures->setDevice(&dev);
	if (auto sd = g.vfs.read("Data/SPHERE.DAT")) g.sphere = parseSphereDat(*sd);
	g.maps.init(dev, &g.vfs, g.textures.get(), &g.sphere);
	{
		std::string err;
		if (args.borderMask && !g.border.init(dev, g.vfs, err)) std::fprintf(stderr, "ramka ekranu: %s\n", err.c_str());
	}

	// Źródło mapy: --save / --template / --scenario / --load (zapis .ows) albo najnowszy zapis gry.
	Source src;
	if (!args.save.empty()) src = {args.save.find('/') != std::string::npos ? "savepath" : "scenario", args.save};
	else if (!args.templ.empty())
		src = {"template", args.templ.size() > 4 && args.templ.substr(args.templ.size() - 4) == ".tcl" ? args.templ : args.templ + ".tcl"};
	else if (args.scenario.empty() && args.load.empty() && args.saveTo.empty() && (args.screenshot.empty() || std::getenv("OW_TEST_MENU")))
		src = {}; // bez źródła: menu wyboru gry
	else if (args.scenario.empty())
	{
		// Domyślnie najnowszy zapis gry z Data/Gamesave, a bez zapisów szablon kris_unq_060.
		std::string newest;
		std::filesystem::file_time_type best{};
		for (const auto& f : g.vfs.list("Data/Gamesave"))
		{
			if (f.size() < 4 || Vfs::lower(f.substr(f.size() - 4)) != ".sav") continue;
			if (auto p = g.vfs.resolve("Data/Gamesave/" + f))
			{
				std::error_code ec;
				auto t = std::filesystem::last_write_time(*p, ec);
				if (!ec && (newest.empty() || t > best)) best = t, newest = f;
			}
		}
		src = newest.empty() ? Source{"template", "kris_unq_060.tcl"} : Source{"scenario", newest};
		std::printf("mapa: %s\n", src.name.c_str());
	}
	else
	{
		std::string sc = args.scenario;
		if (sc.size() < 4 || (sc.substr(sc.size() - 4) != ".tcl" && sc.substr(sc.size() - 4) != ".sav")) sc += ".tcl";
		src = {"scenario", sc};
	}
	if (args.scripts)
	{ // silnik skryptów przed światem: obiekty mapy dostają skrypty przy wczytaniu
		g.scriptGnomes = args.scriptGnomes;
		Uint64 t0 = SDL_GetTicks64();
		std::string serr;
		// język tekstów gry: --locale, inaczej język interfejsu (jeśli MESSAGES.TXT go ma), inaczej język wydania
		if (!g.bootScripts(g.vfs, args.locale.empty() ? i18n::language() : args.locale, serr))
			std::fprintf(stderr, "skrypty: %s\n", serr.c_str());
		std::printf("skrypty: %zu klas w %llu ms\n", g.scripts->classCount(), (unsigned long long)(SDL_GetTicks64() - t0));
	}
	if (!args.load.empty())
	{
		if (!g.loadGame(args.load)) std::fprintf(stderr, "nie udało się wczytać %s\n", args.load.c_str());
	}
	else if (src.kind.empty())
	{
		g.refreshMenu();
		g.showMenu = true;
	}
	else if (!g.loadSource(src))
		std::fprintf(stderr, "mapa: %s\n", g.maps.error().c_str());
	if (args.reveal) g.maps.setFog(MapScene::FogMode::Off);
	if (args.view) g.cam = *args.view;
	for (float t = 0; t < args.warmup; t += 0.1f) g.update(0.1f);
	if (!args.saveTo.empty())
	{
		std::string serr;
		bool sok = args.saveTo.size() > 4 && args.saveTo.substr(args.saveTo.size() - 4) == ".sav" ? g.saveSav(args.saveTo, serr) : g.saveGame(args.saveTo);
		std::printf("%s %s %s\n", sok ? "zapisano grę" : "BŁĄD zapisu gry", args.saveTo.c_str(), serr.c_str());
	}

	ImGui::CreateContext();
	ImGui::GetIO().IniFilename = nullptr;
	ImGui::StyleColorsDark();
	for (const char* f : {"/usr/share/fonts/TTF/DejaVuSans.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
	         "/usr/share/fonts/noto/NotoSans-Regular.ttf", "C:\\Windows\\Fonts\\segoeui.ttf"})
		if (std::filesystem::exists(f) && ImGui::GetIO().Fonts->AddFontFromFileTTF(f, 15.f)) break;
	dev.imguiInit();

	auto frame = [&](std::vector<uint8_t>* readback) {
		int w, h;
		SDL_GetWindowSizeInPixels(win, &w, &h);
		dev.imguiNewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();
		g.hud();
		{
			int fw, fh;
			SDL_GetWindowSizeInPixels(win, &fw, &fh);
			g.drawMarks(fw, fh);
		}
		ImGui::Render();
		glm::vec3 clear(0.f);
		if (const HorizonZone* z = g.maps.zone()) clear = glm::vec3(z->r, z->g, z->b);
		if (!dev.beginFrame(w, h, clear.r, clear.g, clear.b)) return false;
		dev.setViewport(0, 0, w, h);
		if (g.loaded) g.render(w, h);
		if (args.borderMask) g.border.draw(0, w, h);
		dev.imguiRender(ImGui::GetDrawData());
		dev.endFrame(readback);
		return true;
	};
	auto shutdown = [&]() {
		if (g.scripts) g.scripts->writeLog("openwiggles_tcl.log");
		dev.waitIdle();
		dev.imguiShutdown();
		ImGui::DestroyContext();
		std::unique_ptr<rhi::Device> d = std::move(g.device);
		game.reset();
		d.reset();
		SDL_DestroyWindow(win);
		SDL_Quit();
	};

	if (!args.screenshot.empty())
	{
		// Zrzut: krótka symulacja (krasnal idzie w prawo), potem klatka do PNG.
		if (!g.units.empty())
		{
			const Unit& u = g.units[0];
			if (std::getenv("OW_TEST_DIG"))
			{ // test: kopanie 3×2 pól pod pierwszym krasnalem
				g.selected = 0;
				int dx = std::getenv("OW_TEST_DIG_DX") ? std::atoi(std::getenv("OW_TEST_DIG_DX")) : 0;
				g.markDig(u.tx - 1 + dx, u.ty + 1, u.tx + 1 + dx, u.ty + 2);
			}
			else
				g.order(u.tx + 3, u.ty);
		}
		int frames = std::getenv("OW_TEST_DIG") ? std::atoi(std::getenv("OW_TEST_DIG")) : 30; // test kopania: liczba klatek po 0,05 s
		for (int i = 0; i < frames; ++i)
		{
			SDL_Event e;
			while (SDL_PollEvent(&e))
			{
			}
			if (g.loaded) g.update(0.05f);
			frame(nullptr);
		}
		std::vector<uint8_t> px;
		frame(&px);
		int w, h;
		SDL_GetWindowSizeInPixels(win, &w, &h);
		bool ok2 = px.size() == size_t(w) * h * 4 && writePng(args.screenshot, w, h, px);
		std::printf("%s %s\n", ok2 ? "zapisano" : "BŁĄD zapisu", args.screenshot.c_str());
		shutdown();
		return ok2 ? 0 : 1;
	}

	Uint64 last = SDL_GetPerformanceCounter();
	bool running = true, dragging = false;
	while (running)
	{
		SDL_Event e;
		int w, h;
		SDL_GetWindowSize(win, &w, &h);
		while (SDL_PollEvent(&e))
		{
			ImGui_ImplSDL2_ProcessEvent(&e);
			ImGuiIO& io = ImGui::GetIO();
			if (e.type == SDL_QUIT) running = false;
			if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
			{
				if (!g.loaded) running = false;
				else if ((g.showMenu = !g.showMenu)) g.refreshMenu();
			}
			if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F1) g.showHelp = !g.showHelp;
			if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F5)
			{
				g.status = g.quickSave();
			}
			if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F9)
			{
				std::string qp = Game::newestSave();
				bool okl = !qp.empty() && g.loadGame(qp);
				g.status = okl ? std::string(tr("Wczytano: ")) + qp : std::string(tr("Nie udało się wczytać: ")) + qp;
			}
			if (e.type == SDL_KEYDOWN && !io.WantTextInput)
			{
				if (e.key.keysym.sym == SDLK_SPACE) g.paused = !g.paused;
				if (e.key.keysym.sym == SDLK_1) g.speed = 1.f;
				if (e.key.keysym.sym == SDLK_2) g.speed = 2.f;
				if (e.key.keysym.sym == SDLK_3) g.speed = 4.f;
			}
			if (io.WantCaptureMouse || g.showMenu) continue;
			if (e.type == SDL_MOUSEBUTTONDOWN && (e.button.button == SDL_BUTTON_LEFT || e.button.button == SDL_BUTTON_RIGHT))
				g.click(e.button.button, e.button.x, e.button.y, w, h);
			if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_MIDDLE) dragging = true;
			if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_RIGHT) g.release(e.button.x, e.button.y, w, h);
			if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_MIDDLE) dragging = false;
			if (e.type == SDL_MOUSEMOTION)
			{
				if (dragging)
				{
					float s = (g.cam.zoom * 16.f - 10.f) / float(std::max(h, 1)) * 0.9f;
					g.cam.x -= float(e.motion.xrel) * s;
					g.cam.y -= float(e.motion.yrel) * s;
				}
				int tx, ty;
				float fx, fy;
				if (g.pickTile(e.motion.x, e.motion.y, w, h, tx, ty, fx, fy)) g.hoverX = tx, g.hoverY = ty;
			}
			if (e.type == SDL_MOUSEWHEEL) g.cam.zoom = std::clamp(g.cam.zoom * std::pow(0.9f, float(e.wheel.y)), kGameZoomMin, kGameZoomMax);
			if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_n && !io.WantTextInput)
			{
				int mx, my, tx, ty;
				float fx, fy;
				SDL_GetMouseState(&mx, &my);
				if (g.pickTile(mx, my, w, h, tx, ty, fx, fy)) g.spawnUnit(float(tx) + 0.5f, float(ty) + 0.9f);
			}
		}
		Uint64 now = SDL_GetPerformanceCounter();
		float dt = float(now - last) / float(SDL_GetPerformanceFrequency());
		last = now;
		const Uint8* k = SDL_GetKeyboardState(nullptr);
		float sp = (g.cam.zoom * 16.f - 10.f) * 0.8f * dt;
		if (!ImGui::GetIO().WantCaptureKeyboard && !g.showMenu)
		{
			if (k[SDL_SCANCODE_A] || k[SDL_SCANCODE_LEFT]) g.cam.x -= sp;
			if (k[SDL_SCANCODE_D] || k[SDL_SCANCODE_RIGHT]) g.cam.x += sp;
			if (k[SDL_SCANCODE_W] || k[SDL_SCANCODE_UP]) g.cam.y -= sp;
			if (k[SDL_SCANCODE_S] || k[SDL_SCANCODE_DOWN]) g.cam.y += sp;
		}
		// Przesuwanie kamery kursorem przy krawędzi okna (jak w grze; 🔶 szerokość pasa i prędkość na oko).
		if (g.loaded && !g.showMenu && !dragging && SDL_GetMouseFocus() == win)
		{
			int mx, my;
			SDL_GetMouseState(&mx, &my);
			const int edge = std::max(4, std::min(w, h) / 60);
			if (mx < edge) g.cam.x -= sp;
			if (mx >= w - edge) g.cam.x += sp;
			if (my < edge) g.cam.y -= sp;
			if (my >= h - edge) g.cam.y += sp;
		}
		g.menuStep();
		if (g.quit) running = false;
		if (g.loaded) g.update(g.paused || g.showMenu ? 0.f : dt * g.speed);
		if (!frame(nullptr)) SDL_Delay(16);
	}
	shutdown();
	return 0;
}
