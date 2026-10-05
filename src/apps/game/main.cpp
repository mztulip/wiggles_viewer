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
constexpr float kGnomeZ = 8.0f;      // głębokość krasnala w skali skryptu (świat 11 bloków), jak w scenariuszach
constexpr float kWalkSpeed = 1.6f;   // pola na sekundę (🔶)
constexpr float kClimbSpeed = 0.9f;
constexpr float kDigTime = 2.0f;     // sekundy kopania jednego pola (🔶)
constexpr float kPi = 3.14159265f;

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

struct Unit
{
	int obj = -1;
	float x = 0, y = 0;   // stopy: x środka, y dołu pola (bloki)
	int tx = 0, ty = 0;   // bieżące pole
	std::vector<std::pair<int, int>> path;
	size_t step = 0;
	enum State
	{
		Idle,
		Walk,
		Dig
	} state = Idle;
	int digX = -1, digY = -1;
	float digT = 0;
	float facing = -kPi * 0.5f; // −π/2 = w prawo, +π/2 = w lewo (obserwacja modelu mann)
	float z = kGnomeZ;          // głębokość (skala skryptu), z zapisu albo scenariusza
	std::function<void(bool)> onArrive; // akcja walk zlecona przez skrypt
	bool digJob = false;        // przydzielony do kopania zaznaczonych pól (gracz)
	float goalX = -1, goalZ = -1; // dokładny cel chodu skryptu (x w polu, głębokość), −1 = środek pola
};

struct Game;
// Most między silnikiem skryptów a sceną: ID obiektu skryptu = indeks obiektu MapScene + 1.
// Metody zdefiniowane po Game (korzystają z krasnali i nawigacji).
struct ScriptHost : script::Host
{
	Game* g = nullptr;
	int createObject(const std::string& cls, const std::string&, const float pos[3], const float rot[3]) override;
	void deleteObject(int id) override;
	bool getPos(int id, float pos[3]) override;
	void setPos(int id, const float pos[3]) override;
	void setRotY(int id, float r) override;
	float getRotY(int id) override;
	void setAnim(int id, const std::string& anim, int, int mode) override;
	double animLength(int id, const std::string& anim) override;
	bool walkTo(int id, const float target[3], std::function<void(bool)> done) override;
	float hmap(float x, float y) override;
	bool underwater(const float p[3]) override;
	bool findPlace(const float c[3], float r, float out[3]) override;
	int gnomePosition(int id) override;
	void glide(int id, const float to[3], float seconds) override;
	void link(int child, int parent, int link) override;
	bool linkOffset(int id, int link, float out[3]) override;
};

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

struct Game
{
	Vfs vfs;
	Source source;
	int baseObjects = 0;              // obiekty mapy źródłowej (dalej: utworzone w grze)
	std::vector<uint32_t> baseBlocks; // bloki i typy po wczytaniu źródła (różnice idą do zapisu)
	std::vector<uint8_t> baseTypes;
	std::unique_ptr<script::Engine> scripts;
	ScriptHost host;
	bool scriptGnomes = true; // krasnale sterowane skryptami (--player-gnomes: tylko gracz)
	// ID obiektów skryptów: obiekty z zapisu mają ID z zapisu (zmienne Tcl się do nich odwołują),
	// pozostałe od 100000. idx(id) = indeks obiektu MapScene.
	std::unordered_map<int, int> idToIdx;
	std::vector<int> idxToId;
	int nextScriptId = 100000;
	int idOf(int index)
	{
		if (index >= int(idxToId.size())) idxToId.resize(size_t(index) + 1, 0);
		int& id = idxToId[size_t(index)];
		if (!id)
		{
			const std::string& n = maps.objectName(index);
			char* end = nullptr;
			long v = n.empty() ? 0 : std::strtol(n.c_str(), &end, 10);
			id = (v > 0 && end && !*end && !idToIdx.count(int(v))) ? int(v) : nextScriptId++;
			idToIdx[id] = index;
		}
		return id;
	}
	bool valid(int id) const { return idToIdx.count(id) != 0; }
	int idx(int id) const
	{
		auto it = idToIdx.find(id);
		return it == idToIdx.end() ? -1 : it->second;
	}
	struct Glide
	{
		int obj;
		float from[3], to[3], t, dur;
	};
	std::vector<Glide> glides; // płynne ruchy obiektów zlecone przez skrypty
	std::set<std::pair<int, int>> digMarks; // pola zaznaczone do kopania (przeciągnięcie PPM)
	int dragX0 = -1, dragY0 = -1;          // początek przeciągnięcia PPM (pole)
	bool rightDrag = false;
	bool paused = false;
	float speed = 1.f; // tempo gry: 1, 2, 4 (klawisze 1–3)
	float fogTimer = 0;
	double scriptAcc = 0;
	std::unique_ptr<rhi::Device> device;
	std::unique_ptr<TextureCache> textures;
	std::vector<Vec3f> sphere;
	MapScene maps;
	BorderMask border; // ramka ekranu (docs/silnik/09_klatka.md §9.4)
	ModelDrawOptions draw;
	NavGrid nav;
	GameView cam;
	std::vector<Unit> units;
	int selected = -1;
	std::string status;
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

