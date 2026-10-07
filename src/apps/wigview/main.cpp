// wigview: przeglądarka modeli .3db, animacji i tekstur Wiggles.
//
// Użycie:
//   wigview [--game <katalog gry>] [--model NAZWA] [--object NAZWA] [--anim NAZWA] [--frame N]
//           [--light 0..3] [--yaw ST] [--pitch ST] [--flip 0|1] [--size SZERxWYS]
//           [--template SZABLON | --scenario SCENARIUSZ] [--center X Y] [--center-z Z] [--dist D] [--search TEKST]
//           [--view X Y ZOOM DX DY] [--fow TRYB [PROMIEŃ [BAJT]]] [--renderer vulkan|opengl]
//           [--screenshot plik.png] [--ui] [--lang KOD]
// --lang: język interfejsu (pl, en albo inny z lang/*.po); domyślnie zapamiętany w wigview.ini,
//         a przy pierwszym uruchomieniu język systemu (LANG). Napisy na konsoli zostają po polsku.
// --renderer: backend renderowania (domyślnie Vulkan; bez tej opcji, gdy Vulkan nie ruszy, OpenGL).
// --center/--center-z/--dist: kamera map w blokach względem szablonu (do porównań ze zrzutami gry).
// --view: kamera gry jak set_view (współrzędne świata scenariusza), zamiast set_view ze skryptu.
// --fow: podgląd nieodkrytego terenu: 0 brak, 1 wszystko szare, 2 kadr widoczny + reszta szara, 3 kadr + reszta czarna,
//        4 z zapisu gry (domyślnie dla zapisów).
// --save PLIK.sav: zapis gry z Data/Gamesave (to samo co --scenario PLIK.sav) albo ścieżka do pliku .sav.
// Katalog gry: --game, potem zmienna WIGGLES_DIR, potem OW_GAME, potem wybór z okna „Ścieżki” (wigview.ini),
// na końcu ~/.wine/drive_c/Program Files (x86)/Innonics/Wiggles.
// --simulate: symulacja na mapie (skrypty obiektów, krasnale, wukery; jak przełącznik w panelu map). W oknie
//   domyślnie włączona, w zrzutach wsadowych tylko z --simulate; --no-simulate wyłącza. Klawisz N: krasnal pod kursorem, Shift+N: wuker.
// --populate K [W]: z --simulate na mapie bez krasnali stawia K krasnali i W wukerów (domyślnie 1; dodatek przeglądarki).
// --warmup S: z --simulate i --screenshot przewija symulację o S sekund przed zrzutem.
// --http PORT: serwer deweloperski na 127.0.0.1:PORT (Tcl jak WebTraq, API JSON; docs/SERWER_HTTP.md); też OW_HTTP.
// --nav-path X0 Y0 X1 Y1 [W [Z0 Z1]]: z --print-nav droga Calc1 między punktami (bloki), W = 1 ze wspinaniem,
//   i trasa chodu z głębokością (Z0, Z1 w skali skryptu, domyślnie 8).
// --print-nav: z --screenshot wypisuje podpola ruchu (NavWorld: '_' podłoga, '|' ściana, '.' zablokowane) i koszty pól.
// --print-objects: z --screenshot wypisuje obiekty sceny (indeks, klasa, obiekt .3db, animacja, widoczność, pozycja)
//   i postacie symulacji, do testów (tools/tests/run.sh).
// --click X Y: z --screenshot zaznacza obiekt albo pole w punkcie obrazu (piksele) przed zrzutem, jak kliknięcie.
// --paths: otwiera okno „Ścieżki” (katalog gry i zapisów) przy starcie.
// --saves-dir KATALOG: zapisy .sav z tego katalogu w zakładce Zapisy (domyślnie Data/Gamesave gry albo wybór z okna).
// --screenshot: renderuje jedną klatkę, zapisuje PNG i kończy działanie (z --ui razem z interfejsem).
// --template: otwiera zakładkę Mapy z podanym szablonem (np. urw_gng_021_a).
// --particle TYP: otwiera zakładkę Cząsteczki z typem 0..42 (w zrzucie ekranu symulacja jest przewinięta o 3 s).
// --texture PLIK: zakładka Tekstury z plikiem względem Data/ (np. GUI/bordermask.tga); --texture-channel N: 0 RGBA,
//                 1 RGB, 2 alfa, 3 R, 4 G, 5 B.
// --thumbnails KATALOG: miniatury wszystkich obiektów (z --model tylko tego modelu) w animacji „standard”
//                      (albo pierwszej), klatka 0: KATALOG/<model>/<obiekt>.png i KATALOG/index.html; rozmiar z --size
//                      (domyślnie 256x256), kończy działanie.
// --file ŚCIEŻKA: zakładka Pliki z plikiem albo katalogiem (ścieżka gry, np. Data/Scripts/init/out.tcl).
// --emit TYP X Y Z [MAX NAKLATKĘ]: dodatkowe źródło cząsteczek na mapie w punkcie gry (x, y bloki, z jak w skrypcie:
//                 świat = z·0,5 + 7), domyślnie 64 cząsteczki i 2 na klatkę; do porównań ze zrzutami gry (§12.6).
// --place KLASA X Y Z: nieruchomy obiekt klasy w punkcie gry (jak --emit) z jej światłami (class_light), krasnal
//   (Zwerg) z modelem mann i animacją standard; do porównań cieni ze zrzutami gry (BAZA_WIEDZY §9.11).
// --lab-lamps: lampy jak tools/lab/live2.sh (Misc_LightSrc_a co 5 bloków od rogu szablonu, z = 14) zamiast
//   lampy podglądu, do porównań jasności ze zrzutami z laboratorium.
// --border-mask: ramka ekranu gry (Data/GUI/bordermask.tga) na scenie map, jak w grze z BorderMask=1.
// --no-draw-modes: wszystkie linki modeli rysowane jak tryb 1 (bez trybów z flags linku), do porównań.
// --shadow-cmap: w zakładce Modele rozwija sekcję „Cień i cmap” (cień 32×32 i maska kolizji bieżącej klatki).
// --sound PLIK: otwiera zakładkę Dźwięki z plikiem (ścieżka gry, np. Data/Sound/Fenris/fe_schritt1.WAV).
// --overlay KATALOG: nakładka w układzie modów (np. DigglesHDmod): jej Data/ i Data_<język> przykrywają pliki gry;
//                    można podać kilka razy (późniejsza wyżej).
//
// Sterowanie w oknie: LPM obrót, PPM przesunięcie, kółko przybliżenie, spacja play/pauza,
// strzałki lewo/prawo poprzednia/następna klatka, góra/dół poprzedni/następny plik (model albo
// tekstura, dźwięk, zależnie od zakładki), PgUp/PgDn poprzednia/następna animacja, I interpolacja,
// Ctrl+F wyszukiwarka (modele, obiekty w modelach, tekstury, szablony), Q wyjście.
// --frame przyjmuje też ułamki (np. 2.5 = połowa drogi między klatką 2 i 3, z interpolacją).

#include "lib/dev_api.h"
#include "lib/i18n.h"
#include "lib/nav_world.h"
#include "lib/walk_plan.h"
#include "lib/render/border_mask.h"
#include "lib/world_sim.h"
#include "lib/render/model_renderer.h"
#include "lib/render/texture_cache.h"
#include "lib/gltf_export.h"
#include "lib/model3db.h"
#include "lib/png_write.h"
#include "lib/tga.h"
#include "lib/vfs.h"
#include "apps/wigview/dir_picker.h"
#include "apps/wigview/files_view.h"
#include "apps/wigview/map_view.h"
#include "apps/wigview/particle_view.h"
#include "apps/wigview/sound_view.h"

#include <ow_version.h> // generowany przez build.sh: OW_VERSION (commit), OW_VERSION_DATE

#include <SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_internal.h> // ImGuiSettingsHandler (język w wigview.ini)

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <unistd.h> // execv: restart po zmianie katalogu gry

using namespace ow;

namespace
{

struct Args
{
	std::string game, model, object, anim, screenshot, templ, search, scenario, lang, sound, texture, file;
	int textureChannel = 0;
	bool shadowCmap = false;
	bool drawModes = true; // --no-draw-modes
	bool borderMask = false; // --border-mask
	bool labLamps = false;   // --lab-lamps
	std::vector<std::array<float, 6>> emits; // --emit TYP X Y Z MAX NAKLATKĘ
	std::vector<std::pair<std::string, std::array<float, 3>>> places; // --place KLASA X Y Z
	std::string savesDir;                    // --saves-dir
	bool paths = false;                      // --paths: okno „Ścieżki” otwarte od startu
	bool simulate = false;                   // --simulate: symulacja (skrypty) na mapie
	bool noSimulate = false;                 // --no-simulate: bez symulacji (w oknie domyślnie włączona)
	float warmup = 0;                        // --warmup S: przewinięcie symulacji przed zrzutem
	int popGnomes = -1, popWukers = 1;       // --populate K [W]: postacie na mapie bez krasnali
	int clickX = -1, clickY = -1;            // --click X Y: kliknięcie w scenie przed zrzutem (piksele obrazu)
	bool printObjects = false;               // --print-objects: lista obiektów sceny (testy)
	bool printNav = false;                   // --print-nav: podpola ruchu (NavWorld) jako tekst
	std::vector<float> navPath;              // --nav-path X0 Y0 X1 Y1 [W]: droga Calc1 (bloki; W = 1: wspinanie)
	int httpPort = 0;                        // --http PORT: serwer deweloperski (0 = wyłączony)
	std::string gameSource;                  // skąd wzięto katalog gry (do okna „Ścieżki”)
	std::vector<std::string> overlays;
	int particle = -1; // --particle TYP: zakładka Cząsteczki z wybranym typem
	float frame = 0;
	int ipol = 1;
	int light = int(LightingMode::SphereNormal);
	float yaw = 20.f, pitch = 15.f;
	int flip = 1;
	bool ui = false, viewSet = false;
	float camX = -1, camY = -1, camZ = -1, camDist = -1; // kamera map w blokach (lokalnie w szablonie)
	std::optional<GameView> gameView;                    // --view
	int fowMode = -1, fowRadius = 10, fowByte = 255;     // --fow (−1 = domyślnie)
	rhi::Backend renderer = rhi::backendCompiled(rhi::Backend::Vulkan) ? rhi::Backend::Vulkan : rhi::Backend::OpenGL;
	bool rendererForced = false;                          // --renderer: bez przełączania na drugi backend
	int width = 1400, height = 900;
	bool sizeSet = false;
	std::string thumbnails; // --thumbnails KATALOG
	// Tryb wsadowy (zrzut albo miniatury): bez interakcji, renderowanie poza ekranem.
	bool batch() const { return !screenshot.empty() || !thumbnails.empty(); }
};

// Przycięcie kątów set_view przy dużym oddaleniu, ustalone obserwacją gry (docs/silnik/06_kamera.md §6.3):
// gra pokazuje kąty |dx|, |dy| ≤ 2,23 / (R − 15,6), R = 16·zoom − 10 bloków.
// Pomiary: zoom 2,5 → 0,15; 2,25 → 0,23; 2,0 → 0,35; 1,7 i mniej bez przycięcia. 🔶 dopasowanie empiryczne.
float gameViewAngleLimit(float R)
{
	constexpr float kA = 2.23f, kB = 15.6f;
	return R > kB + 1e-3f ? kA / (R - kB) : 1e9f;
}

bool parseArgs(int argc, char** argv, Args& a)
{
	for (int i = 1; i < argc; ++i)
	{
		std::string k = argv[i];
		auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
		if (k == "--game") a.game = next();
		else if (k == "--saves-dir") a.savesDir = next();
		else if (k == "--paths") a.paths = true;
		else if (k == "--print-objects") a.printObjects = true;
		else if (k == "--print-nav") a.printNav = true;
		else if (k == "--nav-path")
		{
			for (int j = 0; j < 4; ++j) a.navPath.push_back(float(std::atof(next())));
			for (int j = 0; j < 3; ++j) a.navPath.push_back(i + 1 < argc && argv[i + 1][0] != '-' ? float(std::atof(next())) : (j ? 8.f : 0.f));
			a.printNav = true;
		}
		else if (k == "--http") a.httpPort = std::atoi(next());
		else if (k == "--click")
		{
			a.clickX = std::atoi(next());
			a.clickY = std::atoi(next());
		}
		else if (k == "--simulate") a.simulate = true;
		else if (k == "--no-simulate") a.noSimulate = true;
		else if (k == "--warmup") a.warmup = float(std::atof(next()));
		else if (k == "--populate")
		{
			a.popGnomes = std::atoi(next());
			if (i + 1 < argc && argv[i + 1][0] != '-') a.popWukers = std::atoi(next());
		}
		else if (k == "--model") a.model = next();
		else if (k == "--object") a.object = next();
		else if (k == "--anim") a.anim = next();
		else if (k == "--template") a.templ = next();
		else if (k == "--particle") a.particle = std::atoi(next());
		else if (k == "--sound") a.sound = next();
		else if (k == "--file") a.file = next();
		else if (k == "--texture") a.texture = next();
		else if (k == "--texture-channel") a.textureChannel = std::atoi(next());
		else if (k == "--shadow-cmap") a.shadowCmap = true;
		else if (k == "--no-draw-modes") a.drawModes = false;
		else if (k == "--border-mask") a.borderMask = true;
		else if (k == "--emit")
		{
			std::array<float, 6> e{0, 0, 0, 0, 64, 2};
			for (int j = 0; j < 4; ++j) e[size_t(j)] = float(std::atof(next()));
			for (int j = 4; j < 6 && i + 1 < argc && argv[i + 1][0] != '-'; ++j) e[size_t(j)] = float(std::atof(next()));
			a.emits.push_back(e);
		}
		else if (k == "--place")
		{
			std::string cls = next();
			std::array<float, 3> p{};
			for (auto& c : p) c = float(std::atof(next()));
			a.places.push_back({cls, p});
		}
		else if (k == "--overlay") a.overlays.push_back(next());
		else if (k == "--lab-lamps") a.labLamps = true;
		else if (k == "--scenario" || k == "--save") a.scenario = next();
		else if (k == "--search") a.search = next();
		else if (k == "--center")
		{
			a.camX = float(std::atof(next()));
			a.camY = float(std::atof(next()));
		}
		else if (k == "--center-z") a.camZ = float(std::atof(next()));
		else if (k == "--dist") a.camDist = float(std::atof(next()));
		else if (k == "--lang") a.lang = next();
		else if (k == "--renderer")
		{
			std::string r = next();
			if (!rhi::parseBackend(r, a.renderer))
			{
				std::fprintf(stderr, "nieznany renderer %s (vulkan, opengl)\n", r.c_str());
				return false;
			}
			a.rendererForced = true;
		}
		else if (k == "--fow")
		{
			a.fowMode = std::atoi(next());
			if (i + 1 < argc && argv[i + 1][0] != '-') a.fowRadius = std::atoi(next());
			if (i + 1 < argc && argv[i + 1][0] != '-') a.fowByte = std::atoi(next());
		}
		else if (k == "--view")
		{
			GameView g;
			g.x = float(std::atof(next()));
			g.y = float(std::atof(next()));
			g.zoom = float(std::atof(next()));
			g.dx = float(std::atof(next()));
			g.dy = float(std::atof(next()));
			a.gameView = g;
		}
		else if (k == "--frame") a.frame = float(std::atof(next()));
		else if (k == "--ipol") a.ipol = std::atoi(next());
		else if (k == "--light") a.light = std::atoi(next());
		else if (k == "--yaw") a.yaw = float(std::atof(next())), a.viewSet = true;
		else if (k == "--pitch") a.pitch = float(std::atof(next())), a.viewSet = true;
		else if (k == "--ui") a.ui = true;
		else if (k == "--flip") a.flip = std::atoi(next());
		else if (k == "--screenshot") a.screenshot = next();
		else if (k == "--size") a.sizeSet = std::sscanf(next(), "%dx%d", &a.width, &a.height) == 2;
		else if (k == "--thumbnails") a.thumbnails = next();
		else
		{
			std::fprintf(stderr, "nieznany parametr: %s\n", k.c_str());
			return false;
		}
	}
	return true;
}

// Wartość klucza z sekcji [OpenWiggles][Ustawienia] pliku wigview.ini, czytana przed startem ImGui
// (katalog gry jest potrzebny, zanim powstanie okno).
std::string iniValue(const char* key)
{
	std::FILE* f = std::fopen("wigview.ini", "rb");
	if (!f) return {};
	char line[2048];
	bool inSection = false;
	std::string want = std::string(key) + "=", out;
	while (std::fgets(line, sizeof line, f))
	{
		std::string l = line;
		while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
		if (!l.empty() && l[0] == '[') inSection = l == "[OpenWiggles][Ustawienia]";
		else if (inSection && l.compare(0, want.size(), want) == 0) out = l.substr(want.size());
	}
	std::fclose(f);
	return out;
}

// Katalog gry: --game > WIGGLES_DIR > OW_GAME > wigview.ini > instalacja w ~/.wine.
void resolveGameDir(Args& a)
{
	if (!a.game.empty())
	{
		a.gameSource = "--game";
		return;
	}
	for (const char* env : {"WIGGLES_DIR", "OW_GAME"})
		if (const char* e = std::getenv(env); e && *e)
		{
			a.game = e;
			a.gameSource = env;
			return;
		}
	if (std::string ini = iniValue("GameDir"); !ini.empty())
	{
		a.game = ini;
		a.gameSource = "wigview.ini";
		return;
	}
	if (const char* h = std::getenv("HOME")) a.game = std::string(h) + "/.wine/drive_c/Program Files (x86)/Innonics/Wiggles";
	a.gameSource = "domyślny";
}

bool endsWithI(const std::string& s, const std::string& suf)
{
	return s.size() >= suf.size() && Vfs::lower(s.substr(s.size() - suf.size())) == suf;
}

struct Viewer
{
	std::unique_ptr<rhi::Device> device; // backend renderowania (Vulkan albo OpenGL)
	Vfs vfs;
	std::unique_ptr<TextureCache> textures;
	std::vector<Vec3f> sphere;
	int activeTab = 0;       // 0 modele, 1 tekstury, 2 mapy, 3 zapisy (2 i 3 pokazują widok mapy), 4 cząsteczki, 5 dźwięki
	bool mapTab() const { return activeTab == 2 || activeTab == 3; }
	int forceTab = -1;       // zakładka do wybrania w najbliższej klatce UI
	MapView maps;
	// Symulacja (skrypty obiektów, krasnale) na scenie map: przełącznik w panelu map, domyślnie wyłączona.
	std::unique_ptr<WorldSim> sim;
	bool simulate = false, simActive = false, keepCamera = false;
	// Ręczne prowadzenie postaci (dodatek przeglądarki do testów, nie z gry): Enter przy zaznaczonej postaci
	// albo przycisk w panelu; strzałki krok o pole, PPM idź do pola, Shift albo Esc puszcza.
	int controlUnit = -1; // indeks w sim->units, −1 = brak
	int tapX = 0, tapZ = 0; // strzałka naciśnięta w tej klatce (ruch co najmniej o jedną klatkę)
	// Kierunek ruchu prowadzonej postaci: ←/→ w bok, ↑ w głąb, ↓ do przodu, dopóki klawisz jest wciśnięty.
	void controlKeys()
	{
		if (controlUnit < 0 || !sim) return;
		const Uint8* k = SDL_GetKeyboardState(nullptr);
		bool text = ImGui::GetIO().WantTextInput;
		int dx = text ? 0 : int(k[SDL_SCANCODE_RIGHT]) - int(k[SDL_SCANCODE_LEFT]);
		int dz = text ? 0 : int(k[SDL_SCANCODE_DOWN]) - int(k[SDL_SCANCODE_UP]);
		if (dx == 0) dx = tapX;
		if (dz == 0) dz = tapZ;
		tapX = tapZ = 0;
		sim->controlMove(controlUnit, float(dx), float(dz));
	}
	// Klawisz N / Shift+N: nowa postać w polu pod kursorem (gdy symulacja jeszcze nie działa: po jej starcie).
	struct PendingSpawn
	{
		int tx, ty;
		bool gnome;
	};
	std::optional<PendingSpawn> pendingSpawn;
	void spawnAt(int tx, int ty, bool gnome)
	{
		if (!simActive || !sim) return;
		int u = sim->spawnUnit(float(tx) + 0.5f, float(ty) + 0.9f, gnome);
		maps.simInfo = strf(tr("krasnale: %d, postacie: %zu"), sim->gnomeCount(), sim->units.size());
		if (std::getenv("OW_TRACE_SIM"))
			std::fprintf(stderr, "[ui] dodano postać %d (%s) przy polu %d, %d\n", u, gnome ? "krasnal" : "wuker", tx, ty);
	}
	int unitOfObject(int obj) const
	{
		if (!simActive || !sim) return -1;
		for (size_t k = 0; k < sim->units.size(); ++k)
			if (sim->units[k].obj == obj) return int(k);
		return -1;
	}
	void takeControl(int unit)
	{
		releaseControl();
		if (!sim || unit < 0) return;
		sim->setControlled(unit, true);
		controlUnit = unit;
		if (std::getenv("OW_TRACE_SIM")) std::fprintf(stderr, "[ui] sterowanie: postać %d\n", unit);
	}
	void releaseControl()
	{
		if (sim && controlUnit >= 0 && controlUnit < int(sim->units.size())) sim->setControlled(controlUnit, false);
		if (controlUnit >= 0 && std::getenv("OW_TRACE_SIM")) std::fprintf(stderr, "[ui] sterowanie: puszczone\n");
		controlUnit = -1;
	}
	// Upływ czasu na mapach (animacje obiektów, cząsteczki, symulacja); niezależny od odtwarzacza zakładki Modele.
	bool mapPlaying = true;
	// Start symulacji na bieżącej mapie (pierwszy raz: silnik skryptów). progress: okno postępu.
	void startSim(const std::function<void(float, const char*)>& progress)
	{
		if (!maps.hasTemplate()) return;
		if (!sim)
		{
			sim = std::make_unique<WorldSim>(maps);
			if (progress) progress(0.2f, N_("Skrypty: klasy obiektów (pierwszy raz kilka sekund)"));
			std::string err;
			if (!sim->bootScripts(vfs, i18n::language(), err)) std::fprintf(stderr, "skrypty: %s\n", err.c_str());
		}
		if (progress) progress(0.8f, N_("Skrypty: obiekty mapy"));
		sim->initUnits();
		if (maps.populate && sim->gnomeCount() == 0) // opcja przeglądarki; wokół środka kadru (jednostki modelu → bloki)
			sim->populate((center.x + pan.x) / MapView::kBlock, (center.y - pan.y) / MapView::kBlock, maps.populateGnomes,
			    maps.populateWukers);
		sim->startScripts();
		simActive = true;
		if (std::getenv("OW_TRACE_SIM"))
			for (const auto& u : sim->units)
				std::fprintf(stderr, "[sim] %s %s pole (%d, %d) stopy (%.2f, %.2f) z %.1f\n", maps.objectClass(u.obj).c_str(),
				    u.gnome ? "krasnal" : "wuker", u.tx, u.ty, u.x, u.y, u.z);
		maps.simInfo = strf(tr("krasnale: %d, postacie: %zu"), sim->gnomeCount(), sim->units.size());
	}
	BorderMask border;       // ramka ekranu gry na obszarze sceny map (--border-mask)
	bool borderMask = false;
	ParticleView particles;
	SoundView sounds;
	FilesView files;
	// Plik z zakładki Pliki w zakładce, która umie go pokazać; doIt = false: tylko sprawdzenie, czy taka jest.
	bool openFile(const std::string& gamePath, bool doIt);
	bool sceneless() const { return activeTab == 1 || activeTab == 5 || activeTab == 6; } // zakładki bez sceny 3D
	bool animSounds = false; // zakładka Modele: odtwarzaj dźwięki klatek (sound anim) podczas animacji
	bool scrollToSel = false; // przewiń listy do zaznaczenia (po zmianie klawiaturą)
	char exportDir[512] = ".";
	std::string exportMsg;
	ModelRenderer renderer;
	std::vector<std::string> modelFiles;

