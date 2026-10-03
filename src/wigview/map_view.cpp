#include "wigview/map_view.h"

#include "lib/gfx/texture_cache.h"
#include "lib/map_world.h"
#include "lib/vfs.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>

using namespace ow;

namespace {

bool endsWithI(const std::string& s, const std::string& suf) {
	return s.size() >= suf.size() && Vfs::lower(s.substr(s.size() - suf.size())) == suf;
}

bool passFilter(const char* filter, const std::string& s) {
	return !*filter || Vfs::lower(s).find(Vfs::lower(filter)) != std::string::npos;
}

int detailIndex(const std::string& d) {
	if (d == "high") return 0;
	if (d == "medium") return 1;
	if (d == "low") return 2;
	if (d == "off") return 3;
	return 4;
}

// Kolory typów pól .PMP (znaczenie typów jeszcze niepewne).
ImU32 typeColor(uint8_t t) {
	static const ImU32 pal[] = {IM_COL32(40, 40, 48, 255),   IM_COL32(120, 90, 60, 255),  IM_COL32(70, 130, 180, 255),
	                            IM_COL32(200, 160, 60, 255), IM_COL32(90, 150, 90, 255),  IM_COL32(170, 90, 160, 255),
	                            IM_COL32(200, 100, 80, 255), IM_COL32(100, 180, 180, 255), IM_COL32(220, 220, 120, 255)};
	return pal[t % (sizeof pal / sizeof pal[0])];
}

ImU32 valueColor(unsigned v, unsigned max) {
	float f = max ? float(v) / float(max) : 0.f;
	return IM_COL32(int(30 + 200 * f), int(60 + 120 * f), int(90 + 60 * (1 - f)), 255);
}

} // namespace

void MapView::init(const Vfs* vfs, TextureCache* textures, const std::vector<Vec3f>* sphere) {
	vfs_ = vfs;
	textures_ = textures;
	sphere_ = sphere;
	classes_.load(*vfs);
	std::string err;
	if (!terrain_.init(err)) blocksError_ = "shader terenu: " + err;
	try {
		blocks_.load(*vfs);
	} catch (const std::exception& e) {
		blocksError_ = e.what();
	}
	scenarios_.clear();
	for (auto& f : vfs->list("Data/Gamesave"))
		if (endsWithI(f, ".tcl")) scenarios_.push_back(f);
	files_.clear();
	for (auto& f : vfs->list("Data/Templates"))
		if (endsWithI(f, ".tcl")) files_.push_back(f);
}

MapView::ModelSlot* MapView::model(const std::string& file) {
	auto key = Vfs::lower(file);
	auto it = models_.find(key);
	if (it != models_.end()) return it->second.failed ? nullptr : &it->second;
	ModelSlot& s = models_[key];
	auto data = vfs_->read("Data/3db/" + file);
	try {
		if (!data) throw std::runtime_error("brak pliku");
		s.model = std::make_unique<Model3db>(parseModel3db(*data));
		s.renderer = std::make_unique<ModelRenderer>();
		std::string err;
		if (!s.renderer->init(err)) throw std::runtime_error(err);
		s.renderer->setSphere(*sphere_);
		s.renderer->setModel(s.model.get(), textures_);
		for (size_t i = 0; i < s.model->objects.size(); ++i)
			objectIndex_.emplace(Vfs::lower(s.model->objects[i].name), std::make_pair(key, int(i)));
	} catch (const std::exception& e) {
		std::fprintf(stderr, "model %s: %s\n", file.c_str(), e.what());
		s.failed = true;
		return nullptr;
	}
	return &s;
}