	// Wysokość stóp w polu (x, ty): podłoże z bloku pod spodem (pierwszy wiersz siatki 8×8 o wysokości ≥ 13
	// w środkowej kolumnie), np. na łące 0,5 pola niżej niż dół pola. Na ścianie: dół pola.
	float floorY(int x, int ty) const
	{
		if (!nav.standable(x, ty) || !grid()) return float(ty) + 1.f;
		const Pmp* g = grid();
		const BlockRecord* r = maps.blockDb().record(g->blocks[size_t(ty + 1) * g->width + size_t(x)]);
		if (!r) return float(ty) + 1.f;
		for (int row = 0; row < 8; ++row)
			if (r->depth[size_t(row * 8 + 4)] >= 13) return float(ty + 1) + float(row) / 8.f;
		return float(ty) + 1.f;
	}

	// Pole, w którym stoi postać o stopach na wysokości y (stopy do 0,6 pola w głąb pola niżej).
	int tileOfFeet(float y) const { return int(std::floor(y - 0.6f)); }

	void snapToFloor(Unit& u)
	{
		int x = int(std::floor(u.x)), y = tileOfFeet(u.y);
		for (int d = 0; d < 40; ++d)
		{
			for (int yy : {y + d, y - d})
				if (nav.standable(x, yy) || (d > 0 && nav.climbable(x, yy)))
				{
					u.tx = x, u.ty = yy;
					u.x = float(x) + 0.5f, u.y = floorY(x, yy);
					return;
				}
		}
		u.tx = x, u.ty = y;
	}

	Unit* unitOf(int obj)
	{
		for (auto& u : units)
			if (u.obj == obj) return &u;
		return nullptr;
	}

	int addUnit(int obj)
	{
		maps.setObjectModel(obj, "mann.3db", "mann");
		Unit u;
		u.obj = obj;
		const float* p = maps.objectPos(obj);
		u.x = p[0], u.y = p[1], u.z = p[2];
		snapToFloor(u);
		units.push_back(u);
		return int(units.size()) - 1;
	}

	void spawnUnit(float x, float y)
	{
		int o = maps.addObject("Zwerg", x, y, kGnomeZ);
		selected = addUnit(o);
	}