	std::string modelFile;
	std::unique_ptr<Model3db> model;
	std::string loadError;
	double loadMs = 0; // czas odczytu + parsowania ostatniego modelu
	int object = 0, anim = 0;
	float frame = 0;
	bool playing = true;
	float fps = 10.f; // tempo animacji gry (docs/silnik/11_obiekty_tcl_i_petla.md §11.6)
	bool loop = true;
	bool interpolate = true; // jak oryginał (AnimIpol, domyślnie włączone)

	ModelDrawOptions draw;
	// Gra (Direct3D) ma układ lewoskrętny: x w prawo, y w dół, z w stronę widza. Odbicie osi Y daje
	// prawoskrętny układ OpenGL z Y w górę bez lustrzanego obrazu (obrót o 180° wokół X odwracał napisy).
	bool flipY = true;
	float yaw = 20.f, pitch = 15.f, dist = 300.f;
	glm::vec3 pan{0.f};
	glm::vec3 center{0.f};
	float radius = 100.f;
	glm::vec3 bg{0.18f, 0.20f, 0.24f};

	float uiRight = 0; // prawa krawędź panelu w punktach ImGui
	float uiWidth = 560, uiHeight = 0, narrowWidth = 0;
	bool wideTab = false; // panel poszerzony dla zakładek Tekstury i Dźwięki
	char objFilter[64] = "";
	char animFilter[64] = "";

	// Przeglądarka tekstur. Katalogi względem Data/ (np. "Texture/m256", "Ground/detail", "GUI/icons").
	std::vector<std::string> texDirs;
	int texDir = 0;
	std::vector<std::string> texFiles;
	int texSel = -1;
	char texFilter[64] = "";
	float texZoom = 0.f;   // 0 = dopasuj do panelu
	int texChannel = 0;    // 0 RGBA, 1 RGB, 2 alfa, 3 R, 4 G, 5 B
	bool texChecker = true; // szachownica pod przezroczystymi pikselami
	std::string texImgPath; // plik wczytany do texImg
	int texImgChannel = -1; // tryb, z którym zbudowano texPreview
	Image texImg;           // piksele wybranej tekstury (do kanałów i wartości pod kursorem)
	rhi::Handle texPreview = rhi::kNull;
	void updateTexPreview(const std::string& path);
	~Viewer()
	{
		if (texPreview && textures && textures->device()) textures->device()->destroyTexture(texPreview);
	}

	const Animation3db* currentAnim() const
	{
		if (!model || model->objects.empty()) return nullptr;
		const auto& o = model->objects[size_t(object)];
		if (o.animations.empty()) return nullptr;
		return &model->animations[o.animations[size_t(anim)]];
	}
	int frameCount() const
	{
		auto a = currentAnim();
		return a ? int(a->frames.size()) : 0;
	}
	int currentMesh() const
	{
		auto a = currentAnim();
		if (!a || a->frames.empty()) return -1;
		int f = std::clamp(int(frame), 0, int(a->frames.size()) - 1);
		return int(a->frames[size_t(f)]);
	}

	void frameCamera()
	{
		auto a = currentAnim();
		if (!a || a->frames.empty()) return;
		glm::vec3 mn(1e9f), mx(-1e9f);
		for (uint32_t f : a->frames)
		{
			const auto& m = model->meshes[f];
			mn = glm::min(mn, glm::vec3(m.bboxMin.x, m.bboxMin.y, m.bboxMin.z));
			mx = glm::max(mx, glm::vec3(m.bboxMax.x, m.bboxMax.y, m.bboxMax.z));
		}
		center = (mn + mx) * 0.5f;
		radius = std::max(1.f, glm::length(mx - mn) * 0.5f);
		dist = radius * 2.6f;
		pan = glm::vec3(0.f);
	}

	void selectAnim(int i)
	{
		anim = i;
		frame = 0;
		frameCamera();
	}
	void selectObject(int i)
	{
		object = i;
		selectAnim(0);
	}

	bool loadModel(const std::string& file)
	{
		loadError.clear();
		renderer.setModel(nullptr, nullptr);
		model.reset();
		modelFile = file;
		Uint64 t0 = SDL_GetPerformanceCounter();
		auto data = vfs.read("Data/3db/" + file);
		if (!data)
		{
			loadError = tr("nie można odczytać pliku");
			return false;
		}
		try
		{
			model = std::make_unique<Model3db>(parseModel3db(*data));
		}
		catch (const std::exception& e)
		{
			loadError = e.what();
			return false;
		}
		loadMs = double(SDL_GetPerformanceCounter() - t0) * 1000.0 / double(SDL_GetPerformanceFrequency());
		std::printf("wczytano %s w %.0f ms\n", file.c_str(), loadMs);
		renderer.setModel(model.get(), textures.get());
		selectObject(0);
		return true;
	}

	glm::mat4 modelMatrix() const
	{
		glm::mat4 m(1.f);
		if (flipY) m = glm::scale(m, glm::vec3(1.f, -1.f, 1.f)); // (x, y, z) gry -> (x, -y, z) OpenGL
		return glm::translate(m, -center);
	}

	// Kamera gry (set_view): gdy aktywna, viewProj używa jej zamiast orbity; ruch myszą ją wyłącza.
	bool gameCam = false;
	ow::GameView gameCamView;

	// Widok mapy jak w grze: kamera z przodu (+z w układzie gry), lekko z góry.
	void frameMap(bool resetAngles = true)
	{
		if (resetAngles)
		{
			yaw = 0.f; // kamera z przodu (+z), jak w grze
			pitch = 10.f;
		}
		center = maps.center();
		radius = maps.radius();
		dist = radius * 2.4f;
		pan = glm::vec3(0.f);
		gameCam = false;
		if (const auto& gv = maps.gameView())
		{
			gameCam = true;
			gameCamView = *gv;
			center = glm::vec3(gv->x, gv->y, 14.f) * MapView::kBlock; // cel kamery gry (z = 14 bloków)
		}
	}

	// Podgląd cząsteczek: emiter w początku układu, kamera z przodu.
	void frameParticles()
	{
		yaw = 0.f;
		pitch = 10.f;
		center = particles.center();
		radius = particles.radius();
		dist = radius * 2.4f;
		pan = glm::vec3(0.f);
		gameCam = false;
	}

	// Promień spod kursora (piksele obrazu mx, my; obraz w×h, scena od kolumny x0) w układzie sceny map
	// (przed macierzą world, jednostki modelu): o + t·d. false: kursor poza sceną.
	bool sceneRay(float mx, float my, int w, int h, int x0, glm::vec3& o, glm::vec3& d) const
	{
		if (mx < float(x0) || w - x0 <= 0 || h <= 0) return false;
		float nx = (mx - float(x0)) / float(w - x0) * 2.f - 1.f, ny = 1.f - my / float(h) * 2.f;
		glm::mat4 inv = glm::inverse(viewProj(w - x0, h) * modelMatrix());
		glm::vec4 a = inv * glm::vec4(nx, ny, -1.f, 1.f), b = inv * glm::vec4(nx, ny, 1.f, 1.f);
		o = glm::vec3(a) / a.w;
		d = glm::vec3(b) / b.w - o;
		return true;
	}
	// Informacje z symulacji o obiekcie mapy (panel zaznaczenia).
	void uiSimObject(int i)
	{
		if (!simActive || !sim)
		{
			ImGui::TextDisabled(tr("Stan skryptu: włącz „Symulacja (skrypty)”."));
			return;
		}
		if (int unit = unitOfObject(i); unit >= 0)
		{
			if (controlUnit == unit)
			{
				if (ImGui::SmallButton(tr("Puść (Shift, Esc)"))) releaseControl();
			}
			else if (ImGui::SmallButton(tr("Steruj (Enter)")))
				takeControl(unit);
			ImGui::SetItemTooltip(tr("Dodatek przeglądarki do testów (nie ma go w grze): kliknięta postać albo stworzenie z animacją chodu\n"
			                         "jest prowadzone: trzymana ←/→ idzie, ↑/↓ w głąb / do przodu, PPM bez przeciągania: idź do pola.\n"
			                         "Skrypt postaci jest wtedy wstrzymany."));
		}
		int id = i < int(sim->idxToId.size()) ? sim->idxToId[size_t(i)] : 0;
		const script::ScriptObject* so = id && sim->scripts ? sim->scripts->object(id) : nullptr;
		if (!so)
		{
			ImGui::TextDisabled(tr("Obiekt bez skryptu (ID %d)."), id);
			return;
		}
		ImGui::Text(tr("Skrypt: ID %d, stan %s (licznik %d), właściciel %d"), id, so->state.empty() ? "-" : so->state.c_str(),
		    so->stateCounter, so->owner);
		if (const SimUnit* u = sim->unitOf(i))
		{
			const char* st = u->state == SimUnit::Walk ? tr("idzie") : u->state == SimUnit::Dig ? tr("kopie")
			                                                                                    : tr("stoi");
			ImGui::Text(tr("Postać: %s, pole %d, %d"), st, u->tx, u->ty);
			static const char* kNeeds[4][2] = {{"atr_Hitpoints", N_("zdrowie")}, {"atr_Nutrition", N_("jedzenie")},
			    {"atr_Alertness", N_("sen")}, {"atr_Mood", N_("nastrój")}};
			if (u->gnome)
				for (auto& n : kNeeds)
				{
					float v = float(sim->scripts->attrib(id, n[0]));
					ImGui::ProgressBar(std::clamp(v, 0.f, 1.f), ImVec2(ImGui::GetFontSize() * 8.f, 0), "");
					ImGui::SameLine();
					ImGui::Text("%s %.2f", tr(n[1]), v);
				}
		}
		if (!so->tasks.empty() && ImGui::TreeNode("zadania", tr("Zadania (%zu)"), so->tasks.size()))
		{
			for (const auto& t : so->tasks) ImGui::TextWrapped("%s", t.c_str());
			ImGui::TreePop();
		}
	}

