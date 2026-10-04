// openwiggles: gra (pierwsza grywalna wersja, docs/silnik/14_gra.md).
//
// Użycie:
//   openwiggles [--game <katalog gry>] [--scenario NAZWA | --save PLIK.sav | --template SZABLON]
//               [--renderer vulkan|opengl] [--size SZERxWYS] [--lang KOD] [--screenshot plik.png]
//               [--view X Y ZOOM DX DY] [--warmup SEKUNDY]
// Bez mapy: najnowszy zapis z Data/Gamesave, a gdy go nie ma, szablon kris_unq_060 z jednym krasnalem.
//
// Sterowanie: LPM zaznacza krasnala, PPM na wolnym polu = idź, PPM na skale = kop.
// WASD / strzałki / środkowy przycisk myszy przesuwają kamerę, kółko przybliża, N = nowy krasnal
// przy kursorze, Esc wychodzi.
//
// 🔶 Tymczasowo: scena i rysowanie z wigview (MapView), ruch po siatce pól (lib/nav.h), kopanie
// zamienia blok na pusty (59). Zachowania krasnali z Tcl (Scripts/classes/zwerg) jeszcze nie działają.
#include "lib/blockdb.h"
#include "lib/gfx/model_renderer.h"
#include "lib/gfx/texture_cache.h"
#include "lib/i18n.h"
#include "lib/map_template.h"
#include "lib/model3db.h"
#include "lib/nav.h"
#include "lib/png_write.h"
#include "lib/rhi/rhi.h"
#include "lib/script/engine.h"
#include "lib/sphere.h"
#include "lib/vfs.h"
#include "ow_version.h"
#include "wigview/map_view.h"

#include <SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

using namespace ow;

