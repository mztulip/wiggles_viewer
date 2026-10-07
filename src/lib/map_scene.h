// Scena mapy: świat złożony z szablonów, scenariusza albo zapisu gry (.sav), z terenem, obiektami .3db,
// oświetleniem gry, nieodkrytym terenem, płynami i cząsteczkami, oraz jej rysowanie.
// Używają jej gra (src/apps/game) i przeglądarka (src/apps/wigview, zakładki Mapy i Zapisy: MapView
// dokłada panel ImGui z listami plików i siatką 2D).
//
// Układ współrzędnych: pozycje obiektów w szablonie są w blokach; 1 blok = 200 jednostek
// modelu (wzorzec: OBJ z save_map gry). Oś Y w dół, jak w modelach.
#pragma once

#include "lib/blockdb.h"
#include "lib/class_db.h"
#include "lib/render/panorama_renderer.h"
#include "lib/fluid_sim.h"
#include "lib/render/fluid_renderer.h"
#include "lib/render/particle_renderer.h"
#include "lib/render/shadow_renderer.h"
#include "lib/render/terrain_renderer.h"
#include "lib/heightfield.h"
#include "lib/terrain_mesh.h"
#include "lib/render/model_renderer.h"
#include "lib/map_template.h"
#include "lib/map_world.h"
#include "lib/save_game.h"
#include "lib/model3db.h"