	// Wyjście z kamery gry na orbitalną bez przeskoku (kółko albo przeciągnięcie myszą po wczytaniu zapisu z
	// kamerą blisko): ten sam cel (pan), kierunek patrzenia (yaw, pitch) i odległość przeliczona tak, żeby przy
	// kącie widzenia orbity (40° w pionie) kadr był ten sam co w kamerze gry.
	void leaveGameCam(int w, int h)
	{
		if (!gameCam) return;
		gameCam = false;
		if (!mapTab()) return;
		float aspect = h > 0 ? float(w) / float(h) : 1.f;
		const auto& g = gameCamView;
		float Rb = std::clamp(g.zoom, kGameZoomMin, kGameZoomMax) * 16.f - 10.f;
		float R = std::max(0.5f, Rb) * MapView::kBlock;
		float lim = gameViewAngleLimit(Rb), dx = std::clamp(g.dx, -lim, lim), dy = std::clamp(g.dy, -lim, lim);
		glm::vec3 eye(std::cos(dx) * -std::sin(dy), -std::sin(dx), std::cos(dx) * std::cos(dy)); // jak viewProj, po odbiciu y
		yaw = glm::degrees(std::atan2(eye.x, eye.z));
		pitch = std::clamp(glm::degrees(std::asin(std::clamp(eye.y, -1.f, 1.f))), -89.f, 89.f);
		float tanV = std::tan(0.3f * aspect) / aspect; // pół-kąt pionowy kamery gry (tangens)
		dist = R * tanV / std::tan(glm::radians(20.f));
	}

	glm::mat4 viewProj(int w, int h) const
	{
		float aspect = h > 0 ? float(w) / float(h) : 1.f;
		if (gameCam && mapTab())
		{
			// Prawo kamery z exe (map_world.h GameView); układ gry -> OpenGL przez odbicie osi Y.
			const auto& g = gameCamView;
			float Rb = std::clamp(g.zoom, kGameZoomMin, kGameZoomMax) * 16.f - 10.f; // zakres jak w grze (§6.1)
			float R = std::max(0.5f, Rb) * MapView::kBlock;
			float lim = gameViewAngleLimit(Rb), dx = std::clamp(g.dx, -lim, lim), dy = std::clamp(g.dy, -lim, lim);
			float cdx = std::cos(dx), sdx = std::sin(dx), cdy = std::cos(dy), sdy = -std::sin(dy);
			glm::vec3 eye(R * cdx * sdy, R * sdx, R * cdx * cdy);  // względem celu, układ gry
			glm::vec3 up(sdx * sdy, -cdx, sdx * cdy);
			eye.y = -eye.y;
			up.y = -up.y;
			float tanV = std::tan(0.3f * aspect) / aspect;     // poziomy pół-kąt = 0,3·proporcje
			// Płaszczyzny przycinania gry: 0,5 i 180 bloków (Diggles-Unity, DigglesCameraController.cs).
			// Bliska wyznacza punkt startowy promieni panoramy.
			glm::mat4 proj = glm::perspective(2.f * std::atan(tanV), aspect, 0.5f * MapView::kBlock, 180.f * MapView::kBlock);
			return proj * glm::lookAt(eye + pan, pan, up);
		}
		glm::mat4 proj = glm::perspective(glm::radians(40.f), aspect, radius * 0.01f, radius * 50.f + dist);
		glm::vec3 eye = glm::vec3(std::cos(glm::radians(pitch)) * std::sin(glm::radians(yaw)),
		                    std::sin(glm::radians(pitch)),
		                    std::cos(glm::radians(pitch)) * std::cos(glm::radians(yaw))) *
		    dist;
		glm::mat4 view = glm::lookAt(eye + pan, pan, glm::vec3(0, 1, 0));
		return proj * view;
	}

	// x0: lewa krawędź obszaru 3D w pikselach (na prawo od panelu UI).
	// Klatki A i B oraz współczynnik t dla bieżącego czasu animacji.
	bool currentFrames(uint32_t& a, uint32_t& b, float& t) const
	{
		auto an = currentAnim();
		if (!an || an->frames.empty()) return false;
		int n = int(an->frames.size());
		int i = std::clamp(int(frame), 0, n - 1);
		int j = loop ? (i + 1) % n : std::min(i + 1, n - 1);
		a = an->frames[size_t(i)];
		b = an->frames[size_t(j)];
		t = interpolate ? frame - float(int(frame)) : 0.f;
		return true;
	}

	// Kolor czyszczenia klatki: w zakładce Mapy kolor strefy horyzontu (jak w grze).
	glm::vec3 clearColor()
	{
		glm::vec3 clear = bg;
		if (mapTab()) maps.updateZone(center + pan * glm::vec3(1, -1, 1));
		if (mapTab())
			if (const HorizonZone* z = maps.zone()) clear = glm::vec3(z->r, z->g, z->b);
		return clear;
	}

	void renderScene(int w, int h, int x0 = 0)
	{
		x0 = std::clamp(x0, 0, w - 1);
		device->setViewport(x0, 0, w - x0, h);
		if (mapTab())
		{
			if (maps.hasTemplate()) maps.render(viewProj(w - x0, h), modelMatrix(), draw, center.z, center + pan * glm::vec3(1, -1, 1));
			if (borderMask) border.draw(x0, w - x0, h);
		}
		else if (activeTab == 4)
		{
			particles.render(viewProj(w - x0, h) * modelMatrix());
		}
		else if (sceneless())
		{
			// Tekstury, Dźwięki i Pliki: bez sceny 3D
		}
		else
		{
			uint32_t a, b;
			float t;
			if (currentFrames(a, b, t)) renderer.drawFrames(a, b, t, viewProj(w - x0, h), modelMatrix(), draw);
		}
		device->setViewport(0, 0, w, h);
	}

	void update(float dt)
	{
		if (!simActive || !sim || controlUnit >= int(sim->units.size())) controlUnit = -1; // nowa mapa albo restart
		if (mapPlaying) maps.update(dt);
		if (simActive && mapTab()) controlKeys();
		if (mapPlaying && simActive && mapTab()) sim->simUpdate(std::min(dt, 0.1f));
		if (activeTab == 4) particles.update(dt);
		int n = frameCount();
		if (!playing || n <= 1) return;
		float prev = frame;
		bool wrapped = false;
		frame += dt * fps;
		if (frame >= float(n))
		{
			if (loop)
			{
				frame = std::fmod(frame, float(n));
				wrapped = true;
			}
			else
			{
				frame = float(n - 1);
				playing = false;
			}
		}
		if (animSounds && activeTab == 0)
		{
			// Klatki przekroczone w tym kroku: (prev, frame], z przejściem przez koniec pętli.
			int a = int(prev), b = int(frame);
			if (wrapped)
			{
				for (int f = a + 1; f < n; ++f) playFrameSounds(f);
				for (int f = 0; f <= b; ++f) playFrameSounds(f);
			}
			else
				for (int f = a + 1; f <= b; ++f) playFrameSounds(f);
		}
	}

	// Klucz animacji w sound anim: "<obiekt>.<animacja>" małymi literami (docs/silnik/15_dzwiek.md).
	std::string animSoundKey() const
	{
		auto a = currentAnim();
		if (!a) return {};
		return Vfs::lower(model->objects[size_t(object)].name + "." + a->name);
	}
	std::vector<int> animSoundRefs()
	{
		auto it = sounds.index().byAnim.find(animSoundKey());
		if (it == sounds.index().byAnim.end()) return {};
		std::vector<int> v;
		for (int i : it->second)
			if (!sounds.index().refs[size_t(i)].commented) v.push_back(i);
		std::stable_sort(v.begin(), v.end(), [&](int x, int y) { return sounds.index().refs[size_t(x)].frame < sounds.index().refs[size_t(y)].frame; });
		return v;
	}
	void playFrameSounds(int f)
	{
		for (int i : animSoundRefs())
		{
			const auto& r = sounds.index().refs[size_t(i)];
			if (r.frame == f) sounds.playName(r.name, r.volume > 0 ? r.volume : 1.f);
		}
	}
	void openAnim(const std::string& obj, const std::string& animName, int fr);

	void refreshTexFiles()
	{
		texFiles.clear();
		texSel = -1;
		if (texDir < 0 || size_t(texDir) >= texDirs.size()) return;
		for (auto& f : vfs.list("Data/" + texDirs[size_t(texDir)]))
			if (endsWithI(f, ".tga")) texFiles.push_back(f);
	}

	void stepModel(int d)
	{
		if (modelFiles.empty()) return;
		int i = 0;
		for (size_t k = 0; k < modelFiles.size(); ++k)
			if (modelFiles[k] == modelFile) i = int(k) + d;
		if (modelFile.empty()) i = d > 0 ? 0 : int(modelFiles.size()) - 1;
		i = (i + int(modelFiles.size())) % int(modelFiles.size());
		loadModel(modelFiles[size_t(i)]);
		scrollToSel = true;
	}
	void stepTexture(int d);
	void stepAnim(int d)
	{
		if (!model || model->objects.empty()) return;
		int n = int(model->objects[size_t(object)].animations.size());
		if (n == 0) return;
		selectAnim((anim + d + n) % n);
		scrollToSel = true;
	}
	void exportGlb(bool allAnims);

	// --- Wyszukiwarka (Ctrl+F) ---
	enum class HitKind
	{
		Model,
		Object,
		Texture,
		Template,
		Scenario,
		Sound
	};
	struct SearchEntry
	{
		HitKind kind;
		std::string label;   // to, co widzi użytkownik
		std::string lower;   // label małymi literami, do dopasowania
		std::string file;    // plik modelu / szablonu / tekstury
		int index = -1;      // obiekt w modelu albo katalog tekstury
	};
	std::vector<SearchEntry> searchIndex;
	bool searchIndexed = false, searchOpen = false, searchRequest = false, objectsIndexed = false;
	char searchText[128] = "";
	int searchSel = 0;
	std::vector<int> searchHits;
	void buildSearchIndex(bool withObjects);
	void updateSearchHits();
	void openHit(const SearchEntry& e);
	void uiSearch();
	// Okno „Ścieżki”: katalog gry (zmiana = restart wigview) i katalog zapisów (od razu).
	void uiPaths();
	std::string gameDir, gameSource, restartGame; // restartGame: nowy katalog gry, pętla główna kończy się i robi execv
	bool pathsOpen = false;
	DirPicker dirPicker;

	void ui();
	void uiModel();
	void uiShadowCmap(const Mesh3db& mesh);
	bool openShadowCmap = false; // --shadow-cmap
	int scrollShadowCmap = 0;   // klatki, przez które przewijamy szczegóły do sekcji (układ ustala się po 1–2 klatkach)
	bool showShadow = true, showCmap = true;
	void uiTextures();
};

bool passFilter(const char* filter, const std::string& s)
{
	return !*filter || Vfs::lower(s).find(Vfs::lower(filter)) != std::string::npos;
}

void Viewer::uiModel()
{
	ImGui::BeginChild("modele", ImVec2(180, 0), ImGuiChildFlags_Borders);
	for (const auto& f : modelFiles)
	{
		if (ImGui::Selectable(f.c_str(), f == modelFile)) loadModel(f);
		if (f == modelFile && scrollToSel) ImGui::SetScrollHereY(0.5f);
	}
	ImGui::EndChild();
	ImGui::SameLine();

	ImGui::BeginChild("szczegoly", ImVec2(0, 0));
	if (!loadError.empty()) ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), tr("Błąd: %s"), loadError.c_str());
	if (!model)
	{
		ImGui::TextDisabled(tr("Wybierz model z listy."));
		ImGui::EndChild();
		return;
	}
	ImGui::Text(tr("%s  (%s)  wczytano w %.0f ms"), model->name.c_str(), modelFile.c_str(), loadMs);
	ImGui::Text(tr("materiały %zu  klatki %zu  obiekty %zu  animacje %zu"), model->materials.size(),
	    model->meshes.size(), model->objects.size(), model->animations.size());

	if (ImGui::CollapsingHeader(tr("Obiekty i animacje"), ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::SetNextItemWidth(-1);
		ImGui::InputTextWithHint("##of", tr("filtr obiektów"), objFilter, sizeof objFilter);
		ImGui::BeginChild("obiekty", ImVec2(0, 140), ImGuiChildFlags_Borders);
		for (int i = 0; i < int(model->objects.size()); ++i)
		{
			const auto& o = model->objects[size_t(i)];
			if (!passFilter(objFilter, o.name)) continue;
			char label[160];
			std::snprintf(label, sizeof label, "%s  (%zu)##o%d", o.name.c_str(), o.animations.size(), i);
			if (ImGui::Selectable(label, i == object)) selectObject(i);
		}
		ImGui::EndChild();
		ImGui::SetNextItemWidth(-1);
		ImGui::InputTextWithHint("##af", tr("filtr animacji"), animFilter, sizeof animFilter);
		ImGui::BeginChild("animacje", ImVec2(0, 180), ImGuiChildFlags_Borders);
		if (!model->objects.empty())
		{
			const auto& o = model->objects[size_t(object)];
			for (int i = 0; i < int(o.animations.size()); ++i)
			{
				const auto& a = model->animations[o.animations[size_t(i)]];
				if (!passFilter(animFilter, a.name)) continue;
				char label[160];
				std::snprintf(label, sizeof label, "%s  [%zu]##a%d", a.name.c_str(), a.frames.size(), i);
				if (ImGui::Selectable(label, i == anim)) selectAnim(i);
				if (i == anim && scrollToSel) ImGui::SetScrollHereY(0.5f);
			}
		}
		ImGui::EndChild();
	}

	if (ImGui::CollapsingHeader(tr("Odtwarzanie"), ImGuiTreeNodeFlags_DefaultOpen))
	{
		int n = frameCount();
		if (ImGui::Button(playing ? tr("Pauza###graj") : tr("Graj###graj"))) playing = !playing;
		ImGui::SameLine();
		if (ImGui::Button("<")) frame = std::max(0.f, float(int(frame) - 1));
		ImGui::SameLine();
		if (ImGui::Button(">")) frame = float(std::min(n - 1, int(frame) + 1));
		ImGui::SameLine();
		ImGui::Checkbox(tr("Pętla"), &loop);
		ImGui::SameLine();
		ImGui::Checkbox(tr("Interpolacja"), &interpolate);
		int f = int(frame);
		if (ImGui::SliderInt(tr("Klatka"), &f, 0, std::max(0, n - 1)))
		{
			frame = float(f);
			playing = false;
		}
		ImGui::SliderFloat(tr("FPS"), &fps, 1.f, 60.f, "%.0f");
		uint32_t fa, fb;
		float ft;
		if (currentFrames(fa, fb, ft) && fa != fb && !renderer.canInterpolate(fa, fb))
			ImGui::TextColored(ImVec4(1, 0.7f, 0.3f, 1), tr("Klatki %u i %u mają inną topologię (bez interpolacji)"), fa, fb);
	}

	std::vector<int> refs = animSoundRefs();
	if (ImGui::CollapsingHeader(tr("Dźwięki animacji"), refs.empty() ? 0 : ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox(tr("Odtwarzaj dźwięki klatek podczas animacji"), &animSounds);
		if (refs.empty()) ImGui::TextDisabled(tr("Brak sound anim dla %s"), animSoundKey().c_str());
		for (size_t k = 0; k < refs.size(); ++k)
		{
			const auto& r = sounds.index().refs[size_t(refs[k])];
			ImGui::PushID(int(k));
			if (ImGui::SmallButton(tr("graj"))) sounds.playName(r.name, r.volume > 0 ? r.volume : 1.f);
			ImGui::SameLine();
			auto f = sounds.index().fileOfName.find(r.name);
			ImGui::Text(tr("klatka %d: %s%s"), r.frame, r.name.c_str(), r.kind == SoundRef::AnimNoSeq ? tr("  (poza sekwencjami)") : "");
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s:%d\n%s\n%s", r.script.c_str(), r.line, r.text.c_str(),
				    f != sounds.index().fileOfName.end() ? f->second.c_str() : tr("(brak sound bind)"));
			if (f != sounds.index().fileOfName.end())
			{
				ImGui::SameLine();
				if (ImGui::SmallButton(tr("plik")))
				{
					for (const auto& path : sounds.files())
						if (soundFileKey(path) == f->second) sounds.select(path);
					forceTab = activeTab = 5;
				}
			}
			ImGui::PopID();
		}
	}
	if (ImGui::CollapsingHeader(tr("Widok"), ImGuiTreeNodeFlags_DefaultOpen))
	{
		const char* modes[] = {tr("Bez światła"), tr("Bajt jako jasność (stare)"), tr("Normalne SPHERE.DAT"), tr("Podgląd normalnych")};
		int m = int(draw.lighting);
		if (ImGui::Combo(tr("Oświetlenie"), &m, modes, 4)) draw.lighting = LightingMode(m);
		ImGui::SliderFloat(tr("Ambient"), &draw.ambient, 0.f, 1.f);
		ImGui::Checkbox(tr("Siatka"), &draw.wireframe);
		ImGui::SameLine();
		ImGui::Checkbox(tr("Alpha test"), &draw.alphaTest);
		ImGui::SameLine();
		ImGui::Checkbox(tr("Tryby rysowania"), &draw.drawModes);
		ImGui::SetItemTooltip(tr("flags linku: 0 niewidoczny, 2 addytywnie, 3 miękka alfa, 5–7 mapowanie otoczenia,\n8 bez oświetlenia; wyłączone: wszystko jak tryb 1"));
		ImGui::SameLine();
		ImGui::Checkbox(tr("Odwróć Y"), &flipY);
		ImGui::ColorEdit3(tr("Tło"), &bg.r, ImGuiColorEditFlags_NoInputs);
		if (ImGui::Button(tr("Wyśrodkuj"))) frameCamera();
	}

	if (openShadowCmap) ImGui::SetNextItemOpen(true), scrollShadowCmap = 3;
	openShadowCmap = false;
	if (ImGui::CollapsingHeader(tr("Cień i cmap")))
	{
		int mi = currentMesh();
		if (mi >= 0) uiShadowCmap(model->meshes[size_t(mi)]);
		if (scrollShadowCmap > 0)
		{
			--scrollShadowCmap;
			ImGui::SetScrollHereY(1.f);
		}
	}

	if (ImGui::CollapsingHeader(tr("Eksport glTF")))
	{
		ImGui::SetNextItemWidth(-60);
		ImGui::InputText(tr("Katalog"), exportDir, sizeof exportDir);
		if (ImGui::Button(tr("Bieżąca animacja"))) exportGlb(false);
		ImGui::SameLine();
		if (ImGui::Button(tr("Wszystkie animacje obiektu"))) exportGlb(true);
		ImGui::TextDisabled(tr("Plik .glb z teksturami; animacje jako morph targets.\nUżywa ustawień FPS i interpolacji z odtwarzania."));
		if (!exportMsg.empty()) ImGui::TextWrapped("%s", exportMsg.c_str());
	}

	if (ImGui::CollapsingHeader(tr("Informacje")))
	{
		if (auto a = currentAnim())
		{
			ImGui::Text(tr("animacja: %s"), a->name.c_str());
			ImGui::Text(tr("flagi %u  f %.4g"), a->unknownFlags, a->unknownF);
			ImGui::Text(tr("ruch (%.3f %.3f %.3f)"), a->move.x, a->move.y, a->move.z);
			ImGui::Text(tr("obrót (%.3f %.3f %.3f)"), a->rotate.x, a->rotate.y, a->rotate.z);
			ImGui::TextWrapped(tr("źródło: %s"), a->sourceFile.c_str());
		}
		int mi = currentMesh();
		if (mi >= 0)
		{
			const auto& mesh = model->meshes[size_t(mi)];
			ImGui::Text(tr("mesh #%d  linki %zu"), mi, mesh.links.size());
			ImGui::Text(tr("bbox (%.1f %.1f %.1f) - (%.1f %.1f %.1f)"), mesh.bboxMin.x, mesh.bboxMin.y, mesh.bboxMin.z,
			    mesh.bboxMax.x, mesh.bboxMax.y, mesh.bboxMax.z);
		}
		ImGui::Text(tr("klatki w GPU: %zu"), renderer.cachedFrames());
		ImGui::SeparatorText(tr("Materiały"));
		for (const auto& mat : model->materials)
		{
			const auto& t = textures->get(mat.name);
			if (t.path.empty()) ImGui::TextColored(ImVec4(1, 0.5f, 0.3f, 1), tr("%s  (brak)"), mat.name.c_str());
			else ImGui::Text("%s  %dx%d", mat.name.c_str(), t.width, t.height);
		}
	}
	ImGui::EndChild();
}

