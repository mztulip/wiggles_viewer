// Zakładka "Mapy" w wigview: przeglądarka szablonów map (Data/Templates).
//
// Siatka .PMP w 2D (typ pola, ID bloku, warstwy, głębokość), teren ze szczegółowych siatek
// bloków BlockDB oraz obiekty z MapTemplateSet w 3D z modelami .3db.
//
// Układ współrzędnych: pozycje obiektów w szablonie są w blokach; 1 blok = 200 jednostek
// modelu (wzorzec: OBJ z save_map gry). Oś Y w dół, jak w modelach.
#pragma once

#include "lib/blockdb.h"
#include "lib/class_db.h"
#include "lib/gfx/panorama_renderer.h"
#include "lib/fluid_sim.h"
#include "lib/gfx/fluid_renderer.h"
#include "lib/gfx/particle_renderer.h"
#include "lib/gfx/terrain_renderer.h"
#include "lib/heightfield.h"
#include "lib/terrain_mesh.h"
#include "lib/gfx/model_renderer.h"
#include "lib/map_template.h"
#include "lib/map_world.h"
#include "lib/model3db.h"

#include <glm/glm.hpp>
#include <map>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ow {
class Vfs;
class TextureCache;
} // namespace ow

class MapView {
public:
	static constexpr float kBlock = 200.f; // jednostek modelu na blok szablonu

	void init(ow::rhi::Device& dev, const ow::Vfs* vfs, ow::TextureCache* textures, const std::vector<ow::Vec3f>* sphere);
	bool loadTemplate(const std::string& file); // nazwa pliku .tcl w Data/Templates
	bool loadScenario(const std::string& file); // nazwa pliku .tcl w Data/Gamesave (map create + szablony)
	bool loadSavePath(const std::string& path);  // zapis gry .sav z dowolnego miejsca na dysku
	void step(int d);                           // poprzedni/następny szablon (strzałki)
	void select(const std::string& file) {      // wczytaj (odroczone) i przewiń listę do szablonu
		list_ = ListMode::Templates;
		pending_ = [this, file] { return loadTemplate(file); };
		scrollToSel_ = true;
	}
	void selectScenario(const std::string& file) {
		bool sav = file.size() > 4 && (file.compare(file.size() - 4, 4, ".sav") == 0 || file.compare(file.size() - 4, 4, ".SAV") == 0);
		list_ = sav ? ListMode::Saves : ListMode::Scenarios;
		pending_ = [this, file] { return loadScenario(file); };
		scrollToSel_ = true;
	}
	// Wczytywanie odroczone: wybór w UI (w trakcie klatki ImGui) tylko zleca wczytanie, a pętla główna
	// wykonuje je między klatkami, rysując postęp (progress).
	bool hasPending() const { return bool(pending_); }
	const std::string& loadingName() const { return file_; }
	void runPending() {
		auto f = std::move(pending_);
		pending_ = nullptr;
		f();
	}
	// Postęp wczytywania: ułamek 0..1 i nazwa etapu. Ustawiany przez pętlę główną (rysuje pasek).
	std::function<void(float, const char*)> progress;
	const std::vector<std::string>& scenarios() const { return scenarios_; }
	const std::vector<std::string>& files() const { return files_; }

