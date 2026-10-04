// wigview: przeglądarka modeli .3db, animacji i tekstur Wiggles.
//
// Użycie:
//   wigview [--game <katalog gry>] [--model NAZWA] [--object NAZWA] [--anim NAZWA] [--frame N]
//           [--light 0..3] [--yaw ST] [--pitch ST] [--flip 0|1] [--size SZERxWYS]
//           [--template SZABLON | --scenario SCENARIUSZ] [--center X Y] [--center-z Z] [--dist D] [--search TEKST]
//           [--view X Y ZOOM DX DY] [--fow TRYB [PROMIEŃ [BAJT]]] [--renderer vulkan|opengl]
//           [--screenshot plik.png] [--ui]
// --renderer: backend renderowania (domyślnie Vulkan; bez tej opcji, gdy Vulkan nie ruszy, OpenGL).
// --center/--center-z/--dist: kamera map w blokach względem szablonu (do porównań ze zrzutami gry).
// --view: kamera gry jak set_view (współrzędne świata scenariusza), zamiast set_view ze skryptu.
// --fow: podgląd mgły wojny: 0 brak, 1 wszystko szare, 2 kadr widoczny + reszta szara, 3 kadr + reszta czarna.
// Bez --game: zmienna OW_GAME albo ~/.wine/drive_c/Program Files (x86)/Innonics/Wiggles.
// --screenshot: renderuje jedną klatkę, zapisuje PNG i kończy działanie (z --ui razem z interfejsem).
// --template: otwiera zakładkę Mapy z podanym szablonem (np. urw_gng_021_a).
//
// Sterowanie w oknie: LPM obrót, PPM przesunięcie, kółko przybliżenie, spacja play/pauza,
// strzałki lewo/prawo poprzednia/następna klatka, góra/dół poprzedni/następny plik (model albo
// tekstura, zależnie od zakładki), PgUp/PgDn poprzednia/następna animacja, I interpolacja,
// Ctrl+F wyszukiwarka (modele, obiekty w modelach, tekstury, szablony), Esc wyjście.
// --frame przyjmuje też ułamki (np. 2.5 = połowa drogi między klatką 2 i 3, z interpolacją).

#include "lib/gfx/model_renderer.h"
#include "lib/gfx/texture_cache.h"
#include "lib/gltf_export.h"
#include "lib/model3db.h"
#include "lib/png_write.h"
#include "lib/vfs.h"
#include "wigview/map_view.h"

#include <ow_version.h> // generowany przez build.sh: OW_VERSION (commit), OW_VERSION_DATE

#include <SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace ow;