// Cień 32×32 rozciągnięty na shadowQuad i maska cmap w kratkach 1/8 bloku. Oba leżą na ziemi, więc rysujemy je
// z góry: x w prawo, z (głębokość, + do widza) w dół rysunku; tło to obrys bbox klatki. Format: docs/formaty/3db.md §3.6–3.7.
void Viewer::uiShadowCmap(const Mesh3db& mesh)
{
	ImGui::Text(tr("model: cienie %zu, cmapy %zu"), model->shadows.size(), model->cmaps.size());
	float q[12];
	std::memcpy(q, mesh.shadowQuad.data(), sizeof q);
	float qx0 = std::min({q[0], q[3], q[6], q[9]}), qx1 = std::max({q[0], q[3], q[6], q[9]});
	float qz0 = std::min({q[2], q[5], q[8], q[11]}), qz1 = std::max({q[2], q[5], q[8], q[11]});
	const auto* sh = mesh.shadowIndex < model->shadows.size() ? &model->shadows[mesh.shadowIndex] : nullptr;
	const CMap3db* cm = mesh.cmapIndex < model->cmaps.size() ? &model->cmaps[mesh.cmapIndex] : nullptr;
	constexpr float kCell = 25.f; // kratka cmapy: 1/8 bloku

	// Wspólna skala: obrys bbox, prostokąt cienia i maska muszą się zmieścić.
	float x0 = std::min(mesh.bboxMin.x, 0.f), x1 = std::max(mesh.bboxMax.x, 0.f);
	float z0 = std::min(mesh.bboxMin.z, 0.f), z1 = std::max(mesh.bboxMax.z, 0.f);
	if (sh) x0 = std::min(x0, qx0), x1 = std::max(x1, qx1), z0 = std::min(z0, qz0), z1 = std::max(z1, qz1);
	if (cm)
	{
		x0 = std::min(x0, cm->originX * kCell), x1 = std::max(x1, (cm->originX + cm->width) * kCell);
		z0 = std::min(z0, cm->originY * kCell), z1 = std::max(z1, (cm->originY + cm->height) * kCell);
	}
	ImGui::Checkbox(tr("cień"), &showShadow);
	ImGui::SameLine();
	ImGui::Checkbox(tr("cmap"), &showCmap);
	float side = std::min(360.f, ImGui::GetContentRegionAvail().x);
	float scale = side / std::max({x1 - x0, z1 - z0, 1.f});
	ImVec2 p = ImGui::GetCursorScreenPos();
	ImGui::InvisibleButton("cien_cmap", ImVec2(side, side));
	ImDrawList* dl = ImGui::GetWindowDrawList();
	dl->AddRectFilled(p, ImVec2(p.x + side, p.y + side), IM_COL32(40, 40, 40, 255));
	float ou = p.x + (side - (x1 - x0) * scale) / 2.f - x0 * scale, ov = p.y + (side - (z1 - z0) * scale) / 2.f - z0 * scale;
	auto at = [&](float x, float z) { return ImVec2(ou + x * scale, ov + z * scale); };
	if (sh && showShadow)
	{
		float cw = (qx1 - qx0) / 32.f, ch = (qz1 - qz0) / 32.f;
		for (int j = 0; j < 32; ++j)
			for (int i = 0; i < 32; ++i)
				if (uint8_t g = (*sh)[size_t(j * 32 + i)])
					dl->AddRectFilled(at(qx0 + i * cw, qz0 + j * ch), at(qx0 + (i + 1) * cw, qz0 + (j + 1) * ch), IM_COL32(g, g, g, 255));
		dl->AddRect(at(qx0, qz0), at(qx1, qz1), IM_COL32(255, 200, 80, 255));
	}
	if (cm && showCmap)
		for (int j = 0; j < cm->height; ++j)
			for (int i = 0; i < cm->width; ++i)
			{
				float x = (cm->originX + i) * kCell, z = (cm->originY + j) * kCell;
				bool on = cm->pixels[size_t(j * cm->width + i)] != 0;
				dl->AddRect(at(x, z), at(x + kCell, z + kCell), on ? IM_COL32(80, 220, 120, 255) : IM_COL32(90, 90, 90, 255));
				if (on) dl->AddRectFilled(at(x, z), at(x + kCell, z + kCell), IM_COL32(80, 220, 120, 90));
			}
	dl->AddRect(at(mesh.bboxMin.x, mesh.bboxMin.z), at(mesh.bboxMax.x, mesh.bboxMax.z), IM_COL32(120, 160, 255, 255));
	ImVec2 o = at(0, 0);
	dl->AddLine(ImVec2(o.x - 5, o.y), ImVec2(o.x + 6, o.y), IM_COL32(255, 80, 80, 255));
	dl->AddLine(ImVec2(o.x, o.y - 5), ImVec2(o.x, o.y + 6), IM_COL32(255, 80, 80, 255));

	if (sh) ImGui::Text(tr("cień #%u  prostokąt x %.0f..%.0f  z %.0f..%.0f"), mesh.shadowIndex, qx0, qx1, qz0, qz1);
	else ImGui::TextDisabled(tr("klatka bez cienia"));
	if (cm) ImGui::Text(tr("cmap #%u  %u×%u kratek, początek (%d, %d)"), mesh.cmapIndex, cm->width, cm->height, cm->originX, cm->originY);
	else ImGui::TextDisabled(tr("klatka bez cmap"));
	ImGui::TextDisabled(tr("widok z góry: x w prawo, z (do widza) w dół\nniebieski: bbox klatki, czerwony krzyż: punkt obiektu,\npomarańczowy: shadowQuad, zielony: zajęte kratki cmap"));
}