	void initWorld()
	{
		units.clear();
		idToIdx.clear(), idxToId.clear(), nextScriptId = 100000;
		for (int i = 0; i < maps.objectCount(); ++i) idOf(i);
		selected = -1;
		if (const Pmp* g = grid()) nav.build(*g, maps.blockDb());
		for (int i = 0; i < maps.objectCount(); ++i)
			if (Vfs::lower(maps.objectClass(i)) == "zwerg") addUnit(i);
		if (const auto& v = maps.view()) cam = *v;
		else if (const Pmp* g = grid()) cam = GameView{float(g->width) * 0.5f, float(g->height) * 0.5f, 1.38f, 0.f, 0.f};
		if (units.empty() && grid())
		{ // mapa bez krasnali: jeden przy środku kamery
			spawnUnit(cam.x, cam.y);
			cam.x = units.back().x, cam.y = units.back().y - 1.f;
		}
		if (!units.empty()) selected = 0;
		status = units.empty() ? tr("Brak krasnali na mapie.") : "";
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

	void dig(int x, int y)
	{
		// 🔶 Wykopane pole: tunel z tylną ścianą (blok 4523: siatka 8×8 o wysokości 7), bez zszywania z sąsiadami.
		// Rodzaj gruntu bez zmian, ale nie „otwór do tła” (typ 5): wtedy grunt pola niżej albo skała A (4).
		uint8_t type = grid()->types[size_t(y) * grid()->width + size_t(x)];
		if ((type & 0x7F) == 5)
		{
			uint8_t below = y + 1 < int(grid()->height) ? grid()->types[size_t(y + 1) * grid()->width + size_t(x)] : 4;
			type = uint8_t((below & 0x7F) == 5 || (below & 0x7F) == 0 ? 4 : (below & 0x7F));
		}
		maps.setBlock(x, y, 4523, type);
		nav.updateTile(*grid(), maps.blockDb(), x, y);
		maps.rebuildTerrain();
		digMarks.erase({x, y});
		// Znalezisko jak w dig_execute (z_dignwalk.tcl): 4% szans; rodzaj losowany (🔶 bez stref kampanii).
		if (float(std::rand()) / float(RAND_MAX) < 0.04f)
		{
			static const char* kFinds[] = {"Stein", "Eisenerz", "Kohle", "Golderz", "Kristallerz"};
			const char* cls = kFinds[std::rand() % 5];
			float pos[3] = {float(x) + 0.5f, floorY(x, y), kGnomeZ}, rot[3] = {0, 0, 0};
			int id = host.createObject(cls, "", pos, rot);
			if (scripts) scripts->attach(id, cls, "");
			status = std::string(tr("Znalezisko: ")) + cls;
		}
	}

	// Następne zaznaczone pole dla krasnala z zadaniem kopania: najbliższe, do którego da się dojść.
	bool nextDigJob(Unit& u)
	{
		std::vector<std::pair<int, std::pair<int, int>>> cand;
		for (auto& m : digMarks) cand.push_back({std::abs(m.first - u.tx) + std::abs(m.second - u.ty), m});
		std::sort(cand.begin(), cand.end());
		for (size_t i = 0; i < cand.size() && i < 12; ++i)
		{
			auto [mx, my] = cand[i].second;
			if (nav.open(mx, my))
			{
				digMarks.erase({mx, my});
				continue;
			}
			const int st[4][2] = {{mx - 1, my}, {mx + 1, my}, {mx, my - 1}, {mx, my + 1}};
			for (auto& c : st)
			{
				if (!(nav.standable(c[0], c[1]) || nav.climbable(c[0], c[1]))) continue;
				auto p = nav.path(u.tx, u.ty, c[0], c[1]);
				if (p.empty()) continue;
				u.path = std::move(p), u.step = 0, u.state = Unit::Walk, u.digX = mx, u.digY = my;
				u.goalX = u.goalZ = -1;
				u.onArrive = nullptr;
				return true;
			}
		}
		return false;
	}

	// Zaznaczenie prostokąta pól skały do kopania; zadanie dostaje zaznaczony krasnal.
	void markDig(int x0, int y0, int x1, int y1)
	{
		int n = 0;
		for (int y = std::min(y0, y1); y <= std::max(y0, y1); ++y)
			for (int x = std::min(x0, x1); x <= std::max(x0, x1); ++x)
				if (!nav.open(x, y) && grid() && x >= 0 && y >= 0 && x < int(grid()->width) && y < int(grid()->height))
					n += digMarks.insert({x, y}).second;
		if (selected >= 0 && selected < int(units.size())) units[size_t(selected)].digJob = true;
		status = std::string(tr("Do kopania: ")) + std::to_string(digMarks.size());
		(void)n;
	}

	void updateUnit(Unit& u, float dt)
	{
		if (u.state == Unit::Walk)
		{
			if (u.step >= u.path.size())
			{
				u.state = u.digX >= 0 ? Unit::Dig : Unit::Idle;
				u.digT = 0;
				if (u.onArrive)
				{
					auto f = std::move(u.onArrive);
					u.onArrive = nullptr;
					f(true);
				}
			}
			else
			{
				auto [wx, wy] = u.path[u.step];
				float gx = float(wx) + 0.5f, gy = floorY(wx, wy);
				bool last = u.step + 1 == u.path.size();
				if (last && u.goalX >= 0) gx = std::clamp(u.goalX, float(wx) + 0.1f, float(wx) + 0.9f);
				if (u.goalZ >= 0)
				{ // w głąb równolegle z chodem
					float dz = u.goalZ - u.z, mz = 2.f * kWalkSpeed * dt;
					u.z += std::abs(dz) <= mz ? dz : (dz > 0 ? mz : -mz);
				}
				bool vertical = wx == u.tx && wy != u.ty;
				float speed = vertical ? kClimbSpeed : kWalkSpeed;
				float ddx = gx - u.x, ddy = gy - u.y, len = std::hypot(ddx, ddy), mv = speed * dt;
				if (std::abs(ddx) > 1e-3f) u.facing = ddx > 0 ? -kPi * 0.5f : kPi * 0.5f;
				if (len <= mv)
				{
					u.x = gx, u.y = gy, u.tx = wx, u.ty = wy;
					++u.step;
				}
				else
					u.x += ddx / len * mv, u.y += ddy / len * mv;
				const char* anim = !vertical ? "gehen_loop" : (ddy < 0 ? "kletter_hoch_loop" : "kletter_runter_loop");
				if (vertical) u.facing = kPi; // twarzą do ściany
				maps.playAnim(u.obj, anim, true);
			}
		}
		if (u.state == Unit::Dig)
		{
			const char* anim = u.digY > u.ty ? "buddeln_unten" : u.digY < u.ty ? "buddeln_oben_normal"
			                                                                   : "buddeln_vorn";
			if (u.digY == u.ty) u.facing = u.digX > u.tx ? -kPi * 0.5f : kPi * 0.5f;
			maps.playAnim(u.obj, anim, true);
			u.digT += dt;
			if (u.digT >= kDigTime)
			{
				dig(u.digX, u.digY);
				u.digX = u.digY = -1;
				u.state = Unit::Idle;
				snapToFloor(u); // pod krasnalem mogło zniknąć podłoże
			}
		}
		if (u.state == Unit::Idle && u.digJob)
		{
			if (digMarks.empty() || !nextDigJob(u)) u.digJob = false; // nic osiągalnego: koniec zadania
		}
		if (u.state == Unit::Idle)
		{
			if (!nav.standable(u.tx, u.ty) && !nav.climbable(u.tx, u.ty) && nav.open(u.tx, u.ty + 1))
			{
				u.path = {{u.tx, u.ty + 1}}, u.step = 0, u.state = Unit::Walk; // spada
			}
			else if (!scripts) // ze skryptami animację w miejscu ustawia skrypt (set_anim)
				maps.playAnim(u.obj, nav.standable(u.tx, u.ty) ? "standard" : "kletterstand", true);
		}
		maps.setObjectPose(u.obj, u.x, u.y, u.z, u.facing);
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
		digMarks.clear(), glides.clear();
		initWorld();
		if (scripts)
		{
			scripts->resetWorld();
			attachScripts();
		}
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

	// Skrypty obiektów świata (obj_init). Krasnale na razie steruje gracz (C++), nie skrypty.
	void attachScripts()
	{
		if (!scripts) return;
		int n = 0;
		for (int i = 0; i < maps.objectCount(); ++i)
		{
			bool gnome = Vfs::lower(maps.objectClass(i)) == "zwerg";
			if (gnome && !scriptGnomes) continue;
			auto* o = scripts->attach(idOf(i), maps.objectClass(i), maps.objectInstanceName(i), false);
			if (!o) continue;
			o->owner = maps.objectOwner(i); // z zapisu
			if (gnome && o->owner < 0) o->owner = 0; // krasnale ze scenariusza: gracz 0
			n += o->interp != nullptr;
		}
		// obj_init dopiero po rejestracji wszystkich obiektów (obj_query widzi cały świat)
		// Potem zmienne Tcl z zapisu (stan skryptów w chwili zapisu; timery ustawił już obj_init).
		for (int i = 0; i < maps.objectCount(); ++i)
		{
			auto* o = scripts->object(idOf(i));
			if (!o || !o->interp || !o->cls) continue;
			scripts->runIn(*o, o->cls->initBody, "obj_init");
			for (auto& [k, v] : maps.objectTclVars(i)) scripts->setVar(*o, k, v);
		}
		std::printf("skrypty: %d obiektów ze skryptami\n", n);
	}

	void update(float dt)
	{
		dt = std::min(dt, 0.1f);
		maps.update(dt);
		if (scripts)
		{ // tura logiki 10 razy na sekundę (rozdział 11.7)
			scriptAcc += dt;
			while (scriptAcc >= 0.1)
			{
				scriptAcc -= 0.1;
				scripts->tick(0.1);
			}
		}
		for (auto& u : units) updateUnit(u, dt);
		fogTimer += dt;
		if (fogTimer >= 0.4f)
		{ // mgła co 4 tury logiki (rozdział 10.2)
			fogTimer = 0;
			for (auto& u : units) maps.revealFog(u.tx, u.ty, 6);
		}
		for (auto it = glides.begin(); it != glides.end();)
		{
			it->t = std::min(it->t + dt, it->dur);
			float k = it->dur > 0 ? it->t / it->dur : 1.f, p[3];
			for (int i = 0; i < 3; ++i) p[i] = it->from[i] + (it->to[i] - it->from[i]) * k;
			float dx = it->to[0] - it->from[0];
			maps.setObjectPose(it->obj, p[0], p[1], p[2], std::abs(dx) > 1e-3f ? (dx > 0 ? -kPi * 0.5f : kPi * 0.5f) : maps.objectRotY(it->obj));
			it = it->t >= it->dur ? glides.erase(it) : it + 1;
		}
	}

	// Pole pod kursorem: promień kamery przecięty z płaszczyzną głębokości krasnali.
	bool pickTile(int mx, int my, int w, int h, int& tx, int& ty, float& fx, float& fy)
	{
		glm::mat4 vp, world;
		gameCamera(cam, float(w) / float(std::max(h, 1)), vp, world);
		glm::mat4 inv = glm::inverse(vp * world);
		float nx = 2.f * float(mx) / float(w) - 1.f, ny = 1.f - 2.f * float(my) / float(h);
		glm::vec4 a = inv * glm::vec4(nx, ny, -1.f, 1.f), b = inv * glm::vec4(nx, ny, 1.f, 1.f);
		glm::vec3 p0 = glm::vec3(a) / a.w, p1 = glm::vec3(b) / b.w;
		float zPlane = (kGnomeZ * 0.5f + 7.f) * kBlock;
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
			int best = -1;
			float bd = 1.2f;
			for (size_t i = 0; i < units.size(); ++i)
			{
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
		ImGui::Text(tr("Krasnale: %zu"), units.size());
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
int ScriptHost::createObject(const std::string& cls, const std::string&, const float pos[3], const float rot[3])
{
	int i = g->maps.addObject(cls, pos[0], pos[1], pos[2]);
	g->maps.setObjectPose(i, pos[0], pos[1], pos[2], rot[1]);
	if (Vfs::lower(cls) == "zwerg") g->addUnit(i);
	return g->idOf(i);
}
void ScriptHost::deleteObject(int id)
{ // 🔶 brak usuwania obiektów sceny: wynosimy poza świat
	if (g->valid(id)) g->maps.setObjectPose(g->idx(id), -1000.f, -1000.f, 0.f, 0.f);
}
bool ScriptHost::getPos(int id, float pos[3])
{
	if (!g->valid(id)) return false;
	const float* p = g->maps.objectPos(g->idx(id));
	pos[0] = p[0], pos[1] = p[1], pos[2] = p[2];
	return true;
}
void ScriptHost::setPos(int id, const float pos[3])
{
	if (!g->valid(id)) return;
	if (Unit* u = g->unitOf(g->idx(id)))
	{
		u->x = pos[0], u->y = pos[1], u->z = pos[2];
		u->path.clear(), u->state = Unit::Idle;
		g->snapToFloor(*u);
		return;
	}
	g->maps.setObjectPose(g->idx(id), pos[0], pos[1], pos[2], g->maps.objectRotY(g->idx(id)));
}
void ScriptHost::setRotY(int id, float r)
{
	if (!g->valid(id)) return;
	if (Unit* u = g->unitOf(g->idx(id))) u->facing = r;
	const float* p = g->maps.objectPos(g->idx(id));
	g->maps.setObjectPose(g->idx(id), p[0], p[1], p[2], r);
}
float ScriptHost::getRotY(int id)
{
	return g->valid(id) ? g->maps.objectRotY(g->idx(id)) : 0.f;
}
void ScriptHost::setAnim(int id, const std::string& anim, int, int mode)
{
	if (!g->valid(id)) return;
	if (Unit* u = g->unitOf(g->idx(id)); u && u->state != Unit::Idle) return; // chód i kopanie mają własne animacje
	size_t dot = anim.find('.');
	g->maps.playAnim(g->idx(id), dot == std::string::npos ? anim : anim.substr(dot + 1), mode == 2);
}
double ScriptHost::animLength(int id, const std::string& anim)
{ // klatki animacji modelu / 10 kl./s
	size_t dot = anim.find('.');
	std::string name = dot == std::string::npos ? anim : anim.substr(dot + 1);
	if (g->valid(id))
	{
		double s = g->maps.animSeconds(g->idx(id), name);
		if (s > 0) return s;
	}
	return 1.5; // 🔶 nieznana animacja albo db_animlength bez obiektu
}
bool ScriptHost::walkTo(int id, const float t[3], std::function<void(bool)> done)
{
	Unit* u = g->unitOf(g->idx(id));
	if (!u || u->digJob) return false; // krasnal pracuje dla gracza
	int tx = int(std::floor(t[0])), ty = g->tileOfFeet(t[1]);
	for (int d = 0; d < 4 && !g->nav.standable(tx, ty) && !g->nav.climbable(tx, ty); ++d) ++ty;
	auto path = g->nav.path(u->tx, u->ty, tx, ty);
	if (path.empty()) return false;
	u->path = std::move(path), u->step = 0, u->state = Unit::Walk, u->digX = -1;
	u->goalX = t[0], u->goalZ = t[2];
	u->onArrive = std::move(done);
	return true;
}
float ScriptHost::hmap(float x, float y)
{ // głębokość tylnej ściany w punkcie: wartość siatki 8×8 (= z skryptu)
	const Pmp* p = g->grid();
	if (!p || x < 0 || y < 0 || x >= float(p->width) || y >= float(p->height)) return 0.f;
	const BlockRecord* r = g->maps.blockDb().record(p->blocks[size_t(int(y)) * p->width + size_t(int(x))]);
	if (!r) return 0.f;
	int fx = std::clamp(int((x - std::floor(x)) * 8.f), 0, 7), fy = std::clamp(int((y - std::floor(y)) * 8.f), 0, 7);
	return float(r->depth[size_t(fy * 8 + fx)]);
}
bool ScriptHost::underwater(const float p[3])
{
	return g->maps.fluidFill(int(std::floor(p[0])), int(std::floor(p[1]))) > 0.5f;
}
bool ScriptHost::findPlace(const float c[3], float r, float out[3])
{
	int cx = int(std::floor(c[0])), cy = g->tileOfFeet(c[1]);
	if (g->nav.standable(cx, cy))
	{ // pole celu jest dobre: zostaje x i głębokość, y na podłożu
		out[0] = c[0], out[1] = g->floorY(cx, cy), out[2] = c[2];
		return true;
	}
	int rad = std::max(1, int(std::ceil(r)));
	for (int d = 0; d <= rad; ++d)
		for (int dy = -d; dy <= d; ++dy)
			for (int dx = -d; dx <= d; ++dx)
			{
				if (std::max(std::abs(dx), std::abs(dy)) != d || !g->nav.standable(cx + dx, cy + dy)) continue;
				out[0] = float(cx + dx) + 0.5f, out[1] = g->floorY(cx + dx, cy + dy), out[2] = c[2];
				return true;
			}
	return false;
}
void ScriptHost::glide(int id, const float to[3], float seconds)
{
	if (!g->valid(id) || g->unitOf(g->idx(id))) return;
	const float* p = g->maps.objectPos(g->idx(id));
	Game::Glide gl{g->idx(id), {p[0], p[1], p[2]}, {to[0], to[1], to[2]}, 0.f, seconds};
	for (auto& x : g->glides)
		if (x.obj == gl.obj)
		{
			x = gl;
			return;
		}
	g->glides.push_back(gl);
}

void ScriptHost::link(int child, int parent, int link)
{
	if (g->valid(child)) g->maps.linkObject(g->idx(child), g->valid(parent) ? g->idx(parent) : -1, link);
}
bool ScriptHost::linkOffset(int id, int link, float out[3])
{
	return g->valid(id) && g->maps.linkOffset(g->idx(id), link, out);
}

int ScriptHost::gnomePosition(int id)
{
	Unit* u = g->unitOf(g->idx(id));
	return u && !g->nav.standable(u->tx, u->ty) ? 1 : 0;
}

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
		g.scripts = std::make_unique<script::Engine>(g.vfs);
		g.host.g = &g;
		g.scriptGnomes = args.scriptGnomes;
		g.scripts->setHost(&g.host);
		{ // język tekstów gry: --locale, inaczej język interfejsu (jeśli MESSAGES.TXT go ma), inaczej język wydania
			const auto& langs = g.scripts->messages().languages();
			if (!args.locale.empty()) g.scripts->setLocale(args.locale);
			else if (std::find(langs.begin(), langs.end(), i18n::language()) != langs.end()) g.scripts->setLocale(i18n::language());
		}
		Uint64 t0 = SDL_GetTicks64();
		std::string serr;
		if (!g.scripts->boot(serr)) std::fprintf(stderr, "skrypty: %s\n", serr.c_str());
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