bool MapView::resolve(const ClassModel& cm, ModelSlot*& slot, int& animation) {
	auto findIn = [&](ModelSlot* s) -> bool {
		if (!s) return false;
		for (const auto& o : s->model->objects) {
			if (Vfs::lower(o.name) != Vfs::lower(cm.object)) continue;
			for (uint32_t a : o.animations)
				if (Vfs::lower(s->model->animations[a].name) == Vfs::lower(cm.animation)) {
					slot = s;
					animation = int(a);
					return true;
				}
			if (!o.animations.empty()) {
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

bool MapView::loadTemplate(const std::string& file) {
	file_ = file;
	scenarioMode_ = false;
	Scenario sc;
	sc.placements.push_back({file, 0, 0});
	std::string world = Vfs::lower(file.substr(0, file.find('_')));
	return finishLoad(sc, 0, world);
}

bool MapView::loadScenario(const std::string& file) {
	file_ = file;
	scenarioMode_ = true;
	Scenario sc = parseScenario(*vfs_, "Data/Gamesave/" + file);
	scenarioInfo_.clear();
	if (sc.placements.empty() && sc.usesGenerator)
		scenarioInfo_ = "Świat generowany proceduralnie (StoryMgr load_level/load_sandbox) - jeszcze nieobsługiwany.";
	// Świat ze szablonu o największej liczbie wystąpień (urw, kris, lava, swf).
	std::map<std::string, int> worlds;
	for (auto& p : sc.placements) worlds[Vfs::lower(p.script.substr(0, p.script.find('_')))]++;
	std::string world = "urw";
	int best = 0;
	for (auto& [k, n] : worlds)
		if (n > best && (k == "urw" || k == "kris" || k == "lava" || k == "swf")) best = n, world = k;
	return finishLoad(sc, 2, world);
}

bool MapView::finishLoad(const Scenario& sc, int margin, std::string world) {
	error_.clear();
	script_.reset();
	pmp_.reset();
	l2m_.reset();
	objects_.clear();
	heights_.reset();
	terrain_.clear();
	placed_ = 0;
	if (world != "urw" && world != "kris" && world != "lava" && world != "swf") world = "urw";

	BuiltWorld bw = buildWorld(*vfs_, sc, margin);
	for (auto& e : bw.errors) error_ += (error_.empty() ? "" : "; ") + e;
	if (!bw.placedScripts.empty()) {
		placed_ = int(bw.placedScripts.size());
		worldOrigin_[0] = bw.originX;
		worldOrigin_[1] = bw.originY;
		pmp_ = std::move(bw.grid);
		l2m_ = std::move(bw.layer);
		script_ = TemplateScript{};
		script_->objects = std::move(bw.objects);
		if (!scenarioMode_) {
			// Nazwy plików szablonu do wyświetlenia.
			if (auto text = vfs_->read("Data/Templates/" + file_)) {
				auto t = parseTemplateScript(std::string(text->begin(), text->end()));
				script_->pmpFile = t.pmpFile;
				script_->l2mFile = t.l2mFile;
			}
		}
	} else if (error_.empty() && scenarioInfo_.empty())
		error_ = "brak szablonów do złożenia";

	if (pmp_ && blocks_.loaded()) {
		heights_ = buildHeightfield(*pmp_, blocks_);
		terrain_.build(buildTerrainMesh(*pmp_, blocks_, *sphere_), *textures_, l2m_ ? &*l2m_ : nullptr, world);
	}
	zone_.reset();
	zoneClass_.clear();
	if (script_) {
		// Strefa horyzontu: obiekt Info_Fog_* najbliższy środkowi (w scenariuszu: środka kadru).
		float best = 1e30f;
		float cx = (pmp_ ? float(pmp_->width) : 4.f) * 0.5f, cy = (pmp_ ? float(pmp_->height) : 4.f) * 0.5f;
		for (const auto& o : script_->objects)
			if (const HorizonZone* z = classes_.horizon(o.className)) {
				float d = (o.pos[0] - cx) * (o.pos[0] - cx) + (o.pos[1] - cy) * (o.pos[1] - cy);
				if (d < best) {
					best = d;
					zone_ = *z;
					zoneClass_ = o.className;
				}
			}
		for (const auto& o : script_->objects) {
			PlacedObject p;
			p.src = o;
			p.cls = classes_.find(o.className);
			if (p.cls) resolve(*p.cls, p.slot, p.animation);
			objects_.push_back(p);
		}
	}
	// Kadr na cały teren (x, y w blokach; z między dnem a przednią płaszczyzną).
	float w = pmp_ ? float(pmp_->width) : 4.f, h = pmp_ ? float(pmp_->height) : 4.f;
	center_ = glm::vec3(w * 0.5f * kBlock, h * 0.5f * kBlock, Heightfield::kFrontZ - 400.f);
	radius_ = (std::max(w, h) + 3.f) * kBlock * 0.55f; // margines: małe szablony nie wypełniają całego kadru
	return error_.empty();
}

void MapView::step(int d) {
	const auto& list = listScenarios_ ? scenarios_ : files_;
	std::vector<int> vis;
	for (int i = 0; i < int(list.size()); ++i)
		if (passFilter(filter_, list[size_t(i)])) vis.push_back(i);
	if (vis.empty()) return;
	int pos = -1;
	for (size_t k = 0; k < vis.size(); ++k)
		if (list[size_t(vis[k])] == file_) pos = int(k);
	if (pos < 0) pos = d > 0 ? -1 : 0;
	pos = (pos + d + int(vis.size())) % int(vis.size());
	if (listScenarios_) loadScenario(list[size_t(vis[size_t(pos)])]);
	else loadTemplate(list[size_t(vis[size_t(pos)])]);
	scrollToSel_ = true;
}

// Macierz obiektu w układzie gry (jednostki modelu). Za Zimennik/Diggles-Unity (MIT),
// Docs/COORDINATE_SYSTEM.md §3.3–3.4, stałe z Wiggles.exe:
//  - głębokość obiektu: z_świata = z_skryptu·0,5 + 7 bloków (DAT_0073708c, DAT_00737520),
//  - obrót: M = Ry(−ry)·Rx(−rx)·Rz(−rz) (macierze gry są transpozycją standardowych).
glm::mat4 MapView::objectMatrix(const TemplateObject& o) {
	glm::mat4 m = glm::translate(glm::mat4(1.f), glm::vec3(o.pos[0], o.pos[1], o.pos[2] * 0.5f + 7.f) * kBlock);
	m = glm::rotate(m, -o.rot[1], glm::vec3(0, 1, 0));
	m = glm::rotate(m, -o.rot[0], glm::vec3(1, 0, 0));
	m = glm::rotate(m, -o.rot[2], glm::vec3(0, 0, 1));
	return m;
}

void MapView::render(const glm::mat4& viewProj, const glm::mat4& world, const ModelDrawOptions& baseOpt,
                     float gameZOffset) {
	if (showTerrain_) terrain_.draw(viewProj, world, baseOpt.lightDir, baseOpt.ambient, baseOpt.wireframe);
	if (!showObjects_) return;
	ModelDrawOptions opt = baseOpt;
	if (const HorizonZone* z = zone()) {
		opt.horizon = glm::vec4(z->r, z->g, z->b, z->depth);
		opt.horizonZOffset = gameZOffset;
	}
	const float fps = 20.f;
	for (const auto& o : objects_) {
		if (!o.slot || o.animation < 0) continue;
		if (!showDetail_[detailIndex(o.src.detail)]) continue;
		if (o.cls && o.cls->editorOnly && !showEditorOnly_) continue;
		const auto& an = o.slot->model->animations[size_t(o.animation)];
		if (an.frames.empty()) continue;
		float f = time_ * fps;
		int n = int(an.frames.size());
		int i = int(f) % n, j = (i + 1) % n;
		glm::mat4 m = world * objectMatrix(o.src);
		o.slot->renderer->drawFrames(an.frames[size_t(i)], an.frames[size_t(j)], f - std::floor(f), viewProj, m, opt);
	}
}

void MapView::uiGrid() {
	if (!pmp_) {
		ImGui::TextDisabled("Brak siatki .PMP");
		return;
	}
	const char* layers[] = {"Typ pola", "ID bloku", "Warstwa 2 (PMP)", "Warstwa .l2m", "Głębokość (BlockDB)"};
	ImGui::SetNextItemWidth(170);
	ImGui::Combo("Warstwa", &gridLayer_, layers, 5);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120);
	ImGui::SliderFloat("Zoom", &cellPx_, 6.f, 48.f, "%.0f px");

	const int W = int(pmp_->width), H = int(pmp_->height);
	ImGui::BeginChild("siatka", ImVec2(0, std::min(420.f, H * cellPx_ + 16)), ImGuiChildFlags_Borders,
	                  ImGuiWindowFlags_HorizontalScrollbar);
	ImVec2 origin = ImGui::GetCursorScreenPos();
	ImDrawList* dl = ImGui::GetWindowDrawList();
	unsigned maxBlock = 1, maxL2 = 1, maxL2m = 1;
	for (auto b : pmp_->blocks) maxBlock = std::max(maxBlock, b);
	for (auto v : pmp_->layer2) maxL2 = std::max<unsigned>(maxL2, v);
	if (l2m_)
		for (auto v : l2m_->values) maxL2m = std::max<unsigned>(maxL2m, v);

	for (int y = 0; y < H; ++y)
		for (int x = 0; x < W; ++x) {
			size_t i = size_t(y) * W + x;
			ImU32 c;
			switch (gridLayer_) {
			case 1: c = pmp_->blocks[i] ? valueColor(pmp_->blocks[i] % 97, 96) : IM_COL32(20, 20, 24, 255); break;
			case 2: c = valueColor(pmp_->layer2[i], maxL2); break;
			case 3:
				c = (l2m_ && l2m_->width == pmp_->width && i < l2m_->values.size()) ? valueColor(l2m_->values[i], maxL2m)
				                                                                     : IM_COL32(20, 20, 24, 255);
				break;
			case 4: c = 0; break; // rysowane niżej w rozdzielczości próbek
			default: c = typeColor(pmp_->types[i]);
			}
			ImVec2 a(origin.x + x * cellPx_, origin.y + y * cellPx_);
			if (gridLayer_ == 4 && heights_) {
				float s = cellPx_ / Heightfield::kSamplesPerBlock;
				for (int sy = 0; sy < Heightfield::kSamplesPerBlock; ++sy)
					for (int sx = 0; sx < Heightfield::kSamplesPerBlock; ++sx) {
						int v = heights_->at(x * 8 + sx, y * 8 + sy) * 17;
						ImVec2 p(a.x + sx * s, a.y + sy * s);
						dl->AddRectFilled(p, ImVec2(p.x + s, p.y + s), IM_COL32(v, v, v, 255));
					}
			} else
				dl->AddRectFilled(a, ImVec2(a.x + cellPx_ - 1, a.y + cellPx_ - 1), c);
		}
	// Obiekty jako kropki (pozycja w blokach).
	for (const auto& o : objects_) {
		if (!showDetail_[detailIndex(o.src.detail)]) continue;
		if (!o.slot && !showUnresolved_) continue;
		ImVec2 p(origin.x + o.src.pos[0] * cellPx_, origin.y + o.src.pos[1] * cellPx_);
		dl->AddCircleFilled(p, std::max(2.f, cellPx_ * 0.12f), o.slot ? IM_COL32(255, 255, 255, 220) : IM_COL32(255, 80, 80, 220));
	}
	ImGui::InvisibleButton("siatka_btn", ImVec2(W * cellPx_, H * cellPx_));
	if (ImGui::IsItemHovered()) {
		ImVec2 m = ImGui::GetMousePos();
		int x = int((m.x - origin.x) / cellPx_), y = int((m.y - origin.y) / cellPx_);
		if (x >= 0 && y >= 0 && x < W && y < H) {
			size_t i = size_t(y) * W + x;
			ImGui::BeginTooltip();
			ImGui::Text("blok (%d, %d)", x, y);
			ImGui::Text("typ %u  ID bloku %u  warstwa2 %u", pmp_->types[i], pmp_->blocks[i], pmp_->layer2[i]);
			if (l2m_ && l2m_->width == pmp_->width && i < l2m_->values.size()) ImGui::Text("l2m 0x%04X", l2m_->values[i]);
			int shown = 0;
			for (const auto& o : objects_)
				if (int(o.src.pos[0]) == x && int(o.src.pos[1]) == y && shown++ < 8)
					ImGui::Text("%s %s", o.src.className.c_str(), o.slot ? "" : "(bez modelu)");
			ImGui::EndTooltip();
		}
	}
	ImGui::EndChild();
}

bool MapView::ui() {
	bool recenter = false;
	ImGui::BeginChild("szablony", ImVec2(200, 0), ImGuiChildFlags_Borders);
	if (ImGui::RadioButton("Szablony", !listScenarios_)) listScenarios_ = false;
	ImGui::SameLine();
	if (ImGui::RadioButton("Scenariusze", listScenarios_)) listScenarios_ = true;
	ImGui::SetNextItemWidth(-1);
	ImGui::InputTextWithHint("##mf", listScenarios_ ? "filtr (np. single_)" : "filtr (np. urw_gng)", filter_, sizeof filter_);
	for (const auto& f : listScenarios_ ? scenarios_ : files_) {
		if (!passFilter(filter_, f)) continue;
		if (ImGui::Selectable(f.c_str(), f == file_)) {
			if (listScenarios_) loadScenario(f);
			else loadTemplate(f);
			recenter = true;
		}
		if (f == file_ && scrollToSel_) ImGui::SetScrollHereY(0.5f);
	}
	if (scrollToSel_) {
		scrollToSel_ = false;
		recenter = true;
	}
	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::BeginChild("mapa_szczegoly");
	if (!error_.empty()) ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Błąd: %s", error_.c_str());
	if (!scenarioInfo_.empty()) ImGui::TextColored(ImVec4(1, 0.8f, 0.4f, 1), "%s", scenarioInfo_.c_str());
	if (!script_) {
		ImGui::TextDisabled("Wybierz szablon z listy (%zu skryptów, %zu klas z modelem).", files_.size(), classes_.size());
		ImGui::EndChild();
		return recenter;
	}
	size_t resolved = 0;
	for (auto& o : objects_) resolved += o.slot != nullptr;
	ImGui::Text("%s", file_.c_str());
	if (scenarioMode_)
		ImGui::Text("Scenariusz: %d szablonów, obszar %ux%u bloków od (%d, %d)", placed_, pmp_ ? pmp_->width : 0,
		            pmp_ ? pmp_->height : 0, worldOrigin_[0], worldOrigin_[1]);
	else
		ImGui::Text("PMP: %s  %ux%u bloków   l2m: %s", script_->pmpFile.c_str(), pmp_ ? pmp_->width : 0,
		            pmp_ ? pmp_->height : 0, script_->l2mFile.empty() ? "-" : script_->l2mFile.c_str());
	ImGui::Text("obiekty: %zu, z modelem: %zu", objects_.size(), resolved);
	if (!blocksError_.empty()) ImGui::TextColored(ImVec4(1, 0.5f, 0.3f, 1), "Teren niedostępny: %s", blocksError_.c_str());
	ImGui::Checkbox("Teren", &showTerrain_);
	ImGui::SameLine();
	ImGui::Checkbox("Obiekty", &showObjects_);
	ImGui::SameLine();
	ImGui::Checkbox("Detale (.l2m)", &terrain_.showDetail);
	ImGui::SameLine();
	ImGui::Checkbox("Strefa horyzontu", &showZone_);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Kolor tła w otworach tylnej ściany (typ pola 5) i zanikanie modeli za skałą.\n%s",
		                  zone_ ? zoneClass_.c_str() : "Szablon nie ma obiektu Info_Fog_*.");



	if (ImGui::CollapsingHeader("Siatka 2D", ImGuiTreeNodeFlags_DefaultOpen)) uiGrid();
	if (ImGui::CollapsingHeader("Filtr obiektów", ImGuiTreeNodeFlags_DefaultOpen)) {
		const char* names[] = {"high", "medium", "low", "off", "bez poziomu"};
		for (int i = 0; i < 5; ++i) {
			if (i) ImGui::SameLine();
			ImGui::Checkbox(names[i], &showDetail_[i]);
		}
		ImGui::Checkbox("Pokaż obiekty bez modelu (czerwone na siatce)", &showUnresolved_);
		ImGui::Checkbox("Znaczniki edytora (światła, obiekty Info)", &showEditorOnly_);
	}
	if (ImGui::CollapsingHeader("Lista obiektów")) {
		ImGui::BeginChild("obj_list", ImVec2(0, 220), ImGuiChildFlags_Borders);
		for (const auto& o : objects_) {
			std::string model = o.slot ? o.cls->object + "." + o.cls->animation : "—";
			ImGui::Text("%-28s %-6s (%.2f %.2f %.2f) %s", o.src.className.c_str(), o.src.detail.c_str(), o.src.pos[0],
			            o.src.pos[1], o.src.pos[2], model.c_str());
		}
		ImGui::EndChild();
	}
	ImGui::EndChild();
	return recenter;
}
