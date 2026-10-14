#include "lib/map_scene.h"

#include "lib/i18n.h"
#include "lib/render/gl.h"
#include "lib/render/texture_cache.h"
#include "lib/level_script.h"
#include "lib/map_world.h"
#include "lib/save_game.h"
#include "lib/vfs.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace ow
{

namespace
{

bool endsWithI(const std::string& s, const std::string& suf)
{
	return s.size() >= suf.size() && Vfs::lower(s.substr(s.size() - suf.size())) == suf;
}

} // namespace

void MapScene::init(rhi::Device& dev, const Vfs* vfs, TextureCache* textures, const std::vector<Vec3f>* sphere)
{
	dev_ = &dev;
	vfs_ = vfs;
	textures_ = textures;
	sphere_ = sphere;
	classes_.load(*vfs);
	std::string err;
	if (!terrain_.init(dev, err)) blocksError_ = tr("shader terenu: ") + err;
	panoramaReady_ = panorama_.init(dev, err, *textures);
	particlesReady_ = particles_.init(dev, err, *textures);
	shadowsReady_ = shadows_.init(dev, err, *textures);
	fluidsReady_ = fluids_.init(dev, err, *textures);
	try
	{
		blocks_.load(*vfs);
	}
	catch (const std::exception& e)
	{
		blocksError_ = e.what();
	}
}


MapScene::ModelSlot* MapScene::model(const std::string& file)
{
	auto key = Vfs::lower(file);
	auto it = models_.find(key);
	if (it != models_.end()) return it->second.failed ? nullptr : &it->second;
	ModelSlot& s = models_[key];
	auto data = vfs_->read("Data/3db/" + file);
	try
	{
		if (!data) throw std::runtime_error("brak pliku");
		s.model = std::make_unique<Model3db>(parseModel3db(*data));
		s.renderer = std::make_unique<ModelRenderer>();
		std::string err;
		if (!s.renderer->init(*dev_, err)) throw std::runtime_error(err);
		s.renderer->setSphere(*sphere_);
		s.renderer->setModel(s.model.get(), textures_);
		for (size_t i = 0; i < s.model->objects.size(); ++i)
			objectIndex_.emplace(Vfs::lower(s.model->objects[i].name), std::make_pair(key, int(i)));
	}
	catch (const std::exception& e)
	{
		std::fprintf(stderr, "model %s: %s\n", file.c_str(), e.what());
		s.failed = true;
		return nullptr;
	}
	return &s;
}

bool MapScene::resolve(const ClassModel& cm, ModelSlot*& slot, int& animation)
{
	auto findIn = [&](ModelSlot* s) -> bool {
		if (!s) return false;
		for (const auto& o : s->model->objects)
		{
			if (Vfs::lower(o.name) != Vfs::lower(cm.object)) continue;
			for (uint32_t a : o.animations)
				if (Vfs::lower(s->model->animations[a].name) == Vfs::lower(cm.animation))
				{
					slot = s;
					animation = int(a);
					return true;
				}
			if (!o.animations.empty())
			{
				slot = s;
				animation = int(o.animations.front());
				return true;
			}
		}
		return false;
	};
	if (!cm.modelHint.empty() && findIn(model(cm.modelHint))) return true;
	auto it = objectIndex_.find(Vfs::lower(cm.object));
	if (it != objectIndex_.end() && findIn(model(it->second.first))) return true;
	// Ostatecznie: wczytaj wszystkie modele (indeks obiektów wypełnia się przy wczytywaniu).
	for (auto& f : vfs_->list("Data/3db"))
		if (endsWithI(f, ".3db")) model(f);
	it = objectIndex_.find(Vfs::lower(cm.object));
	return it != objectIndex_.end() && findIn(model(it->second.first));
}

bool MapScene::loadTemplate(const std::string& file)
{
	file_ = file;
	baseSave_.reset();
	scenarioMode_ = false;
	storyLevel_.clear();
	levelCalls_.clear();
	levelStoryCalls_.clear();
	Scenario sc;
	sc.placements.push_back({file, 0, 0});
	std::string world = Vfs::lower(file.substr(0, file.find('_')));
	return finishLoad(sc, 0, world);
}

// Zapis gry .sav (docs/formaty/sav.md): pełna mapa, obiekty, kamera i nieodkryty teren z chwili zapisu.
bool MapScene::loadSave(const std::string& file)
{
	auto data = vfs_->read("Data/Gamesave/" + file);
	if (!data)
	{
		file_ = file;
		error_ = tr("zapis ") + file + ": " + tr("brak pliku");
		return false;
	}
	return loadSaveData(file, *data);
}

bool MapScene::loadSavePath(const std::string& path)
{
	std::ifstream in(path, std::ios::binary);
	if (!in)
	{
		error_ = tr("zapis ") + path + ": " + tr("nie można otworzyć pliku");
		return false;
	}
	std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	return loadSaveData(path, data);
}


bool MapScene::loadSaveData(const std::string& file, const std::vector<uint8_t>& data)
{
	file_ = file;
	scenarioMode_ = true;
	scenarioInfo_.clear();
	storyLevel_.clear();
	levelCalls_.clear();
	levelStoryCalls_.clear();
	saveFog_.clear();
	error_.clear();
	Scenario sc;
	BuiltWorld bw;
	try
	{
		report(0.02f, N_("Rozpakowywanie i odczyt zapisu"));
		SaveGame s = parseSaveGame(data);
		report(0.04f, N_("Budowa świata z zapisu"));
		bw = buildWorldFromSave(s, blocks_.loaded() ? &blocks_ : nullptr, sc);
		saveFog_.resize(s.tiles.size());
		for (size_t i = 0; i < s.tiles.size(); ++i) saveFog_[i] = s.tiles[i].fog[0];
		char info[160];
		std::snprintf(info, sizeof info, tr("Zapis %s, czas gry %.0f s, %zu obiektów."), s.header.c_str(), s.gameTime, s.objects.size());
		scenarioInfo_ = info;
		baseSave_ = std::make_shared<SaveGame>(std::move(s)); // baza do zapisu łatą
	}
	catch (const std::exception& e)
	{
		error_ = tr("zapis ") + file + ": " + e.what();
		return false;
	}
	// Motyw świata (zestaw detali .l2m) z klas stref horyzontu Info_Fog_<Świat>_*.
	std::map<std::string, int> worlds;
	for (const auto& o : bw.objects)
	{
		std::string c = Vfs::lower(o.className);
		if (c.rfind("info_fog_", 0) == 0) worlds[c.substr(9, c.find('_', 9) - 9)]++;
	}
	std::string world = "urw";
	int best = 0;
	for (auto& [k, n] : worlds)
		if (n > best && (k == "urw" || k == "kris" || k == "lava" || k == "swf")) best = n, world = k;
	bool ok = finishLoad(sc, 0, world, &bw);
	setFog(revealMap_ ? FogMode::Off : FogMode::FromSave, fogRadius_, fogByte_); // nieodkryty teren jak w chwili zapisu
	return ok;
}

bool MapScene::loadScenario(const std::string& file)
{
	if (endsWithI(file, ".sav")) return loadSave(file);
	file_ = file;
	scenarioMode_ = true;
	baseSave_.reset();
	// systeminit.tcl (menu gry) leży w Data/, pozostałe scenariusze w Data/Gamesave/.
	std::string path = Vfs::lower(file) == "systeminit.tcl" ? "Data/systeminit.tcl" : "Data/Gamesave/" + file;
	Scenario sc = parseScenario(*vfs_, path);
	scenarioInfo_.clear();
	int margin = 2;
	if (sc.placements.empty() && sc.usesGenerator)
	{
		// Kampania i inne światy StoryMgr: skrypt poziomu wykonany naprawdę (level_script.h).
		// OW_LEVEL_SEED: stałe ziarno losowania układu jaskiń (testy).
		const char* seed = std::getenv("OW_LEVEL_SEED");
		sc = runLevelScript(*vfs_, path, seed ? unsigned(std::strtoul(seed, nullptr, 10)) : 0u);
		for (auto& e : sc.levelErrors) std::fprintf(stderr, "skrypt poziomu: %s\n", e.c_str());
		if (sc.stamps.empty())
			scenarioInfo_ = tr("Świat generowany proceduralnie (StoryMgr load_level/load_sandbox) - jeszcze nieobsługiwany.");
		margin = 0; // świat StoryMgr: cała mapa z map create (buildWorld)
	}
	storyLevel_ = sc.storyLevel;
	levelCalls_ = sc.calls;
	levelStoryCalls_ = sc.storyCalls;
	// Świat ze szablonu o największej liczbie wystąpień (urw, kris, lava, swf).
	std::map<std::string, int> worlds;
	for (auto& p : sc.placements) worlds[Vfs::lower(p.script.substr(0, p.script.find('_')))]++;
	for (auto& st : sc.stamps)
		if (st.layer)
		{
			std::string f = st.file.substr(st.file.find_last_of("/\\") + 1);
			if (Vfs::lower(f).rfind("splt_", 0) == 0) f = f.substr(5);
			worlds[Vfs::lower(f.substr(0, f.find('_')))]++;
		}
	std::string world = "urw";
	int best = 0;
	for (auto& [k, n] : worlds)
		if (n > best && (k == "urw" || k == "kris" || k == "lava" || k == "swf")) best = n, world = k;
	return finishLoad(sc, margin, world);
}

bool MapScene::finishLoad(const Scenario& sc, int margin, std::string world, BuiltWorld* prebuilt)
{
	error_.clear();
	script_.reset();
	pmp_.reset();
	l2m_.reset();
	objects_.clear();
	links_.clear();
	heights_.reset();
	terrain_.clear();
	placed_ = 0;
	skyTiles_ = 0;
	lightBegin_ = sc.lightBegin;
	fowBegin_ = -1;
	fogDirty_ = true;
	gameView_.reset();
	if (world != "urw" && world != "kris" && world != "lava" && world != "swf") world = "urw";

	if (!prebuilt) report(0.04f, N_("Składanie świata z szablonów"));
	fluidType_.clear();
	hasTpl_.clear();
	fluidFill_.clear();
	BuiltWorld bw = prebuilt ? std::move(*prebuilt) : buildWorld(*vfs_, sc, margin, blocks_.loaded() ? &blocks_ : nullptr);
	if (!prebuilt) saveFog_.clear();
	if (fogMode_ == FogMode::FromSave && saveFog_.empty()) fogMode_ = FogMode::Off;
	for (auto& e : bw.errors) error_ += (error_.empty() ? "" : "; ") + e;
	if (!bw.placedScripts.empty())
	{
		placed_ = int(bw.placedScripts.size());
		gameView_ = bw.view;
		skyTiles_ = bw.skyTiles;
		worldOrigin_[0] = bw.originX;
		worldOrigin_[1] = bw.originY;
		fowBegin_ = sc.fowBegin ? *sc.fowBegin - bw.originY : -1;
		pmp_ = std::move(bw.grid);
		fluidType_ = std::move(bw.fluidType);
		fluidFill_ = std::move(bw.fluidFill);
		hasTpl_ = std::move(bw.hasTpl);
		l2m_ = std::move(bw.layer);
		script_ = TemplateScript{};
		script_->objects = std::move(bw.objects);
		if (!scenarioMode_)
		{
			// Nazwy plików szablonu do wyświetlenia.
			if (auto text = vfs_->read("Data/Templates/" + file_))
			{
				auto t = parseTemplateScript(std::string(text->begin(), text->end()));
				script_->pmpFile = t.pmpFile;
				script_->l2mFile = t.l2mFile;
			}
		}
	}
	else if (error_.empty() && scenarioInfo_.empty())
		error_ = tr("brak szablonów do złożenia");

	// Panorama (panorama_renderer.h): wiersz gruntu z set_ground_begin, a bez niego z set_light_begin.
	// Bez nieba (set_light_begin nie podane) gra panoramy nie rysuje (obserwacja w laboratorium).
	hasOpenAir_ = false;
	if (pmp_)
	{
		int row = sc.groundBegin ? *sc.groundBegin : sc.lightBegin;
		if (sc.groundBegin || sc.lightBegin != Scenario::kNoSky)
		{
			hasOpenAir_ = true;
			groundRow_ = float(row - worldOrigin_[1]);
		}
	}
	if (pmp_ && blocks_.loaded())
	{
		report(0.06f, N_("Siatka terenu"));
		heights_ = buildHeightfield(*pmp_, blocks_);
		auto mesh = buildTerrainMesh(*pmp_, blocks_, *sphere_);
		report(0.09f, N_("Tekstury terenu"));
		world_ = world;
		terrain_.build(std::move(mesh), *textures_, l2m_ ? &*l2m_ : nullptr, world);
	}
	zone_.reset();
	zoneClass_.clear();
	zoneObjects_.clear();
	if (script_)
	{
		for (const auto& o : script_->objects)
			if (const HorizonZone* z = classes_.horizon(o.className))
				zoneObjects_.push_back({o.pos[0], o.pos[1], *z, o.className});
		size_t done = 0, total = script_->objects.size();
		for (const auto& o : script_->objects)
		{
			report(0.13f + 0.86f * float(done++) / float(std::max<size_t>(total, 1)), N_("Modele obiektów"));
			PlacedObject p;
			p.src = o;
			p.cls = classes_.find(o.className);
			p.marker = markerKind(o.className, p.cls);
			if (p.cls) resolve(*p.cls, p.slot, p.animation);
			objects_.push_back(p);
		}
	}
	// Krasnale: klasa Zwerg nie ma class_defaultanim (model nadaje gra), więc bez tego z zapisu widać same czapki.
	for (int i = 0; i < int(objects_.size()); ++i)
		if (!objects_[size_t(i)].slot && Vfs::lower(objects_[size_t(i)].src.className) == "zwerg") setObjectModel(i, "mann.3db", "mann");
	// Lampy z class_light obiektów (pozycja: obiekt + przesunięcie, z·0,5 + 7; kolor int(c·31)·0,5).
	// Przypięcia z zapisu (czapki, narzędzia): rodzic po ID zapisanym w nazwie obiektu.
	{
		std::map<std::string, int> byName;
		for (size_t i = 0; i < objects_.size(); ++i) byName[objects_[i].src.name] = int(i);
		for (size_t i = 0; i < objects_.size(); ++i)
			if (objects_[i].src.linkParent >= 0)
				if (auto it = byName.find(std::to_string(objects_[i].src.linkParent)); it != byName.end())
					linkObject(int(i), it->second, objects_[i].src.linkPoint);
	}
	buildEmitters();
	initFluidSim();
	buildFluids();
	allLights_.clear();
	ownedLights_.clear();
	if (script_)
		for (const auto& o : script_->objects) pushClassLights(o.className, o.pos[0], o.pos[1], o.pos[2]);
	previewLamp_ = !scenarioMode_ && allLights_.empty();
	// Kadr na cały teren (x, y w blokach; z między dnem a przednią płaszczyzną).
	float w = pmp_ ? float(pmp_->width) : 4.f, h = pmp_ ? float(pmp_->height) : 4.f;
	center_ = glm::vec3(w * 0.5f * kBlock, h * 0.5f * kBlock, Heightfield::kFrontZ - 400.f);
	radius_ = (std::max(w, h) + 3.f) * kBlock * 0.55f; // margines: małe szablony nie wypełniają całego kadru
	report(1.f, N_("Gotowe"));
	return error_.empty();
}


// Macierz obiektu w układzie gry (jednostki modelu). Za Zimennik/Diggles-Unity (MIT),
// Docs/COORDINATE_SYSTEM.md §3.3–3.4, stałe z Wiggles.exe:
//  - głębokość obiektu: z_świata = z_skryptu·0,5 + 7 bloków (DAT_0073708c, DAT_00737520),
//  - obrót: M = Ry(−ry)·Rx(−rx)·Rz(−rz) (macierze gry są transpozycją standardowych).
// Odległość (bloki) od punktu (x, y; y w dół) do litej skały pod nim w mapie wysokości: podłoże cząsteczek
// „ziemnych” (krople, wióry). Bez terenu albo bez skały w zasięgu 8 bloków: 3 bloki (🔶 jak dotąd).
float MapScene::floorDrop(float x, float y) const
{
	if (!heights_) return 3.f;
	const Heightfield& h = *heights_;
	constexpr int S = Heightfield::kSamplesPerBlock;
	int sx = int(std::floor(x * S)), sy0 = int(std::floor(y * S));
	for (int sy = std::max(sy0, 0); sy < std::min(h.height, sy0 + 8 * S); ++sy)
		if (h.at(sx, sy) >= 15) return std::max(0.f, float(sy) / S - y);
	return 3.f;
}

bool MapScene::addEmitter(const ParticleSource& s, float x, float y, float zWorld)
{
	const ParticleType* t = particlesReady_ ? particleType(s.type) : nullptr;
	if (!t) return false;
	MapEmitter m;
	m.x = int(std::floor(x));
	m.y = int(std::floor(y));
	m.emitter.configure(*t, s, uint32_t(emitters_.size() + 1) * 2654435761u);
	m.emitter.setUsableCells(particles_.usableCells(t->tex, t->grid));
	m.emitter.setOrigin(x, y, zWorld);
	m.emitter.setFloorDrop(floorDrop(x, y));
	for (int i = 0; i < 36; ++i) m.emitter.step(); // 3 s przy 12 klatkach na sekundę, jak zrzuty z gry
	emitters_.push_back(std::move(m));
	return true;
}

// Emiter źródła s przypiętego do obiektu obj (pozycja z obiektu, podłoże z terenu).
void MapScene::configureEmitter(MapEmitter& m, const ParticleType& t, const ParticleSource& s, int obj)
{
	const float* p = objects_[size_t(obj)].src.pos;
	m.obj = obj;
	m.x = int(std::floor(p[0]));
	m.y = int(std::floor(p[1]));
	m.emitter.configure(t, s, uint32_t(emitters_.size() + 1) * 2654435761u);
	m.emitter.setUsableCells(particles_.usableCells(t.tex, t.grid));
	m.emitter.setOrigin(p[0], p[1], p[2] * 0.5f + 7.f);
	m.emitter.setFloorDrop(floorDrop(p[0], p[1] + s.rel[1]));
}

void MapScene::setObjectParticle(int obj, int slot, const ParticleSource& s)
{
	const ParticleType* t = particlesReady_ && obj >= 0 && obj < int(objects_.size()) ? particleType(s.type) : nullptr;
	if (!t) return;
	for (auto& e : emitters_)
		if (e.obj == obj && e.slot == slot)
		{
			configureEmitter(e, *t, s, obj);
			return;
		}
	MapEmitter m;
	m.slot = slot;
	configureEmitter(m, *t, s, obj);
	m.emitter.setActive(false); // nowe źródło ze skryptu stoi, dopóki set_particlesource go nie włączy
	emitters_.push_back(std::move(m));
}

int MapScene::objectParticles(int obj) const
{
	int n = 0;
	for (const auto& e : emitters_)
		if (e.obj == obj) n += e.emitter.alive();
	return n;
}

void MapScene::enableObjectParticle(int obj, int slot, bool on)
{
	for (auto& e : emitters_)
		if (e.obj == obj && e.slot == slot) e.emitter.setActive(on);
}

// Emitery cząsteczek obiektów (class_particle klasy, sloty 0, 1, …), rozgrzane o 4 s symulacji, żeby efekt już trwał.
void MapScene::buildEmitters()
{
	emitters_.clear();
	particleAcc_ = 0.f;
	if (!particlesReady_) return;
	for (int i = 0; i < int(objects_.size()); ++i)
	{
		const auto* list = classes_.particles(objects_[size_t(i)].src.className);
		if (!list) continue;
		for (size_t k = 0; k < list->size(); ++k)
		{
			const ParticleSource& s = (*list)[k];
			const ParticleType* t = particleType(s.type);
			if (!t) continue;
			MapEmitter m;
			m.slot = int(k);
			configureEmitter(m, *t, s, i);
			for (int j = 0; j < 48; ++j) m.emitter.step();
			emitters_.push_back(std::move(m));
		}
	}
}

// Siatka płynów; pola całkiem zakryte mgłą (czarne we wszystkich 4 narożnikach) gra pomija.
void MapScene::buildFluids()
{
	fluidCells_ = 0;
	fluids_.clear();
	if (!fluidsReady_ || !pmp_ || !blocks_.loaded() || fluidType_.empty()) return;
	int W = int(pmp_->width), H = int(pmp_->height);
	std::vector<bool> vis;
	if (fogMode_ != FogMode::Off && fog_.size() == size_t(W) * H)
	{
		vis.assign(fog_.size(), false);
		for (int y = 0; y < H; ++y)
			for (int x = 0; x < W; ++x)
				vis[size_t(y) * W + x] = fogAt(x, y) || fogAt(x + 1, y) || fogAt(x, y + 1) || fogAt(x + 1, y + 1);
	}
	fluidCells_ = fluids_.build(W, H, fluidType_, fluidFill_, pmp_->blocks, blocks_, vis.empty() ? nullptr : &vis);
}

// Symulacja płynów: stan z zapisu i źródła z obiektów Wasser/Sulfur/Lava (Editor_Set_Info).
// 🔶 Zapisy: źródła z zapisu (stan Tcl obiektów) jeszcze nie są czytane, więc woda tylko się wyrównuje.
void MapScene::initFluidSim()
{
	if (!pmp_ || !blocks_.loaded())
	{
		fluidSim_.init(0, 0, nullptr, nullptr);
		return;
	}
	int W = int(pmp_->width), H = int(pmp_->height);
	fluidSim_.init(W, H, &pmp_->blocks, &blocks_);
	if (!fluidType_.empty()) fluidSim_.setState(fluidType_, fluidFill_);
	for (const auto& o : objects_)
	{
		FluidSource s;
		if (!o.src.info.empty() && fluidSourceFromObject(o.src.className, o.src.info, o.src.pos, s)) fluidSim_.addSource(s);
	}
}

void MapScene::update(float dt)
{
	time_ += dt;
	if (!fluidSim_.empty())
	{
		fluidSim_.update(dt);
		if (fluidSim_.changed())
		{
			fluidType_ = fluidSim_.type();
			fluidFill_ = fluidSim_.fill();
			buildFluids();
		}
	}
	// Cząsteczki: 12 klatek symulacji na sekundę (❓ w grze co klatkę obrazu; za Diggles-Unity).
	particleAcc_ += dt;
	int steps = 0;
	while (particleAcc_ >= 1.f / 12.f && steps < 4)
	{
		for (auto& e : emitters_)
		{
			if (e.obj >= 0 && e.obj < int(objects_.size()))
			{ // obiekt mógł się przesunąć (skrypty, krasnale)
				const float* p = objects_[size_t(e.obj)].src.pos;
				e.emitter.setOrigin(p[0], p[1], p[2] * 0.5f + 7.f);
				e.x = int(std::floor(p[0])), e.y = int(std::floor(p[1]));
			}
			e.emitter.step();
		}
		particleAcc_ -= 1.f / 12.f;
		++steps;
	}
	if (steps == 4) particleAcc_ = 0.f;
}

std::string MapScene::objectModelObject(int i) const
{
	const auto& o = objects_[size_t(i)];
	return o.slot && o.cls ? o.cls->object : std::string();
}

bool MapScene::pushClassLights(const std::string& className, float x, float y, float zScript, int owner)
{
	const auto* ls = classes_.lights(className);
	if (!ls) return false;
	for (const auto& l : *ls)
	{
		if (owner >= 0) ownedLights_.push_back({owner, allLights_.size(), glm::vec3(l.offset[0], l.offset[1], l.offset[2])});
		GameLight g;
		g.posRadius = glm::vec4(x + l.offset[0], y + l.offset[1], (zScript + l.offset[2]) * 0.5f + 7.f, l.radius);
		g.color = glm::vec4(std::floor(l.color[0] * 31.f) * 0.5f, std::floor(l.color[1] * 31.f) * 0.5f,
		    std::floor(l.color[2] * 31.f) * 0.5f, 0.f);
		allLights_.push_back(g);
	}
	return true;
}

void MapScene::updateOwnedLights()
{
	for (const auto& ol : ownedLights_)
	{
		const float* p = objects_[size_t(ol.object)].src.pos;
		auto& g = allLights_[ol.light].posRadius;
		g.x = p[0] + ol.offset.x, g.y = p[1] + ol.offset.y, g.z = (p[2] + ol.offset.z) * 0.5f + 7.f;
	}
}

std::vector<glm::vec3> MapScene::ownedLightPositions(int i) const
{
	std::vector<glm::vec3> r;
	for (const auto& ol : ownedLights_)
		if (ol.object == i)
		{
			const float* p = objects_[size_t(i)].src.pos;
			r.push_back(glm::vec3(p[0] + ol.offset.x, p[1] + ol.offset.y, (p[2] + ol.offset.z) * 0.5f + 7.f));
		}
	return r;
}

bool MapScene::addClassLights(const std::string& className, float x, float y, float zScript)
{
	if (!pushClassLights(className, x, y, zScript)) return false;
	previewLamp_ = false;
	return true;
}

bool MapScene::setObjectModel(int i, const std::string& modelFile, const std::string& object)
{
	auto& o = objects_[size_t(i)];
	ClassModel& cm = gameModels_[Vfs::lower(modelFile + "/" + object)];
	cm.object = object;
	cm.animation = "standard";
	cm.modelHint = modelFile;
	cm.animMode = 2;
	o.cls = &cm;
	o.slot = nullptr;
	o.animation = -1;
	return resolve(cm, o.slot, o.animation);
}

void MapScene::setObjectPose(int i, float x, float y, float zScript, float rotY)
{
	auto& o = objects_[size_t(i)].src;
	o.pos[0] = x, o.pos[1] = y, o.pos[2] = zScript;
	o.rot[1] = rotY;
}

bool MapScene::playAnim(int i, const std::string& anim, bool loop)
{
	auto& o = objects_[size_t(i)];
	if (!o.slot || !o.cls) return false;
	std::string want = Vfs::lower(anim);
	for (const auto& ob : o.slot->model->objects)
	{
		if (Vfs::lower(ob.name) != Vfs::lower(o.cls->object)) continue;
		for (uint32_t a : ob.animations)
			if (Vfs::lower(o.slot->model->animations[a].name) == want)
			{
				if (o.animation != int(a) || o.animMode != (loop ? 2 : 1)) o.animStart = time_;
				o.animation = int(a);
				o.animMode = loop ? 2 : 1;
				return true;
			}
	}
	return false;
}

float MapScene::animLength(int i) const
{
	const auto& o = objects_[size_t(i)];
	if (!o.slot || o.animation < 0) return 0.f;
	return float(o.slot->model->animations[size_t(o.animation)].frames.size()) / 10.f;
}

float MapScene::animSeconds(int i, const std::string& anim) const
{
	const auto& o = objects_[size_t(i)];
	if (!o.slot || !o.cls) return 0.f;
	std::string want = Vfs::lower(anim);
	for (const auto& ob : o.slot->model->objects)
	{
		if (Vfs::lower(ob.name) != Vfs::lower(o.cls->object)) continue;
		for (uint32_t a : ob.animations)
			if (Vfs::lower(o.slot->model->animations[a].name) == want) return float(o.slot->model->animations[a].frames.size()) / 10.f;
	}
	return 0.f;
}

bool MapScene::animMotion(int i, const std::string& anim, Vec3f& move, Vec3f& rotate, int& frames) const
{
	const auto& o = objects_[size_t(i)];
	if (!o.slot || !o.cls) return false;
	std::string want = Vfs::lower(anim);
	for (const auto& ob : o.slot->model->objects)
	{
		if (Vfs::lower(ob.name) != Vfs::lower(o.cls->object)) continue;
		for (uint32_t a : ob.animations)
		{
			const auto& an = o.slot->model->animations[a];
			if (Vfs::lower(an.name) != want) continue;
			move = an.move, rotate = an.rotate, frames = int(an.frames.size());
			return true;
		}
	}
	return false;
}

int MapScene::markerKind(const std::string& className, const ClassModel* cls) const
{
	if (classes_.isInfo(className)) return Vfs::lower(className).rfind("info_coll", 0) == 0 ? 3 : 2;
	return cls && cls->editorOnly ? 1 : 0;
}

bool MapScene::objectShown(const PlacedObject& o) const
{
	if (!o.slot || o.animation < 0 || !o.visible) return false;
	if (!showDetail_[detailIndex(o.src.detail)]) return false;
	if (!showMarker_[o.marker]) return false;
	if (fogMode_ != FogMode::Off && !objectVisibleInFog(o)) return false;
	return true;
}

const Model3db* MapScene::objectModel(int i) const
{
	const auto& o = objects_[size_t(i)];
	return o.slot ? o.slot->model.get() : nullptr;
}

std::vector<std::string> MapScene::objectAnimations(int i) const
{
	std::vector<std::string> out;
	const auto& o = objects_[size_t(i)];
	if (!o.slot || !o.cls) return out;
	for (const auto& ob : o.slot->model->objects)
		if (Vfs::lower(ob.name) == Vfs::lower(o.cls->object))
			for (uint32_t a : ob.animations) out.push_back(o.slot->model->animations[a].name);
	return out;
}

std::string MapScene::objectAnimation(int i) const
{
	const auto& o = objects_[size_t(i)];
	return o.slot && o.animation >= 0 ? o.slot->model->animations[size_t(o.animation)].name : std::string();
}

int MapScene::pickObject(const glm::vec3& ro, const glm::vec3& rd) const
{
	int best = -1;
	float bestT = 1e30f;
	for (int i = 0; i < int(objects_.size()); ++i)
	{
		const auto& o = objects_[size_t(i)];
		if (!objectShown(o)) continue;
		int mesh = currentMesh(o);
		if (mesh < 0) continue;
		const Mesh3db& m = o.slot->model->meshes[size_t(mesh)];
		// promień do układu obiektu (obrót i przesunięcie z objectMatrix), test wstępny „slab” z bbox klatki
		glm::mat4 inv = glm::inverse(objectMatrix(o.src));
		glm::vec3 lo = glm::vec3(inv * glm::vec4(ro, 1.f)), ld = glm::vec3(inv * glm::vec4(rd, 0.f));
		float mn[3] = {m.bboxMin.x, m.bboxMin.y, m.bboxMin.z}, mx[3] = {m.bboxMax.x, m.bboxMax.y, m.bboxMax.z};
		float t0 = 0.f, t1 = bestT;
		bool hit = true;
		for (int k = 0; k < 3 && hit; ++k)
		{
			if (std::abs(ld[k]) < 1e-9f)
			{
				hit = lo[k] >= mn[k] && lo[k] <= mx[k];
				continue;
			}
			float a = (mn[k] - lo[k]) / ld[k], b = (mx[k] - lo[k]) / ld[k];
			if (a > b) std::swap(a, b);
			t0 = std::max(t0, a), t1 = std::min(t1, b);
			hit = t0 <= t1;
		}
		if (!hit) continue;
		// dokładnie: najbliższy trójkąt klatki (Möller–Trumbore), żeby duże obiekty (drzewa) nie zasłaniały
		// mniejszych za pustą przestrzenią swojego bbox
		const Model3db& md = *o.slot->model;
		for (const MeshLink& l : m.links)
		{
			const auto& pts = md.points[l.points];
			const auto& tri = md.triangles[l.triangles];
			for (size_t k = 0; k + 2 < tri.size(); k += 3)
			{
				if (tri[k] >= pts.size() || tri[k + 1] >= pts.size() || tri[k + 2] >= pts.size()) continue;
				Vec3f a0 = Model3db::dequant(pts[tri[k]]), a1 = Model3db::dequant(pts[tri[k + 1]]), a2 = Model3db::dequant(pts[tri[k + 2]]);
				glm::vec3 v0(a0.x, a0.y, a0.z), e1 = glm::vec3(a1.x, a1.y, a1.z) - v0, e2 = glm::vec3(a2.x, a2.y, a2.z) - v0;
				glm::vec3 pv = glm::cross(ld, e2);
				float det = glm::dot(e1, pv);

				if (std::abs(det) < 1e-9f) continue;
				float inv = 1.f / det;
				glm::vec3 tv = lo - v0;
				float u = glm::dot(tv, pv) * inv;
				if (u < 0.f || u > 1.f) continue;
				glm::vec3 qv = glm::cross(tv, e1);
				float w = glm::dot(ld, qv) * inv;
				if (w < 0.f || u + w > 1.f) continue;
				float t = glm::dot(e2, qv) * inv;
				if (t > 0.f && t < bestT) bestT = t, best = i;
			}
		}
	}
	return best;
}

bool MapScene::pickTerrain(const glm::vec3& ro, const glm::vec3& rd, int& tx, int& ty, glm::vec3* hit) const
{
	if (!heights_ || !pmp_ || std::abs(rd.z) < 1e-9f) return false;
	const Heightfield& h = *heights_;
	constexpr float S = Heightfield::kSampleUnits;
	// od przedniej płaszczyzny (z = 14,5 bloku) w głąb, krokiem ćwierci próbki; trafienie: punkt za powierzchnią
	float tStart = (Heightfield::kFrontZ + 1.f - ro.z) / rd.z, tEnd = (1300.f - ro.z) / rd.z;
	if (tStart > tEnd) std::swap(tStart, tEnd);
	tStart = std::max(tStart, 0.f);
	float step = S * 0.25f / std::max(glm::length(rd), 1e-6f);
	for (float t = tStart; t <= tEnd; t += step)
	{
		glm::vec3 p = ro + rd * t;
		int sx = int(std::floor(p.x / S)), sy = int(std::floor(p.y / S));
		if (sx < 0 || sy < 0 || sx >= h.width || sy >= h.height) continue;
		if (p.z <= Heightfield::depthZ(h.at(sx, sy)))
		{
			tx = sx / Heightfield::kSamplesPerBlock, ty = sy / Heightfield::kSamplesPerBlock;
			if (hit) *hit = p;
			return true;
		}
	}
	return false;
}

bool MapScene::currentFrames(const PlacedObject& o, uint32_t& a, uint32_t& b, float& t) const
{
	if (!o.slot || o.animation < 0) return false;
	const auto& an = o.slot->model->animations[size_t(o.animation)];
	if (an.frames.empty()) return false;
	int n = int(an.frames.size());
	// Tryb animacji klasy: 0 = ANIM_STILL (klatka 0), 1 = raz, 2 = pętla; animrandom = losowy start.
	int mode = o.animMode >= 0 ? o.animMode : o.cls ? o.cls->animMode
	                                                : 2;
	float f = 0.f;
	if (mode != 0)
	{
		f = (time_ - o.animStart) * 10.f; // tempo animacji gry, §11.6
		if (o.cls && o.cls->animRandom) f += float((unsigned(o.src.pos[0] * 131.f) * 2654435761u >> 8) % unsigned(n));
		if (mode == 1) f = std::min(f, float(n - 1));
	}
	int i = int(f) % n, j = mode == 2 ? (i + 1) % n : std::min(i + 1, n - 1);
	a = an.frames[size_t(i)], b = an.frames[size_t(j)], t = f - std::floor(f);
	return true;
}

int MapScene::currentMesh(const PlacedObject& o) const
{
	uint32_t a, b;
	float t;
	return currentFrames(o, a, b, t) ? int(a) : -1;
}

bool MapScene::linkedMatrix(int child, glm::mat4& out, int depth) const
{
	auto it = links_.find(child);
	if (it == links_.end() || depth > 4) return false;
	int parent = it->second.first, link = it->second.second;
	if (parent < 0 || parent >= int(objects_.size())) return false;
	const PlacedObject& p = objects_[size_t(parent)];
	glm::mat4 pm;
	if (!linkedMatrix(parent, pm, depth + 1)) pm = objectMatrix(p.src);
	uint32_t a, b;
	float t;
	Vec3f oa, ob, ea{0, 0, 0}, eb{0, 0, 0};
	if (!currentFrames(p, a, b, t) || !p.slot->model->linkOffset(a, link, oa, false))
	{
		out = pm * glm::translate(glm::mat4(1.f), glm::vec3(0.f, -kBlock, 0.f));
		return true;
	}
	if (!p.slot->model->linkOffset(b, link, ob, false)) ob = oa, b = a;
	p.slot->model->linkEuler(a, link, ea);
	if (!p.slot->model->linkEuler(b, link, eb)) eb = ea;
	glm::vec3 pos = glm::mix(glm::vec3(oa.x, oa.y, oa.z), glm::vec3(ob.x, ob.y, ob.z), t);
	// Orientacja punktu: kąty Eulera jak obrót obiektu w objectMatrix (Ry·Rx·Rz, kąty z minusem); klatki a, b
	// łączone sferycznie (kwaterniony), żeby obrót przez ±π nie wirował.
	auto quat = [](const Vec3f& e) {
		return glm::angleAxis(-e.y, glm::vec3(0, 1, 0)) * glm::angleAxis(-e.x, glm::vec3(1, 0, 0)) *
		    glm::angleAxis(-e.z, glm::vec3(0, 0, 1));
	};
	glm::quat q = glm::slerp(quat(ea), quat(eb), t);
	out = pm * glm::translate(glm::mat4(1.f), pos) * glm::mat4_cast(q);
	return true;
}

bool MapScene::linkEuler(int i, int link, Vec3f& out) const
{
	const auto& o = objects_[size_t(i)];
	int mesh = currentMesh(o);
	return mesh >= 0 && o.slot->model->linkEuler(uint32_t(mesh), link, out);
}

bool MapScene::linkOffset(int i, int link, float out[3], bool scriptZ) const
{
	const auto& o = objects_[size_t(i)];
	int mesh = currentMesh(o);
	Vec3f v;
	if (mesh < 0 || !o.slot->model->linkOffset(uint32_t(mesh), link, v, scriptZ)) return false;
	// Obrót jak w objectMatrix (oś pionowa, kąt −ry), potem jednostki modelu -> bloki; z świata -> skala skryptu (×2).
	float c = std::cos(-o.src.rot[1]), s = std::sin(-o.src.rot[1]);
	float x = c * v.x + s * v.z, z = -s * v.x + c * v.z;
	out[0] = x / kBlock, out[1] = v.y / kBlock, out[2] = 2.f * z / kBlock;
	return true;
}

bool MapScene::linkEuler(int i, int link, float out[3]) const
{
	const auto& o = objects_[size_t(i)];
	int mesh = currentMesh(o);
	Vec3f v;
	if (mesh < 0 || !o.slot->model->linkEuler(uint32_t(mesh), link, v)) return false;
	out[0] = v.x, out[1] = v.y, out[2] = v.z;
	return true;
}

void MapScene::linkObject(int child, int parent, int link)
{
	if (parent < 0) links_.erase(child);
	else links_[child] = {parent, link};
}

void MapScene::applyLinks()
{
	for (auto& [child, pl] : links_)
	{
		if (child >= int(objects_.size()) || pl.first >= int(objects_.size())) continue;
		// Gra osadza początek modelu dziecka w punkcie rodzica liczonym jak wierzchołki (bez z×2 z get_linkpos)
		// i obraca go orientacją punktu (głowa przechyla czapkę). Diggles-Unity HAT_LINK_SYSTEM §5.
		float off[3];
		const auto& p = objects_[size_t(pl.first)].src;
		auto& c = objects_[size_t(child)].src;
		Vec3f e{0, 0, 0};
		if (!linkOffset(pl.first, pl.second, off, false)) off[0] = off[2] = 0, off[1] = -1.f;
		else linkEuler(pl.first, pl.second, e);
		c.pos[0] = p.pos[0] + off[0], c.pos[1] = p.pos[1] + off[1], c.pos[2] = p.pos[2] + off[2];
		// objectMatrix: Ry(−ry)·Rx(−rx)·Rz(−rz); rodzic obraca tylko wokół osi pionowej, więc kąt Y się sumuje.
		c.rot[0] = p.rot[0] + e.x, c.rot[1] = p.rot[1] + e.y, c.rot[2] = p.rot[2] + e.z;
	}
}

int MapScene::addObject(const std::string& className, float x, float y, float zScript)
{
	PlacedObject p;
	p.src.className = className;
	p.src.pos[0] = x, p.src.pos[1] = y, p.src.pos[2] = zScript;
	p.cls = classes_.find(className);
	p.marker = markerKind(className, p.cls);
	if (p.cls) resolve(*p.cls, p.slot, p.animation);
	objects_.push_back(p);
	int i = int(objects_.size()) - 1;
	pushClassLights(className, x, y, zScript, i); // światła klasy idą za obiektem (lampy, ogniska ze skryptów)
	return i;
}

void MapScene::setBlock(int x, int y, uint32_t block, uint8_t type)
{
	if (!pmp_ || x < 0 || y < 0 || x >= int(pmp_->width) || y >= int(pmp_->height)) return;
	size_t i = size_t(y) * pmp_->width + x;
	pmp_->blocks[i] = block;
	pmp_->types[i] = type;
	if (l2m_ && l2m_->width == pmp_->width && i < l2m_->values.size()) l2m_->values[i] = 0; // bez starego detalu (mech, trawa)
}

bool MapScene::tileTemplated(int wx, int wy) const
{
	if (!pmp_) return false;
	int x = wx - worldOrigin_[0], y = wy - worldOrigin_[1];
	if (x < 0 || y < 0 || x >= int(pmp_->width) || y >= int(pmp_->height)) return false;
	size_t i = size_t(y) * pmp_->width + x;
	return hasTpl_.size() == pmp_->blocks.size() ? hasTpl_[i] != 0 : pmp_->blocks[i] != 0;
}

bool MapScene::stamp(const Pmp* tpl, const L2m* layer, int wx, int wy, int rect[4])
{
	if (!pmp_) return false;
	if (!l2m_ || l2m_->width != pmp_->width || l2m_->height != pmp_->height)
	{
		l2m_ = L2m{};
		l2m_->width = pmp_->width, l2m_->height = pmp_->height;
		l2m_->values.assign(size_t(pmp_->width) * pmp_->height, 0);
	}
	static const Pmp kNone;
	int ox = wx - worldOrigin_[0], oy = wy - worldOrigin_[1];
	blitTemplate(*pmp_, *l2m_, tpl ? *tpl : kNone, layer, ox, oy);
	if (tpl && hasTpl_.size() == pmp_->types.size())
		for (int y = std::max(oy, 0); y < std::min(oy + int(tpl->height), int(pmp_->height)); ++y)
			for (int x = std::max(ox, 0); x < std::min(ox + int(tpl->width), int(pmp_->width)); ++x) hasTpl_[size_t(y) * pmp_->width + x] = 1;
	int w = tpl ? int(tpl->width) : layer ? int(layer->width)
	                                      : 0,
	    h = tpl ? int(tpl->height) : layer ? int(layer->height)
	                                       : 0;
	rect[0] = std::max(ox, 0), rect[1] = std::max(oy, 0);
	rect[2] = std::min(ox + w, int(pmp_->width)), rect[3] = std::min(oy + h, int(pmp_->height));
	return rect[2] > rect[0] && rect[3] > rect[1];
}

void MapScene::rebuildTerrain()
{
	if (!pmp_ || !blocks_.loaded()) return;
	heights_ = buildHeightfield(*pmp_, blocks_);
	terrain_.build(buildTerrainMesh(*pmp_, blocks_, *sphere_), *textures_, l2m_ ? &*l2m_ : nullptr, world_);
	buildFluids();
}

glm::mat4 MapScene::objectMatrix(const TemplateObject& o)
{
	glm::mat4 m = glm::translate(glm::mat4(1.f), glm::vec3(o.pos[0], o.pos[1], o.pos[2] * 0.5f + 7.f) * kBlock);
	m = glm::rotate(m, -o.rot[1], glm::vec3(0, 1, 0));
	m = glm::rotate(m, -o.rot[0], glm::vec3(1, 0, 0));
	m = glm::rotate(m, -o.rot[2], glm::vec3(0, 0, 1));
	return m;
}

void MapScene::updateZone(const glm::vec3& focus)
{
	float fx = focus.x / kBlock, fy = focus.y / kBlock, best = 1e30f;
	zone_.reset();
	zoneClass_.clear();
	for (const auto& z : zoneObjects_)
	{
		float d = std::hypot(z.x - fx, z.y - fy);
		if (d < z.zone.size && d < best)
		{
			best = d;
			zone_ = z.zone;
			zoneClass_ = z.className;
		}
	}
}

float MapScene::baseLight(float x, float y) const
{
	if (!pmp_) return 128.f;
	int W = int(pmp_->width), H = int(pmp_->height);
	auto zone = [&](int ix, int iy) {
		ix = std::clamp(ix, 0, W - 1);
		iy = std::clamp(iy, 0, H - 1);
		return float(pmp_->layer2[size_t(iy) * W + ix]);
	};
	int x0 = int(std::floor(x)), y0 = int(std::floor(y));
	float tx = x - float(x0), ty = y - float(y0);
	float z = (1 - tx) * (1 - ty) * zone(x0, y0) + tx * (1 - ty) * zone(x0 + 1, y0) + (1 - tx) * ty * zone(x0, y0 + 1) +
	    tx * ty * zone(x0 + 1, y0 + 1);
	return 16.f * z + 64.f;
}

void MapScene::render(const glm::mat4& viewProj, const glm::mat4& world, const ModelDrawOptions& baseOpt,
    float gameZOffset, const glm::vec3& focus)
{
	applyLinks(); // przypięte obiekty (czapki, narzędzia) za punktem rodzica w tej klatce
	updateOwnedLights();
	// Lampy: do kmaxGameLights najbliższych środka kadru.
	std::vector<GameLight> lights;
	if (gameLighting_)
	{
		lights = allLights_;
		glm::vec3 f = focus / kBlock;
		if (lights.size() > size_t(kMaxGameLights))
		{
			std::partial_sort(lights.begin(), lights.begin() + kMaxGameLights, lights.end(),
			    [&](const GameLight& a, const GameLight& b) {
				    glm::vec2 da = glm::vec2(a.posRadius) - glm::vec2(f), db = glm::vec2(b.posRadius) - glm::vec2(f);
				    return glm::dot(da, da) < glm::dot(db, db);
			    });
			lights.resize(kMaxGameLights);
		}
		// Lampa podglądu (nie z gry): stoi przed przednią płaszczyzną nad ogniskiem kamery, z kolorem
		// c = 11 (jak class_light {0,7 0,7 0,7}) i promieniem 25 pól. Domyślnie włączona dla szablonów
		// bez własnych świateł (proste korytarze są w grze czarne bez lamp), dla scenariuszy wyłączona.
		if (previewLamp_)
		{
			GameLight l;
			l.posRadius = glm::vec4(f.x, f.y, 17.f, 25.f);
			l.color = glm::vec4(11.f, 11.f, 11.f, 0.f);
			lights.push_back(l);
		}
	}
	const std::vector<GameLight>* gl = gameLighting_ ? &lights : nullptr;
	updateFog(focus);
	const GameFog* fog = fogMode_ != FogMode::Off ? &gameFog_ : nullptr;
	uploadScene(*dev_, gl, gl ? fog : nullptr); // lampy i mgła klatki (wiązanie 1), wspólne dla terenu i modeli
	if (showPanorama_ && hasOpenAir_ && panoramaReady_ && PanoramaRenderer::gateOpen(focus.y / kBlock, groundRow_))
	{
		panorama_.draw(glm::scale(glm::mat4(1.f), glm::vec3(1.f / kBlock)) * glm::inverse(viewProj * world));
		if (showCarpet_)
			panorama_.drawCarpet(viewProj * world * glm::scale(glm::mat4(1.f), glm::vec3(kBlock)), focus.x / kBlock, groundRow_);
	}
	if (showTerrain_) terrain_.draw(viewProj, world, baseOpt.lightDir, baseOpt.ambient, baseOpt.wireframe, gl != nullptr);
	if (!showObjects_) return;
	ModelDrawOptions opt = baseOpt;
	opt.gameLights = gl;
	if (const HorizonZone* z = zone())
	{
		opt.horizon = glm::vec4(z->r, z->g, z->b, z->depth);
		opt.horizonZOffset = gameZOffset;
	}
	// Obiekty do faz rysowania: najpierw wszystkie nieprzezroczyste, potem miękkie brzegi i blask od najdalszego.
	struct Draw
	{
		const PlacedObject* o;
		uint32_t a, b;
		float t;
		glm::mat4 gm;
		float base;
	};
	std::vector<Draw> draws;
	draws.reserve(objects_.size());
	for (const auto& o : objects_)
	{
		if (!objectShown(o)) continue;
		uint32_t fa, fb;
		float ft;
		if (!currentFrames(o, fa, fb, ft)) continue;
		glm::mat4 gm;
		if (!linkedMatrix(int(&o - objects_.data()), gm)) gm = objectMatrix(o.src); // przypięte: za punktem rodzica
		draws.push_back({&o, fa, fb, ft, gm, baseLight(o.src.pos[0], o.src.pos[1])});
	}
	// Cienie od lamp gry na podłodze (docs/silnik/18_cienie.md), przed modelami: obiekt zasłania swój cień.
	if (shadowsReady_ && !allLights_.empty())
	{
		std::vector<ShadowRenderer::Caster> casters;
		for (const auto& d : draws)
			if (d.o->slot && d.o->slot->model) casters.push_back({d.o->slot->model.get(), d.a, d.gm, d.o->src.pos[0], d.o->src.pos[1]});
		shadows_.draw(casters, allLights_, viewProj * world);
	}
	auto drawAll = [&](ModelDrawOptions::Phase phase) {
		opt.phase = phase;
		for (const auto& d : draws)
		{
			opt.gameModel = d.gm;
			opt.base = d.base;
			int i = int(d.o - objects_.data());
			opt.highlight = i == selectedObject ? 0.4f : i == hoverObject ? 0.22f
			                                                              : 0.f;
			d.o->slot->renderer->drawFrames(d.a, d.b, d.t, viewProj, world * d.gm, opt);
		}
	};
	drawAll(ModelDrawOptions::Phase::Opaque);
	// Kamera gry patrzy w stronę −z (przednia płaszczyzna z = 14,5 jest najbliżej): od najmniejszego z.
	std::stable_sort(draws.begin(), draws.end(), [](const Draw& x, const Draw& y) { return x.gm[3][2] < y.gm[3][2]; });
	drawAll(ModelDrawOptions::Phase::Transparent);
	if (showFluids_) fluids_.draw(viewProj * world, time_);
	// Cząsteczki po modelach (addytywne, bez zapisu głębi). W nieodkrytym terenie tylko na polach z bajtem ≥ 0x81 (§10.5).
	if (showParticles_)
	{
		glm::mat4 gameToClip = viewProj * world;
		for (const auto& e : emitters_)
		{
			if (fog && fogAt(e.x, e.y) < 0x81) continue;
			sprites_.clear();
			e.emitter.collect(sprites_, kParticleSizeScale);
			particles_.draw(sprites_, *e.emitter.type(), gameToClip);
		}
	}
}

uint8_t MapScene::fogAt(int x, int y) const
{
	if (!pmp_ || fog_.empty()) return 0xFF;
	x = std::clamp(x, 0, int(pmp_->width) - 1);
	y = std::clamp(y, 0, int(pmp_->height) - 1);
	return fog_[size_t(y) * pmp_->width + x];
}

// Stan mgły według trybu podglądu i tekstura narożników (docs/silnik/10_nieodkryty_teren.md §10.5).
void MapScene::updateFog(const glm::vec3& focus)
{
	if (fogMode_ == FogMode::Off || !pmp_) return;
	int W = int(pmp_->width), H = int(pmp_->height);
	int fx = int(std::floor(focus.x / kBlock)), fy = int(std::floor(focus.y / kBlock));
	bool view = fogMode_ == FogMode::ViewGrey || fogMode_ == FogMode::ViewBlack;
	if (!fogDirty_ && (!view || (fx == fogFocus_[0] && fy == fogFocus_[1]))) return;
	fogDirty_ = false;
	fogFocus_[0] = fx;
	fogFocus_[1] = fy;
	uint8_t rest = fogMode_ == FogMode::ViewBlack ? 0 : 1;
	fog_.assign(size_t(W) * H, rest);
	if (fogMode_ == FogMode::FromSave && saveFog_.size() == fog_.size()) fog_ = saveFog_;
	else
		for (int y = 0; y < H; ++y)
			for (int x = 0; x < W; ++x)
			{
				bool inView = view && std::abs(x - fx) <= fogRadius_ && std::abs(y - fy) <= fogRadius_;
				if (inView) fog_[size_t(y) * W + x] = uint8_t(fogByte_);
				if (y >= 5 && y < fowBegin_) fog_[size_t(y) * W + x] = 0xFF; // set_fow_begin: niebo zawsze widoczne
			}
	// Dekodowanie bajtu na narożnik: szarość g, czerń k, skala lamp S (tabela w §10.5).
	std::vector<uint8_t> rgb(size_t(W) * H * 4, 255);
	for (size_t i = 0; i < fog_.size(); ++i)
	{
		int b = fog_[i];
		int g = 0, k = 255, s = 255;
		if (b == 0) g = 0, k = 0, s = 0;
		else if (b < 0x80) g = 255, s = 0;
		else if (b < 0xE3)
		{
			int u = std::min(128, int(float(b - 0x80) * 1.3f));
			g = std::max(0, 255 - 2 * u);
			s = std::min(255, u * 2);
		}
		else g = 0;
		rgb[i * 4] = uint8_t(g), rgb[i * 4 + 1] = uint8_t(k), rgb[i * 4 + 2] = uint8_t(s);
	}
	rhi::TextureDesc d;
	d.width = W;
	d.height = H;
	d.mipmaps = false;
	d.repeatU = d.repeatV = false;
	if (!gameFog_.texture) gameFog_.texture = dev_->createTexture(d, rgb.data());
	else dev_->updateTexture(gameFog_.texture, d, rgb.data());
	gameFog_.width = W;
	gameFog_.height = H;
	buildFluids();
}

// Obiekt jest rysowany, gdy największy bajt mgły z 7 punktów wokół niego (środek, 4 narożniki,
// środki boków pionowych prostokąta ±1 pole) wynosi ≥ 0x80; klasy viewinfog wystarczy ≥ 1 (§10.5).
bool MapScene::objectVisibleInFog(const PlacedObject& o) const
{
	int x = int(std::floor(o.src.pos[0])), y = int(std::floor(o.src.pos[1]));
	const int pts[7][2] = {{x, y}, {x - 1, y - 1}, {x + 1, y - 1}, {x - 1, y + 1}, {x + 1, y + 1}, {x - 1, y}, {x + 1, y}};
	int best = 0;
	for (auto& p : pts) best = std::max(best, int(fogAt(p[0], p[1])));
	bool ghost = o.cls && o.cls->viewInFog;
	return best >= (ghost ? 1 : 0x80);
}

int MapScene::detailIndex(const std::string& d)
{
	if (d == "high") return 0;
	if (d == "medium") return 1;
	if (d == "low") return 2;
	if (d == "off") return 3;
	return 4;
}

} // namespace ow