void Viewer::uiTextures()
{
	ImGui::BeginChild("katalogi", ImVec2(140, 0), ImGuiChildFlags_Borders);
	for (int i = 0; i < int(texDirs.size()); ++i)
		if (ImGui::Selectable(texDirs[size_t(i)].c_str(), i == texDir))
		{
			texDir = i;
			refreshTexFiles();
		}
	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::BeginChild("pliki", ImVec2(220, 0), ImGuiChildFlags_Borders);
	ImGui::SetNextItemWidth(-1);
	ImGui::InputTextWithHint("##tf", tr("filtr"), texFilter, sizeof texFilter);
	for (int i = 0; i < int(texFiles.size()); ++i)
		if (passFilter(texFilter, texFiles[size_t(i)]))
		{
			if (ImGui::Selectable(texFiles[size_t(i)].c_str(), i == texSel)) texSel = i;
			if (i == texSel && scrollToSel) ImGui::SetScrollHereY(0.5f);
		}
	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::BeginChild("podglad");
	if (texSel >= 0)
	{
		std::string p = "Data/" + texDirs[size_t(texDir)] + "/" + texFiles[size_t(texSel)];
		updateTexPreview(p);
		const Image& im = texImg;
		ImGui::Text("%s  %dx%d  %s", p.c_str(), im.width, im.height, im.hasAlpha ? "RGBA" : "RGB");
		const char* modes[] = {"RGBA", "RGB", tr("alfa"), "R", "G", "B"};
		ImGui::SetNextItemWidth(90);
		ImGui::Combo(tr("Kanały"), &texChannel, modes, 6);
		ImGui::SameLine();
		ImGui::Checkbox(tr("Szachownica"), &texChecker);
		ImGui::SameLine();
		if (ImGui::Button(tr("Dopasuj"))) texZoom = 0.f;
		ImGui::SameLine();
		if (ImGui::Button("1:1")) texZoom = 1.f;
		ImGui::SameLine();
		float avail = ImGui::GetContentRegionAvail().x;
		float fit = std::min(avail, 512.f) / float(std::max(1, im.width));
		float z = texZoom > 0.f ? texZoom : fit;
		ImGui::SetNextItemWidth(std::max(60.f, ImGui::GetContentRegionAvail().x));
		if (ImGui::SliderFloat("##zoom", &z, 0.125f, 16.f, tr("powiększenie %.2f×"), ImGuiSliderFlags_Logarithmic)) texZoom = z;
		ImGui::BeginChild("obraz", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
		if (texPreview && im.width > 0)
		{
			ImVec2 p0 = ImGui::GetCursorScreenPos(), size(im.width * z, im.height * z);
			if (texChecker)
			{
				auto* dl = ImGui::GetWindowDrawList();
				const float c = 8.f;
				ImVec2 clipMin = ImGui::GetWindowPos(), clipMax(clipMin.x + ImGui::GetWindowSize().x, clipMin.y + ImGui::GetWindowSize().y);
				for (float y = 0; y < size.y; y += c)
					for (float x = 0; x < size.x; x += c)
					{
						ImVec2 a(p0.x + x, p0.y + y), b(std::min(p0.x + x + c, p0.x + size.x), std::min(p0.y + y + c, p0.y + size.y));
						if (b.x < clipMin.x || b.y < clipMin.y || a.x > clipMax.x || a.y > clipMax.y) continue;
						bool odd = (int(x / c) + int(y / c)) & 1;
						dl->AddRectFilled(a, b, odd ? IM_COL32(110, 110, 110, 255) : IM_COL32(160, 160, 160, 255));
					}
			}
			ImGui::Image(ImTextureID(device->imguiTexture(texPreview)), size);
			if (ImGui::IsItemHovered())
			{
				ImGuiIO& io = ImGui::GetIO();
				if (io.KeyCtrl && io.MouseWheel != 0.f) texZoom = std::clamp(z * std::pow(1.25f, io.MouseWheel), 0.125f, 16.f);
				int px = int((io.MousePos.x - p0.x) / z), py = int((io.MousePos.y - p0.y) / z);
				if (px >= 0 && py >= 0 && px < im.width && py < im.height)
				{
					const uint8_t* c = &im.rgba[(size_t(py) * im.width + px) * 4];
					ImGui::SetTooltip(tr("piksel (%d, %d)\nR %d  G %d  B %d  A %d"), px, py, c[0], c[1], c[2], c[3]);
				}
			}
		}
		else ImGui::TextDisabled("%s", tr("nie można wczytać pliku"));
		ImGui::EndChild();
	}
	ImGui::EndChild();
}

// Piksele wybranej tekstury i tekstura podglądu w trybie kanałów (texChannel): RGBA bez zmian,
// RGB z pełną alfą, pojedynczy kanał jako szarość.
void Viewer::updateTexPreview(const std::string& path)
{
	if (path != texImgPath)
	{
		texImgPath = path;
		texImg = Image{};
		if (auto d = vfs.read(path)) loadTga(*d, texImg);
		texImgChannel = -1;
	}
	if (texImgChannel == texChannel || texImg.width <= 0) return;
	texImgChannel = texChannel;
	std::vector<uint8_t> px = texImg.rgba;
	for (size_t i = 0; i < px.size(); i += 4)
	{
		if (texChannel == 1) px[i + 3] = 255;
		else if (texChannel >= 2)
		{
			uint8_t v = texChannel == 2 ? px[i + 3] : px[i + size_t(texChannel - 3)];
			px[i] = px[i + 1] = px[i + 2] = v;
			px[i + 3] = 255;
		}
	}
	rhi::TextureDesc d;
	d.width = texImg.width;
	d.height = texImg.height;
	d.mipmaps = false;
	d.repeatU = d.repeatV = false;
	if (texPreview) device->updateTexture(texPreview, d, px.data());
	else texPreview = device->createTexture(d, px.data());
}

void Viewer::buildSearchIndex(bool withObjects)
{
	auto add = [&](HitKind k, std::string label, std::string file, int index) {
		SearchEntry e{k, label, Vfs::lower(label), std::move(file), index};
		searchIndex.push_back(std::move(e));
	};
	if (!searchIndexed)
	{
		for (auto& f : modelFiles) add(HitKind::Model, f, f, -1);
		for (int d = 0; d < int(texDirs.size()); ++d)
			for (auto& f : vfs.list("Data/" + texDirs[size_t(d)]))
				if (endsWithI(f, ".tga")) add(HitKind::Texture, texDirs[size_t(d)] + "/" + f, f, d);
		for (auto& f : maps.files()) add(HitKind::Template, f, f, -1);
		for (auto& f : maps.scenarios()) add(HitKind::Scenario, f, f, -1);
		// Dźwięki: ścieżka i nazwy logiczne (sound bind), żeby dało się szukać po nazwie ze skryptu.
		for (auto& f : sounds.files())
		{
			std::string label = f.substr(5);
			auto names = sounds.index().namesForFile(soundFileKey(f));
			for (size_t k = 0; k < names.size(); ++k) label += (k ? ", " : "  [") + names[k] + (k + 1 == names.size() ? "]" : "");
			add(HitKind::Sound, label, f, -1);
		}
		searchIndexed = true;
	}
	if (withObjects && !objectsIndexed)
	{
		// Obiekty wymagają wczytania wszystkich modeli (jednorazowo, ok. 1 s w wersji release).
		for (auto& f : modelFiles)
		{
			auto data = vfs.read("Data/3db/" + f);
			if (!data) continue;
			try
			{
				Model3db m = parseModel3db(*data);
				for (size_t i = 0; i < m.objects.size(); ++i)
					add(HitKind::Object, m.objects[i].name + "  (" + f + ")", f, int(i));
			}
			catch (...)
			{}
		}
		objectsIndexed = true;
	}
}

void Viewer::updateSearchHits()
{
	searchHits.clear();
	std::vector<std::string> terms;
	{
		std::string t = Vfs::lower(searchText), cur;
		for (char c : t + " ")
		{
			if (c == ' ')
			{
				if (!cur.empty()) terms.push_back(cur);
				cur.clear();
			}
			else cur += c;
		}
	}
	if (terms.empty()) return;
	for (int i = 0; i < int(searchIndex.size()); ++i)
	{
		bool ok = true;
		for (auto& t : terms)
			if (searchIndex[size_t(i)].lower.find(t) == std::string::npos)
			{
				ok = false;
				break;
			}
		if (ok) searchHits.push_back(i);
	}
	// Kolejność: modele, szablony, obiekty, tekstury (tekstur jest najwięcej, często w kilku rozdzielczościach).
	auto rank = [&](int i) {
		switch (searchIndex[size_t(i)].kind)
		{
		case HitKind::Model: return 0;
		case HitKind::Scenario: return 1;
		case HitKind::Template: return 1;
		case HitKind::Object: return 2;
		case HitKind::Sound: return 3;
		default: return 4;
		}
	};
	std::stable_sort(searchHits.begin(), searchHits.end(), [&](int a, int b) { return rank(a) < rank(b); });
	if (searchHits.size() > 500) searchHits.resize(500);
	searchSel = std::clamp(searchSel, 0, std::max(0, int(searchHits.size()) - 1));
}

void Viewer::openHit(const SearchEntry& e)
{
	switch (e.kind)
	{
	case HitKind::Model:
	case HitKind::Object:
		forceTab = 0;
		activeTab = 0;
		if (modelFile != e.file) loadModel(e.file);
		if (e.kind == HitKind::Object && model && e.index < int(model->objects.size())) selectObject(e.index);
		break;
	case HitKind::Texture:
		forceTab = 1;
		activeTab = 1;
		texDir = e.index;
		refreshTexFiles();
		for (int i = 0; i < int(texFiles.size()); ++i)
			if (texFiles[size_t(i)] == e.file) texSel = i;
		break;
	case HitKind::Template:
	case HitKind::Scenario:
		forceTab = activeTab = 2;
		if (e.kind == HitKind::Scenario) maps.selectScenario(e.file);
		else maps.select(e.file);
		frameMap();
		break;
	case HitKind::Sound:
		forceTab = activeTab = 5;
		sounds.select(e.file);
		break;
	}
	scrollToSel = true;
}

bool Viewer::openFile(const std::string& gamePath, bool doIt)
{
	auto hit = [&](HitKind k, const std::string& file, int index) {
		SearchEntry e;
		e.kind = k, e.file = file, e.index = index;
		openHit(e);
	};
	std::string lp = Vfs::lower(gamePath);
	size_t slash = gamePath.rfind('/');
	std::string dir = gamePath.substr(0, slash), name = gamePath.substr(slash + 1), ldir = Vfs::lower(dir);
	std::string ext = name.rfind('.') == std::string::npos ? "" : Vfs::lower(name.substr(name.rfind('.') + 1));
	if (ldir == "data/3db" && ext == "3db")
	{
		if (doIt) hit(HitKind::Model, name, 0);
		return true;
	}
	if (ext == "tga" && ldir.size() > 5)
		for (int i = 0; i < int(texDirs.size()); ++i)
			if (Vfs::lower(texDirs[size_t(i)]) == ldir.substr(5))
			{
				if (doIt) hit(HitKind::Texture, name, i);
				return true;
			}
	if ((ext == "wav" || ext == "mp3") && (lp.rfind("data/sound/", 0) == 0 || lp.rfind("data/music/", 0) == 0))
	{
		if (doIt) hit(HitKind::Sound, gamePath, 0);
		return true;
	}
	// Szablon: <nazwa>.tcl w Data/Templates (części .pmp, .l2m, .ent otwierają ten sam szablon).
	if (ldir == "data/templates" && (ext == "tcl" || ext == "pmp" || ext == "l2m" || ext == "ent"))
	{
		std::string base = Vfs::lower(name.substr(0, name.rfind('.')));
		for (const auto& f : maps.files())
			if (Vfs::lower(f.substr(0, f.rfind('.'))) == base)
			{
				if (doIt) hit(HitKind::Template, f, 0);
				return true;
			}
		return false;
	}
	if ((ldir == "data/gamesave" && (ext == "tcl" || ext == "sav")) || lp == "data/systeminit.tcl")
	{
		if (doIt)
		{
			hit(HitKind::Scenario, name, 0);
			if (ext == "sav") forceTab = activeTab = 3;
		}
		return true;
	}
	return false;
}

void Viewer::uiSearch()
{
	if (searchRequest)
	{
		ImGui::OpenPopup("Szukaj");
		searchRequest = false;
		searchOpen = true;
	}
	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + 60), ImGuiCond_Always, ImVec2(0.5f, 0.f));
	ImGui::SetNextWindowSize(ImVec2(std::min(720.f, vp->WorkSize.x - 40), 480), ImGuiCond_Always);
	if (!ImGui::BeginPopupModal("Szukaj", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize))
	{
		searchOpen = false;
		return;
	}
	const char* kKind[] = {tr("model"), tr("obiekt"), tr("tekstura"), tr("szablon"), tr("scenariusz"), tr("dźwięk")};
	static const ImVec4 kColor[] = {{0.5f, 0.8f, 1, 1}, {0.6f, 1, 0.6f, 1}, {1, 0.8f, 0.4f, 1}, {0.9f, 0.6f, 1, 1}, {1, 0.5f, 0.5f, 1}, {0.5f, 1, 0.9f, 1}};

	if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
	ImGui::SetNextItemWidth(-1);
	if (ImGui::InputTextWithHint("##q", tr("szukaj: modele, obiekty w modelach, tekstury, szablony, scenariusze, dźwięki (kilka słów = wszystkie muszą pasować)"),
	        searchText, sizeof searchText))
	{
		searchSel = 0;
		buildSearchIndex(std::string(searchText).size() >= 2);
		updateSearchHits();
	}
	bool close = ImGui::IsKeyPressed(ImGuiKey_Escape);
	if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) searchSel = std::min(searchSel + 1, int(searchHits.size()) - 1);
	if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) searchSel = std::max(searchSel - 1, 0);
	if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) searchSel = std::min(searchSel + 15, int(searchHits.size()) - 1);
	if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) searchSel = std::max(searchSel - 15, 0);
	bool open = ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter);

	if (!objectsIndexed && std::string(searchText).size() < 2)
		ImGui::TextDisabled(tr("Obiekty w modelach zostaną zindeksowane po wpisaniu 2 znaków."));
	else
		ImGui::TextDisabled(tr("%zu wyników%s   (strzałki, Enter otwiera, Esc zamyka)"), searchHits.size(),
		    searchHits.size() >= 500 ? "+" : "");
	ImGui::BeginChild("wyniki", ImVec2(0, 0), ImGuiChildFlags_Borders);
	for (int i = 0; i < int(searchHits.size()); ++i)
	{
		const auto& e = searchIndex[size_t(searchHits[size_t(i)])];
		ImGui::PushID(i);
		bool sel = i == searchSel;
		if (ImGui::Selectable("##hit", sel, ImGuiSelectableFlags_AllowDoubleClick))
		{
			searchSel = i;
			if (ImGui::IsMouseDoubleClicked(0)) open = true;
		}
		if (sel && (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_PageDown) || ImGui::IsKeyPressed(ImGuiKey_PageUp)))
			ImGui::SetScrollHereY(0.5f);
		ImGui::SameLine(8);
		ImGui::TextColored(kColor[int(e.kind)], "%-8s", kKind[int(e.kind)]);
		ImGui::SameLine(90);
		ImGui::TextUnformatted(e.label.c_str());
		ImGui::PopID();
	}
	ImGui::EndChild();
	if (open && searchSel >= 0 && searchSel < int(searchHits.size()))
	{
		openHit(searchIndex[size_t(searchHits[size_t(searchSel)])]);
		close = true;
	}
	if (close)
	{
		ImGui::CloseCurrentPopup();
		searchOpen = false;
	}
	ImGui::EndPopup();
}

void Viewer::ui()
{
	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 8, vp->WorkPos.y + 8), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(560, vp->WorkSize.y - 16), ImGuiCond_FirstUseEver);
	// Zakładki Tekstury, Dźwięki i Pliki nie mają sceny 3D: panel poszerza się na czas jej otwarcia i wraca do poprzedniej szerokości.
	if (sceneless() != wideTab)
	{
		wideTab = sceneless();
		if (ImGuiWindow* w = ImGui::FindWindowByName("wigview")) uiWidth = w->Size.x, uiHeight = w->Size.y;
		else uiHeight = vp->WorkSize.y - 16; // pierwsza klatka, okno jeszcze nie istnieje
		if (wideTab)
		{
			narrowWidth = uiWidth;
			ImGui::SetNextWindowSize(ImVec2(std::max(uiWidth, std::min(1250.f, vp->WorkSize.x - 16)), uiHeight));
		}
		else if (narrowWidth > 0) ImGui::SetNextWindowSize(ImVec2(narrowWidth, uiHeight));
	}
	ImGui::Begin("wigview");
	if (ImGui::Button(tr("Szukaj (Ctrl+F)"))) searchRequest = true;
	ImGui::SetItemTooltip(tr("modele, obiekty, tekstury, szablony, dźwięki"));
	ImGui::SameLine();
	if (ImGui::Button(tr("Ścieżki…"))) pathsOpen = true;
	ImGui::SetItemTooltip(tr("Katalog gry: %s\nZapisy: %s"), gameDir.c_str(), maps.savesDir().empty() ? "Data/Gamesave" : maps.savesDir().c_str());
	{
		// Wybór języka (lang/*.po); zapamiętywany w wigview.ini.
		const auto& langs = i18n::languages();
		const char* cur = i18n::language().c_str();
		for (const auto& l : langs)
			if (l.code == i18n::language()) cur = l.name.c_str();
		float w = ImGui::GetFontSize() * 7.f;
		ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - w));
		ImGui::SetNextItemWidth(w);
		if (ImGui::BeginCombo("##jezyk", cur))
		{
			for (const auto& l : langs)
				if (ImGui::Selectable(l.name.c_str(), l.code == i18n::language()))
				{
					i18n::setLanguage(l.code);
					ImGui::MarkIniSettingsDirty();
				}
			ImGui::EndCombo();
		}
		ImGui::SetItemTooltip("Język / Language");
	}
	if (ImGui::BeginTabBar("tabs"))
	{
		auto tabFlags = [&](int k) { return forceTab == k ? ImGuiTabItemFlags_SetSelected : 0; };
		if (ImGui::BeginTabItem(tr("Modele###modele"), nullptr, tabFlags(0)))
		{
			activeTab = 0;
			uiModel();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem(tr("Tekstury###tekstury"), nullptr, tabFlags(1)))
		{
			activeTab = 1;
			uiTextures();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem(tr("Mapy###mapy"), nullptr, tabFlags(2)))
		{
			if (!mapTab()) frameMap();
			activeTab = 2;
			if (maps.ui(false)) frameMap();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem(tr("Zapisy###zapisy"), nullptr, tabFlags(3)))
		{
			if (!mapTab()) frameMap();
			activeTab = 3;
			if (maps.ui(true)) frameMap();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem(tr("Cząsteczki###czasteczki"), nullptr, tabFlags(4)))
		{
			if (activeTab != 4) particles.ensureUsage(), frameParticles();
			activeTab = 4;
			if (particles.ui()) frameParticles();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem(tr("Dźwięki###dzwieki"), nullptr, tabFlags(5)))
		{
			activeTab = 5;
			sounds.ui();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem(tr("Pliki###pliki"), nullptr, tabFlags(6)))
		{
			activeTab = 6;
			files.ui();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
		forceTab = -1;
	}
	uiRight = ImGui::GetWindowPos().x + ImGui::GetWindowSize().x;
	uiSearch();
	uiPaths();
	scrollToSel = false;
	ImGui::End();
}

void Viewer::stepTexture(int d)
{
	std::vector<int> visible;
	for (int i = 0; i < int(texFiles.size()); ++i)
		if (passFilter(texFilter, texFiles[size_t(i)])) visible.push_back(i);
	if (visible.empty()) return;
	int pos = -1;
	for (size_t k = 0; k < visible.size(); ++k)
		if (visible[k] == texSel) pos = int(k);
	if (pos < 0) pos = d > 0 ? -1 : 0;
	pos = (pos + d + int(visible.size())) % int(visible.size());
	texSel = visible[size_t(pos)];
	scrollToSel = true;
}

std::string safeFileName(std::string s)
{
	for (auto& c : s)
		if (!(std::isalnum((unsigned char)c) || c == '_' || c == '-')) c = '_';
	return s;
}

// Animacja z odwołania sound anim: obiekt szukany we wszystkich modelach (indeks obiektów wyszukiwarki).
void Viewer::openAnim(const std::string& obj, const std::string& animName, int fr)
{
	buildSearchIndex(true);
	for (const auto& e : searchIndex)
	{
		if (e.kind != HitKind::Object || Vfs::lower(e.label.substr(0, e.label.find("  ("))) != Vfs::lower(obj)) continue;
		forceTab = activeTab = 0;
		if (modelFile != e.file) loadModel(e.file);
		if (!model || e.index >= int(model->objects.size())) return;
		selectObject(e.index);
		const auto& o = model->objects[size_t(object)];
		for (size_t k = 0; k < o.animations.size(); ++k)
			if (Vfs::lower(model->animations[o.animations[k]].name) == Vfs::lower(animName)) selectAnim(int(k));
		frame = float(std::clamp(fr, 0, std::max(0, frameCount() - 1)));
		playing = false;
		scrollToSel = true;
		return;
	}
	std::fprintf(stderr, "nie znaleziono obiektu %s w modelach\n", obj.c_str());
}

void Viewer::exportGlb(bool allAnims)
{
	if (!model || model->objects.empty()) return;
	GltfExportOptions opt;
	opt.fps = fps;
	opt.interpolate = interpolate;
	const auto& o = model->objects[size_t(object)];
	std::string name = safeFileName(model->name) + "_" + safeFileName(o.name);
	if (!allAnims)
	{
		opt.animations = {anim};
		if (auto a = currentAnim()) name += "_" + safeFileName(a->name);
	}
	std::string path = std::string(exportDir) + "/" + name + ".glb";
	std::string err;
	GltfExportStats st;
	Uint64 t0 = SDL_GetPerformanceCounter();
	if (exportGltf(*model, vfs, sphere, object, opt, path, err, &st))
	{
		double ms = double(SDL_GetPerformanceCounter() - t0) * 1000.0 / double(SDL_GetPerformanceFrequency());
		char buf[512];
		std::snprintf(buf, sizeof buf, tr("Zapisano %s\n%zu animacji, %zu klatek, %zu tekstur, %.1f MB, %.0f ms%s"),
		    path.c_str(), st.animations, st.frames, st.textures, st.bytes / 1048576.0, ms,
		    st.incompatibleFrames ? tr("\n(część klatek o innej topologii zastąpiona poprzednią)") : "");
		exportMsg = buf;
	}
	else exportMsg = tr("Błąd eksportu: ") + err;
	std::printf("%s\n", exportMsg.c_str());
}

int findIndex(const std::vector<std::string>& v, const std::string& name)
{
	for (size_t i = 0; i < v.size(); ++i)
		if (Vfs::lower(v[i]) == Vfs::lower(name)) return int(i);
	return -1;
}

} // namespace

// Język interfejsu i ścieżki w wigview.ini (sekcja [OpenWiggles][Ustawienia]); --lang ma pierwszeństwo.
bool gLangFromArgs = false;
std::string gIniGameDir, gIniSavesDir; // wybór z okna „Ścieżki” (pusty = bez wpisu)
void addSettingsHandler()
{
	ImGuiSettingsHandler h;
	h.TypeName = "OpenWiggles";
	h.TypeHash = ImHashStr("OpenWiggles");
	h.ReadOpenFn = [](ImGuiContext*, ImGuiSettingsHandler*, const char*) -> void* { return (void*)1; };
	h.ReadLineFn = [](ImGuiContext*, ImGuiSettingsHandler*, void*, const char* line) {
		char code[32];
		if (!gLangFromArgs && std::sscanf(line, "Language=%31s", code) == 1) i18n::setLanguage(code);
	};
	h.WriteAllFn = [](ImGuiContext*, ImGuiSettingsHandler* self, ImGuiTextBuffer* buf) {
		buf->appendf("[%s][Ustawienia]\nLanguage=%s\n", self->TypeName, i18n::language().c_str());
		if (!gIniGameDir.empty()) buf->appendf("GameDir=%s\n", gIniGameDir.c_str());
		if (!gIniSavesDir.empty()) buf->appendf("SavesDir=%s\n", gIniSavesDir.c_str());
		buf->appendf("\n");
	};
	ImGui::AddSettingsHandler(&h);
}

