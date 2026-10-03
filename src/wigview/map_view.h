// Zakładka "Mapy" w wigview: przeglądarka szablonów map (Data/Templates).
//
// Siatka .PMP w 2D (typ pola, ID bloku, warstwy), teren z mapy wysokości (bloki .PMP ->
// siatki 8×8 z BlockDB) oraz obiekty z MapTemplateSet w 3D z modelami .3db.
//
// Układ współrzędnych: pozycje obiektów w szablonie są w blokach; 1 blok = 200 jednostek
// modelu (wzorzec: OBJ z save_map gry). Oś Y w dół, jak w modelach.
#pragma once

#include "lib/blockdb.h"
#include "lib/class_db.h"
#include "lib/gfx/terrain_renderer.h"
#include "lib/heightfield.h"
#include "lib/gfx/model_renderer.h"
#include "lib/map_template.h"
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

	void init(const ow::Vfs* vfs, ow::TextureCache* textures, const std::vector<ow::Vec3f>* sphere);
	bool loadTemplate(const std::string& file); // nazwa pliku .tcl w Data/Templates
	void step(int d);                           // poprzedni/następny szablon (strzałki)
	void select(const std::string& file) {      // wczytaj i przewiń listę do szablonu
		loadTemplate(file);
		scrollToSel_ = true;
	}
	const std::vector<std::string>& files() const { return files_; }

	// Rysuje panel UI (lista, siatka 2D, informacje). Zwraca true, gdy kamera powinna się wycentrować.
	bool ui();
	void update(float dt) { time_ += dt; }
	void render(const glm::mat4& viewProj, const glm::mat4& world, const ow::ModelDrawOptions& opt);

	// Obszar sceny do ustawienia kamery (w jednostkach modelu, Y w dół).
	glm::vec3 center() const { return center_; }
	float radius() const { return radius_; }
	bool hasTemplate() const { return script_.has_value(); }
	const std::string& current() const { return file_; }
	const std::string& error() const { return error_; }

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

	const ow::Vfs* vfs_ = nullptr;
	ow::TextureCache* textures_ = nullptr;
	const std::vector<ow::Vec3f>* sphere_ = nullptr;
	ow::ClassDb classes_;
	ow::BlockDb blocks_;
	std::string blocksError_;
	ow::TerrainRenderer terrain_;
	std::optional<ow::Heightfield> heights_;
	bool showTerrain_ = true, showObjects_ = true, showUndug_ = false;
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