	// Rysuje panel UI (lista, siatka 2D, informacje). Zwraca true, gdy kamera powinna się wycentrować.
	// savesTab: zakładka Zapisy (lista zapisów gry) zamiast Mapy (szablony i scenariusze).
	bool ui(bool savesTab);
	bool showingSave() const { return list_ == ListMode::Saves; }
	void update(float dt);
	// gameZOffset: wartość do dodania do z po macierzy world, żeby otrzymać z gry (jednostki modelu).
	// focus: środek kadru w jednostkach modelu (wybór najbliższych lamp przy oświetleniu gry).
	void render(const glm::mat4& viewProj, const glm::mat4& world, const ow::ModelDrawOptions& opt, float gameZOffset,
	            const glm::vec3& focus = glm::vec3(0.f));
	// Strefa horyzontu (kolor tła otworów i zanikanie modeli za skałą). Prawo gry (FUN_004750b0):
	// co klatkę najbliższy obiekt Info_Fog_*, którego odległość 2D od ogniska kamery < size;
	// bez takiego obiektu strefy nie ma. focus w jednostkach modelu (układ gry).
	void updateZone(const glm::vec3& focus);
	const ow::HorizonZone* zone() const { return showZone_ && zone_ ? &*zone_ : nullptr; }
	// Kamera startowa scenariusza (set_view), we współrzędnych przyciętego świata.
	const std::optional<ow::GameView>& gameView() const { return gameView_; }
	const int* origin() const { return worldOrigin_; } // lewy górny róg przyciętego świata (bloki)

	// Obszar sceny do ustawienia kamery (w jednostkach modelu, Y w dół).
	glm::vec3 center() const { return center_; }
	float radius() const { return radius_; }
	bool hasTemplate() const { return script_.has_value(); }
	const std::string& current() const { return file_; }
	const std::string& error() const { return error_; }

	// --- Interfejs gry (src/game): obiekty sterowane z zewnątrz i zmiany terenu. 🔶 Tymczasowo w MapView;
	// docelowo wspólna scena w bibliotece (TODO). Indeksy obiektów ważne do następnego wczytania mapy.
	int objectCount() const { return int(objects_.size()); }
	const std::string& objectClass(int i) const { return objects_[size_t(i)].src.className; }
	const float* objectPos(int i) const { return objects_[size_t(i)].src.pos; }
	float objectRotY(int i) const { return objects_[size_t(i)].src.rot[1]; }
	int objectOwner(int i) const { return objects_[size_t(i)].src.owner; }
	const std::string& objectName(int i) const { return objects_[size_t(i)].src.name; }
	const std::vector<std::pair<std::string, std::string>>& objectTclVars(int i) const { return objects_[size_t(i)].src.tclVars; }
	// Model z pliku .3db i obiektu (dla klas bez class_defaultanim, np. Zwerg = mann.3db, obiekt mann).
	bool setObjectModel(int i, const std::string& modelFile, const std::string& object);
	void setObjectPose(int i, float x, float y, float zScript, float rotY);
	// Animacja obiektu po nazwie (np. "gehen_loop"); loop = pętla, inaczej raz i stop na ostatniej klatce.
	bool playAnim(int i, const std::string& anim, bool loop);
	float animLength(int i) const; // długość bieżącej animacji w sekundach (10 kl./s)
	float animSeconds(int i, const std::string& anim) const; // długość animacji obiektu po nazwie, 0 = brak
	// Punkt zaczepienia obiektu w bieżącej klatce, obrócony jak obiekt: przesunięcie x, y (bloki), z (skala skryptu).
	bool linkOffset(int i, int link, float out[3]) const;
	// link_obj: dziecko podąża za punktem zaczepienia rodzica (parent < 0 odpina).
	void linkObject(int child, int parent, int link);
	int addObject(const std::string& className, float x, float y, float zScript);
	const ow::Pmp* grid() const { return pmp_ ? &*pmp_ : nullptr; }
	const ow::BlockDb& blockDb() const { return blocks_; }
	void setBlock(int x, int y, uint32_t block, uint8_t type);
	void rebuildTerrain(); // po setBlock: siatka terenu, wysokości, płyny
	float time() const { return time_; }
	// Odkrycie mgły wokół punktu (krasnale gracza). 🔶 Koło zamiast promieni z rozdziału 10.3.
	void revealFog(int cx, int cy, int r) {
		if (!pmp_ || saveFog_.empty()) return;
		bool ch = false;
		for (int y = cy - r; y <= cy + r; ++y)
			for (int x = cx - r; x <= cx + r; ++x) {
				if (x < 0 || y < 0 || x >= int(pmp_->width) || y >= int(pmp_->height) || (x - cx) * (x - cx) + (y - cy) * (y - cy) > r * r) continue;
				uint8_t& b = saveFog_[size_t(y) * pmp_->width + size_t(x)];
				if (b != 0xFF) b = 0xFF, ch = true;
			}
		if (ch) fogDirty_ = true;
	}
	float fluidFill(int x, int y) const {
		if (!pmp_ || fluidFill_.empty() || x < 0 || y < 0 || x >= int(pmp_->width) || y >= int(pmp_->height)) return 0.f;
		return fluidFill_[size_t(y) * pmp_->width + size_t(x)];
	}
	const std::optional<ow::GameView>& view() const { return gameView_; }