void Viewer::uiPaths()
{
	dirPicker.ui();
	if (!pathsOpen) return;
	ImGui::SetNextWindowSize(ImVec2(660, 0), ImGuiCond_Appearing);
	if (!ImGui::Begin(tr("Ścieżki###sciezki"), &pathsOpen, ImGuiWindowFlags_NoCollapse))
	{
		ImGui::End();
		return;
	}
	ImGui::SeparatorText(tr("Katalog gry"));
	ImGui::TextWrapped("%s", gameDir.c_str());
	ImGui::TextDisabled(tr("źródło: %s"), gameSource == "domyślny" ? tr("domyślny (~/.wine)") : gameSource.c_str());
	if (ImGui::Button(tr("Zmień…##gra")))
		dirPicker.open(
		    tr("Katalog gry"), gameDir,
		    [](const std::string& d, std::string& status) {
			    Vfs t;
			    t.addRoot(d);
			    if (!t.exists("Data/3db"))
			    {
				    status = tr("brak Data/3db: to nie jest katalog gry");
				    return false;
			    }
			    size_t n = 0;
			    for (const auto& f : t.list("Data/3db")) n += endsWithI(f, ".3db");
			    status = strf(tr("gra znaleziona (%zu modeli)"), n);
			    return true;
		    },
		    [this](const std::string& d) {
			    gIniGameDir = d;
			    ImGui::MarkIniSettingsDirty();
			    restartGame = d;
		    });
	if (!gIniGameDir.empty())
	{
		ImGui::SameLine();
		if (ImGui::Button(tr("Zapomnij wybór")))
		{
			gIniGameDir.clear();
			ImGui::MarkIniSettingsDirty();
		}
		ImGui::SetItemTooltip(tr("Usuwa katalog z wigview.ini (działa od następnego uruchomienia)."));
	}
	ImGui::TextDisabled(tr("Kolejność: --game, zmienna WIGGLES_DIR, OW_GAME, wybór z tego okna (wigview.ini),\n"
	                       "instalacja w ~/.wine. Zmiana katalogu uruchamia wigview ponownie."));
	for (const char* env : {"WIGGLES_DIR", "OW_GAME"})
		if (const char* e = std::getenv(env); e && *e)
		{
			ImGui::TextColored(ImVec4(1, 0.8f, 0.4f, 1), tr("Ustawiona zmienna %s ma pierwszeństwo przed wyborem z okna."), env);
			break;
		}

	ImGui::SeparatorText(tr("Katalog zapisów (.sav)"));
	const std::string& sd = maps.savesDir();
	ImGui::TextWrapped("%s", sd.empty() ? tr("Data/Gamesave gry") : sd.c_str());
	if (ImGui::Button(tr("Zmień…##zapisy")))
		dirPicker.open(
		    tr("Katalog zapisów"), sd.empty() ? gameDir + "/Data/Gamesave" : sd,
		    [](const std::string& d, std::string& status) {
			    size_t n = 0;
			    std::error_code ec;
			    for (std::filesystem::directory_iterator it(d, ec), end; !ec && it != end; it.increment(ec))
				    n += it->is_regular_file(ec) && endsWithI(it->path().filename().string(), ".sav");
			    status = strf(tr("zapisów .sav: %zu"), n);
			    return true;
		    },
		    [this](const std::string& d) {
			    maps.setSavesDir(d);
			    gIniSavesDir = d;
			    ImGui::MarkIniSettingsDirty();
			    forceTab = activeTab = 3;
		    });
	if (!sd.empty())
	{
		ImGui::SameLine();
		if (ImGui::Button(tr("Data/Gamesave gry")))
		{
			maps.setSavesDir("");
			gIniSavesDir.clear();
			ImGui::MarkIniSettingsDirty();
		}
	}
	ImGui::TextDisabled(tr("Zmiana działa od razu i jest zapamiętana (--saves-dir ma pierwszeństwo)."));
	ImGui::End();
}

// --thumbnails: miniatury obiektów do KATALOG/<model>/<obiekt>.png i strona KATALOG/index.html z siatką.
// render(px, w, h) rysuje jedną klatkę bieżącego widoku i zwraca jej obraz. Zwraca kod wyjścia programu.
template <class Render>
int writeThumbnails(Viewer& v, const Args& args, Render&& render)
{
	namespace fs = std::filesystem;
	std::vector<std::string> models = args.model.empty() ? v.modelFiles : std::vector<std::string>{v.modelFile};
	std::vector<uint8_t> px;
	int w = 0, h = 0, written = 0, failed = 0;
	for (int i = 0; i < 4; ++i) render(px, w, h); // okno musi się zmapować (jak przy --screenshot)
	std::string html = "<!doctype html><meta charset=utf-8><title>wigview: miniatury</title>\n"
	                   "<style>body{font:13px sans-serif;background:#222;color:#ddd}div{display:inline-block;margin:4px;"
	                   "text-align:center;width:" +
	    std::to_string(w) + "px;overflow:hidden}img{display:block}</style>\n";
	for (const auto& file : models)
	{
		if (file != v.modelFile && !v.loadModel(file)) continue;
		if (!v.model) continue;
		std::string dir = safeFileName(Vfs::lower(file.substr(0, file.rfind('.'))));
		fs::create_directories(fs::path(args.thumbnails) / dir);
		html += "<h2>" + file + "</h2>\n";
		for (int o = 0; o < int(v.model->objects.size()); ++o)
		{
			v.selectObject(o);
			const auto& obj = v.model->objects[size_t(o)];
			for (size_t k = 0; k < obj.animations.size(); ++k)
				if (v.model->animations[obj.animations[k]].name == "standard") v.selectAnim(int(k));
			if (!render(px, w, h) || px.size() != size_t(w) * h * 4)
			{
				++failed;
				continue;
			}
			std::string rel = dir + "/" + safeFileName(obj.name) + ".png";
			if (writePng((fs::path(args.thumbnails) / rel).string(), w, h, px)) ++written;
			else ++failed;
			const auto* a = v.currentAnim();
			html += "<div><img src=\"" + rel + "\" width=" + std::to_string(w) + " loading=lazy>" + obj.name +
			    "<br><small>" + (a ? a->name : std::string()) + "</small></div>\n";
		}
		std::printf("%s: %zu obiektów\n", file.c_str(), v.model->objects.size());
	}
	std::FILE* f = std::fopen((fs::path(args.thumbnails) / "index.html").string().c_str(), "wb");
	if (f)
	{
		std::fwrite(html.data(), 1, html.size(), f);
		std::fclose(f);
	}
	std::printf("miniatury: zapisano %d, błędy %d, %s/index.html\n", written, failed, args.thumbnails.c_str());
	return failed || !f ? 1 : 0;
}

