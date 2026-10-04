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
#include <memory>
#include <optional>
#include <string>
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
};

// Most między silnikiem skryptów a sceną: ID obiektu skryptu = indeks obiektu MapView + 1.
struct ScriptHost : script::Host {
	MapView* maps = nullptr;
	int createObject(const std::string& cls, const std::string&, const float pos[3], const float rot[3]) override {
		int i = maps->addObject(cls, pos[0], pos[1], pos[2]);
		maps->setObjectPose(i, pos[0], pos[1], pos[2], rot[1]);
		return i + 1;
	}
	void deleteObject(int id) override { // 🔶 brak usuwania obiektów sceny: wynosimy poza świat
		if (id > 0 && id <= maps->objectCount()) maps->setObjectPose(id - 1, -1000.f, -1000.f, 0.f, 0.f);
	}
	bool getPos(int id, float pos[3]) override {
		if (id <= 0 || id > maps->objectCount()) return false;
		const float* p = maps->objectPos(id - 1);
		pos[0] = p[0], pos[1] = p[1], pos[2] = p[2];
		return true;
	}
	void setPos(int id, const float pos[3]) override {
		if (id > 0 && id <= maps->objectCount()) maps->setObjectPose(id - 1, pos[0], pos[1], pos[2], maps->objectRotY(id - 1));
	}
	void setAnim(int id, const std::string& anim, int, int mode) override {
		if (id <= 0 || id > maps->objectCount()) return;
		size_t dot = anim.find('.');
		maps->playAnim(id - 1, dot == std::string::npos ? anim : anim.substr(dot + 1), mode == 2);
	}
};

struct Game {
	Vfs vfs;
	std::unique_ptr<script::Engine> scripts;
	ScriptHost host;
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

	void snapToFloor(Unit& u) {
		int x = int(std::floor(u.x)), y = int(std::floor(u.y - 0.01f));
		for (int d = 0; d < 40; ++d) {
			for (int yy : {y + d, y - d})
				if (nav.standable(x, yy) || (d > 0 && nav.climbable(x, yy))) {
					u.tx = x, u.ty = yy;
					u.x = float(x) + 0.5f, u.y = float(yy) + 1.f;
					return;
				}
		}
		u.tx = x, u.ty = y;
	}

	int addUnit(int obj) {
		maps.setObjectModel(obj, "mann.3db", "mann");
		Unit u;
		u.obj = obj;
		const float* p = maps.objectPos(obj);
		u.x = p[0], u.y = p[1];
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
		maps.setBlock(x, y, 59, 3);
		nav.updateTile(*grid(), maps.blockDb(), x, y);
		maps.rebuildTerrain();
	}

	void updateUnit(Unit& u, float dt) {
		if (u.state == Unit::Walk) {
			if (u.step >= u.path.size()) {
				u.state = u.digX >= 0 ? Unit::Dig : Unit::Idle;
				u.digT = 0;
			} else {
				auto [wx, wy] = u.path[u.step];
				float gx = float(wx) + 0.5f, gy = float(wy) + 1.f;
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
		if (u.state == Unit::Idle) {
			if (!nav.standable(u.tx, u.ty) && !nav.climbable(u.tx, u.ty) && nav.open(u.tx, u.ty + 1)) {
				u.path = {{u.tx, u.ty + 1}}, u.step = 0, u.state = Unit::Walk; // spada
			} else
				maps.playAnim(u.obj, nav.standable(u.tx, u.ty) ? "standard" : "kletterstand", true);
		}
		maps.setObjectPose(u.obj, u.x, u.y, kGnomeZ, u.facing);
	}

	// Skrypty obiektów świata (obj_init). Krasnale na razie steruje gracz (C++), nie skrypty.
	void attachScripts() {
		if (!scripts) return;
		int n = 0;
		for (int i = 0; i < maps.objectCount(); ++i) {
			if (Vfs::lower(maps.objectClass(i)) == "zwerg") continue;
			auto* o = scripts->attach(i + 1, maps.objectClass(i), "");
			n += o && o->interp;
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
		} else if (button == SDL_BUTTON_RIGHT)
			order(tx, ty);
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
			ImGui::TextDisabled("%s", tr("WASD, strzałki, środkowy przycisk: kamera   kółko: przybliżenie"));
			ImGui::TextDisabled("%s", tr("N: nowy krasnal przy kursorze   F1: pomoc   Esc: wyjście"));
		}
		ImGui::End();
	}
};

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
	if (args.scripts) {
		g.scripts = std::make_unique<script::Engine>(g.vfs);
		g.host.maps = &g.maps;
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
			g.order(u.tx + 3, u.ty);
		}
		for (int i = 0; i < 30; ++i) {
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