namespace {

struct Args {
	std::string game, model, object, anim, screenshot, templ, search, scenario;
	float frame = 0;
	int ipol = 1;
	int light = int(LightingMode::SphereNormal);
	float yaw = 20.f, pitch = 15.f;
	int flip = 1;
	bool ui = false, viewSet = false;
	float camX = -1, camY = -1, camZ = -1, camDist = -1; // kamera map w blokach (lokalnie w szablonie)
	std::optional<GameView> gameView;                    // --view
	int fowMode = 0, fowRadius = 10, fowByte = 255;      // --fow
	rhi::Backend renderer = rhi::backendCompiled(rhi::Backend::Vulkan) ? rhi::Backend::Vulkan : rhi::Backend::OpenGL;
	bool rendererForced = false;                          // --renderer: bez przełączania na drugi backend
	int width = 1400, height = 900;
};

// Przycięcie kątów set_view przy dużym oddaleniu, ustalone obserwacją gry (docs/silnik/06_kamera.md §6.3):
// gra pokazuje kąty |dx|, |dy| ≤ 2,23 / (R − 15,6), R = 16·zoom − 10 bloków.
// Pomiary: zoom 2,5 → 0,15; 2,25 → 0,23; 2,0 → 0,35; 1,7 i mniej bez przycięcia. 🔶 dopasowanie empiryczne.
float gameViewAngleLimit(float R) {
	constexpr float kA = 2.23f, kB = 15.6f;
	return R > kB + 1e-3f ? kA / (R - kB) : 1e9f;
}

bool parseArgs(int argc, char** argv, Args& a) {
	for (int i = 1; i < argc; ++i) {
		std::string k = argv[i];
		auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
		if (k == "--game") a.game = next();
		else if (k == "--model") a.model = next();
		else if (k == "--object") a.object = next();
		else if (k == "--anim") a.anim = next();
		else if (k == "--template") a.templ = next();
		else if (k == "--scenario") a.scenario = next();
		else if (k == "--search") a.search = next();
		else if (k == "--center") {
			a.camX = float(std::atof(next()));
			a.camY = float(std::atof(next()));
		} else if (k == "--center-z") a.camZ = float(std::atof(next()));
		else if (k == "--dist") a.camDist = float(std::atof(next()));
		else if (k == "--renderer") {
			std::string r = next();
			if (!rhi::parseBackend(r, a.renderer)) {
				std::fprintf(stderr, "nieznany renderer %s (vulkan, opengl)\n", r.c_str());
				return false;
			}
			a.rendererForced = true;
		} else if (k == "--fow") {
			a.fowMode = std::atoi(next());
			if (i + 1 < argc && argv[i + 1][0] != '-') a.fowRadius = std::atoi(next());
			if (i + 1 < argc && argv[i + 1][0] != '-') a.fowByte = std::atoi(next());
		} else if (k == "--view") {
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
		else if (k == "--size") std::sscanf(next(), "%dx%d", &a.width, &a.height);
		else {
			std::fprintf(stderr, "nieznany parametr: %s\n", k.c_str());
			return false;
		}
	}
	if (a.game.empty()) {
		if (const char* e = std::getenv("OW_GAME")) a.game = e;
		else if (const char* h = std::getenv("HOME"))
			a.game = std::string(h) + "/.wine/drive_c/Program Files (x86)/Innonics/Wiggles";
	}
	return true;
}

bool endsWithI(const std::string& s, const std::string& suf) {
	return s.size() >= suf.size() && Vfs::lower(s.substr(s.size() - suf.size())) == suf;
}

struct Viewer {
	std::unique_ptr<rhi::Device> device; // backend renderowania (Vulkan albo OpenGL)
	Vfs vfs;
	std::unique_ptr<TextureCache> textures;
	std::vector<Vec3f> sphere;
	int activeTab = 0;       // 0 modele, 1 tekstury, 2 mapy
	int forceTab = -1;       // zakładka do wybrania w najbliższej klatce UI
	MapView maps;
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
	char objFilter[64] = "";
	char animFilter[64] = "";

	// Przeglądarka tekstur
	std::vector<std::string> texDirs;
	int texDir = 0;
	std::vector<std::string> texFiles;
	int texSel = -1;
	char texFilter[64] = "";

	const Animation3db* currentAnim() const {
		if (!model || model->objects.empty()) return nullptr;
		const auto& o = model->objects[size_t(object)];
		if (o.animations.empty()) return nullptr;
		return &model->animations[o.animations[size_t(anim)]];
	}
	int frameCount() const {
		auto a = currentAnim();
		return a ? int(a->frames.size()) : 0;
	}
	int currentMesh() const {
		auto a = currentAnim();
		if (!a || a->frames.empty()) return -1;
		int f = std::clamp(int(frame), 0, int(a->frames.size()) - 1);
		return int(a->frames[size_t(f)]);
	}

	void frameCamera() {
		auto a = currentAnim();
		if (!a || a->frames.empty()) return;
		glm::vec3 mn(1e9f), mx(-1e9f);
		for (uint32_t f : a->frames) {
			const auto& m = model->meshes[f];
			mn = glm::min(mn, glm::vec3(m.bboxMin.x, m.bboxMin.y, m.bboxMin.z));
			mx = glm::max(mx, glm::vec3(m.bboxMax.x, m.bboxMax.y, m.bboxMax.z));
		}
		center = (mn + mx) * 0.5f;
		radius = std::max(1.f, glm::length(mx - mn) * 0.5f);
		dist = radius * 2.6f;
		pan = glm::vec3(0.f);
	}

	void selectAnim(int i) {
		anim = i;
		frame = 0;
		frameCamera();
	}
	void selectObject(int i) {
		object = i;
		selectAnim(0);
	}

	bool loadModel(const std::string& file) {
		loadError.clear();
		renderer.setModel(nullptr, nullptr);
		model.reset();
		modelFile = file;
		Uint64 t0 = SDL_GetPerformanceCounter();
		auto data = vfs.read("Data/3db/" + file);
		if (!data) {
			loadError = "nie można odczytać pliku";
			return false;
		}
		try {
			model = std::make_unique<Model3db>(parseModel3db(*data));
		} catch (const std::exception& e) {
			loadError = e.what();
			return false;
		}
		loadMs = double(SDL_GetPerformanceCounter() - t0) * 1000.0 / double(SDL_GetPerformanceFrequency());
		std::printf("wczytano %s w %.0f ms\n", file.c_str(), loadMs);
		renderer.setModel(model.get(), textures.get());
		selectObject(0);
		return true;
	}

	glm::mat4 modelMatrix() const {
		glm::mat4 m(1.f);
		if (flipY) m = glm::scale(m, glm::vec3(1.f, -1.f, 1.f)); // (x, y, z) gry -> (x, -y, z) OpenGL
		return glm::translate(m, -center);
	}

	// Kamera gry (set_view): gdy aktywna, viewProj używa jej zamiast orbity; ruch myszą ją wyłącza.
	bool gameCam = false;
	ow::GameView gameCamView;

	// Widok mapy jak w grze: kamera z przodu (+z w układzie gry), lekko z góry.
	void frameMap(bool resetAngles = true) {
		if (resetAngles) {
			yaw = 0.f; // kamera z przodu (+z), jak w grze
			pitch = 10.f;
		}
		center = maps.center();
		radius = maps.radius();
		dist = radius * 2.4f;
		pan = glm::vec3(0.f);
		gameCam = false;
		if (const auto& gv = maps.gameView()) {
			gameCam = true;
			gameCamView = *gv;
			center = glm::vec3(gv->x, gv->y, 14.f) * MapView::kBlock; // cel kamery gry (z = 14 bloków)
		}
	}

	glm::mat4 viewProj(int w, int h) const {
		float aspect = h > 0 ? float(w) / float(h) : 1.f;
		if (gameCam && activeTab == 2) {
			// Prawo kamery z exe (map_world.h GameView); układ gry -> OpenGL przez odbicie osi Y.
			const auto& g = gameCamView;
			float Rb = g.zoom * 16.f - 10.f;
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
		                          std::cos(glm::radians(pitch)) * std::cos(glm::radians(yaw))) * dist;
		glm::mat4 view = glm::lookAt(eye + pan, pan, glm::vec3(0, 1, 0));
		return proj * view;
	}

	// x0: lewa krawędź obszaru 3D w pikselach (na prawo od panelu UI).
	// Klatki A i B oraz współczynnik t dla bieżącego czasu animacji.
	bool currentFrames(uint32_t& a, uint32_t& b, float& t) const {
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
	glm::vec3 clearColor() {
		glm::vec3 clear = bg;
		if (activeTab == 2) maps.updateZone(center + pan * glm::vec3(1, -1, 1));
		if (activeTab == 2)
			if (const HorizonZone* z = maps.zone()) clear = glm::vec3(z->r, z->g, z->b);
		return clear;
	}

	void renderScene(int w, int h, int x0 = 0) {
		x0 = std::clamp(x0, 0, w - 1);
		device->setViewport(x0, 0, w - x0, h);
		if (activeTab == 2) {
			if (maps.hasTemplate()) maps.render(viewProj(w - x0, h), modelMatrix(), draw, center.z, center + pan * glm::vec3(1, -1, 1));
		} else {
			uint32_t a, b;
			float t;
			if (currentFrames(a, b, t)) renderer.drawFrames(a, b, t, viewProj(w - x0, h), modelMatrix(), draw);
		}
		device->setViewport(0, 0, w, h);
	}

	void update(float dt) {
		if (playing) maps.update(dt);
		int n = frameCount();
		if (!playing || n <= 1) return;
		frame += dt * fps;
		if (frame >= float(n)) {
			if (loop) frame = std::fmod(frame, float(n));
			else {
				frame = float(n - 1);
				playing = false;
			}
		}
	}

	void refreshTexFiles() {
		texFiles.clear();
		texSel = -1;
		if (texDir < 0 || size_t(texDir) >= texDirs.size()) return;
		for (auto& f : vfs.list("Data/Texture/" + texDirs[size_t(texDir)]))
			if (endsWithI(f, ".tga")) texFiles.push_back(f);
	}

	void stepModel(int d) {
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
	void stepAnim(int d) {
		if (!model || model->objects.empty()) return;
		int n = int(model->objects[size_t(object)].animations.size());
		if (n == 0) return;
		selectAnim((anim + d + n) % n);
		scrollToSel = true;
	}
	void exportGlb(bool allAnims);

	// --- Wyszukiwarka (Ctrl+F) ---
	enum class HitKind { Model, Object, Texture, Template, Scenario };
	struct SearchEntry {
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

	void ui();
	void uiModel();
	void uiTextures();
};

bool passFilter(const char* filter, const std::string& s) {
	return !*filter || Vfs::lower(s).find(Vfs::lower(filter)) != std::string::npos;
}

void Viewer::uiModel() {
	ImGui::BeginChild("modele", ImVec2(180, 0), ImGuiChildFlags_Borders);
	for (const auto& f : modelFiles) {
		if (ImGui::Selectable(f.c_str(), f == modelFile)) loadModel(f);
		if (f == modelFile && scrollToSel) ImGui::SetScrollHereY(0.5f);
	}
	ImGui::EndChild();
	ImGui::SameLine();

	ImGui::BeginChild("szczegoly", ImVec2(0, 0));
	if (!loadError.empty()) ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Błąd: %s", loadError.c_str());
	if (!model) {
		ImGui::TextDisabled("Wybierz model z listy.");
		ImGui::EndChild();
		return;
	}
	ImGui::Text("%s  (%s)  wczytano w %.0f ms", model->name.c_str(), modelFile.c_str(), loadMs);
	ImGui::Text("materiały %zu  klatki %zu  obiekty %zu  animacje %zu", model->materials.size(),
	            model->meshes.size(), model->objects.size(), model->animations.size());

	if (ImGui::CollapsingHeader("Obiekty i animacje", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::SetNextItemWidth(-1);
		ImGui::InputTextWithHint("##of", "filtr obiektów", objFilter, sizeof objFilter);
		ImGui::BeginChild("obiekty", ImVec2(0, 140), ImGuiChildFlags_Borders);
		for (int i = 0; i < int(model->objects.size()); ++i) {
			const auto& o = model->objects[size_t(i)];
			if (!passFilter(objFilter, o.name)) continue;
			char label[160];
			std::snprintf(label, sizeof label, "%s  (%zu)##o%d", o.name.c_str(), o.animations.size(), i);
			if (ImGui::Selectable(label, i == object)) selectObject(i);
		}
		ImGui::EndChild();
		ImGui::SetNextItemWidth(-1);
		ImGui::InputTextWithHint("##af", "filtr animacji", animFilter, sizeof animFilter);
		ImGui::BeginChild("animacje", ImVec2(0, 180), ImGuiChildFlags_Borders);
		if (!model->objects.empty()) {
			const auto& o = model->objects[size_t(object)];
			for (int i = 0; i < int(o.animations.size()); ++i) {
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

	if (ImGui::CollapsingHeader("Odtwarzanie", ImGuiTreeNodeFlags_DefaultOpen)) {
		int n = frameCount();
		if (ImGui::Button(playing ? "Pauza" : "Graj")) playing = !playing;
		ImGui::SameLine();
		if (ImGui::Button("<")) frame = std::max(0.f, float(int(frame) - 1));
		ImGui::SameLine();
		if (ImGui::Button(">")) frame = float(std::min(n - 1, int(frame) + 1));
		ImGui::SameLine();
		ImGui::Checkbox("Pętla", &loop);
		ImGui::SameLine();
		ImGui::Checkbox("Interpolacja", &interpolate);
		int f = int(frame);
		if (ImGui::SliderInt("Klatka", &f, 0, std::max(0, n - 1))) {
			frame = float(f);
			playing = false;
		}
		ImGui::SliderFloat("FPS", &fps, 1.f, 60.f, "%.0f");
		uint32_t fa, fb;
		float ft;
		if (currentFrames(fa, fb, ft) && fa != fb && !renderer.canInterpolate(fa, fb))
			ImGui::TextColored(ImVec4(1, 0.7f, 0.3f, 1), "Klatki %u i %u mają inną topologię (bez interpolacji)", fa, fb);
	}

	if (ImGui::CollapsingHeader("Widok", ImGuiTreeNodeFlags_DefaultOpen)) {
		const char* modes[] = {"Bez światła", "Bajt jako jasność (stare)", "Normalne SPHERE.DAT", "Podgląd normalnych"};
		int m = int(draw.lighting);
		if (ImGui::Combo("Oświetlenie", &m, modes, 4)) draw.lighting = LightingMode(m);
		ImGui::SliderFloat("Ambient", &draw.ambient, 0.f, 1.f);
		ImGui::Checkbox("Siatka", &draw.wireframe);
		ImGui::SameLine();
		ImGui::Checkbox("Alpha test", &draw.alphaTest);
		ImGui::SameLine();
		ImGui::Checkbox("Odwróć Y", &flipY);
		ImGui::ColorEdit3("Tło", &bg.r, ImGuiColorEditFlags_NoInputs);
		if (ImGui::Button("Wyśrodkuj")) frameCamera();
	}

	if (ImGui::CollapsingHeader("Eksport glTF")) {
		ImGui::SetNextItemWidth(-60);
		ImGui::InputText("Katalog", exportDir, sizeof exportDir);
		if (ImGui::Button("Bieżąca animacja")) exportGlb(false);
		ImGui::SameLine();
		if (ImGui::Button("Wszystkie animacje obiektu")) exportGlb(true);
		ImGui::TextDisabled("Plik .glb z teksturami; animacje jako morph targets.\nUżywa ustawień FPS i interpolacji z odtwarzania.");
		if (!exportMsg.empty()) ImGui::TextWrapped("%s", exportMsg.c_str());
	}

	if (ImGui::CollapsingHeader("Informacje")) {
		if (auto a = currentAnim()) {
			ImGui::Text("animacja: %s", a->name.c_str());
			ImGui::Text("flagi %u  f %.4g", a->unknownFlags, a->unknownF);
			ImGui::Text("ruch (%.3f %.3f %.3f)", a->move.x, a->move.y, a->move.z);
			ImGui::Text("obrót (%.3f %.3f %.3f)", a->rotate.x, a->rotate.y, a->rotate.z);
			ImGui::TextWrapped("źródło: %s", a->sourceFile.c_str());
		}
		int mi = currentMesh();
		if (mi >= 0) {
			const auto& mesh = model->meshes[size_t(mi)];
			ImGui::Text("mesh #%d  linki %zu", mi, mesh.links.size());
			ImGui::Text("bbox (%.1f %.1f %.1f) - (%.1f %.1f %.1f)", mesh.bboxMin.x, mesh.bboxMin.y, mesh.bboxMin.z,
			            mesh.bboxMax.x, mesh.bboxMax.y, mesh.bboxMax.z);
		}
		ImGui::Text("klatki w GPU: %zu", renderer.cachedFrames());
		ImGui::SeparatorText("Materiały");
		for (const auto& mat : model->materials) {
			const auto& t = textures->get(mat.name);
			if (t.path.empty()) ImGui::TextColored(ImVec4(1, 0.5f, 0.3f, 1), "%s  (brak)", mat.name.c_str());
			else ImGui::Text("%s  %dx%d", mat.name.c_str(), t.width, t.height);
		}
	}
	ImGui::EndChild();
}

void Viewer::uiTextures() {
	ImGui::BeginChild("katalogi", ImVec2(140, 0), ImGuiChildFlags_Borders);
	for (int i = 0; i < int(texDirs.size()); ++i)
		if (ImGui::Selectable(texDirs[size_t(i)].c_str(), i == texDir)) {
			texDir = i;
			refreshTexFiles();
		}
	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::BeginChild("pliki", ImVec2(220, 0), ImGuiChildFlags_Borders);
	ImGui::SetNextItemWidth(-1);
	ImGui::InputTextWithHint("##tf", "filtr", texFilter, sizeof texFilter);
	for (int i = 0; i < int(texFiles.size()); ++i)
		if (passFilter(texFilter, texFiles[size_t(i)])) {
			if (ImGui::Selectable(texFiles[size_t(i)].c_str(), i == texSel)) texSel = i;
			if (i == texSel && scrollToSel) ImGui::SetScrollHereY(0.5f);
		}
	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::BeginChild("podglad");
	if (texSel >= 0) {
		std::string p = "Data/Texture/" + texDirs[size_t(texDir)] + "/" + texFiles[size_t(texSel)];
		const auto& t = textures->getPath(p);
		ImGui::Text("%s  %dx%d  %s", p.c_str(), t.width, t.height, t.hasAlpha ? "RGBA" : "RGB");
		float avail = ImGui::GetContentRegionAvail().x;
		float s = std::min(avail, 512.f) / float(std::max(1, t.width));
		ImGui::Image(ImTextureID(device->imguiTexture(t.id)), ImVec2(t.width * s, t.height * s));
	}
	ImGui::EndChild();
}

void Viewer::buildSearchIndex(bool withObjects) {
	auto add = [&](HitKind k, std::string label, std::string file, int index) {
		SearchEntry e{k, label, Vfs::lower(label), std::move(file), index};
		searchIndex.push_back(std::move(e));
	};
	if (!searchIndexed) {
		for (auto& f : modelFiles) add(HitKind::Model, f, f, -1);
		for (int d = 0; d < int(texDirs.size()); ++d)
			for (auto& f : vfs.list("Data/Texture/" + texDirs[size_t(d)]))
				if (endsWithI(f, ".tga")) add(HitKind::Texture, texDirs[size_t(d)] + "/" + f, f, d);
		for (auto& f : maps.files()) add(HitKind::Template, f, f, -1);
		for (auto& f : maps.scenarios()) add(HitKind::Scenario, f, f, -1);
		searchIndexed = true;
	}
	if (withObjects && !objectsIndexed) {
		// Obiekty wymagają wczytania wszystkich modeli (jednorazowo, ok. 1 s w wersji release).
		for (auto& f : modelFiles) {
			auto data = vfs.read("Data/3db/" + f);
			if (!data) continue;
			try {
				Model3db m = parseModel3db(*data);
				for (size_t i = 0; i < m.objects.size(); ++i)
					add(HitKind::Object, m.objects[i].name + "  (" + f + ")", f, int(i));
			} catch (...) {}
		}
		objectsIndexed = true;
	}
}

void Viewer::updateSearchHits() {
	searchHits.clear();
	std::vector<std::string> terms;
	{
		std::string t = Vfs::lower(searchText), cur;
		for (char c : t + " ") {
			if (c == ' ') {
				if (!cur.empty()) terms.push_back(cur);
				cur.clear();
			} else cur += c;
		}
	}
	if (terms.empty()) return;
	for (int i = 0; i < int(searchIndex.size()); ++i) {
		bool ok = true;
		for (auto& t : terms)
			if (searchIndex[size_t(i)].lower.find(t) == std::string::npos) {
				ok = false;
				break;
			}
		if (ok) searchHits.push_back(i);
	}
	// Kolejność: modele, szablony, obiekty, tekstury (tekstur jest najwięcej, często w kilku rozdzielczościach).
	auto rank = [&](int i) {
		switch (searchIndex[size_t(i)].kind) {
		case HitKind::Model: return 0;
		case HitKind::Scenario: return 1;
		case HitKind::Template: return 1;
		case HitKind::Object: return 2;
		default: return 3;
		}
	};
	std::stable_sort(searchHits.begin(), searchHits.end(), [&](int a, int b) { return rank(a) < rank(b); });
	if (searchHits.size() > 500) searchHits.resize(500);
	searchSel = std::clamp(searchSel, 0, std::max(0, int(searchHits.size()) - 1));
}

void Viewer::openHit(const SearchEntry& e) {
	switch (e.kind) {
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
		forceTab = 2;
		activeTab = 2;
		if (e.kind == HitKind::Scenario) maps.selectScenario(e.file);
		else maps.select(e.file);
		frameMap();
		break;
	}
	scrollToSel = true;
}

void Viewer::uiSearch() {
	if (searchRequest) {
		ImGui::OpenPopup("Szukaj");
		searchRequest = false;
		searchOpen = true;
	}
	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + 60), ImGuiCond_Always, ImVec2(0.5f, 0.f));
	ImGui::SetNextWindowSize(ImVec2(std::min(720.f, vp->WorkSize.x - 40), 480), ImGuiCond_Always);
	if (!ImGui::BeginPopupModal("Szukaj", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
		searchOpen = false;
		return;
	}
	static const char* kKind[] = {"model", "obiekt", "tekstura", "szablon", "scenariusz"};
	static const ImVec4 kColor[] = {{0.5f, 0.8f, 1, 1}, {0.6f, 1, 0.6f, 1}, {1, 0.8f, 0.4f, 1}, {0.9f, 0.6f, 1, 1}, {1, 0.5f, 0.5f, 1}};

	if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
	ImGui::SetNextItemWidth(-1);
	if (ImGui::InputTextWithHint("##q", "szukaj: modele, obiekty w modelach, tekstury, szablony, scenariusze (kilka słów = wszystkie muszą pasować)",
	                             searchText, sizeof searchText)) {
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
		ImGui::TextDisabled("Obiekty w modelach zostaną zindeksowane po wpisaniu 2 znaków.");
	else
		ImGui::TextDisabled("%zu wyników%s   (strzałki, Enter otwiera, Esc zamyka)", searchHits.size(),
		                    searchHits.size() >= 500 ? "+" : "");
	ImGui::BeginChild("wyniki", ImVec2(0, 0), ImGuiChildFlags_Borders);
	for (int i = 0; i < int(searchHits.size()); ++i) {
		const auto& e = searchIndex[size_t(searchHits[size_t(i)])];
		ImGui::PushID(i);
		bool sel = i == searchSel;
		if (ImGui::Selectable("##hit", sel, ImGuiSelectableFlags_AllowDoubleClick)) {
			searchSel = i;
			if (ImGui::IsMouseDoubleClicked(0)) open = true;
		}
		if (sel && (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_UpArrow) ||
		            ImGui::IsKeyPressed(ImGuiKey_PageDown) || ImGui::IsKeyPressed(ImGuiKey_PageUp)))
			ImGui::SetScrollHereY(0.5f);
		ImGui::SameLine(8);
		ImGui::TextColored(kColor[int(e.kind)], "%-8s", kKind[int(e.kind)]);
		ImGui::SameLine(90);
		ImGui::TextUnformatted(e.label.c_str());
		ImGui::PopID();
	}
	ImGui::EndChild();
	if (open && searchSel >= 0 && searchSel < int(searchHits.size())) {
		openHit(searchIndex[size_t(searchHits[size_t(searchSel)])]);
		close = true;
	}
	if (close) {
		ImGui::CloseCurrentPopup();
		searchOpen = false;
	}
	ImGui::EndPopup();
}

void Viewer::ui() {
	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 8, vp->WorkPos.y + 8), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(560, vp->WorkSize.y - 16), ImGuiCond_FirstUseEver);
	ImGui::Begin("wigview");
	if (ImGui::Button("Szukaj (Ctrl+F)")) searchRequest = true;
	ImGui::SameLine();
	ImGui::TextDisabled("modele, obiekty, tekstury, szablony");
	if (ImGui::BeginTabBar("tabs")) {
		auto tabFlags = [&](int k) { return forceTab == k ? ImGuiTabItemFlags_SetSelected : 0; };
		if (ImGui::BeginTabItem("Modele", nullptr, tabFlags(0))) {
			activeTab = 0;
			uiModel();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Tekstury", nullptr, tabFlags(1))) {
			activeTab = 1;
			uiTextures();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Mapy", nullptr, tabFlags(2))) {
			if (activeTab != 2) frameMap();
			activeTab = 2;
			if (maps.ui()) frameMap();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
		forceTab = -1;
	}
	uiRight = ImGui::GetWindowPos().x + ImGui::GetWindowSize().x;
	uiSearch();
	scrollToSel = false;
	ImGui::End();
}

void Viewer::stepTexture(int d) {
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

std::string safeFileName(std::string s) {
	for (auto& c : s)
		if (!(std::isalnum((unsigned char)c) || c == '_' || c == '-')) c = '_';
	return s;
}

void Viewer::exportGlb(bool allAnims) {
	if (!model || model->objects.empty()) return;
	GltfExportOptions opt;
	opt.fps = fps;
	opt.interpolate = interpolate;
	const auto& o = model->objects[size_t(object)];
	std::string name = safeFileName(model->name) + "_" + safeFileName(o.name);
	if (!allAnims) {
		opt.animations = {anim};
		if (auto a = currentAnim()) name += "_" + safeFileName(a->name);
	}
	std::string path = std::string(exportDir) + "/" + name + ".glb";
	std::string err;
	GltfExportStats st;
	Uint64 t0 = SDL_GetPerformanceCounter();
	if (exportGltf(*model, vfs, sphere, object, opt, path, err, &st)) {
		double ms = double(SDL_GetPerformanceCounter() - t0) * 1000.0 / double(SDL_GetPerformanceFrequency());
		char buf[512];
		std::snprintf(buf, sizeof buf, "Zapisano %s\n%zu animacji, %zu klatek, %zu tekstur, %.1f MB, %.0f ms%s",
		              path.c_str(), st.animations, st.frames, st.textures, st.bytes / 1048576.0, ms,
		              st.incompatibleFrames ? "\n(część klatek o innej topologii zastąpiona poprzednią)" : "");
		exportMsg = buf;
	} else exportMsg = "Błąd eksportu: " + err;
	std::printf("%s\n", exportMsg.c_str());
}

int findIndex(const std::vector<std::string>& v, const std::string& name) {
	for (size_t i = 0; i < v.size(); ++i)
		if (Vfs::lower(v[i]) == Vfs::lower(name)) return int(i);
	return -1;
}

} // namespace

int main(int argc, char** argv) {
	Args args;
	if (!parseArgs(argc, argv, args)) return 2;

	auto viewer = std::make_unique<Viewer>();
	Viewer& v = *viewer;
	v.vfs.addRoot(args.game);
	if (!v.vfs.exists("Data/3db")) {
		std::fprintf(stderr, "Nie znaleziono Data/3db w '%s'. Podaj --game <katalog gry>.\n", args.game.c_str());
		return 1;
	}
	for (auto& f : v.vfs.list("Data/3db"))
		if (endsWithI(f, ".3db")) v.modelFiles.push_back(f);
	for (auto& d : v.vfs.list("Data/Texture"))
		if (v.vfs.resolve("Data/Texture/" + d) && std::filesystem::is_directory(*v.vfs.resolve("Data/Texture/" + d)))
			v.texDirs.push_back(d);

	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
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
		for (rhi::Backend b : order) {
			if (!rhi::backendCompiled(b)) {
				std::fprintf(stderr, "Backend %s nie jest wkompilowany\n", rhi::backendName(b));
				continue;
			}
			rhi::prepareWindow(b);
			win = SDL_CreateWindow("wigview - OpenWiggles " OW_VERSION, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			                       args.width, args.height, rhi::windowFlags(b) | SDL_WINDOW_RESIZABLE);
			if (!win) {
				std::fprintf(stderr, "SDL_CreateWindow (%s): %s\n", rhi::backendName(b), SDL_GetError());
				continue;
			}
			v.device = rhi::createDevice(b, win, true, err);
			if (v.device) break;
			std::fprintf(stderr, "%s: %s\n", rhi::backendName(b), err.c_str());
			SDL_DestroyWindow(win);
			win = nullptr;
		}
		if (!v.device) {
			std::fprintf(stderr, "Nie udało się uruchomić żadnego backendu renderowania\n");
			return 1;
		}
		SDL_SetWindowTitle(win, (std::string("wigview - OpenWiggles " OW_VERSION " (") + rhi::backendName(v.device->backend()) + ")").c_str());
		std::printf("Renderer: %s\n", v.device->description().c_str());
	}
	rhi::Device& dev = *v.device;

	if (!v.renderer.init(dev, err)) {
		std::fprintf(stderr, "Shader: %s\n", err.c_str());
		return 1;
	}
	v.textures = std::make_unique<TextureCache>(v.vfs);
	v.textures->setDevice(&dev);
	v.maps.init(dev, &v.vfs, v.textures.get(), &v.sphere);
	if (auto sd = v.vfs.read("Data/SPHERE.DAT")) {
		v.sphere = parseSphereDat(*sd);
		v.renderer.setSphere(v.sphere);
	}
	else std::fprintf(stderr, "Uwaga: brak Data/SPHERE.DAT\n");

	v.draw.lighting = LightingMode(std::clamp(args.light, 0, 3));
	v.yaw = args.yaw;
	v.pitch = args.pitch;
	v.flipY = args.flip != 0;
	v.interpolate = args.ipol != 0;

	if (!args.model.empty()) {
		int i = findIndex(v.modelFiles, args.model);
		if (i < 0) i = findIndex(v.modelFiles, args.model + ".3db");
		if (i < 0) {
			std::fprintf(stderr, "Nie ma modelu %s\n", args.model.c_str());
			return 1;
		}
		if (!v.loadModel(v.modelFiles[size_t(i)])) {
			std::fprintf(stderr, "Błąd modelu: %s\n", v.loadError.c_str());
			return 1;
		}
		if (!args.object.empty())
			for (size_t k = 0; k < v.model->objects.size(); ++k)
				if (Vfs::lower(v.model->objects[k].name) == Vfs::lower(args.object)) v.selectObject(int(k));
		if (!args.anim.empty()) {
			const auto& o = v.model->objects[size_t(v.object)];
			for (size_t k = 0; k < o.animations.size(); ++k)
				if (Vfs::lower(v.model->animations[o.animations[k]].name) == Vfs::lower(args.anim)) v.selectAnim(int(k));
		}
		v.frame = std::clamp(args.frame, 0.f, float(std::max(0, v.frameCount() - 1)) + 0.999f);
	}

	if (!args.templ.empty() || !args.scenario.empty()) {
		if (!args.scenario.empty()) {
			std::string t = args.scenario;
			bool hasExt = t.size() > 4 && Vfs::lower(t.substr(t.size() - 4)) == ".tcl";
			if (!hasExt && !v.vfs.exists("Data/Gamesave/" + t)) t += ".tcl";
			if (!v.maps.loadScenario(t)) std::fprintf(stderr, "scenariusz %s: %s\n", t.c_str(), v.maps.error().c_str());
		} else {
			std::string t = args.templ;
			if (!v.vfs.exists("Data/Templates/" + t)) t += ".tcl";
			if (!v.maps.loadTemplate(t)) std::fprintf(stderr, "szablon %s: %s\n", t.c_str(), v.maps.error().c_str());
		}
		v.activeTab = 2;
		v.forceTab = 2;
		v.frameMap();
		if (args.viewSet) {
			v.yaw = args.yaw;
			v.pitch = args.pitch;
		}
		if (args.camX >= 0) v.center.x = args.camX * MapView::kBlock, v.center.y = args.camY * MapView::kBlock;
		if (args.camZ >= 0) v.center.z = args.camZ * MapView::kBlock;
		if (args.camDist > 0) v.dist = args.camDist * MapView::kBlock;
		if (args.fowMode > 0) v.maps.setFog(MapView::FogMode(std::clamp(args.fowMode, 0, 3)), args.fowRadius, args.fowByte);
		if (args.gameView) {
			GameView g = *args.gameView;
			g.x -= float(v.maps.origin()[0]);
			g.y -= float(v.maps.origin()[1]);
			v.gameCam = true;
			v.gameCamView = g;
			v.center = glm::vec3(g.x, g.y, 14.f) * MapView::kBlock;
		}
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::GetIO().IniFilename = args.screenshot.empty() ? "wigview.ini" : nullptr;
	ImGui::StyleColorsDark();
	// Domyślny font ImGui nie ma polskich znaków. Bierzemy systemowy TTF (glify ładowane na żądanie, ImGui 1.92+).
	for (const char* f : {"/usr/share/fonts/TTF/DejaVuSans.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
	                      "/usr/share/fonts/noto/NotoSans-Regular.ttf", "C:\\Windows\\Fonts\\segoeui.ttf"})
		if (std::filesystem::exists(f) && ImGui::GetIO().Fonts->AddFontFromFileTTF(f, 15.f)) break;
	dev.imguiInit();
	v.refreshTexFiles();

	// Jedna klatka: scena 3D (na prawo od panelu UI) i opcjonalnie interfejs.
	// readback != nullptr: kopia obrazu klatki (RGBA, wiersz 0 = góra) do zrzutu ekranu.
	auto renderFrame = [&](int w, int h, std::vector<uint8_t>* readback = nullptr) {
		bool withUi = args.screenshot.empty() || args.ui;
		if (withUi) {
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
	auto drawableSize = [&](int& w, int& h) { SDL_GetWindowSizeInPixels(win, &w, &h); };
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

	if (!args.search.empty()) {
		std::snprintf(v.searchText, sizeof v.searchText, "%s", args.search.c_str());
		v.buildSearchIndex(true);
		v.updateSearchHits();
		v.searchRequest = true;
	}

	if (!args.screenshot.empty()) {
		int w, h;
		// Kilka klatek, żeby okno zdążyło się zmapować (inaczej zawartość bufora jest niezdefiniowana).
		for (int i = 0; i < 5; ++i) {
			SDL_Event e;
			while (SDL_PollEvent(&e)) {}
			drawableSize(w, h);
			renderFrame(w, h);
			SDL_Delay(30);
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


	Uint64 last = SDL_GetPerformanceCounter();
	bool running = true;
	while (running) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			ImGui_ImplSDL2_ProcessEvent(&e);
			ImGuiIO& io = ImGui::GetIO();
			if (e.type == SDL_QUIT) running = false;
			if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE) running = false;
			if (!io.WantCaptureMouse) {
				if ((e.type == SDL_MOUSEMOTION && (e.motion.state & (SDL_BUTTON_LMASK | SDL_BUTTON_RMASK))) ||
				    e.type == SDL_MOUSEWHEEL)
					v.gameCam = false; // przejście na kamerę orbitalną
				if (e.type == SDL_MOUSEMOTION && (e.motion.state & SDL_BUTTON_LMASK)) {
					v.yaw -= float(e.motion.xrel) * 0.4f;
					v.pitch = std::clamp(v.pitch + float(e.motion.yrel) * 0.4f, -89.f, 89.f);
				}
				if (e.type == SDL_MOUSEMOTION && (e.motion.state & SDL_BUTTON_RMASK)) {
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
			if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE && !io.WantTextInput) running = false;
			if (!io.WantTextInput && e.type == SDL_KEYDOWN) {
				int n = v.frameCount();
				switch (e.key.keysym.sym) {
				case SDLK_i: v.interpolate = !v.interpolate; break;
				case SDLK_UP:
					if (v.activeTab == 0) v.stepModel(-1);
					else if (v.activeTab == 1) v.stepTexture(-1);
					else v.maps.step(-1);
					break;
				case SDLK_DOWN:
					if (v.activeTab == 0) v.stepModel(1);
					else if (v.activeTab == 1) v.stepTexture(1);
					else v.maps.step(1);
					break;
				case SDLK_PAGEUP: v.stepAnim(-1); break;
				case SDLK_PAGEDOWN: v.stepAnim(1); break;
				case SDLK_SPACE: v.playing = !v.playing; break;
				case SDLK_LEFT: v.playing = false; v.frame = std::max(0.f, float(int(v.frame) - 1)); break;
				case SDLK_RIGHT: v.playing = false; v.frame = float(std::min(n - 1, int(v.frame) + 1)); break;
				default: break;
				}
			}
		}
		Uint64 now = SDL_GetPerformanceCounter();
		float dt = float(now - last) / float(SDL_GetPerformanceFrequency());
		last = now;
		v.update(dt);

		int w, h;
		drawableSize(w, h);
		if (!renderFrame(w, h)) SDL_Delay(16); // np. okno zminimalizowane
	}

	shutdown();
	return 0;
}