#include <glm/glm.hpp>
#include <map>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ow
{
class Vfs;
class TextureCache;

class MapScene
{
public:
	static constexpr float kBlock = 200.f; // jednostek modelu na blok szablonu

	void init(rhi::Device& dev, const Vfs* vfs, TextureCache* textures, const std::vector<Vec3f>* sphere);
	bool loadTemplate(const std::string& file); // nazwa pliku .tcl w Data/Templates
	bool loadScenario(const std::string& file); // nazwa pliku .tcl w Data/Gamesave (map create + szablony)
	bool loadSavePath(const std::string& path);  // zapis gry .sav z dowolnego miejsca na dysku

	// Postęp wczytywania: ułamek 0..1 i nazwa etapu. Ustawiany przez pętlę główną (rysuje pasek).
	std::function<void(float, const char*)> progress;
	void update(float dt);
	// gameZOffset: wartość do dodania do z po macierzy world, żeby otrzymać z gry (jednostki modelu).
	// focus: środek kadru w jednostkach modelu (wybór najbliższych lamp przy oświetleniu gry).
	void render(const glm::mat4& viewProj, const glm::mat4& world, const ModelDrawOptions& opt, float gameZOffset,
	    const glm::vec3& focus = glm::vec3(0.f));
	// Strefa horyzontu (kolor tła otworów i zanikanie modeli za skałą). Prawo gry (FUN_004750b0):
	// co klatkę najbliższy obiekt Info_Fog_*, którego odległość 2D od ogniska kamery < size;
	// bez takiego obiektu strefy nie ma. focus w jednostkach modelu (układ gry).
	void updateZone(const glm::vec3& focus);
	const HorizonZone* zone() const { return showZone_ && zone_ ? &*zone_ : nullptr; }
	// Dodatkowe źródło cząsteczek w punkcie (x, y bloki sceny, y w dół; zWorld = z świata w blokach), jak
	// change_particlesource na obiekcie stojącym w tym punkcie (porównania ze zrzutami gry). false: nieznany typ.
	bool addEmitter(const ParticleSource& s, float x, float y, float zWorld);
	// Światła klasy (class_light) w punkcie jak obiekt skryptu (x, y bloki, z skryptu), np. lampy laboratorium
	// Misc_LightSrc_a z tools/lab/live2.sh; wyłącza lampę podglądu. false: klasa bez świateł.
	bool addClassLights(const std::string& className, float x, float y, float zScript);
	int mapWidth() const { return pmp_ ? int(pmp_->width) : 0; }
	int mapHeight() const { return pmp_ ? int(pmp_->height) : 0; }
	float floorDrop(float x, float y) const;
	// Wskazywanie (przeglądarka). Promień w układzie sceny (przed macierzą world, jednostki modelu): o + t·d.
	// pickObject: obiekt z najbliższym trójkątem bieżącej klatki na promieniu (bbox w układzie obiektu jako test
	// wstępny); −1 = brak. pickTerrain: pole terenu trafione promieniem (marsz po mapie wysokości), false = brak.
	int pickObject(const glm::vec3& o, const glm::vec3& d) const;
	bool pickTerrain(const glm::vec3& o, const glm::vec3& d, int& tx, int& ty, glm::vec3* hit = nullptr) const; // hit: punkt trafienia
	int hoverObject = -1, selectedObject = -1; // podświetlane przy rysowaniu
	const Model3db* objectModel(int i) const;
	std::string objectAnimation(int i) const;
	std::vector<std::string> objectAnimations(int i) const; // animacje obiektu .3db, którym rysowany jest obiekt
	// Nazwa obiektu .3db, którym rysowany jest obiekt mapy; pusta bez modelu.
	std::string objectModelObject(int i) const;
	// Grupa znacznika edytora obiektu: 0 zwykły, 1 źródło światła albo efektu, 2 obiekt Info, 3 bryła kolizji.
	int objectMarker(int i) const { return objects_[size_t(i)].marker; }
	int objectParticles(int i) const; // żywe cząsteczki emiterów obiektu (wszystkie sloty)
	// Światła obiektu dodanego w trakcie (addObject) w bieżącej pozycji: x, y w blokach, z świata w blokach.
	std::vector<glm::vec3> ownedLightPositions(int i) const;
	int objectLights(int i) const // lampy klasy obiektu (class_light)
	{
		const auto* l = classes_.lights(objects_[size_t(i)].src.className);
		return l ? int(l->size()) : 0;
	}
	const TemplateObject& objectSource(int i) const { return objects_[size_t(i)].src; }
	int currentMeshOf(int i) const { return currentMesh(objects_[size_t(i)]); }
	static glm::mat4 objectMatrixOf(const TemplateObject& o) { return objectMatrix(o); }
	// Źródło cząsteczek obiektu ze skryptu (change_particlesource): nowe albo zmienione w slocie; podąża za obiektem.
	void setObjectParticle(int obj, int slot, const ParticleSource& s);
	void enableObjectParticle(int obj, int slot, bool on); // set_particlesource // bloki od punktu do litej skały pod nim (podłoże cząsteczek)
	// Kamera startowa scenariusza (set_view), we współrzędnych przyciętego świata.
	const std::optional<GameView>& gameView() const { return gameView_; }
	const int* origin() const { return worldOrigin_; } // lewy górny róg przyciętego świata (bloki)

	// Obszar sceny do ustawienia kamery (w jednostkach modelu, Y w dół).
	glm::vec3 center() const { return center_; }
	float radius() const { return radius_; }
	bool hasTemplate() const { return script_.has_value(); }
	const std::string& current() const { return file_; }
	const std::string& error() const { return error_; }

	// --- Interfejs gry (src/apps/game): obiekty sterowane z zewnątrz i zmiany terenu.
	// Indeksy obiektów ważne do następnego wczytania mapy.
	int objectCount() const { return int(objects_.size()); }
	const std::string& objectClass(int i) const { return objects_[size_t(i)].src.className; }
	const float* objectPos(int i) const { return objects_[size_t(i)].src.pos; }
	float objectRotY(int i) const { return objects_[size_t(i)].src.rot[1]; }
	int objectOwner(int i) const { return objects_[size_t(i)].src.owner; }
	const std::string& objectName(int i) const { return objects_[size_t(i)].src.name; }
	const std::string& objectInstanceName(int i) const { return objects_[size_t(i)].src.instanceName; }
	const std::vector<std::pair<std::string, std::string>>& objectTclVars(int i) const { return objects_[size_t(i)].src.tclVars; }
	// Model z pliku .3db i obiektu (dla klas bez class_defaultanim, np. Zwerg = mann.3db, obiekt mann).
	bool setObjectModel(int i, const std::string& modelFile, const std::string& object);
	void setObjectPose(int i, float x, float y, float zScript, float rotY);
	// Animacja obiektu po nazwie (np. "gehen_loop"); loop = pętla, inaczej raz i stop na ostatniej klatce.
	bool playAnim(int i, const std::string& anim, bool loop);
	float animLength(int i) const; // długość bieżącej animacji w sekundach (10 kl./s)
	float animSeconds(int i, const std::string& anim) const; // długość animacji obiektu po nazwie, 0 = brak
	// Ruch zapisany w animacji (Animation3db::move, rotate: na klatkę) i liczba klatek; false = brak animacji.
	bool animMotion(int i, const std::string& anim, Vec3f& move, Vec3f& rotate, int& frames) const;
	// Punkt zaczepienia obiektu w bieżącej klatce, obrócony jak obiekt: przesunięcie x, y (bloki), z (skala skryptu).
	bool linkOffset(int i, int link, float out[3], bool scriptZ = true) const; // scriptZ: z×2 jak get_linkpos
	bool linkEuler(int i, int link, Vec3f& out) const; // orientacja punktu (get_linkrot), radiany
	// link_obj: dziecko podąża za punktem zaczepienia rodzica (parent < 0 odpina).
	void linkObject(int child, int parent, int link);
	void setObjectVisible(int i, bool v)
	{
		if (i >= 0 && i < int(objects_.size())) objects_[size_t(i)].visible = v;
	}
	int linkParent(int child) const // rodzic przypiętego obiektu (czapka → krasnal), −1 = brak
	{
		auto it = links_.find(child);
		return it == links_.end() ? -1 : it->second.first;
	}
	int addObject(const std::string& className, float x, float y, float zScript);
	const Pmp* grid() const { return pmp_ ? &*pmp_ : nullptr; }
	Pmp* mutableGrid() { return pmp_ ? &*pmp_ : nullptr; } // zmiany bloków: potem setBlock/rebuildTerrain
	const BlockDb& blockDb() const { return blocks_; }
	void setBlock(int x, int y, uint32_t block, uint8_t type);
	void rebuildTerrain(); // po setBlock: siatka terenu, wysokości, płyny
	float time() const { return time_; }
	// Odkrycie mgły wokół punktu (krasnale gracza). 🔶 Koło zamiast promieni z rozdziału 10.3.
	void revealFog(int cx, int cy, int r)
	{
		if (!pmp_ || saveFog_.empty()) return;
		bool ch = false;
		for (int y = cy - r; y <= cy + r; ++y)
			for (int x = cx - r; x <= cx + r; ++x)
			{
				if (x < 0 || y < 0 || x >= int(pmp_->width) || y >= int(pmp_->height) || (x - cx) * (x - cx) + (y - cy) * (y - cy) > r * r) continue;
				uint8_t& b = saveFog_[size_t(y) * pmp_->width + size_t(x)];
				if (b != 0xFF) b = 0xFF, ch = true;
			}
		if (ch) fogDirty_ = true;
	}
	const SaveGame* baseSave() const { return baseSave_.get(); } // wczytany zapis .sav (baza zapisu łatą)
	const std::vector<uint8_t>& fogBytes() const { return saveFog_; } // nieodkryty teren gracza 0 (z zapisu)
	const L2m* layer() const { return l2m_ ? &*l2m_ : nullptr; }
	const std::vector<uint8_t>& fluidTypes() const { return fluidType_; }
	const std::vector<float>& fluidFills() const { return fluidFill_; }
	void setFluidState(const std::vector<uint8_t>& type, const std::vector<float>& fill)
	{
		fluidType_ = type, fluidFill_ = fill;
		fluidSim_.setState(type, fill);
		buildFluids();
	}
	const float* objectRot(int i) const { return objects_[size_t(i)].src.rot; }
	float fluidFill(int x, int y) const
	{
		if (!pmp_ || fluidFill_.empty() || x < 0 || y < 0 || x >= int(pmp_->width) || y >= int(pmp_->height)) return 0.f;
		return fluidFill_[size_t(y) * pmp_->width + size_t(x)];
	}
	const std::optional<GameView>& view() const { return gameView_; }

	// Podgląd nieodkrytego terenu (docs/silnik/10_nieodkryty_teren.md). Bez symulacji obiektów-odkrywców: stan pól
	// ustawiany według trybu, wokół ogniska kamery prostokąt ±fogRadius pól z bajtem fogByte.
	enum class FogMode
	{
		Off,
		Explored,
		ViewGrey,
		ViewBlack,
		FromSave
	};
	void setFog(FogMode mode, int radius = 10, int viewByte = 0xFF)
	{
		fogMode_ = mode;
		fogRadius_ = radius;
		fogByte_ = viewByte;
		fogDirty_ = true;
		if (mode == FogMode::Off) buildFluids(); // bez mgły wszystkie pola z płynem
	}

protected: // dostęp dla panelu przeglądarki (MapView)
	// Poziom szczegółów obiektu z szablonu: 0 high, 1 medium, 2 low, 3 off, 4 brak.
	static int detailIndex(const std::string& d);
	struct ModelSlot
	{
		std::unique_ptr<Model3db> model;
		std::unique_ptr<ModelRenderer> renderer;
		bool failed = false;
	};
	struct PlacedObject
	{
		TemplateObject src;
		const ClassModel* cls = nullptr;
		ModelSlot* slot = nullptr;
		int animation = -1; // indeks w Model3db::animations
		int animMode = -1;  // nadpisanie trybu animacji (interfejs gry): 1 raz, 2 pętla
		float animStart = 0.f;
		bool visible = true; // set_visibility ze skryptu
		// Znacznik edytora (rysowany tylko z włączoną swoją grupą): 0 zwykły obiekt, 1 źródło światła albo efektu
		// (model tylko w edytorze: if {[get_mapedit]} {class_defaultanim …}), 2 obiekt „info” (ClassDb::isInfo),
		// 3 bryła kolizji Info_Coll_*.
		int marker = 0;
	};
	std::string world_ = "urw"; // motyw świata (zestaw detali .l2m)
	std::map<std::string, ClassModel> gameModels_;
	std::map<int, std::pair<int, int>> links_; // dziecko -> (rodzic, punkt)
	int currentMesh(const PlacedObject& o) const; // indeks meshu bieżącej klatki animacji
	// Bieżące klatki animacji do interpolacji: meshe a → b, udział t (jak przy rysowaniu). false: bez animacji.
	bool currentFrames(const PlacedObject& o, uint32_t& a, uint32_t& b, float& t) const;
	// Macierz dziecka przypiętego (link_obj) w przestrzeni modelu rodzica: rodzic · T(punkt) · R(orientacja
	// punktu), punkt i orientacja interpolowane jak wierzchołki rodzica (Diggles-Unity HAT_LINK_SYSTEM §5).
	bool linkedMatrix(int child, glm::mat4& out, int depth = 0) const;
	bool objectShown(const PlacedObject& o) const; // jak przy rysowaniu: detale, obiekty edytora, mgła
	int markerKind(const std::string& className, const ClassModel* cls) const; // PlacedObject::marker
	void applyLinks(); // modele nadane przez grę (stałe adresy)
	// Do allLights_; owner ≥ 0: światło obiektu sceny, które co klatkę idzie za jego pozycją (updateOwnedLights).
	bool pushClassLights(const std::string& className, float x, float y, float zScript, int owner = -1);
	void updateOwnedLights();

	ModelSlot* model(const std::string& file);
	// Szuka obiektu najpierw we wskazanym pliku, potem we wszystkich modelach.
	bool resolve(const ClassModel& cm, ModelSlot*& slot, int& animation);
	static glm::mat4 objectMatrix(const TemplateObject& o);
	bool finishLoad(const Scenario& sc, int margin, std::string world, BuiltWorld* prebuilt = nullptr);
	bool loadSave(const std::string& file);                                    // nazwa pliku w Data/Gamesave
	bool loadSaveData(const std::string& name, const std::vector<uint8_t>& data);
	void report(float f, const char* stage) const
	{
		if (progress) progress(f, stage);
	}

	rhi::Device* dev_ = nullptr;
	const Vfs* vfs_ = nullptr;
	TextureCache* textures_ = nullptr;
	const std::vector<Vec3f>* sphere_ = nullptr;
	ClassDb classes_;
	BlockDb blocks_;
	std::string blocksError_;
	TerrainRenderer terrain_;
	std::optional<Heightfield> heights_;
	bool showTerrain_ = true, showObjects_ = true, showZone_ = true, showMarker_[4] = {true, false, false, false}, gameLighting_ = true;
	bool showPanorama_ = true, hasOpenAir_ = false; // panorama tylko, gdy scenariusz ma niebo (set_light_begin)
	bool showCarpet_ = true;
	bool previewLamp_ = true; // lampa podglądu dla szablonów bez własnych świateł
	float groundRow_ = 0.f; // wiersz gruntu panoramy (bloki)
	PanoramaRenderer panorama_;
	// Cząsteczki obiektów z class_particle (docs/silnik/12_czasteczki.md).
	struct MapEmitter
	{
		ParticleEmitter emitter;
		int x = 0, y = 0;         // pole obiektu (nieodkryty teren)
		int obj = -1, slot = -1;  // obiekt i slot źródła (class_particle: kolejne sloty od 0)
	};
	void configureEmitter(MapEmitter& m, const ParticleType& t, const ParticleSource& s, int obj);
	ParticleRenderer particles_;
	ShadowRenderer shadows_;
	bool shadowsReady_ = false;
	bool particlesReady_ = false, showParticles_ = true;
	std::vector<MapEmitter> emitters_;
	std::vector<ParticleSprite> sprites_;
	float particleAcc_ = 0.f;
	void buildEmitters();
	// Płyny (docs/silnik/13_plyny.md): na razie tylko stan z zapisu gry.
	FluidRenderer fluids_;
	bool fluidsReady_ = false, showFluids_ = true;
	std::vector<uint8_t> fluidType_;
	std::vector<float> fluidFill_;
	int fluidCells_ = 0;
	FluidSim fluidSim_; // przepływ (docs/silnik/13_plyny.md §13.5)
	void buildFluids();
	void initFluidSim();
	bool panoramaReady_ = false;
	std::vector<GameLight> allLights_; // lampy wszystkich obiektów (class_light)
	struct OwnedLight
	{
		int object;      // indeks w objects_
		size_t light;    // indeks w allLights_
		glm::vec3 offset; // przesunięcie class_light (x, y w blokach, z skryptu)
	};
	std::vector<OwnedLight> ownedLights_; // światła obiektów dodanych w trakcie (addObject)
	float baseLight(float x, float y) const; // B = 16·warstwa2 (dwuliniowo) + 64
	bool scenarioMode_ = false;
	std::string scenarioInfo_;
	std::optional<GameView> gameView_;
	int placed_ = 0, worldOrigin_[2] = {0, 0};
	int skyTiles_ = 0, lightBegin_ = Scenario::kNoSky;
	struct ZoneObject
	{
		float x, y;
		HorizonZone zone;
		std::string className;
	};
	std::vector<ZoneObject> zoneObjects_;
	std::optional<HorizonZone> zone_;
	std::string zoneClass_;
	// Nieodkryty teren: bajt na pole (gracz lokalny), tekstura narożników (szarość, czerń, skala lamp).
	FogMode fogMode_ = FogMode::Off;
	FogMode revealPrev_ = FogMode::Off; // tryb sprzed „Odkryj mapę”
	bool revealMap_ = false;            // „Odkryj mapę” zaznaczone przez użytkownika
	int fogRadius_ = 10, fogByte_ = 0xFF, fowBegin_ = -1;
	bool fogDirty_ = true;
	int fogFocus_[2] = {-99999, -99999};
	std::vector<uint8_t> fog_;
	std::vector<uint8_t> saveFog_; // bajty mgły gracza 0 z wczytanego zapisu (FogMode::FromSave)
	GameFog gameFog_;
	void updateFog(const glm::vec3& focus);
	uint8_t fogAt(int x, int y) const;
	bool objectVisibleInFog(const PlacedObject& o) const;
	std::map<std::string, ModelSlot> models_;
	std::map<std::string, std::pair<std::string, int>> objectIndex_; // obiekt -> (plik, indeks), wypełniane leniwie

	std::string file_, error_;
	std::optional<TemplateScript> script_;
	std::optional<Pmp> pmp_;
	std::optional<L2m> l2m_;
	std::shared_ptr<SaveGame> baseSave_;
	std::vector<PlacedObject> objects_;
	glm::vec3 center_{0.f};
	float radius_ = 500.f;
	float time_ = 0.f;
	bool showDetail_[5] = {true, true, true, true, true}; // high, medium, low, off, brak
};

} // namespace ow