	// Podgląd mgły wojny (docs/silnik/10_mgla_wojny.md). Bez symulacji obiektów-odkrywców: stan pól
	// ustawiany według trybu, wokół ogniska kamery prostokąt ±fogRadius pól z bajtem fogByte.
	enum class FogMode { Off, Explored, ViewGrey, ViewBlack, FromSave };
	void setFog(FogMode mode, int radius = 10, int viewByte = 0xFF) {
		fogMode_ = mode;
		fogRadius_ = radius;
		fogByte_ = viewByte;
		fogDirty_ = true;
		if (mode == FogMode::Off) buildFluids(); // bez mgły wszystkie pola z płynem
	}

private:
	struct ModelSlot {
		std::unique_ptr<ow::Model3db> model;
		std::unique_ptr<ow::ModelRenderer> renderer;
		bool failed = false;
	};
	struct PlacedObject {
		ow::TemplateObject src;
		const ow::ClassModel* cls = nullptr;
		ModelSlot* slot = nullptr;
		int animation = -1; // indeks w Model3db::animations
		int animMode = -1;  // nadpisanie trybu animacji (interfejs gry): 1 raz, 2 pętla
		float animStart = 0.f;
	};
	std::string world_ = "urw"; // motyw świata (zestaw detali .l2m)
	std::map<std::string, ow::ClassModel> gameModels_;
	std::map<int, std::pair<int, int>> links_; // dziecko -> (rodzic, punkt)
	int currentMesh(const PlacedObject& o) const; // indeks meshu bieżącej klatki animacji
	void applyLinks(); // modele nadane przez grę (stałe adresy)

	ModelSlot* model(const std::string& file);
	// Szuka obiektu najpierw we wskazanym pliku, potem we wszystkich modelach.
	bool resolve(const ow::ClassModel& cm, ModelSlot*& slot, int& animation);
	void uiGrid();
	static glm::mat4 objectMatrix(const ow::TemplateObject& o);
	bool finishLoad(const ow::Scenario& sc, int margin, std::string world, ow::BuiltWorld* prebuilt = nullptr);
	bool loadSave(const std::string& file);                                    // nazwa pliku w Data/Gamesave
	bool loadSaveData(const std::string& name, const std::vector<uint8_t>& data);
	void report(float f, const char* stage) const {
		if (progress) progress(f, stage);
	}
	std::function<bool()> pending_;
	void refreshSaves();
	void uiSaves(bool& recenter);

