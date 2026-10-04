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
#include "lib/gfx/terrain_renderer.h"
#include "lib/heightfield.h"
#include "lib/terrain_mesh.h"
#include "lib/gfx/model_renderer.h"
#include "lib/map_template.h"
#include "lib/map_world.h"
#include "lib/model3db.h"

#include <glm/glm.hpp>
#include <map>
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
	void select(const std::string& file) {      // wczytaj i przewiń listę do szablonu
		list_ = ListMode::Templates;
		loadTemplate(file);
		scrollToSel_ = true;
	}
	void selectScenario(const std::string& file) {
		bool sav = file.size() > 4 && (file.compare(file.size() - 4, 4, ".sav") == 0 || file.compare(file.size() - 4, 4, ".SAV") == 0);
		list_ = sav ? ListMode::Saves : ListMode::Scenarios;
		loadScenario(file);
		scrollToSel_ = true;
	}
	const std::vector<std::string>& scenarios() const { return scenarios_; }
	const std::vector<std::string>& files() const { return files_; }

	// Rysuje panel UI (lista, siatka 2D, informacje). Zwraca true, gdy kamera powinna się wycentrować.
	bool ui();
	void update(float dt) { time_ += dt; }
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

	// Podgląd mgły wojny (docs/silnik/10_mgla_wojny.md). Bez symulacji obiektów-odkrywców: stan pól
	// ustawiany według trybu, wokół ogniska kamery prostokąt ±fogRadius pól z bajtem fogByte.
	enum class FogMode { Off, Explored, ViewGrey, ViewBlack, FromSave };
	void setFog(FogMode mode, int radius = 10, int viewByte = 0xFF) {
		fogMode_ = mode;
		fogRadius_ = radius;
		fogByte_ = viewByte;
		fogDirty_ = true;
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
	};

	ModelSlot* model(const std::string& file);
	// Szuka obiektu najpierw we wskazanym pliku, potem we wszystkich modelach.
	bool resolve(const ow::ClassModel& cm, ModelSlot*& slot, int& animation);
	void uiGrid();
	static glm::mat4 objectMatrix(const ow::TemplateObject& o);
	bool finishLoad(const ow::Scenario& sc, int margin, std::string world, ow::BuiltWorld* prebuilt = nullptr);
	bool loadSave(const std::string& file);                                    // nazwa pliku w Data/Gamesave
	bool loadSaveData(const std::string& name, const std::vector<uint8_t>& data);
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