int main(int argc, char** argv)
{
	Args args;
	if (!parseArgs(argc, argv, args)) return 2;
	gIniGameDir = iniValue("GameDir");
	gIniSavesDir = iniValue("SavesDir");
	resolveGameDir(args);
	if (!args.thumbnails.empty() && !args.sizeSet) args.width = args.height = 256;
	{
		std::vector<std::string> warnings;
		i18n::loadBuiltin(&warnings);
		for (const auto& w : warnings) std::fprintf(stderr, "tłumaczenia: %s\n", w.c_str());
		gLangFromArgs = !args.lang.empty();
		if (gLangFromArgs && !i18n::setLanguage(args.lang))
		{
			std::fprintf(stderr, "nieznany język %s; dostępne:", args.lang.c_str());
			for (const auto& l : i18n::languages()) std::fprintf(stderr, " %s", l.code.c_str());
			std::fprintf(stderr, "\n");
			return 2;
		}
		if (!gLangFromArgs) i18n::setLanguage(i18n::systemLanguage());
	}

	auto viewer = std::make_unique<Viewer>();
	Viewer& v = *viewer;
	v.vfs.addRoot(args.game);
	for (const auto& o : args.overlays)
		if (!v.vfs.addOverlay(o, i18n::language())) std::fprintf(stderr, "nakładka %s: brak katalogu Data ani Data_%s\n", o.c_str(), i18n::language().c_str());
	if (!v.vfs.exists("Data/3db"))
	{
		std::fprintf(stderr, "Nie znaleziono Data/3db w '%s' (źródło: %s). Podaj --game <katalog gry> albo ustaw WIGGLES_DIR.\n",
		    args.game.c_str(), args.gameSource.c_str());
		return 1;
	}
	v.gameDir = args.game;
	v.gameSource = args.gameSource;
	v.pathsOpen = args.paths;
	for (auto& f : v.vfs.list("Data/3db"))
		if (endsWithI(f, ".3db")) v.modelFiles.push_back(f);
	{ // katalogi z plikami TGA w Data/Texture, Data/Ground i Data/GUI (z podkatalogami)
		std::vector<std::string> stack = {"GUI", "Ground", "Texture"};
		while (!stack.empty())
		{
			std::string d = stack.back();
			stack.pop_back();
			bool tga = false;
			std::vector<std::string> subs;
			for (auto& f : v.vfs.list("Data/" + d))
			{
				auto r = v.vfs.resolve("Data/" + d + "/" + f);
				if (r && std::filesystem::is_directory(*r)) subs.push_back(d + "/" + f);
				else if (endsWithI(f, ".tga")) tga = true;
			}
			if (tga) v.texDirs.push_back(d);
			for (auto it = subs.rbegin(); it != subs.rend(); ++it) stack.push_back(*it);
		}
	}

	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
		return 1;
	}
	// Backend renderowania: --renderer albo domyślnie Vulkan; gdy się nie uda, a OpenGL jest wkompilowany,
	// przełączamy się na OpenGL.
	SDL_Window* win = nullptr;
	std::string err;
	{
		std::vector<rhi::Backend> order;
		order.push_back(args.renderer);
		rhi::Backend other = args.renderer == rhi::Backend::Vulkan ? rhi::Backend::OpenGL : rhi::Backend::Vulkan;
		if (!args.rendererForced && rhi::backendCompiled(other)) order.push_back(other);
		for (rhi::Backend b : order)
		{
			if (!rhi::backendCompiled(b))
			{
				std::fprintf(stderr, "Backend %s nie jest wkompilowany\n", rhi::backendName(b));
				continue;
			}
			rhi::prepareWindow(b);
			win = SDL_CreateWindow("wigview - OpenWiggles " OW_VERSION, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			    args.width, args.height, rhi::windowFlags(b) | SDL_WINDOW_RESIZABLE);
			if (!win)
			{
				std::fprintf(stderr, "SDL_CreateWindow (%s): %s\n", rhi::backendName(b), SDL_GetError());
				continue;
			}
			// Zrzut ekranu: Vulkan renderuje poza ekranem (działa też w Xvfb bez DRI3).
			v.device = rhi::createDevice(b, win, true, err, args.batch());
			if (v.device) break;
			std::fprintf(stderr, "%s: %s\n", rhi::backendName(b), err.c_str());
			SDL_DestroyWindow(win);
			win = nullptr;
		}
		if (!v.device)
		{
			std::fprintf(stderr, "Nie udało się uruchomić żadnego backendu renderowania\n");
			return 1;
		}
		SDL_SetWindowTitle(win, (std::string("wigview - OpenWiggles " OW_VERSION " (") + rhi::backendName(v.device->backend()) + ")").c_str());
		std::printf("Renderer: %s\n", v.device->description().c_str());
	}
	rhi::Device& dev = *v.device;

	// ImGui od razu po urządzeniu: okno postępu startu rysuje się w trakcie wczytywania danych.
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	addSettingsHandler();
	ImGui::GetIO().IniFilename = args.batch() ? nullptr : "wigview.ini";
	ImGui::StyleColorsDark();
	// Domyślny font ImGui nie ma polskich znaków. Bierzemy systemowy TTF (glify ładowane na żądanie, ImGui 1.92+).
	for (const char* f : {"/usr/share/fonts/TTF/DejaVuSans.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
	         "/usr/share/fonts/noto/NotoSans-Regular.ttf", "C:\\Windows\\Fonts\\segoeui.ttf"})
		if (std::filesystem::exists(f) && ImGui::GetIO().Fonts->AddFontFromFileTTF(f, 15.f)) break;
	for (const char* f : {"/usr/share/fonts/TTF/DejaVuSansMono.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
	         "/usr/share/fonts/noto/NotoSansMono-Regular.ttf", "C:\\Windows\\Fonts\\consola.ttf"})
		if (std::filesystem::exists(f) && (v.files.mono = ImGui::GetIO().Fonts->AddFontFromFileTTF(f, 14.f))) break;
	dev.imguiInit();

	// Postęp startu (po zmianie katalogu gry pierwszy start trwa kilka do kilkunastu sekund: pliki gry czytane z
	// zimnego dysku). Osobna klatka z samym oknem postępu; OW_PROGRESS_LOG=1 wypisuje czasy etapów.
	const bool startLog = std::getenv("OW_PROGRESS_LOG") != nullptr;
	const Uint64 startT0 = SDL_GetTicks64();
	auto startupProgress = [&](float f, const char* stage) {
		if (startLog) std::fprintf(stderr, "[start] %6llu ms  %3.0f%%  %s\n", (unsigned long long)(SDL_GetTicks64() - startT0), f * 100.f, stage);
		if (args.batch()) return;
		SDL_PumpEvents();
		int w, h;
		SDL_GetWindowSizeInPixels(win, &w, &h);
		dev.imguiNewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();
		ImVec2 ds = ImGui::GetIO().DisplaySize;
		ImGui::SetNextWindowPos(ImVec2(ds.x * 0.5f, ds.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(520, 0));
		ImGui::Begin(tr("Uruchamianie###start"), nullptr,
		    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
		ImGui::TextWrapped(tr("Katalog gry: %s"), args.game.c_str());
		ImGui::TextDisabled("%s", tr(stage));
		ImGui::ProgressBar(std::clamp(f, 0.f, 1.f), ImVec2(-1, 0));
		ImGui::TextDisabled(tr("Pierwszy start z nowym katalogiem gry trwa dłużej (pliki gry czytane z dysku)."));
		ImGui::End();
		ImGui::Render();
		if (!dev.beginFrame(w, h, 0.06f, 0.06f, 0.08f)) return;
		dev.imguiRender(ImGui::GetDrawData());
		dev.endFrame();
	};
	startupProgress(0.02f, N_("Shadery"));

	if (!v.renderer.init(dev, err))
	{
		std::fprintf(stderr, "Shader: %s\n", err.c_str());
		return 1;
	}
	startupProgress(0.08f, N_("Klasy obiektów, szablony i scenariusze"));
	v.textures = std::make_unique<TextureCache>(v.vfs);
	v.textures->setDevice(&dev);
	v.maps.init(dev, &v.vfs, v.textures.get(), &v.sphere);
	if (std::string sd = args.savesDir.empty() ? gIniSavesDir : args.savesDir; !sd.empty()) v.maps.setSavesDir(sd);
	{
		std::string err;
		if (!v.border.init(dev, v.vfs, err)) std::fprintf(stderr, "ramka ekranu: %s\n", err.c_str());
	}
	v.borderMask = args.borderMask;
	v.maps.borderMask = &v.borderMask;
	v.maps.simulate = &v.simulate;
	v.maps.playing = &v.mapPlaying;
	v.maps.objectExtra = [&v](int i) { v.uiSimObject(i); };
	v.maps.openModel = [&v](const std::string& obj, const std::string& anim) { v.openAnim(obj, anim, 0); };
	// Symulacja: w oknie zawsze włączona przy starcie (--no-simulate wyłącza), w zrzutach tylko z --simulate.
	v.simulate = args.simulate || (!args.batch() && !args.noSimulate);
	if (args.popGnomes >= 0) v.maps.populate = true, v.maps.populateGnomes = args.popGnomes, v.maps.populateWukers = args.popWukers;
	startupProgress(0.6f, N_("Cząsteczki i ramka ekranu"));
	v.particles.init(dev, &v.vfs, v.textures.get());
	startupProgress(0.7f, N_("Dźwięki"));
	v.sounds.init(&v.vfs);
	v.files.init(&v.vfs);
	v.files.canOpen = [&v](const std::string& p) { return v.openFile(p, false); };
	v.files.open = [&v](const std::string& p) { return v.openFile(p, true); };
	v.sounds.onOpenAnim = [&v](const std::string& obj, const std::string& anim, int fr) { v.openAnim(obj, anim, fr); };
	startupProgress(0.8f, N_("Modele i tekstury"));
	if (auto sd = v.vfs.read("Data/SPHERE.DAT"))
	{
		v.sphere = parseSphereDat(*sd);
		v.renderer.setSphere(v.sphere);
	}
	else std::fprintf(stderr, "Uwaga: brak Data/SPHERE.DAT\n");

	v.draw.lighting = LightingMode(std::clamp(args.light, 0, 3));
	v.yaw = args.yaw;
	v.pitch = args.pitch;
	v.flipY = args.flip != 0;
	v.interpolate = args.ipol != 0;

	if (!args.model.empty())
	{
		int i = findIndex(v.modelFiles, args.model);
		if (i < 0) i = findIndex(v.modelFiles, args.model + ".3db");
		if (i < 0)
		{
			std::fprintf(stderr, "Nie ma modelu %s\n", args.model.c_str());
			return 1;
		}
		if (!v.loadModel(v.modelFiles[size_t(i)]))
		{
			std::fprintf(stderr, "Błąd modelu: %s\n", v.loadError.c_str());
			return 1;
		}
		if (!args.object.empty())
			for (size_t k = 0; k < v.model->objects.size(); ++k)
				if (Vfs::lower(v.model->objects[k].name) == Vfs::lower(args.object)) v.selectObject(int(k));
		if (!args.anim.empty())
		{
			const auto& o = v.model->objects[size_t(v.object)];
			for (size_t k = 0; k < o.animations.size(); ++k)
				if (Vfs::lower(v.model->animations[o.animations[k]].name) == Vfs::lower(args.anim)) v.selectAnim(int(k));
		}
		v.frame = std::clamp(args.frame, 0.f, float(std::max(0, v.frameCount() - 1)) + 0.999f);
	}

	if (args.particle >= 0)
	{
		v.particles.select(args.particle);
		v.particles.prewarm(36); // 3 s przy 12 klatkach symulacji na sekundę
		v.activeTab = v.forceTab = 4;
		v.frameParticles();
	}
	v.openShadowCmap = args.shadowCmap;
	v.draw.drawModes = args.drawModes;
	if (!args.texture.empty())
	{
		std::string t = args.texture;
		std::string dir = t.find('/') != std::string::npos ? t.substr(0, t.rfind('/')) : "", file = t.substr(t.rfind('/') + 1);
		for (int i = 0; i < int(v.texDirs.size()); ++i)
			if (Vfs::lower(v.texDirs[size_t(i)]) == Vfs::lower(dir)) v.texDir = i;
		v.refreshTexFiles();
		for (int i = 0; i < int(v.texFiles.size()); ++i)
			if (Vfs::lower(v.texFiles[size_t(i)]) == Vfs::lower(file)) v.texSel = i;
		v.texChannel = std::clamp(args.textureChannel, 0, 5);
		v.activeTab = v.forceTab = 1;
	}
	if (!args.file.empty())
	{
		v.files.select(args.file);
		v.activeTab = v.forceTab = 6;
	}
	if (!args.sound.empty())
	{
		v.sounds.setAutoplay(!args.batch());
		v.sounds.select(args.sound);
		v.activeTab = v.forceTab = 5;
	}
	if (!args.templ.empty() || !args.scenario.empty())
	{
		if (!args.scenario.empty())
		{
			std::string t = args.scenario;
			bool hasExt = t.size() > 4 && Vfs::lower(t.substr(t.size() - 4)) == ".tcl";
			bool path = t.find('/') != std::string::npos && Vfs::lower(t).size() > 4 && Vfs::lower(t.substr(t.size() - 4)) == ".sav";
			if (!hasExt && !path && !v.vfs.exists("Data/Gamesave/" + t)) t += ".tcl";
			if (!(path ? v.maps.loadSavePath(t) : v.maps.loadScenario(t))) std::fprintf(stderr, "scenariusz %s: %s\n", t.c_str(), v.maps.error().c_str());
		}
		else
		{
			std::string t = args.templ;
			if (!v.vfs.exists("Data/Templates/" + t)) t += ".tcl";
			if (!v.maps.loadTemplate(t)) std::fprintf(stderr, "szablon %s: %s\n", t.c_str(), v.maps.error().c_str());
		}
		v.activeTab = v.forceTab = v.maps.showingSave() ? 3 : 2;
		v.frameMap();
		if (args.viewSet)
		{
			v.yaw = args.yaw;
			v.pitch = args.pitch;
		}
		if (args.camX >= 0) v.center.x = args.camX * MapView::kBlock, v.center.y = args.camY * MapView::kBlock;
		if (args.camZ >= 0) v.center.z = args.camZ * MapView::kBlock;
		if (args.camDist > 0) v.dist = args.camDist * MapView::kBlock;
		if (args.fowMode >= 0) v.maps.setFog(MapView::FogMode(std::clamp(args.fowMode, 0, 4)), args.fowRadius, args.fowByte);
		if (args.gameView)
		{
			GameView g = *args.gameView;
			g.x -= float(v.maps.origin()[0]);
			g.y -= float(v.maps.origin()[1]);
			v.gameCam = true;
			v.gameCamView = g;
			v.center = glm::vec3(g.x, g.y, 14.f) * MapView::kBlock;
		}
		if (args.labLamps) // jak ow_show w tools/lab/live2.sh: szablon w (16, 16), lampy w (16 + x + 1, 16 + y + 1, 14)
			for (int y = 0; y <= v.maps.mapHeight(); y += 5)
				for (int x = 0; x <= v.maps.mapWidth(); x += 5)
					v.maps.addClassLights("Misc_LightSrc_a", float(x + 1), float(y + 1), 14.f);
		for (const auto& e : args.emits)
		{
			ParticleSource s;
			s.type = int(e[0]);
			s.maxParticles = int(e[4]);
			s.perFrame = int(e[5]);
			if (!v.maps.addEmitter(s, e[1] - float(v.maps.origin()[0]), e[2] - float(v.maps.origin()[1]), e[3] * 0.5f + 7.f))
				std::fprintf(stderr, "--emit: nieznany typ %d\n", s.type);
		}
		for (const auto& [cls, p] : args.places)
		{
			float x = p[0] - float(v.maps.origin()[0]), y = p[1] - float(v.maps.origin()[1]);
			int i = v.maps.addObject(cls, x, y, p[2]);
			if (Vfs::lower(cls) == "zwerg" && v.maps.setObjectModel(i, "mann.3db", "mann")) v.maps.playAnim(i, "standard", true);
			v.maps.addClassLights(cls, x, y, p[2]);
		}
	}

	startupProgress(0.95f, N_("Lista tekstur"));
	if (v.texFiles.empty()) v.refreshTexFiles(); // --texture mogło już wybrać katalog i plik

	// Jedna klatka: scena 3D (na prawo od panelu UI) i opcjonalnie interfejs.
	// readback != nullptr: kopia obrazu klatki (RGBA, wiersz 0 = góra) do zrzutu ekranu.
	auto renderFrame = [&](int w, int h, std::vector<uint8_t>* readback = nullptr) {
		bool withUi = !args.batch() || args.ui;
		if (withUi)
		{
			dev.imguiNewFrame();
			ImGui_ImplSDL2_NewFrame();
			ImGui::NewFrame();
			v.ui();
			{
				// Wersja programu w prawym dolnym rogu (identyfikacja wersji na zrzutach ekranu).
				std::string verText = std::string("wigview " OW_VERSION " (" OW_VERSION_DATE ") ") + rhi::backendName(dev.backend());
				const char* ver = verText.c_str();
				ImVec2 ds = ImGui::GetIO().DisplaySize, ts = ImGui::CalcTextSize(ver);
				ImVec2 pos(ds.x - ts.x - 8.f, ds.y - ts.y - 6.f);
				auto* dl = ImGui::GetForegroundDrawList();
				dl->AddRectFilled(ImVec2(pos.x - 4, pos.y - 2), ImVec2(pos.x + ts.x + 4, pos.y + ts.y + 2), IM_COL32(0, 0, 0, 150), 3.f);
				dl->AddText(pos, IM_COL32(220, 220, 220, 255), ver);
				if (v.controlUnit >= 0 && v.mapTab())
				{ // tryb sterowania: napis na górze sceny
					std::string msg = strf(tr("Sterowanie: %s  (←/→: idź, ↑/↓: w głąb / do przodu, PPM: idź do pola, Shift lub Esc: puść)"),
					    v.maps.objectClass(v.sim->units[size_t(v.controlUnit)].obj).c_str());
					ImVec2 ms = ImGui::CalcTextSize(msg.c_str()), mp(v.uiRight + (ds.x - v.uiRight - ms.x) * 0.5f, 12.f);
					dl->AddRectFilled(ImVec2(mp.x - 8, mp.y - 4), ImVec2(mp.x + ms.x + 8, mp.y + ms.y + 4), IM_COL32(20, 40, 70, 210), 4.f);
					dl->AddText(mp, IM_COL32(255, 230, 120, 255), msg.c_str());
				}
			}
			ImGui::Render();
		}
		float scale = ImGui::GetIO().DisplayFramebufferScale.x;
		glm::vec3 clear = v.clearColor();
		if (!dev.beginFrame(w, h, clear.r, clear.g, clear.b)) return false;
		v.renderScene(w, h, withUi ? int(v.uiRight * scale) : 0);
		if (withUi) dev.imguiRender(ImGui::GetDrawData());
		dev.endFrame(readback);
		return true;
	};
	// Tryb wsadowy bez UI w Vulkanie rysuje do obrazu w pamięci o rozmiarze z --size: menedżer okien (np. kafelkujący)
	// może zmienić rozmiar okna. OpenGL i --ui potrzebują rozmiaru okna.
	auto drawableSize = [&](int& w, int& h) {
		if (args.batch() && !args.ui && dev.backend() == rhi::Backend::Vulkan) w = args.width, h = args.height;
		else SDL_GetWindowSizeInPixels(win, &w, &h);
	};
	// Okno postępu wczytywania mapy: osobna klatka z samym paskiem (scena jest w trakcie przebudowy,
	// więc jej nie rysujemy). Wołane między klatkami, najwyżej co 30 ms.
	Uint64 lastProgress = 0;
	static const bool logProgress = std::getenv("OW_PROGRESS_LOG") != nullptr; // OW_PROGRESS_LOG=1: czasy etapów na stderr
	static Uint64 loadStart = 0;
	auto drawProgress = [&](float f, const char* stage) {
		Uint64 now = SDL_GetTicks64();
		if (logProgress)
		{
			static std::string lastStage;
			if (lastStage.empty()) loadStart = now; // pierwszy etap wczytywania
			if (lastStage != stage) std::fprintf(stderr, "[wczytywanie] %6llu ms  %3.0f%%  %s\n", (unsigned long long)(now - loadStart), f * 100.f, stage);
			lastStage = f >= 1.f ? "" : stage;
		}
		if (now - lastProgress < 30) return;
		lastProgress = now;
		SDL_PumpEvents(); // okno odpowiada systemowi; zdarzenia obsłuży pętla główna po wczytaniu
		int w, h;
		drawableSize(w, h);
		dev.imguiNewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();
		ImVec2 ds = ImGui::GetIO().DisplaySize;
		ImGui::SetNextWindowPos(ImVec2(ds.x * 0.5f, ds.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(420, 0));
		ImGui::Begin(tr("Wczytywanie###wczytywanie"), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
		ImGui::TextUnformatted(v.maps.loadingName().c_str());
		ImGui::TextDisabled("%s", tr(stage));
		ImGui::ProgressBar(std::clamp(f, 0.f, 1.f), ImVec2(-1, 0));
		ImGui::End();
		ImGui::Render();
		if (!dev.beginFrame(w, h, 0.06f, 0.06f, 0.08f)) return;
		dev.imguiRender(ImGui::GetDrawData());
		dev.endFrame();
	};
	if (!args.batch()) v.maps.progress = drawProgress;
	// Kolejność sprzątania: ImGui, potem obiekty z zasobami GPU (Viewer), urządzenie, na końcu okno.
	auto shutdown = [&]() {
		dev.waitIdle();
		dev.imguiShutdown();
		ImGui::DestroyContext();
		std::unique_ptr<rhi::Device> device = std::move(v.device);
		viewer.reset();
		device.reset();
		SDL_DestroyWindow(win);
		SDL_Quit();
	};

	if (!args.search.empty())
	{
		std::snprintf(v.searchText, sizeof v.searchText, "%s", args.search.c_str());
		v.buildSearchIndex(true);
		v.updateSearchHits();
		v.searchRequest = true;
	}

	if (!args.screenshot.empty())
	{
		if (v.simulate && v.maps.hasTemplate())
		{
			v.startSim(nullptr);
			for (float t = 0; t < args.warmup; t += 0.1f) v.update(0.1f);
		}
		int w, h;
		// Kilka klatek, żeby okno zdążyło się zmapować (inaczej zawartość bufora jest niezdefiniowana).
		for (int i = 0; i < 5; ++i)
		{
			SDL_Event e;
			while (SDL_PollEvent(&e))
			{
			}
			drawableSize(w, h);
			renderFrame(w, h);
			SDL_Delay(30);
		}
		if (args.printNav && v.maps.hasTemplate() && v.maps.grid())
		{
			NavWorld nw;
			auto t0 = std::chrono::steady_clock::now();
			nw.build(*v.maps.grid(), v.maps.blockDb());
			double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
			std::printf("nav %d×%d pól, podpól do chodzenia %zu, %.0f ms\n", nw.width(), nw.height(), nw.walkableCount(), ms);
			if (nw.width() <= 48 && nw.height() <= 40) std::printf("%s", nw.ascii(0, 0, nw.width() - 1, nw.height() - 1).c_str());
			if (args.navPath.size() == 7)
			{
				auto& np = args.navPath;
				auto t1 = std::chrono::steady_clock::now();
				auto path = nw.findPath(int(np[0] * 8), int(np[1] * 8), int(np[2] * 8), int(np[3] * 8), np[4] != 0);
				double pms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t1).count();
				std::printf("droga (%g, %g) → (%g, %g)%s: %zu punktów, %.2f ms:", np[0], np[1], np[2], np[3], np[4] != 0 ? " ze wspinaniem" : "",
				    path.size(), pms);
				for (auto& [x, y] : path) std::printf(" (%.3f, %.3f)%s", x / 8.f, y / 8.f, nw.floor(x, y) ? "p" : nw.walkable(x, y) ? "ś"
					                                                                                                                : "?");
				std::printf("\n");
				float s0[3] = {np[0], np[1], np[5]}, g0[3] = {np[2], np[3], np[6]};
				std::string why;
				auto route = planWalk(nw, s0, g0, np[4] != 0, false, &why);
				std::printf("trasa z = %g → %g: %zu punktów%s%s:", np[5], np[6], route.size(), why.empty() ? "" : ", ", why.c_str());
				for (auto& p : route) std::printf(" (%.3f, %.3f, %.2f)%s", p.x, p.y, p.z, p.wall ? "ś" : "");
				std::printf("\n");
			}
			for (int ty = 0; ty < nw.height() && nw.width() <= 48; ++ty)
			{
				std::printf("koszt %2d:", ty);
				for (int tx = 0; tx < nw.width(); ++tx)
					std::printf(" %3d%s", nw.baseCost(tx, ty), nw.tileFloor(tx, ty) ? "p" : " ");
				std::printf("\n");
			}
		}
		if (args.printObjects && v.maps.hasTemplate())
		{
			for (int i = 0; i < v.maps.objectCount(); ++i)
			{
				const auto& s = v.maps.objectSource(i);
				std::string obj = v.maps.objectModelObject(i), anim = v.maps.objectAnimation(i);
				std::printf("obiekt %d %s model=%s anim=%s pos=%.2f,%.2f,%.2f lampy=%d znacznik=%d\n", i, s.className.c_str(), obj.empty() ? "-" : obj.c_str(),
				    anim.empty() ? "-" : anim.c_str(), s.pos[0], s.pos[1], s.pos[2], v.maps.objectLights(i), v.maps.objectMarker(i));
			}
			if (v.simActive && v.sim)
				for (const auto& u : v.sim->units)
					std::printf("postać %s obj=%d x=%.2f y=%.2f pole=%d,%d z=%.2f minz=%.2f\n", u.gnome ? "krasnal" : "wuker", u.obj, u.x, u.y,
					    u.tx, u.ty, u.z, v.sim->minDepth(u.x, u.y));
		}
		if (args.clickX >= 0)
		{
			glm::vec3 ro, rd;
			float sc = ImGui::GetIO().DisplayFramebufferScale.x;
			if (v.sceneRay(float(args.clickX), float(args.clickY), w, h, int(v.uiRight * sc), ro, rd))
			{
				v.maps.selectedObject = v.maps.hoverObject = v.maps.pickObject(ro, rd);
				if (v.maps.selectedObject < 0) v.maps.pickTerrain(ro, rd, v.maps.selectedTileX, v.maps.selectedTileY);
				std::printf("klik (%d, %d): obiekt %d, pole %d, %d\n", args.clickX, args.clickY, v.maps.selectedObject,
				    v.maps.selectedTileX, v.maps.selectedTileY);
			}
			for (int i = 0; i < 2; ++i) renderFrame(w, h); // panel zaznaczenia w interfejsie
		}
		std::vector<uint8_t> px;
		renderFrame(w, h, &px);
		std::printf("model %s obiekt %d anim %d klatka %d mesh %d  środek (%.1f %.1f %.1f) r=%.1f dist=%.1f\n",
		    v.modelFile.c_str(), v.object, v.anim, int(v.frame), v.currentMesh(), v.center.x, v.center.y,
		    v.center.z, v.radius, v.dist);
		bool ok = px.size() == size_t(w) * h * 4 && writePng(args.screenshot, w, h, px);
		std::printf("%s %s\n", ok ? "zapisano" : "BŁĄD zapisu", args.screenshot.c_str());
		shutdown();
		return ok ? 0 : 1;
	}

	if (!args.thumbnails.empty())
	{
		int ok = writeThumbnails(v, args, [&](std::vector<uint8_t>& px, int& w, int& h) {
			SDL_Event e;
			while (SDL_PollEvent(&e))
			{
			}
			drawableSize(w, h);
			return renderFrame(w, h, &px);
		});
		shutdown();
		return ok;
	}

	// Serwer deweloperski: żądania wykonywane w tej pętli (devServer.poll() raz na klatkę), w wątku głównym.
	DevServer devServer;
	if (args.httpPort == 0 && std::getenv("OW_HTTP")) args.httpPort = std::atoi(std::getenv("OW_HTTP"));
	if (args.httpPort > 0)
	{
		DevHooks hk;
		hk.program = "wigview";
		hk.sim = [&v]() { return v.simActive ? v.sim.get() : nullptr; };
		hk.scene = [&v]() -> MapScene* { return v.maps.hasTemplate() ? &v.maps : nullptr; };
		hk.setSimulate = [&v](bool on) {
			v.simulate = on;
			return v.maps.hasTemplate();
		};
		hk.paused = [&v]() { return !v.mapPlaying; };
		hk.setPaused = [&v](bool p) { v.mapPlaying = !p; };
		hk.advance = [&v](double s) {
			for (double t = 0; t < s - 1e-6; t += 0.1)
			{
				float d = float(std::min(0.1, s - t));
				v.maps.update(d);
				if (v.simActive && v.sim) v.sim->simUpdate(d);
			}
		};
		hk.screenshot = [&](int& w, int& h, std::vector<uint8_t>& px) {
			drawableSize(w, h);
			return renderFrame(w, h, &px);
		};
		hk.controlled = [&v]() { return v.controlUnit; };
		hk.setControlled = [&v](int k) {
			if (k >= 0) v.takeControl(k);
			else v.releaseControl();
		};
		hk.mapName = [&v]() { return v.maps.current(); };
		registerDevApi(devServer, hk);
		std::string err;
		if (devServer.start(args.httpPort, err)) std::printf("serwer deweloperski: http://127.0.0.1:%d/\n", args.httpPort);
		else std::fprintf(stderr, "serwer deweloperski: %s\n", err.c_str());
		std::fflush(stdout);
	}

	Uint64 last = SDL_GetPerformanceCounter();
	bool running = true;
	bool clickArmed = false, rclickArmed = false;
	int clickX = 0, clickY = 0;
	std::string restartGame;
	while (running)
	{
		devServer.poll();
		if (!v.restartGame.empty())
		{
			restartGame = v.restartGame; // okno „Ścieżki”: nowy katalog gry
			break;
		}
		SDL_Event e;
		while (SDL_PollEvent(&e))
		{
			ImGui_ImplSDL2_ProcessEvent(&e);
			ImGuiIO& io = ImGui::GetIO();
			if (e.type == SDL_QUIT) running = false;
			if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE) running = false;
			if (v.controlUnit >= 0 && e.type == SDL_KEYDOWN && !io.WantTextInput)
			{ // sterowanie postacią przechwytuje strzałki; Shift albo Esc oddaje klawiaturę przeglądarce
				SDL_Keycode k = e.key.keysym.sym;
				bool used = true;
				// strzałki: ruch, dopóki trzymane (stan klawiatury w Viewer::update); tu tylko krótkie stuknięcie,
				// żeby naciśnięcie krótsze niż klatka też przesunęło postać
				if (k == SDLK_LEFT) v.tapX = -1;
				else if (k == SDLK_RIGHT) v.tapX = 1;
				else if (k == SDLK_UP) v.tapZ = -1;
				else if (k == SDLK_DOWN) v.tapZ = 1;
				else if (k == SDLK_LSHIFT || k == SDLK_RSHIFT || k == SDLK_ESCAPE) v.releaseControl();
				else used = false;
				if (used) continue;
			}
			if (e.type == SDL_KEYDOWN && !io.WantTextInput && v.controlUnit < 0 &&
			    (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_KP_ENTER) && v.mapTab())
			{ // zaznaczona postać, a bez zaznaczenia pierwszy krasnal
				int unit = v.unitOfObject(v.maps.selectedObject);
				for (size_t k = 0; unit < 0 && v.simActive && v.sim && k < v.sim->units.size(); ++k)
					if (v.sim->units[k].gnome) unit = int(k);
				if (unit >= 0)
				{
					v.takeControl(unit);
					continue;
				}
			}
			if (e.type == SDL_KEYDOWN && !io.WantTextInput && e.key.keysym.sym == SDLK_n && v.mapTab() && v.maps.hasTemplate() &&
			    !(e.key.keysym.mod & (KMOD_CTRL | KMOD_ALT)))
			{ // N: krasnal, Shift+N: wuker w polu pod kursorem (bez trafienia w teren: środek kadru)
				int mx, my, w, h, tx = -1, ty = -1;
				SDL_GetMouseState(&mx, &my);
				drawableSize(w, h);
				float sc = io.DisplayFramebufferScale.x;
				glm::vec3 ro, rd;
				if (!v.sceneRay(float(mx) * sc, float(my) * sc, w, h, int(v.uiRight * sc), ro, rd) || !v.maps.pickTerrain(ro, rd, tx, ty))
				{
					glm::vec3 c = v.center + v.pan * glm::vec3(1, -1, 1);
					tx = int(c.x / MapView::kBlock), ty = int(c.y / MapView::kBlock);
				}
				bool gnome = !(e.key.keysym.mod & KMOD_SHIFT);
				if (v.simActive && v.sim) v.spawnAt(tx, ty, gnome);
				else v.simulate = true, v.pendingSpawn = Viewer::PendingSpawn{tx, ty, gnome};
				continue;
			}
			if (!io.WantCaptureMouse && v.mapTab() && v.controlUnit >= 0)
			{ // sterowanie: PPM bez przeciągania = idź do pola (przeciąganie dalej przesuwa kamerę)
				if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT)
					rclickArmed = true, clickX = e.button.x, clickY = e.button.y;
				if (e.type == SDL_MOUSEMOTION && rclickArmed && std::abs(e.motion.x - clickX) + std::abs(e.motion.y - clickY) > 4)
					rclickArmed = false;
				if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_RIGHT && rclickArmed)
				{
					rclickArmed = false;
					int w, h;
					drawableSize(w, h);
					float sc = io.DisplayFramebufferScale.x;
					glm::vec3 ro, rd;
					if (v.sceneRay(float(e.button.x) * sc, float(e.button.y) * sc, w, h, int(v.uiRight * sc), ro, rd))
						v.sim->controlGoToRay(v.controlUnit, ro, rd);
				}
			}
			if (!io.WantCaptureMouse && v.mapTab())
			{ // zaznaczanie obiektów: kliknięcie bez przeciągania (przeciąganie obraca kamerę)
				if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT)
					clickArmed = true, clickX = e.button.x, clickY = e.button.y;
				if (e.type == SDL_MOUSEMOTION && clickArmed && std::abs(e.motion.x - clickX) + std::abs(e.motion.y - clickY) > 4)
					clickArmed = false;
				if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT && clickArmed)
				{
					clickArmed = false;
					int w, h;
					drawableSize(w, h);
					float sc = io.DisplayFramebufferScale.x;
					glm::vec3 ro, rd;
					v.maps.clearSelection();
					if (v.sceneRay(float(e.button.x) * sc, float(e.button.y) * sc, w, h, int(v.uiRight * sc), ro, rd))
					{
						v.maps.selectedObject = v.maps.pickObject(ro, rd);
						// przypięty obiekt (czapka, narzędzie) zaznacza swoją postać
						for (int k = 0; k < 3 && v.maps.selectedObject >= 0 && v.maps.linkParent(v.maps.selectedObject) >= 0; ++k)
							v.maps.selectedObject = v.maps.linkParent(v.maps.selectedObject);
						if (v.maps.selectedObject < 0) v.maps.pickTerrain(ro, rd, v.maps.selectedTileX, v.maps.selectedTileY);
					}
					// z symulacją kliknięta postać od razu jest prowadzona strzałkami; kliknięcie w inne miejsce ją puszcza
					// stworzenie z animacją chodu (chomik, pająk, troll…) staje się postacią symulacji
					int unit = v.unitOfObject(v.maps.selectedObject);
					if (unit < 0 && v.simActive && v.sim && v.maps.selectedObject >= 0) unit = v.sim->adoptUnit(v.maps.selectedObject);
					if (unit >= 0) v.takeControl(unit);
					else v.releaseControl();
				}
			}
			if (!io.WantCaptureMouse)
			{
				if ((e.type == SDL_MOUSEMOTION && (e.motion.state & (SDL_BUTTON_LMASK | SDL_BUTTON_RMASK))) ||
				    e.type == SDL_MOUSEWHEEL)
				{ // przejście na kamerę orbitalną od bieżącego widoku
					int ww, wh;
					drawableSize(ww, wh);
					v.leaveGameCam(ww - int(v.uiRight * io.DisplayFramebufferScale.x), wh);
				}
				if (e.type == SDL_MOUSEMOTION && (e.motion.state & SDL_BUTTON_LMASK))
				{
					v.yaw -= float(e.motion.xrel) * 0.4f;
					v.pitch = std::clamp(v.pitch + float(e.motion.yrel) * 0.4f, -89.f, 89.f);
				}
				if (e.type == SDL_MOUSEMOTION && (e.motion.state & SDL_BUTTON_RMASK))
				{
					float s = v.dist * 0.0015f;
					glm::vec3 right(std::cos(glm::radians(v.yaw)), 0, -std::sin(glm::radians(v.yaw)));
					v.pan += -right * float(e.motion.xrel) * s + glm::vec3(0, 1, 0) * float(e.motion.yrel) * s;
				}
				if (e.type == SDL_MOUSEWHEEL) v.dist *= std::pow(0.88f, float(e.wheel.y));
			}
			if (e.type == SDL_KEYDOWN && (e.key.keysym.mod & KMOD_CTRL) &&
			    (e.key.keysym.sym == SDLK_f || e.key.keysym.sym == SDLK_p))
				v.searchRequest = true;
			if (v.searchOpen || v.searchRequest) continue; // klawisze obsługuje okno wyszukiwania
			// Q zamyka program (Esc nie: łatwo go nacisnąć przypadkiem; puszcza sterowaną postać i zamyka wyszukiwarkę)
			if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_q && !(e.key.keysym.mod & (KMOD_CTRL | KMOD_ALT | KMOD_SHIFT)) &&
			    !io.WantTextInput)
				running = false;
			if (!io.WantTextInput && e.type == SDL_KEYDOWN)
			{
				int n = v.frameCount();
				switch (e.key.keysym.sym)
				{
				case SDLK_i: v.interpolate = !v.interpolate; break;
				case SDLK_UP:
					if (v.activeTab == 0) v.stepModel(-1);
					else if (v.activeTab == 1) v.stepTexture(-1);
					else if (v.activeTab == 4) v.particles.step(-1);
					else if (v.activeTab == 5) v.sounds.step(-1);
					else v.maps.step(-1);
					break;
				case SDLK_DOWN:
					if (v.activeTab == 0) v.stepModel(1);
					else if (v.activeTab == 1) v.stepTexture(1);
					else if (v.activeTab == 4) v.particles.step(1);
					else if (v.activeTab == 5) v.sounds.step(1);
					else v.maps.step(1);
					break;
				case SDLK_PAGEUP: v.stepAnim(-1); break;
				case SDLK_PAGEDOWN: v.stepAnim(1); break;
				case SDLK_SPACE:
					if (v.activeTab == 5) v.sounds.togglePlay();
					else if (v.mapTab()) v.mapPlaying = !v.mapPlaying;
					else v.playing = !v.playing;
					break;
				case SDLK_LEFT: // klatki modelu (zakładka Modele)
					if (v.activeTab != 0) break;
					v.playing = false;
					v.frame = std::max(0.f, float(int(v.frame) - 1));
					break;
				case SDLK_RIGHT:
					if (v.activeTab != 0) break;
					v.playing = false;
					v.frame = float(std::min(n - 1, int(v.frame) + 1));
					break;
				default: break;
				}
			}
		}
		// Symulacja: wyłączenie i „Od nowa” wczytują mapę ponownie (obiekty wracają na miejsca), bez zmiany kamery.
		if ((!v.simulate && v.simActive) || v.maps.simRestart)
		{
			v.simActive = false;
			v.maps.simRestart = false;
			v.maps.simInfo.clear();
			v.maps.reloadCurrent();
			v.keepCamera = true;
		}
		if (v.maps.hasPending())
		{ // wczytanie wybrane w UI w poprzedniej klatce
			lastProgress = 0;
			v.maps.runPending();
			if (std::getenv("OW_TRACE_SIM")) std::fprintf(stderr, "[ui] wczytano mapę\n");
			v.maps.clearSelection(); // indeksy obiektów i pola dotyczyły poprzedniej mapy
			if (!v.keepCamera) v.frameMap();
			v.keepCamera = false;
			v.simActive = false;
			last = SDL_GetPerformanceCounter(); // czas wczytywania nie przesuwa animacji
		}
		if (v.simulate && !v.simActive && v.maps.hasTemplate())
		{ // nowa mapa albo włączenie: skrypty na bieżącej mapie
			lastProgress = 0;
			v.startSim(drawProgress);
			last = SDL_GetPerformanceCounter();
		}
		if (v.pendingSpawn && v.simActive)
		{ // N przed startem symulacji
			v.spawnAt(v.pendingSpawn->tx, v.pendingSpawn->ty, v.pendingSpawn->gnome);
			v.pendingSpawn.reset();
		}
		Uint64 now = SDL_GetPerformanceCounter();
		float dt = float(now - last) / float(SDL_GetPerformanceFrequency());
		last = now;
		v.update(dt);

		int w, h;
		drawableSize(w, h);
		{ // obiekt pod kursorem (podświetlenie) w zakładkach map
			v.maps.hoverObject = -1;
			int mx, my;
			SDL_GetMouseState(&mx, &my);
			float sc = ImGui::GetIO().DisplayFramebufferScale.x;
			glm::vec3 ro, rd;
			if (v.mapTab() && v.maps.hasTemplate() && !ImGui::GetIO().WantCaptureMouse &&
			    v.sceneRay(float(mx) * sc, float(my) * sc, w, h, int(v.uiRight * sc), ro, rd))
				v.maps.hoverObject = v.maps.pickObject(ro, rd);
		}
		if (!renderFrame(w, h)) SDL_Delay(16); // np. okno zminimalizowane
	}

	shutdown(); // zapisuje też wigview.ini (z nowym GameDir)
	if (!restartGame.empty())
	{
		// Ponowne uruchomienie z tymi samymi opcjami poza --game. Katalog weźmie się z wigview.ini, chyba że
		// ustawiona zmienna środowiska ma pierwszeństwo: wtedy podajemy go jawnie.
		std::vector<std::string> a;
		for (int i = 0; i < argc; ++i)
		{
			if (std::string(argv[i]) == "--game" && i + 1 < argc)
			{
				++i;
				continue;
			}
			a.push_back(argv[i]);
		}
		const char* e1 = std::getenv("WIGGLES_DIR");
		const char* e2 = std::getenv("OW_GAME");
		if ((e1 && *e1) || (e2 && *e2)) a.insert(a.end(), {"--game", restartGame});
		std::vector<char*> av;
		for (auto& x : a) av.push_back(x.data());
		av.push_back(nullptr);
		execv("/proc/self/exe", av.data());
		std::perror("execv");
		return 1;
	}
	return 0;
}