	ow::rhi::Device* dev_ = nullptr;
	const ow::Vfs* vfs_ = nullptr;
	ow::TextureCache* textures_ = nullptr;
	const std::vector<ow::Vec3f>* sphere_ = nullptr;
	ow::ClassDb classes_;
	ow::BlockDb blocks_;
	std::string blocksError_;
	ow::TerrainRenderer terrain_;
	std::optional<ow::Heightfield> heights_;
	bool showTerrain_ = true, showObjects_ = true, showZone_ = true, showEditorOnly_ = false, gameLighting_ = true;
	bool showPanorama_ = true, hasOpenAir_ = false; // panorama tylko, gdy scenariusz ma niebo (set_light_begin)
	bool showCarpet_ = true;
	bool previewLamp_ = true; // lampa podglądu dla szablonów bez własnych świateł
	float groundRow_ = 0.f; // wiersz gruntu panoramy (bloki)
	ow::PanoramaRenderer panorama_;
	// Cząsteczki obiektów z class_particle (docs/silnik/12_czasteczki.md).
	struct MapEmitter {
		ow::ParticleEmitter emitter;
		int x = 0, y = 0; // pole obiektu (mgła wojny)
	};
	ow::ParticleRenderer particles_;
	bool particlesReady_ = false, showParticles_ = true;
	std::vector<MapEmitter> emitters_;
	std::vector<ow::ParticleSprite> sprites_;
	float particleAcc_ = 0.f;
	void buildEmitters();
	// Płyny (docs/silnik/13_plyny.md): na razie tylko stan z zapisu gry.
	ow::FluidRenderer fluids_;
	bool fluidsReady_ = false, showFluids_ = true;
	std::vector<uint8_t> fluidType_;
	std::vector<float> fluidFill_;
	int fluidCells_ = 0;
	ow::FluidSim fluidSim_; // przepływ (docs/silnik/13_plyny.md §13.5)
	void buildFluids();
	void initFluidSim();
	bool panoramaReady_ = false;
	std::vector<ow::GameLight> allLights_; // lampy wszystkich obiektów (class_light)
	float baseLight(float x, float y) const; // B = 16·warstwa2 (dwuliniowo) + 64
	enum class ListMode { Templates, Scenarios, Saves };
	ListMode list_ = ListMode::Templates;
	const std::vector<std::string>& currentList() const {
		return list_ == ListMode::Templates ? files_ : list_ == ListMode::Scenarios ? scenarios_ : saves_;
	}
	bool scenarioMode_ = false;
	std::vector<std::string> scenarios_, saves_;
	char savePath_[512] = "";
	std::string scenarioInfo_;
	std::optional<ow::GameView> gameView_;
	int placed_ = 0, worldOrigin_[2] = {0, 0};
	int skyTiles_ = 0, lightBegin_ = ow::Scenario::kNoSky;
	struct ZoneObject {
		float x, y;
		ow::HorizonZone zone;
		std::string className;
	};
	std::vector<ZoneObject> zoneObjects_;
	std::optional<ow::HorizonZone> zone_;
	std::string zoneClass_;
	// Mgła wojny: bajt na pole (gracz lokalny), tekstura narożników (szarość, czerń, skala lamp).
	FogMode fogMode_ = FogMode::Off;
	FogMode revealPrev_ = FogMode::Off; // tryb sprzed „Odkryj mapę”
	bool revealMap_ = false;            // „Odkryj mapę” zaznaczone przez użytkownika
	int fogRadius_ = 10, fogByte_ = 0xFF, fowBegin_ = -1;
	bool fogDirty_ = true;
	int fogFocus_[2] = {-99999, -99999};
	std::vector<uint8_t> fog_;
	std::vector<uint8_t> saveFog_; // bajty mgły gracza 0 z wczytanego zapisu (FogMode::FromSave)
	ow::GameFog gameFog_;
	void updateFog(const glm::vec3& focus);
	uint8_t fogAt(int x, int y) const;
	bool objectVisibleInFog(const PlacedObject& o) const;
	std::vector<std::string> files_; // skrypty .tcl szablonów
	std::map<std::string, ModelSlot> models_;
	std::map<std::string, std::pair<std::string, int>> objectIndex_; // obiekt -> (plik, indeks), wypełniane leniwie

	std::string file_, error_;
	std::optional<ow::TemplateScript> script_;
	std::optional<ow::Pmp> pmp_;
	std::optional<ow::L2m> l2m_;
	std::vector<PlacedObject> objects_;
	glm::vec3 center_{0.f};
	float radius_ = 500.f;
	float time_ = 0.f;

	char filter_[64] = "";
	bool scrollToSel_ = false;
	int gridLayer_ = 0; // 0 typ, 1 ID bloku, 2 warstwa2, 3 l2m
	float cellPx_ = 22.f;
	bool showDetail_[5] = {true, true, true, true, true}; // high, medium, low, off, brak
	bool showUnresolved_ = true;
};