namespace {

constexpr float kBlock = MapView::kBlock;
constexpr float kGnomeZ = 8.0f;      // głębokość krasnala w skali skryptu (świat 11 bloków), jak w scenariuszach
constexpr float kWalkSpeed = 1.6f;   // pola na sekundę (🔶)
constexpr float kClimbSpeed = 0.9f;
constexpr float kDigTime = 2.0f;     // sekundy kopania jednego pola (🔶)
constexpr float kPi = 3.14159265f;

struct Args {
	std::string game, scenario, save, templ, screenshot, lang;
	rhi::Backend renderer = rhi::Backend::Vulkan;
	bool rendererForced = false;
	int width = 1280, height = 800;
	bool scripts = true;          // --no-scripts: bez silnika Tcl
	bool scriptGnomes = true;     // --player-gnomes: krasnale tylko na rozkaz gracza, bez skryptów
	bool reveal = false;          // --reveal: bez mgły wojny
	float warmup = 0;            // --warmup S: przewiń symulację o S sekund (zrzuty, testy)
	std::optional<GameView> view; // --view x y zoom dx dy
};

bool parseArgs(int argc, char** argv, Args& a) {
	for (int i = 1; i < argc; ++i) {
		std::string k = argv[i];
		auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
		if (k == "--game") a.game = next();
		else if (k == "--scenario") a.scenario = next();
		else if (k == "--save") a.save = next();
		else if (k == "--template") a.templ = next();
		else if (k == "--screenshot") a.screenshot = next();
		else if (k == "--lang") a.lang = next();
		else if (k == "--size") std::sscanf(next(), "%dx%d", &a.width, &a.height);
		else if (k == "--warmup") a.warmup = float(std::atof(next()));
		else if (k == "--no-scripts") a.scripts = false;
		else if (k == "--player-gnomes") a.scriptGnomes = false;
		else if (k == "--reveal") a.reveal = true;
		else if (k == "--view") {
			GameView v;
			v.x = float(std::atof(next()));
			v.y = float(std::atof(next()));
			v.zoom = float(std::atof(next()));
			v.dx = float(std::atof(next()));
			v.dy = float(std::atof(next()));
			a.view = v;
		}
		else if (k == "--renderer") {
			if (!rhi::parseBackend(next(), a.renderer)) return false;
			a.rendererForced = true;
		} else {
			std::fprintf(stderr, "nieznana opcja %s\n", k.c_str());
			return false;
		}
	}
	if (a.game.empty()) {
		if (const char* e = std::getenv("OW_GAME")) a.game = e;
		else if (const char* h = std::getenv("HOME")) a.game = std::string(h) + "/.wine/drive_c/Program Files (x86)/Innonics/Wiggles";
	}
	return true;
}

// Przycięcie kątów kamery gry (docs/silnik/06_kamera.md §6.3).
float angleLimit(float R) { return R > 15.6f + 1e-3f ? 2.23f / (R - 15.6f) : 1e9f; }

// Kamera gry (set_view x y zoom dx dy): macierz rzutowania·widoku i macierz świata (gra -> OpenGL).
void gameCamera(const GameView& g, float aspect, glm::mat4& viewProj, glm::mat4& world) {
	float Rb = g.zoom * 16.f - 10.f;
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

struct Unit {
	int obj = -1;
	float x = 0, y = 0;   // stopy: x środka, y dołu pola (bloki)
	int tx = 0, ty = 0;   // bieżące pole
	std::vector<std::pair<int, int>> path;
	size_t step = 0;
	enum State { Idle, Walk, Dig } state = Idle;
	int digX = -1, digY = -1;
	float digT = 0;
	float facing = -kPi * 0.5f; // −π/2 = w prawo, +π/2 = w lewo (obserwacja modelu mann)
	float z = kGnomeZ;          // głębokość (skala skryptu), z zapisu albo scenariusza
	std::function<void(bool)> onArrive; // akcja walk zlecona przez skrypt
	bool digJob = false;        // przydzielony do kopania zaznaczonych pól (gracz)
};

struct Game;
// Most między silnikiem skryptów a sceną: ID obiektu skryptu = indeks obiektu MapView + 1.
// Metody zdefiniowane po Game (korzystają z krasnali i nawigacji).
struct ScriptHost : script::Host {
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

struct Game {
	Vfs vfs;
	std::unique_ptr<script::Engine> scripts;
	ScriptHost host;
	bool scriptGnomes = true; // krasnale sterowane skryptami (--player-gnomes: tylko gracz)
	// ID obiektów skryptów: obiekty z zapisu mają ID z zapisu (zmienne Tcl się do nich odwołują),
	// pozostałe od 100000. idx(id) = indeks obiektu MapView.
	std::unordered_map<int, int> idToIdx;
	std::vector<int> idxToId;
	int nextScriptId = 100000;
	int idOf(int index) {
		if (index >= int(idxToId.size())) idxToId.resize(size_t(index) + 1, 0);
		int& id = idxToId[size_t(index)];
		if (!id) {
			const std::string& n = maps.objectName(index);
			char* end = nullptr;
			long v = n.empty() ? 0 : std::strtol(n.c_str(), &end, 10);
			id = (v > 0 && end && !*end && !idToIdx.count(int(v))) ? int(v) : nextScriptId++;
			idToIdx[id] = index;
		}
		return id;
	}
	bool valid(int id) const { return idToIdx.count(id) != 0; }
	int idx(int id) const { auto it = idToIdx.find(id); return it == idToIdx.end() ? -1 : it->second; }
	struct Glide {
		int obj;
		float from[3], to[3], t, dur;
	};
	std::vector<Glide> glides; // płynne ruchy obiektów zlecone przez skrypty
	std::set<std::pair<int, int>> digMarks; // pola zaznaczone do kopania (przeciągnięcie PPM)
	int dragX0 = -1, dragY0 = -1;          // początek przeciągnięcia PPM (pole)
	bool rightDrag = false;
	float fogTimer = 0;
	double scriptAcc = 0;
	std::unique_ptr<rhi::Device> device;
	std::unique_ptr<TextureCache> textures;
	std::vector<Vec3f> sphere;
	MapView maps;
	ModelDrawOptions draw;
	NavGrid nav;
	GameView cam;
	std::vector<Unit> units;
	int selected = -1;
	std::string status;
	int hoverX = -1, hoverY = -1;
	bool showHelp = true;

	const Pmp* grid() const { return maps.grid(); }

	// Wysokość stóp w polu (x, ty): podłoże z bloku pod spodem (pierwszy wiersz siatki 8×8 o wysokości ≥ 13
	// w środkowej kolumnie), np. na łące 0,5 pola niżej niż dół pola. Na ścianie: dół pola.
	float floorY(int x, int ty) const {
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

	void snapToFloor(Unit& u) {
		int x = int(std::floor(u.x)), y = tileOfFeet(u.y);
		for (int d = 0; d < 40; ++d) {
			for (int yy : {y + d, y - d})
				if (nav.standable(x, yy) || (d > 0 && nav.climbable(x, yy))) {
					u.tx = x, u.ty = yy;
					u.x = float(x) + 0.5f, u.y = floorY(x, yy);
					return;
				}
		}
		u.tx = x, u.ty = y;
	}

	Unit* unitOf(int obj) {
		for (auto& u : units)
			if (u.obj == obj) return &u;
		return nullptr;
	}

	int addUnit(int obj) {
		maps.setObjectModel(obj, "mann.3db", "mann");
		Unit u;
		u.obj = obj;
		const float* p = maps.objectPos(obj);
		u.x = p[0], u.y = p[1], u.z = p[2];
		snapToFloor(u);
		units.push_back(u);
		return int(units.size()) - 1;
	}

	void spawnUnit(float x, float y) {
		int o = maps.addObject("Zwerg", x, y, kGnomeZ);
		selected = addUnit(o);
	}

	void initWorld() {
		units.clear();
		idToIdx.clear(), idxToId.clear(), nextScriptId = 100000;
		for (int i = 0; i < maps.objectCount(); ++i) idOf(i);
		selected = -1;
		if (const Pmp* g = grid()) nav.build(*g, maps.blockDb());
		for (int i = 0; i < maps.objectCount(); ++i)
			if (Vfs::lower(maps.objectClass(i)) == "zwerg") addUnit(i);
		if (const auto& v = maps.view()) cam = *v;
		else if (const Pmp* g = grid()) cam = GameView{float(g->width) * 0.5f, float(g->height) * 0.5f, 1.38f, 0.f, 0.f};
		if (units.empty() && grid()) { // mapa bez krasnali: jeden przy środku kamery
			spawnUnit(cam.x, cam.y);
			cam.x = units.back().x, cam.y = units.back().y - 1.f;
		}
		if (!units.empty()) selected = 0;
		status = units.empty() ? tr("Brak krasnali na mapie.") : "";
	}

	// Rozkaz: idź na pole albo kop pole skały.
	void order(int tx, int ty) {
		if (selected < 0 || selected >= int(units.size())) return;
		Unit& u = units[size_t(selected)];
		if (nav.open(tx, ty)) {
			int gx = tx, gy = ty;
			for (int d = 0; d < 6 && !nav.standable(gx, gy) && !nav.climbable(gx, gy); ++d) ++gy; // cel w powietrzu: na ziemię
			auto p = nav.path(u.tx, u.ty, gx, gy);
			if (p.empty()) {
				status = tr("Nie ma drogi.");
				return;
			}
			u.path = std::move(p), u.step = 0, u.state = Unit::Walk, u.digX = -1;
			status.clear();
			return;
		}
		// Kopanie: stanowisko obok pola (z boku, nad nim albo pod nim przy ścianie), najkrótsza droga.
		std::vector<std::pair<int, int>> best;
		const int cand[4][2] = {{tx - 1, ty}, {tx + 1, ty}, {tx, ty - 1}, {tx, ty + 1}};
		for (auto& c : cand) {
			if (!(nav.standable(c[0], c[1]) || nav.climbable(c[0], c[1]))) continue;
			auto p = nav.path(u.tx, u.ty, c[0], c[1]);
			if (!p.empty() && (best.empty() || p.size() < best.size())) best = std::move(p);
		}
		if (best.empty()) {
			status = tr("Nie da się dojść do tej skały.");
			return;
		}
		u.path = std::move(best), u.step = 0, u.state = Unit::Walk, u.digX = tx, u.digY = ty;
		status.clear();
	}

	void dig(int x, int y) {
		// 🔶 Wykopane pole: tunel z tylną ścianą (blok 4523: siatka 8×8 o wysokości 7), bez zszywania z sąsiadami.
		// Rodzaj gruntu bez zmian, ale nie „otwór do tła” (typ 5): wtedy grunt pola niżej albo skała A (4).
		uint8_t type = grid()->types[size_t(y) * grid()->width + size_t(x)];
		if ((type & 0x7F) == 5) {
			uint8_t below = y + 1 < int(grid()->height) ? grid()->types[size_t(y + 1) * grid()->width + size_t(x)] : 4;
			type = uint8_t((below & 0x7F) == 5 || (below & 0x7F) == 0 ? 4 : (below & 0x7F));
		}
		maps.setBlock(x, y, 4523, type);
		nav.updateTile(*grid(), maps.blockDb(), x, y);
		maps.rebuildTerrain();
		digMarks.erase({x, y});
		// Znalezisko jak w dig_execute (z_dignwalk.tcl): 4% szans; rodzaj losowany (🔶 bez stref kampanii).
		if (float(std::rand()) / float(RAND_MAX) < 0.04f) {
			static const char* kFinds[] = {"Stein", "Eisenerz", "Kohle", "Golderz", "Kristallerz"};
			const char* cls = kFinds[std::rand() % 5];
			float pos[3] = {float(x) + 0.5f, floorY(x, y), kGnomeZ}, rot[3] = {0, 0, 0};
			int id = host.createObject(cls, "", pos, rot);
			if (scripts) scripts->attach(id, cls, "");
			status = std::string(tr("Znalezisko: ")) + cls;
		}
	}

	// Następne zaznaczone pole dla krasnala z zadaniem kopania: najbliższe, do którego da się dojść.
	bool nextDigJob(Unit& u) {
		std::vector<std::pair<int, std::pair<int, int>>> cand;
		for (auto& m : digMarks) cand.push_back({std::abs(m.first - u.tx) + std::abs(m.second - u.ty), m});
		std::sort(cand.begin(), cand.end());
		for (size_t i = 0; i < cand.size() && i < 12; ++i) {
			auto [mx, my] = cand[i].second;
			if (nav.open(mx, my)) {
				digMarks.erase({mx, my});
				continue;
			}
			const int st[4][2] = {{mx - 1, my}, {mx + 1, my}, {mx, my - 1}, {mx, my + 1}};
			for (auto& c : st) {
				if (!(nav.standable(c[0], c[1]) || nav.climbable(c[0], c[1]))) continue;
				auto p = nav.path(u.tx, u.ty, c[0], c[1]);
				if (p.empty()) continue;
				u.path = std::move(p), u.step = 0, u.state = Unit::Walk, u.digX = mx, u.digY = my;
				u.onArrive = nullptr;
				return true;
			}
		}
		return false;
	}

	// Zaznaczenie prostokąta pól skały do kopania; zadanie dostaje zaznaczony krasnal.
	void markDig(int x0, int y0, int x1, int y1) {
		int n = 0;
		for (int y = std::min(y0, y1); y <= std::max(y0, y1); ++y)
			for (int x = std::min(x0, x1); x <= std::max(x0, x1); ++x)
				if (!nav.open(x, y) && grid() && x >= 0 && y >= 0 && x < int(grid()->width) && y < int(grid()->height))
					n += digMarks.insert({x, y}).second;
		if (selected >= 0 && selected < int(units.size())) units[size_t(selected)].digJob = true;
		status = std::string(tr("Do kopania: ")) + std::to_string(digMarks.size());
		(void)n;
	}

	void updateUnit(Unit& u, float dt) {
		if (u.state == Unit::Walk) {
			if (u.step >= u.path.size()) {
				u.state = u.digX >= 0 ? Unit::Dig : Unit::Idle;
				u.digT = 0;
				if (u.onArrive) {
					auto f = std::move(u.onArrive);
					u.onArrive = nullptr;
					f(true);
				}
			} else {
				auto [wx, wy] = u.path[u.step];
				float gx = float(wx) + 0.5f, gy = floorY(wx, wy);
				bool vertical = wx == u.tx && wy != u.ty;
				float speed = vertical ? kClimbSpeed : kWalkSpeed;
				float ddx = gx - u.x, ddy = gy - u.y, len = std::hypot(ddx, ddy), mv = speed * dt;
				if (std::abs(ddx) > 1e-3f) u.facing = ddx > 0 ? -kPi * 0.5f : kPi * 0.5f;
				if (len <= mv) {
					u.x = gx, u.y = gy, u.tx = wx, u.ty = wy;
					++u.step;
				} else
					u.x += ddx / len * mv, u.y += ddy / len * mv;
				const char* anim = !vertical ? "gehen_loop" : (ddy < 0 ? "kletter_hoch_loop" : "kletter_runter_loop");
				if (vertical) u.facing = kPi; // twarzą do ściany
				maps.playAnim(u.obj, anim, true);
			}
		}
		if (u.state == Unit::Dig) {
			const char* anim = u.digY > u.ty ? "buddeln_unten" : u.digY < u.ty ? "buddeln_oben_normal" : "buddeln_vorn";
			if (u.digY == u.ty) u.facing = u.digX > u.tx ? -kPi * 0.5f : kPi * 0.5f;
			maps.playAnim(u.obj, anim, true);
			u.digT += dt;
			if (u.digT >= kDigTime) {
				dig(u.digX, u.digY);
				u.digX = u.digY = -1;
				u.state = Unit::Idle;
				snapToFloor(u); // pod krasnalem mogło zniknąć podłoże
			}
		}
		if (u.state == Unit::Idle && u.digJob) {
			if (digMarks.empty() || !nextDigJob(u)) u.digJob = false; // nic osiągalnego: koniec zadania
		}
		if (u.state == Unit::Idle) {
			if (!nav.standable(u.tx, u.ty) && !nav.climbable(u.tx, u.ty) && nav.open(u.tx, u.ty + 1)) {
				u.path = {{u.tx, u.ty + 1}}, u.step = 0, u.state = Unit::Walk; // spada
			} else if (!scripts) // ze skryptami animację w miejscu ustawia skrypt (set_anim)
				maps.playAnim(u.obj, nav.standable(u.tx, u.ty) ? "standard" : "kletterstand", true);
		}
		maps.setObjectPose(u.obj, u.x, u.y, u.z, u.facing);
	}

	// Skrypty obiektów świata (obj_init). Krasnale na razie steruje gracz (C++), nie skrypty.
	void attachScripts() {
		if (!scripts) return;
		int n = 0;
		for (int i = 0; i < maps.objectCount(); ++i) {
			bool gnome = Vfs::lower(maps.objectClass(i)) == "zwerg";
			if (gnome && !scriptGnomes) continue;
			auto* o = scripts->attach(idOf(i), maps.objectClass(i), "", false);
			if (!o) continue;
			o->owner = maps.objectOwner(i); // z zapisu
			if (gnome && o->owner < 0) o->owner = 0; // krasnale ze scenariusza: gracz 0
			n += o->interp != nullptr;
		}
		// obj_init dopiero po rejestracji wszystkich obiektów (obj_query widzi cały świat)
		// Potem zmienne Tcl z zapisu (stan skryptów w chwili zapisu; timery ustawił już obj_init).
		for (int i = 0; i < maps.objectCount(); ++i) {
			auto* o = scripts->object(idOf(i));
			if (!o || !o->interp || !o->cls) continue;
			scripts->runIn(*o, o->cls->initBody, "obj_init");
			for (auto& [k, v] : maps.objectTclVars(i)) scripts->setVar(*o, k, v);
		}
		std::printf("skrypty: %d obiektów ze skryptami\n", n);
	}

	void update(float dt) {
		dt = std::min(dt, 0.1f);
		maps.update(dt);
		if (scripts) { // tura logiki 10 razy na sekundę (rozdział 11.7)
			scriptAcc += dt;
			while (scriptAcc >= 0.1) {
				scriptAcc -= 0.1;
				scripts->tick(0.1);
			}
		}
		for (auto& u : units) updateUnit(u, dt);
		fogTimer += dt;
		if (fogTimer >= 0.4f) { // mgła co 4 tury logiki (rozdział 10.2)
			fogTimer = 0;
			for (auto& u : units) maps.revealFog(u.tx, u.ty, 6);
		}
		for (auto it = glides.begin(); it != glides.end();) {
			it->t = std::min(it->t + dt, it->dur);
			float k = it->dur > 0 ? it->t / it->dur : 1.f, p[3];
			for (int i = 0; i < 3; ++i) p[i] = it->from[i] + (it->to[i] - it->from[i]) * k;
			float dx = it->to[0] - it->from[0];
			maps.setObjectPose(it->obj, p[0], p[1], p[2], std::abs(dx) > 1e-3f ? (dx > 0 ? -kPi * 0.5f : kPi * 0.5f) : maps.objectRotY(it->obj));
			it = it->t >= it->dur ? glides.erase(it) : it + 1;
		}
	}

	// Pole pod kursorem: promień kamery przecięty z płaszczyzną głębokości krasnali.
	bool pickTile(int mx, int my, int w, int h, int& tx, int& ty, float& fx, float& fy) {
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

	void click(int button, int mx, int my, int w, int h) {
		int tx, ty;
		float fx, fy;
		if (!pickTile(mx, my, w, h, tx, ty, fx, fy)) return;
		if (button == SDL_BUTTON_LEFT) {
			int best = -1;
			float bd = 1.2f;
			for (size_t i = 0; i < units.size(); ++i) {
				float d = std::hypot(units[i].x - fx, (units[i].y - 0.6f) - fy);
				if (d < bd) bd = d, best = int(i);
			}
			if (best >= 0) selected = best;
		} else if (button == SDL_BUTTON_RIGHT) {
			rightDrag = true;
			dragX0 = tx, dragY0 = ty;
		}
	}

	void release(int mx, int my, int w, int h) {
		if (!rightDrag) return;
		rightDrag = false;
		int tx, ty;
		float fx, fy;
		if (!pickTile(mx, my, w, h, tx, ty, fx, fy)) return;
		if (tx == dragX0 && ty == dragY0) {
			if (selected >= 0 && selected < int(units.size())) units[size_t(selected)].digJob = false;
			order(tx, ty);
		} else
			markDig(dragX0, dragY0, tx, ty);
	}

	// Znaczniki kopania i prostokąt przeciągania: rzut narożników pola (przednia płaszczyzna) na ekran.
	void drawMarks(int w, int h) {
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
		if (rightDrag && hoverX >= 0)
			quad(std::min(dragX0, hoverX), std::min(dragY0, hoverY), std::max(dragX0, hoverX), std::max(dragY0, hoverY), 0,
			     IM_COL32(255, 255, 255, 200));
	}

	void render(int w, int h) {
		glm::mat4 vp, world;
		gameCamera(cam, float(w) / float(std::max(h, 1)), vp, world);
		glm::vec3 target = glm::vec3(cam.x, cam.y, 14.f) * kBlock;
		maps.render(vp, world, draw, target.z, target);
	}

	void hud() {
		ImGui::SetNextWindowPos(ImVec2(8, 8), ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.6f);
		ImGui::Begin("hud", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
		ImGui::Text("OpenWiggles %s", OW_VERSION);
		ImGui::Text(tr("Krasnale: %zu"), units.size());
		if (selected >= 0 && selected < int(units.size())) {
			const Unit& u = units[size_t(selected)];
			const char* st = u.state == Unit::Walk ? tr("idzie") : u.state == Unit::Dig ? tr("kopie") : tr("czeka");
			ImGui::Text(tr("Zaznaczony: krasnal %d (%s), pole %d, %d"), selected + 1, st, u.tx, u.ty);
		}
		if (hoverX >= 0) ImGui::TextDisabled(tr("Pole pod kursorem: %d, %d (%s)"), hoverX, hoverY, nav.open(hoverX, hoverY) ? tr("wolne") : tr("skała"));
		if (!status.empty()) ImGui::TextColored(ImVec4(1, 0.7f, 0.3f, 1), "%s", status.c_str());
		if (showHelp) {
			ImGui::Separator();
			ImGui::TextDisabled("%s", tr("LPM: zaznacz krasnala   PPM: idź / kop skałę"));
			ImGui::TextDisabled("%s", tr("Przeciągnij PPM: zaznacz skałę do kopania (zaznaczony krasnal kopie)"));
			ImGui::TextDisabled("%s", tr("WASD, strzałki, środkowy przycisk: kamera   kółko: przybliżenie"));
			ImGui::TextDisabled("%s", tr("N: nowy krasnal przy kursorze   F1: pomoc   Esc: wyjście"));
		}
		ImGui::End();
	}
};

// --- ScriptHost ---
int ScriptHost::createObject(const std::string& cls, const std::string&, const float pos[3], const float rot[3]) {
	int i = g->maps.addObject(cls, pos[0], pos[1], pos[2]);
	g->maps.setObjectPose(i, pos[0], pos[1], pos[2], rot[1]);
	if (Vfs::lower(cls) == "zwerg") g->addUnit(i);
	return g->idOf(i);
}
void ScriptHost::deleteObject(int id) { // 🔶 brak usuwania obiektów sceny: wynosimy poza świat
	if (g->valid(id)) g->maps.setObjectPose(g->idx(id), -1000.f, -1000.f, 0.f, 0.f);
}
bool ScriptHost::getPos(int id, float pos[3]) {
	if (!g->valid(id)) return false;
	const float* p = g->maps.objectPos(g->idx(id));
	pos[0] = p[0], pos[1] = p[1], pos[2] = p[2];
	return true;
}
void ScriptHost::setPos(int id, const float pos[3]) {
	if (!g->valid(id)) return;
	if (Unit* u = g->unitOf(g->idx(id))) {
		u->x = pos[0], u->y = pos[1], u->z = pos[2];
		u->path.clear(), u->state = Unit::Idle;
		g->snapToFloor(*u);
		return;
	}
	g->maps.setObjectPose(g->idx(id), pos[0], pos[1], pos[2], g->maps.objectRotY(g->idx(id)));
}
void ScriptHost::setRotY(int id, float r) {
	if (!g->valid(id)) return;
	if (Unit* u = g->unitOf(g->idx(id))) u->facing = r;
	const float* p = g->maps.objectPos(g->idx(id));
	g->maps.setObjectPose(g->idx(id), p[0], p[1], p[2], r);
}
float ScriptHost::getRotY(int id) { return g->valid(id) ? g->maps.objectRotY(g->idx(id)) : 0.f; }
void ScriptHost::setAnim(int id, const std::string& anim, int, int mode) {
	if (!g->valid(id)) return;
	if (Unit* u = g->unitOf(g->idx(id)); u && u->state != Unit::Idle) return; // chód i kopanie mają własne animacje
	size_t dot = anim.find('.');
	g->maps.playAnim(g->idx(id), dot == std::string::npos ? anim : anim.substr(dot + 1), mode == 2);
}
double ScriptHost::animLength(int id, const std::string& anim) { // klatki animacji modelu / 10 kl./s
	size_t dot = anim.find('.');
	std::string name = dot == std::string::npos ? anim : anim.substr(dot + 1);
	if (g->valid(id)) {
		double s = g->maps.animSeconds(g->idx(id), name);
		if (s > 0) return s;
	}
	return 1.5; // 🔶 nieznana animacja albo db_animlength bez obiektu
}
bool ScriptHost::walkTo(int id, const float t[3], std::function<void(bool)> done) {
	Unit* u = g->unitOf(g->idx(id));
	if (!u || u->digJob) return false; // krasnal pracuje dla gracza
	int tx = int(std::floor(t[0])), ty = g->tileOfFeet(t[1]);
	for (int d = 0; d < 4 && !g->nav.standable(tx, ty) && !g->nav.climbable(tx, ty); ++d) ++ty;
	auto path = g->nav.path(u->tx, u->ty, tx, ty);
	if (path.empty()) return false;
	u->path = std::move(path), u->step = 0, u->state = Unit::Walk, u->digX = -1;
	u->onArrive = std::move(done);
	return true;
}
float ScriptHost::hmap(float x, float y) { // głębokość tylnej ściany w punkcie: wartość siatki 8×8 (= z skryptu)
	const Pmp* p = g->grid();
	if (!p || x < 0 || y < 0 || x >= float(p->width) || y >= float(p->height)) return 0.f;
	const BlockRecord* r = g->maps.blockDb().record(p->blocks[size_t(int(y)) * p->width + size_t(int(x))]);
	if (!r) return 0.f;
	int fx = std::clamp(int((x - std::floor(x)) * 8.f), 0, 7), fy = std::clamp(int((y - std::floor(y)) * 8.f), 0, 7);
	return float(r->depth[size_t(fy * 8 + fx)]);
}
bool ScriptHost::underwater(const float p[3]) { return g->maps.fluidFill(int(std::floor(p[0])), int(std::floor(p[1]))) > 0.5f; }
bool ScriptHost::findPlace(const float c[3], float r, float out[3]) {
	int cx = int(std::floor(c[0])), cy = g->tileOfFeet(c[1]);
	int rad = std::max(1, int(std::ceil(r)));
	for (int d = 0; d <= rad; ++d)
		for (int dy = -d; dy <= d; ++dy)
			for (int dx = -d; dx <= d; ++dx) {
				if (std::max(std::abs(dx), std::abs(dy)) != d || !g->nav.standable(cx + dx, cy + dy)) continue;
				out[0] = float(cx + dx) + 0.5f, out[1] = g->floorY(cx + dx, cy + dy), out[2] = c[2];
				return true;
			}
	return false;
}
void ScriptHost::glide(int id, const float to[3], float seconds) {
	if (!g->valid(id) || g->unitOf(g->idx(id))) return;
	const float* p = g->maps.objectPos(g->idx(id));
	Game::Glide gl{g->idx(id), {p[0], p[1], p[2]}, {to[0], to[1], to[2]}, 0.f, seconds};
	for (auto& x : g->glides)
		if (x.obj == gl.obj) {
			x = gl;
			return;
		}
	g->glides.push_back(gl);
}

void ScriptHost::link(int child, int parent, int link) {
	if (g->valid(child)) g->maps.linkObject(g->idx(child), g->valid(parent) ? g->idx(parent) : -1, link);
}
bool ScriptHost::linkOffset(int id, int link, float out[3]) {
	return g->valid(id) && g->maps.linkOffset(g->idx(id), link, out);
}

int ScriptHost::gnomePosition(int id) {
	Unit* u = g->unitOf(g->idx(id));
	return u && !g->nav.standable(u->tx, u->ty) ? 1 : 0;
}

} // namespace

int main(int argc, char** argv) {
	Args args;
	if (!parseArgs(argc, argv, args)) return 2;
	i18n::loadBuiltin();
	i18n::setLanguage(args.lang.empty() ? i18n::systemLanguage() : args.lang);

	auto game = std::make_unique<Game>();
	Game& g = *game;
	g.vfs.addRoot(args.game);
	if (!g.vfs.exists("Data/3db")) {
		std::fprintf(stderr, "Nie znaleziono Data/3db w '%s'. Podaj --game <katalog gry>.\n", args.game.c_str());
		return 1;
	}
	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
		return 1;
	}
	SDL_Window* win = nullptr;
	std::string err;
	{
		std::vector<rhi::Backend> order{args.renderer};
		rhi::Backend other = args.renderer == rhi::Backend::Vulkan ? rhi::Backend::OpenGL : rhi::Backend::Vulkan;
		if (!args.rendererForced && rhi::backendCompiled(other)) order.push_back(other);
		for (rhi::Backend b : order) {
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
		if (!g.device) {
			std::fprintf(stderr, "Nie udało się uruchomić żadnego backendu renderowania\n");
			return 1;
		}
	}
	rhi::Device& dev = *g.device;
	g.textures = std::make_unique<TextureCache>(g.vfs);
	g.textures->setDevice(&dev);
	if (auto sd = g.vfs.read("Data/SPHERE.DAT")) g.sphere = parseSphereDat(*sd);
	g.maps.init(dev, &g.vfs, g.textures.get(), &g.sphere);

	bool ok;
	if (!args.save.empty())
		ok = args.save.find('/') != std::string::npos ? g.maps.loadSavePath(args.save) : g.maps.loadScenario(args.save);
	else if (!args.templ.empty())
		ok = g.maps.loadTemplate(args.templ.size() > 4 && args.templ.substr(args.templ.size() - 4) == ".tcl" ? args.templ : args.templ + ".tcl");
	else if (args.scenario.empty()) {
		// Domyślnie najnowszy zapis gry z Data/Gamesave, a bez zapisów szablon kris_unq_060.
		std::string newest;
		std::filesystem::file_time_type best{};
		for (const auto& f : g.vfs.list("Data/Gamesave")) {
			if (f.size() < 4 || Vfs::lower(f.substr(f.size() - 4)) != ".sav") continue;
			if (auto p = g.vfs.resolve("Data/Gamesave/" + f)) {
				std::error_code ec;
				auto t = std::filesystem::last_write_time(*p, ec);
				if (!ec && (newest.empty() || t > best)) best = t, newest = f;
			}
		}
		ok = newest.empty() ? g.maps.loadTemplate("kris_unq_060.tcl") : g.maps.loadScenario(newest);
		std::printf("mapa: %s\n", newest.empty() ? "kris_unq_060" : newest.c_str());
	} else {
		std::string s = args.scenario;
		if (s.size() < 4 || (s.substr(s.size() - 4) != ".tcl" && s.substr(s.size() - 4) != ".sav")) s += ".tcl";
		ok = g.maps.loadScenario(s);
	}
	if (!ok) std::fprintf(stderr, "mapa: %s\n", g.maps.error().c_str());
	g.initWorld();
	if (args.reveal) g.maps.setFog(MapView::FogMode::Off);
	if (args.scripts) {
		g.scripts = std::make_unique<script::Engine>(g.vfs);
		g.host.g = &g;
		g.scriptGnomes = args.scriptGnomes;
		g.scripts->setHost(&g.host);
		Uint64 t0 = SDL_GetTicks64();
		std::string serr;
		if (!g.scripts->boot(serr)) std::fprintf(stderr, "skrypty: %s\n", serr.c_str());
		std::printf("skrypty: %zu klas w %llu ms\n", g.scripts->classCount(), (unsigned long long)(SDL_GetTicks64() - t0));
		g.attachScripts();
	}
	if (args.view) g.cam = *args.view;
	for (float t = 0; t < args.warmup; t += 0.1f) g.update(0.1f);

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
		g.render(w, h);
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

	if (!args.screenshot.empty()) {
		// Zrzut: krótka symulacja (krasnal idzie w prawo), potem klatka do PNG.
		if (!g.units.empty()) {
			const Unit& u = g.units[0];
			if (std::getenv("OW_TEST_DIG")) { // test: kopanie 3×2 pól pod pierwszym krasnalem
				g.selected = 0;
				int dx = std::getenv("OW_TEST_DIG_DX") ? std::atoi(std::getenv("OW_TEST_DIG_DX")) : 0;
				g.markDig(u.tx - 1 + dx, u.ty + 1, u.tx + 1 + dx, u.ty + 2);
			} else
				g.order(u.tx + 3, u.ty);
		}
		int frames = std::getenv("OW_TEST_DIG") ? std::atoi(std::getenv("OW_TEST_DIG")) : 30; // test kopania: liczba klatek po 0,05 s
		for (int i = 0; i < frames; ++i) {
			SDL_Event e;
			while (SDL_PollEvent(&e)) {}
			g.update(0.05f);
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
	while (running) {
		SDL_Event e;
		int w, h;
		SDL_GetWindowSize(win, &w, &h);
		while (SDL_PollEvent(&e)) {
			ImGui_ImplSDL2_ProcessEvent(&e);
			ImGuiIO& io = ImGui::GetIO();
			if (e.type == SDL_QUIT) running = false;
			if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = false;
			if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F1) g.showHelp = !g.showHelp;
			if (io.WantCaptureMouse) continue;
			if (e.type == SDL_MOUSEBUTTONDOWN && (e.button.button == SDL_BUTTON_LEFT || e.button.button == SDL_BUTTON_RIGHT))
				g.click(e.button.button, e.button.x, e.button.y, w, h);
			if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_MIDDLE) dragging = true;
			if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_RIGHT) g.release(e.button.x, e.button.y, w, h);
			if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_MIDDLE) dragging = false;
			if (e.type == SDL_MOUSEMOTION) {
				if (dragging) {
					float s = (g.cam.zoom * 16.f - 10.f) / float(std::max(h, 1)) * 0.9f;
					g.cam.x -= float(e.motion.xrel) * s;
					g.cam.y -= float(e.motion.yrel) * s;
				}
				int tx, ty;
				float fx, fy;
				if (g.pickTile(e.motion.x, e.motion.y, w, h, tx, ty, fx, fy)) g.hoverX = tx, g.hoverY = ty;
			}
			if (e.type == SDL_MOUSEWHEEL) g.cam.zoom = std::clamp(g.cam.zoom * std::pow(0.9f, float(e.wheel.y)), 0.9f, 3.f);
			if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_n && !io.WantTextInput) {
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
		if (!ImGui::GetIO().WantCaptureKeyboard) {
			float sp = (g.cam.zoom * 16.f - 10.f) * 0.8f * dt;
			if (k[SDL_SCANCODE_A] || k[SDL_SCANCODE_LEFT]) g.cam.x -= sp;
			if (k[SDL_SCANCODE_D] || k[SDL_SCANCODE_RIGHT]) g.cam.x += sp;
			if (k[SDL_SCANCODE_W] || k[SDL_SCANCODE_UP]) g.cam.y -= sp;
			if (k[SDL_SCANCODE_S] || k[SDL_SCANCODE_DOWN]) g.cam.y += sp;
		}
		g.update(dt);
		if (!frame(nullptr)) SDL_Delay(16);
	}
	shutdown();
	return 0;
}
