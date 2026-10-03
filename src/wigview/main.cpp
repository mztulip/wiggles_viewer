// wigview: przeglądarka modeli .3db, animacji i tekstur Wiggles.
//
// Użycie:
//   wigview [--game <katalog gry>] [--model NAZWA] [--object NAZWA] [--anim NAZWA] [--frame N]
//           [--light 0..3] [--yaw ST] [--pitch ST] [--flip 0|1] [--size SZERxWYS]
//           [--screenshot plik.png]
// Bez --game: zmienna OW_GAME albo ~/.wine/drive_c/Program Files (x86)/Innonics/Wiggles.
// --screenshot: renderuje jedną klatkę (bez UI), zapisuje PNG i kończy działanie.
//
// Sterowanie w oknie: LPM obrót, PPM przesunięcie, kółko przybliżenie, spacja play/pauza,
// strzałki lewo/prawo poprzednia/następna klatka.

#include "lib/gfx/gl.h"
#include "lib/gfx/model_renderer.h"
#include "lib/gfx/texture_cache.h"
#include "lib/model3db.h"
#include "lib/png_write.h"
#include "lib/vfs.h"

#include <SDL.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

using namespace ow;

namespace {

struct Args {
	std::string game, model, object, anim, screenshot;
	int frame = 0;
	int light = int(LightingMode::SphereNormal);
	float yaw = 200.f, pitch = 15.f;
	int flip = 1;
	int width = 1400, height = 900;
};

bool parseArgs(int argc, char** argv, Args& a) {
	for (int i = 1; i < argc; ++i) {
		std::string k = argv[i];
		auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
		if (k == "--game") a.game = next();
		else if (k == "--model") a.model = next();
		else if (k == "--object") a.object = next();
		else if (k == "--anim") a.anim = next();
		else if (k == "--frame") a.frame = std::atoi(next());
		else if (k == "--light") a.light = std::atoi(next());
		else if (k == "--yaw") a.yaw = float(std::atof(next()));
		else if (k == "--pitch") a.pitch = float(std::atof(next()));
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
	Vfs vfs;
	std::unique_ptr<TextureCache> textures;
	ModelRenderer renderer;
	std::vector<std::string> modelFiles;

	std::string modelFile;
	std::unique_ptr<Model3db> model;
	std::string loadError;
	int object = 0, anim = 0;
	float frame = 0;
	bool playing = true;
	float fps = 20.f;
	bool loop = true;

	ModelDrawOptions draw;
	bool flipY = true; // modele mają oś Y skierowaną w dół (zakres y ujemny = góra)
	float yaw = 200.f, pitch = 15.f, dist = 300.f;
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
		renderer.setModel(model.get(), textures.get());
		selectObject(0);
		return true;
	}

	glm::mat4 modelMatrix() const {
		glm::mat4 m(1.f);
		// Obrót o 180° wokół X: y w dół -> y w górę, bez lustrzanego odbicia.
		if (flipY) m = glm::rotate(m, glm::radians(180.f), glm::vec3(1, 0, 0));
		return glm::translate(m, -center);
	}

	glm::mat4 viewProj(int w, int h) const {
		float aspect = h > 0 ? float(w) / float(h) : 1.f;
		glm::mat4 proj = glm::perspective(glm::radians(40.f), aspect, radius * 0.01f, radius * 50.f + dist);
		glm::vec3 eye = glm::vec3(std::cos(glm::radians(pitch)) * std::sin(glm::radians(yaw)),
		                          std::sin(glm::radians(pitch)),
		                          std::cos(glm::radians(pitch)) * std::cos(glm::radians(yaw))) * dist;
		glm::mat4 view = glm::lookAt(eye + pan, pan, glm::vec3(0, 1, 0));
		return proj * view;
	}

	// x0: lewa krawędź obszaru 3D w pikselach (na prawo od panelu UI).
	void renderScene(int w, int h, int x0 = 0) {
		gl::Viewport(0, 0, w, h);
		gl::ClearColor(bg.r, bg.g, bg.b, 1.f);
		gl::Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		x0 = std::clamp(x0, 0, w - 1);
		gl::Viewport(x0, 0, w - x0, h);
		int mesh = currentMesh();
		if (mesh >= 0) renderer.drawFrame(uint32_t(mesh), viewProj(w - x0, h), modelMatrix(), draw);
		gl::Viewport(0, 0, w, h);
	}

	void update(float dt) {
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

	void ui();
	void uiModel();
	void uiTextures();
};

bool passFilter(const char* filter, const std::string& s) {
	return !*filter || Vfs::lower(s).find(Vfs::lower(filter)) != std::string::npos;
}

void Viewer::uiModel() {
	ImGui::BeginChild("modele", ImVec2(180, 0), ImGuiChildFlags_Borders);
	for (const auto& f : modelFiles)
		if (ImGui::Selectable(f.c_str(), f == modelFile)) loadModel(f);
	ImGui::EndChild();
	ImGui::SameLine();

	ImGui::BeginChild("szczegoly", ImVec2(0, 0));
	if (!loadError.empty()) ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Błąd: %s", loadError.c_str());
	if (!model) {
		ImGui::TextDisabled("Wybierz model z listy.");
		ImGui::EndChild();
		return;
	}
	ImGui::Text("%s  (%s)", model->name.c_str(), modelFile.c_str());
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
		int f = int(frame);
		if (ImGui::SliderInt("Klatka", &f, 0, std::max(0, n - 1))) {
			frame = float(f);
			playing = false;
		}
		ImGui::SliderFloat("FPS", &fps, 1.f, 60.f, "%.0f");
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
		if (passFilter(texFilter, texFiles[size_t(i)]) && ImGui::Selectable(texFiles[size_t(i)].c_str(), i == texSel))
			texSel = i;
	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::BeginChild("podglad");
	if (texSel >= 0) {
		std::string p = "Data/Texture/" + texDirs[size_t(texDir)] + "/" + texFiles[size_t(texSel)];
		const auto& t = textures->getPath(p);
		ImGui::Text("%s  %dx%d  %s", p.c_str(), t.width, t.height, t.hasAlpha ? "RGBA" : "RGB");
		float avail = ImGui::GetContentRegionAvail().x;
		float s = std::min(avail, 512.f) / float(std::max(1, t.width));
		ImGui::Image(ImTextureID(intptr_t(t.id)), ImVec2(t.width * s, t.height * s));
	}
	ImGui::EndChild();
}

void Viewer::ui() {
	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 8, vp->WorkPos.y + 8), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(560, vp->WorkSize.y - 16), ImGuiCond_FirstUseEver);
	ImGui::Begin("wigview");
	if (ImGui::BeginTabBar("tabs")) {
		if (ImGui::BeginTabItem("Modele")) {
			uiModel();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Tekstury")) {
			uiTextures();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	uiRight = ImGui::GetWindowPos().x + ImGui::GetWindowSize().x;
	ImGui::End();
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
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_Window* win = SDL_CreateWindow("wigview - OpenWiggles", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
	                                   args.width, args.height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	if (!win) {
		std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
		return 1;
	}
	SDL_GLContext ctx = SDL_GL_CreateContext(win);
	if (!ctx) {
		std::fprintf(stderr, "SDL_GL_CreateContext: %s\n", SDL_GetError());
		return 1;
	}
	SDL_GL_SetSwapInterval(1);
	if (const char* missing = gl::load(SDL_GL_GetProcAddress)) {
		std::fprintf(stderr, "Brak funkcji OpenGL: %s\n", missing);
		return 1;
	}
	std::printf("OpenGL: %s / %s\n", (const char*)gl::GetString(GL_VERSION), (const char*)gl::GetString(GL_RENDERER));

	std::string err;
	if (!v.renderer.init(err)) {
		std::fprintf(stderr, "Shader: %s\n", err.c_str());
		return 1;
	}
	v.textures = std::make_unique<TextureCache>(v.vfs);
	if (auto sd = v.vfs.read("Data/SPHERE.DAT")) v.renderer.setSphere(parseSphereDat(*sd));
	else std::fprintf(stderr, "Uwaga: brak Data/SPHERE.DAT\n");

	v.draw.lighting = LightingMode(std::clamp(args.light, 0, 3));
	v.yaw = args.yaw;
	v.pitch = args.pitch;
	v.flipY = args.flip != 0;

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
		v.frame = float(std::clamp(args.frame, 0, std::max(0, v.frameCount() - 1)));
	}

	if (!args.screenshot.empty()) {
		int w, h;
		// Kilka klatek, żeby okno zdążyło się zmapować (inaczej zawartość bufora jest niezdefiniowana).
		for (int i = 0; i < 5; ++i) {
			SDL_Event e;
			while (SDL_PollEvent(&e)) {}
			SDL_GL_GetDrawableSize(win, &w, &h);
			v.renderScene(w, h);
			SDL_GL_SwapWindow(win);
			SDL_Delay(30);
		}
		v.renderScene(w, h);
		if (GLenum ge = gl::GetError()) std::fprintf(stderr, "GL error 0x%x\n", ge);
		std::printf("model %s obiekt %d anim %d klatka %d mesh %d  środek (%.1f %.1f %.1f) r=%.1f dist=%.1f\n",
		            v.modelFile.c_str(), v.object, v.anim, int(v.frame), v.currentMesh(), v.center.x, v.center.y,
		            v.center.z, v.radius, v.dist);
		std::vector<uint8_t> px(size_t(w) * h * 4), flipped(px.size());
		gl::PixelStorei(GL_PACK_ALIGNMENT, 1);
		gl::ReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
		for (int y = 0; y < h; ++y)
			std::copy_n(&px[size_t(h - 1 - y) * w * 4], size_t(w) * 4, &flipped[size_t(y) * w * 4]);
		bool ok = writePng(args.screenshot, w, h, flipped);
		std::printf("%s %s\n", ok ? "zapisano" : "BŁĄD zapisu", args.screenshot.c_str());
		v.renderer.setModel(nullptr, nullptr);
		v.textures.reset();
		SDL_GL_DeleteContext(ctx);
		SDL_DestroyWindow(win);
		SDL_Quit();
		return ok ? 0 : 1;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::GetIO().IniFilename = "wigview.ini";
	ImGui::StyleColorsDark();
	// Domyślny font ImGui nie ma polskich znaków. Bierzemy systemowy TTF (glify ładowane na żądanie, ImGui 1.92+).
	for (const char* f : {"/usr/share/fonts/TTF/DejaVuSans.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
	                      "/usr/share/fonts/noto/NotoSans-Regular.ttf", "C:\\Windows\\Fonts\\segoeui.ttf"})
		if (std::filesystem::exists(f) && ImGui::GetIO().Fonts->AddFontFromFileTTF(f, 15.f)) break;
	ImGui_ImplSDL2_InitForOpenGL(win, ctx);
	ImGui_ImplOpenGL3_Init("#version 330 core");
	v.refreshTexFiles();

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
			if (!io.WantCaptureKeyboard && e.type == SDL_KEYDOWN) {
				int n = v.frameCount();
				switch (e.key.keysym.sym) {
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

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();
		v.ui();
		ImGui::Render();

		int w, h;
		SDL_GL_GetDrawableSize(win, &w, &h);
		float scale = ImGui::GetIO().DisplayFramebufferScale.x;
		v.renderScene(w, h, int(v.uiRight * scale));
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(win);
	}

	v.renderer.setModel(nullptr, nullptr);
	v.textures.reset();
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
	SDL_GL_DeleteContext(ctx);
	SDL_DestroyWindow(win);
	SDL_Quit();
	return 0;
}
