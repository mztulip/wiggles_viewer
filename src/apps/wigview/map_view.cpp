#include "apps/wigview/map_view.h"

#include "lib/i18n.h"
#include "lib/text.h"
#include "lib/map_template.h"
#include "lib/model3db.h"
#include "lib/heightfield.h"
#include "lib/blockdb.h"
#include "lib/vfs.h"

#include <imgui.h>

#include <algorithm>
#include <filesystem>

using namespace ow;

namespace
{

bool endsWithI(const std::string& s, const std::string& suf)
{
	return s.size() >= suf.size() && Vfs::lower(s.substr(s.size() - suf.size())) == suf;
}

bool passFilter(const char* filter, const std::string& s)
{
	return !*filter || Vfs::lower(s).find(Vfs::lower(filter)) != std::string::npos;
}

// Kolory typów pól .PMP (znaczenie typów jeszcze niepewne).
ImU32 typeColor(uint8_t t)
{
	static const ImU32 pal[] = {IM_COL32(40, 40, 48, 255), IM_COL32(120, 90, 60, 255), IM_COL32(70, 130, 180, 255),
	    IM_COL32(200, 160, 60, 255), IM_COL32(90, 150, 90, 255), IM_COL32(170, 90, 160, 255),
	    IM_COL32(200, 100, 80, 255), IM_COL32(100, 180, 180, 255), IM_COL32(220, 220, 120, 255)};
	return pal[t % (sizeof pal / sizeof pal[0])];
}

ImU32 valueColor(unsigned v, unsigned max)
{
	float f = max ? float(v) / float(max) : 0.f;
	return IM_COL32(int(30 + 200 * f), int(60 + 120 * f), int(90 + 60 * (1 - f)), 255);
}

} // namespace

void MapView::init(rhi::Device& dev, const Vfs* vfs, TextureCache* textures, const std::vector<Vec3f>* sphere)
{
	MapScene::init(dev, vfs, textures, sphere);
	scenarios_.clear();
	scenarios_.push_back("systeminit.tcl"); // menu gry
	for (auto& f : vfs->list("Data/Gamesave"))
		if (endsWithI(f, ".tcl")) scenarios_.push_back(f);
	refreshSaves();
	files_.clear();
	for (auto& f : vfs->list("Data/Templates"))
		if (endsWithI(f, ".tcl")) files_.push_back(f);
}

bool MapView::loadScenario(const std::string& file)
{
	list_ = endsWithI(file, ".sav") ? ListMode::Saves : ListMode::Scenarios;
	return MapScene::loadScenario(file);
}

bool MapView::loadSavePath(const std::string& path)
{
	list_ = ListMode::Saves;
	return MapScene::loadSavePath(path);
}

// Lista zapisów: z Data/Gamesave (gra dopisuje nowe w trakcie działania, stąd odświeżanie) albo z wybranego
// katalogu na dysku (pełne ścieżki, wczytywane przez loadSavePath).
void MapView::refreshSaves()
{
	saves_.clear();
	if (savesDir_.empty())
	{
		for (auto& f : vfs_->list("Data/Gamesave"))
			if (endsWithI(f, ".sav")) saves_.push_back(f);
	}
	else
	{
		std::error_code ec;
		for (std::filesystem::directory_iterator it(savesDir_, ec), end; !ec && it != end; it.increment(ec))
			if (it->is_regular_file(ec) && endsWithI(it->path().filename().string(), ".sav")) saves_.push_back(it->path().string());
	}
	std::sort(saves_.begin(), saves_.end());
}

void MapView::setSavesDir(const std::string& dir)
{
	savesDir_ = dir;
	refreshSaves();
}

// Panel zaznaczenia: obiekt pod kliknięciem albo pole terenu (szablon: jego elementy składowe).
void MapView::uiSelection()
{
	if (selectedObject >= objectCount()) selectedObject = -1;
	bool tile = selectedObject < 0 && pmp_ && selectedTileX >= 0 && selectedTileY >= 0 && selectedTileX < int(pmp_->width) &&
	    selectedTileY < int(pmp_->height);
	if (selectedObject < 0 && !tile)
	{
		ImGui::TextDisabled(tr("Kliknij obiekt albo teren w scenie, żeby zobaczyć szczegóły."));
		return;
	}
	ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.35f, 0.55f, 1));
	bool open = ImGui::CollapsingHeader(tr("Zaznaczenie###zaznaczenie"), ImGuiTreeNodeFlags_DefaultOpen);
	ImGui::PopStyleColor();
	if (!open) return;
	if (ImGui::SmallButton(tr("Odznacz")))
	{ // dalej w tej klatce nie ma już czego pokazywać (pole −1, −1 poza siatką)
		clearSelection();
		return;
	}
	if (selectedObject >= 0)
	{
		int i = selectedObject;
		const PlacedObject& o = objects_[size_t(i)];
		const TemplateObject& s = o.src;
		ImGui::Text(tr("Klasa: %s"), s.className.c_str());
		if (!s.instanceName.empty()) ImGui::TextWrapped(tr("Nazwa: %s"), cp1250ToUtf8(s.instanceName).c_str()); // zapis w CP1250
		if (!s.name.empty()) ImGui::Text(tr("Nazwa w skrypcie / ID: %s"), s.name.c_str());
		ImGui::TextWrapped(tr("Pozycja: x %.2f  y %.2f bloku, z %.2f (skrypt) = %.2f bloku świata"), s.pos[0], s.pos[1], s.pos[2],
		    s.pos[2] * 0.5f + 7.f);
		ImGui::Text(tr("Pole: %d, %d   obrót: %.0f°"), int(std::floor(s.pos[0])), int(std::floor(s.pos[1])),
		    s.rot[1] * 57.2958f);
		if (const Model3db* m = objectModel(i))
		{
			std::string anim = objectAnimation(i);
			int mesh = currentMesh(o);
			ImGui::TextWrapped(tr("Model: %s, obiekt %s, animacja %s"), m->name.c_str(), o.cls ? o.cls->object.c_str() : "?",
			    anim.c_str());
			if (mesh >= 0)
			{
				const Mesh3db& me = m->meshes[size_t(mesh)];
				ImGui::TextWrapped(tr("Klatka (mesh) %d, rozmiar %.2f × %.2f × %.2f bloku"), mesh, (me.bboxMax.x - me.bboxMin.x) / kBlock,
				    (me.bboxMax.y - me.bboxMin.y) / kBlock, (me.bboxMax.z - me.bboxMin.z) / kBlock);
			}
			if (openModel && o.cls && ImGui::SmallButton(tr("Otwórz model w zakładce Modele"))) openModel(o.cls->object, anim);
		}
		else ImGui::TextDisabled(tr("Bez modelu"));
		if (o.cls)
		{
			const char* modes[] = {"ANIM_STILL", "ANIM_ONCE", "ANIM_LOOP"};
			ImGui::TextWrapped(tr("Klasa: tryb animacji %s%s%s%s"), modes[std::clamp(o.cls->animMode, 0, 2)],
			    o.cls->animRandom ? ", animrandom" : "", o.cls->viewInFog ? ", viewinfog" : "", o.cls->editorOnly ? tr(", tylko edytor") : "");
		}
		if (const auto* ls = classes_.lights(s.className))
		{ // lampy klasy (class_light, rozdział 7.4): wkład w kolor wierzchołka c · exp(−2 · d² / R²) · max(N · L, 0,1), zero od d ≥ R
			for (size_t k = 0; k < ls->size(); ++k)
			{
				const ClassLight& l = (*ls)[k];
				float c[3];
				for (int j = 0; j < 3; ++j) c[j] = float(int(l.color[j] * 31.f)) * 0.5f;
				ImGui::Text(tr("Lampa %zu (class_light): promień R %.2f bloku"), k + 1, l.radius);
				ImGui::TextWrapped(tr("  kolor %.2f %.2f %.2f → c = %.1f %.1f %.1f (int(kolor · 31) · 0,5)"), l.color[0], l.color[1], l.color[2], c[0],
				    c[1], c[2]);
				ImGui::TextWrapped(tr("  przesunięcie %.2f %.2f %.2f → lampa w x %.2f  y %.2f  z %.2f bloku świata"), l.offset[0], l.offset[1],
				    l.offset[2], s.pos[0] + l.offset[0], s.pos[1] + l.offset[1], (s.pos[2] + l.offset[2]) * 0.5f + 7.f);
			}
		}
		if (!s.detail.empty()) ImGui::TextDisabled(tr("Poziom detali: %s"), s.detail.c_str());
		if (s.owner >= 0) ImGui::Text(tr("Właściciel: gracz %d"), s.owner);
		if (s.linkParent >= 0) ImGui::Text(tr("Przypięty do obiektu %d, punkt %d"), s.linkParent, s.linkPoint);
		if (!s.info.empty()) ImGui::TextWrapped(tr("Parametry z edytora: %s"), s.info.c_str());
		if (objectExtra) objectExtra(i);
		if (!s.tclVars.empty() && ImGui::TreeNode("zmienne", tr("Zmienne Tcl z zapisu (%zu)"), s.tclVars.size()))
		{
			for (const auto& [k, v] : s.tclVars) ImGui::TextWrapped("%s = %s", k.c_str(), cp1250ToUtf8(v).c_str());
			ImGui::TreePop();
		}
	}
	else
	{
		int x = selectedTileX, y = selectedTileY;
		size_t k = size_t(y) * pmp_->width + size_t(x);
		uint8_t type = pmp_->types[k];
		static const char* kGround[] = {"A", "B", "C"};
		ImGui::Text(tr("Pole terenu: %d, %d"), x, y);
		ImGui::Text(tr("Blok BlockDB: %u"), pmp_->blocks[k]);
		ImGui::Text(tr("Typ pola: 0x%02X (grunt %s%s)"), type, kGround[std::clamp(groundFromPmpType(type), 0, 2)],
		    (type & 0x7F) == 5 ? tr(", otwór do tła") : "");
		if (const L2m* l = layer(); l && k < l->values.size()) ImGui::Text(tr("Warstwa detali (.l2m): %u"), unsigned(l->values[k]));
		if (const BlockRecord* r = blockDb().record(pmp_->blocks[k]))
		{
			int solid = 0, mn = 15, mx = 0;
			for (int j = 0; j < 64; ++j) solid += r->depth[size_t(j)] >= 15, mn = std::min(mn, int(r->depth[size_t(j)])), mx = std::max(mx, int(r->depth[size_t(j)]));
			ImGui::Text(tr("Głębokość: %d..%d (15 = lita skała), lite drobne pola %d/64"), mn, mx, solid);
		}
		float fl = fluidFill(x, y);
		if (fl > 0.f) ImGui::Text(tr("Płyn: wypełnienie %.2f"), fl);
		if (fogMode_ != FogMode::Off) ImGui::Text(tr("Mgła: bajt 0x%02X"), fogAt(x, y));
		int n = 0;
		for (int j = 0; j < objectCount(); ++j)
			n += int(std::floor(objects_[size_t(j)].src.pos[0])) == x && int(std::floor(objects_[size_t(j)].src.pos[1])) == y;
		ImGui::Text(tr("Obiekty w polu: %d"), n);
	}
	ImGui::Separator();
}

void MapView::reloadCurrent()
{
	if (file_.empty()) return;
	std::string f = file_;
	if (f.find('/') != std::string::npos && (endsWithI(f, ".sav"))) pending_ = [this, f] { return loadSavePath(f); };
	else pending_ = [this, f] { return loadListed(f); };
}

// Wczytanie pozycji listy: zapisy z wybranego katalogu to pełne ścieżki.
bool MapView::loadListed(const std::string& f)
{
	if (list_ == ListMode::Saves && !savesDir_.empty()) return loadSavePath(f);
	return list_ == ListMode::Templates ? loadTemplate(f) : loadScenario(f);
}

// Pola wyboru zapisu: odświeżenie listy i wczytanie pliku spoza katalogu gry.
void MapView::uiSaves(bool& recenter)
{
	if (ImGui::Button(tr("Odśwież listę"))) refreshSaves();
	ImGui::SetItemTooltip(tr("Gra dopisuje nowe zapisy do Data/Gamesave w trakcie działania."));
	ImGui::SameLine();
	ImGui::TextDisabled("%s", savesDir_.empty() ? "Data/Gamesave" : savesDir_.c_str());
	ImGui::SetItemTooltip(tr("Katalog zapisów; zmiana: przycisk „Ścieżki…” u góry okna."));
	ImGui::SetNextItemWidth(-(ImGui::CalcTextSize(tr("Wczytaj")).x + ImGui::GetStyle().FramePadding.x * 2 + ImGui::GetStyle().ItemSpacing.x));
	bool enter = ImGui::InputTextWithHint("##savepath", tr("ścieżka do pliku .sav"), savePath_, sizeof savePath_,
	    ImGuiInputTextFlags_EnterReturnsTrue);
	ImGui::SameLine();
	if ((ImGui::Button(tr("Wczytaj")) || enter) && savePath_[0]) pending_ = [this, p = std::string(savePath_)] { return loadSavePath(p); };
	(void)recenter;
	ImGui::Separator();
}

void MapView::step(int d)
{
	const auto& list = currentList();
	std::vector<int> vis;
	for (int i = 0; i < int(list.size()); ++i)
		if (passFilter(filter_, list[size_t(i)])) vis.push_back(i);
	if (vis.empty()) return;
	int pos = -1;
	for (size_t k = 0; k < vis.size(); ++k)
		if (list[size_t(vis[k])] == file_) pos = int(k);
	if (pos < 0) pos = d > 0 ? -1 : 0;
	pos = (pos + d + int(vis.size())) % int(vis.size());
	loadListed(list[size_t(vis[size_t(pos)])]);
	scrollToSel_ = true;
}

void MapView::uiGrid()
{
	if (!pmp_)
	{
		ImGui::TextDisabled(tr("Brak siatki .PMP"));
		return;
	}
	const char* layers[] = {tr("Typ pola"), tr("ID bloku"), tr("Warstwa 2 (PMP)"), tr("Warstwa .l2m"), tr("Głębokość (BlockDB)")};
	ImGui::SetNextItemWidth(170);
	ImGui::Combo(tr("Warstwa"), &gridLayer_, layers, 5);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120);
	ImGui::SliderFloat(tr("Zoom"), &cellPx_, 6.f, 48.f, "%.0f px");

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
		for (int x = 0; x < W; ++x)
		{
			size_t i = size_t(y) * W + x;
			ImU32 c;
			switch (gridLayer_)
			{
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
			if (gridLayer_ == 4 && heights_)
			{
				float s = cellPx_ / Heightfield::kSamplesPerBlock;
				for (int sy = 0; sy < Heightfield::kSamplesPerBlock; ++sy)
					for (int sx = 0; sx < Heightfield::kSamplesPerBlock; ++sx)
					{
						int v = heights_->at(x * 8 + sx, y * 8 + sy) * 17;
						ImVec2 p(a.x + sx * s, a.y + sy * s);
						dl->AddRectFilled(p, ImVec2(p.x + s, p.y + s), IM_COL32(v, v, v, 255));
					}
			}
			else
				dl->AddRectFilled(a, ImVec2(a.x + cellPx_ - 1, a.y + cellPx_ - 1), c);
		}
	// Obiekty jako kropki (pozycja w blokach).
	for (const auto& o : objects_)
	{
		if (!showDetail_[MapScene::detailIndex(o.src.detail)]) continue;
		if (!o.slot && !showUnresolved_) continue;
		ImVec2 p(origin.x + o.src.pos[0] * cellPx_, origin.y + o.src.pos[1] * cellPx_);
		dl->AddCircleFilled(p, std::max(2.f, cellPx_ * 0.12f), o.slot ? IM_COL32(255, 255, 255, 220) : IM_COL32(255, 80, 80, 220));
	}
	ImGui::InvisibleButton("siatka_btn", ImVec2(W * cellPx_, H * cellPx_));
	if (ImGui::IsItemHovered())
	{
		ImVec2 m = ImGui::GetMousePos();
		int x = int((m.x - origin.x) / cellPx_), y = int((m.y - origin.y) / cellPx_);
		if (x >= 0 && y >= 0 && x < W && y < H)
		{
			size_t i = size_t(y) * W + x;
			ImGui::BeginTooltip();
			ImGui::Text(tr("blok (%d, %d)"), x, y);
			ImGui::Text(tr("typ %u  ID bloku %u  warstwa2 %u"), pmp_->types[i], pmp_->blocks[i], pmp_->layer2[i]);
			if (l2m_ && l2m_->width == pmp_->width && i < l2m_->values.size()) ImGui::Text(tr("l2m 0x%04X"), l2m_->values[i]);
			int shown = 0;
			for (const auto& o : objects_)
				if (int(o.src.pos[0]) == x && int(o.src.pos[1]) == y && shown++ < 8)
					ImGui::Text("%s %s", o.src.className.c_str(), o.slot ? "" : tr("(bez modelu)"));
			ImGui::EndTooltip();
		}
	}
	ImGui::EndChild();
}

bool MapView::ui(bool savesTab)
{
	bool recenter = false;
	ImGui::BeginChild("szablony", ImVec2(ImGui::GetFontSize() * (savesTab ? 21.f : 16.f), 0), ImGuiChildFlags_Borders);
	if (savesTab)
	{
		list_ = ListMode::Saves;
		uiSaves(recenter);
	}
	else
	{
		if (list_ == ListMode::Saves) list_ = scenarioMode_ ? ListMode::Scenarios : ListMode::Templates;
		if (ImGui::RadioButton(tr("Szablony"), list_ == ListMode::Templates)) list_ = ListMode::Templates;
		ImGui::SameLine();
		if (ImGui::RadioButton(tr("Scenariusze"), list_ == ListMode::Scenarios)) list_ = ListMode::Scenarios;
	}
	ImGui::SetNextItemWidth(-1);
	const char* hint = list_ == ListMode::Templates ? tr("filtr (np. urw_gng)") : list_ == ListMode::Scenarios ? tr("filtr (np. single_)")
	                                                                                                           : tr("filtr (np. quick)");
	ImGui::InputTextWithHint("##mf", hint, filter_, sizeof filter_);
	if (list_ == ListMode::Saves && saves_.empty())
		ImGui::TextDisabled(tr("Brak zapisów w %s."), savesDir_.empty() ? "Data/Gamesave" : savesDir_.c_str());
	for (const auto& f : currentList())
	{
		if (!passFilter(filter_, f)) continue;
		// zapisy z wybranego katalogu: pełna ścieżka na liście, pokazujemy samą nazwę
		std::string label = list_ == ListMode::Saves && !savesDir_.empty() ? std::filesystem::path(f).filename().string() : f;
		if (ImGui::Selectable((label + "##" + f).c_str(), f == file_)) pending_ = [this, f] { return loadListed(f); };
		if (f == file_ && scrollToSel_) ImGui::SetScrollHereY(0.5f);
	}
	if (scrollToSel_)
	{
		scrollToSel_ = false;
		recenter = true;
	}
	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::BeginChild("mapa_szczegoly");
	uiSelection();
	if (!error_.empty()) ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), tr("Błąd: %s"), error_.c_str());
	if (!scenarioInfo_.empty()) ImGui::TextColored(ImVec4(1, 0.8f, 0.4f, 1), "%s", scenarioInfo_.c_str());
	if (!script_)
	{
		ImGui::TextDisabled(tr("Wybierz szablon z listy (%zu skryptów, %zu klas z modelem)."), files_.size(), classes_.size());
		ImGui::EndChild();
		return recenter;
	}
	size_t resolved = 0;
	for (auto& o : objects_) resolved += o.slot != nullptr;
	ImGui::Text("%s", file_.c_str());
	if (scenarioMode_)
	{
		ImGui::Text(tr("Scenariusz: %d szablonów, obszar %ux%u bloków od (%d, %d)"), placed_, pmp_ ? pmp_->width : 0,
		    pmp_ ? pmp_->height : 0, worldOrigin_[0], worldOrigin_[1]);
		if (lightBegin_ != Scenario::kNoSky)
			ImGui::Text(tr("Niebo do wiersza %d (set_light_begin): %d pól w świetle dziennym"), lightBegin_, skyTiles_);
	}
	else
		ImGui::Text(tr("PMP: %s  %ux%u bloków   l2m: %s"), script_->pmpFile.c_str(), pmp_ ? pmp_->width : 0,
		    pmp_ ? pmp_->height : 0, script_->l2mFile.empty() ? "-" : script_->l2mFile.c_str());
	ImGui::Text(tr("obiekty: %zu, z modelem: %zu"), objects_.size(), resolved);
	if (!blocksError_.empty()) ImGui::TextColored(ImVec4(1, 0.5f, 0.3f, 1), tr("Teren niedostępny: %s"), blocksError_.c_str());
	ImGui::Checkbox(tr("Teren"), &showTerrain_);
	ImGui::SameLine();
	ImGui::Checkbox(tr("Obiekty"), &showObjects_);
	ImGui::SameLine();
	ImGui::Checkbox(tr("Detale (.l2m)"), &terrain_.showDetail);
	ImGui::SameLine();
	ImGui::Checkbox(tr("Strefa horyzontu"), &showZone_);
	ImGui::SameLine();
	ImGui::Checkbox(tr("Cząsteczki"), &showParticles_);
	ImGui::SetItemTooltip(tr("Efekty z class_particle obiektów (%zu emiterów)."), emitters_.size());
	ImGui::SameLine();
	ImGui::Checkbox(tr("Płyny"), &showFluids_);
	ImGui::SetItemTooltip(tr("Woda, siarka i lawa z zapisu gry (%d pól)."), fluidCells_);
	if (playing)
	{
		if (ImGui::SmallButton(*playing ? tr("Pauza###mapa_graj") : tr("Graj###mapa_graj"))) *playing = !*playing;
		ImGui::SetItemTooltip(tr("Zatrzymuje animacje, cząsteczki i symulację na mapie (spacja)."));
		ImGui::SameLine();
	}
	if (simulate)
	{
		ImGui::Checkbox(tr("Symulacja (skrypty)"), simulate);
		ImGui::SetItemTooltip(tr("Wykonuje skrypty obiektów jak w grze: postacie z mapy (zapis, scenariusz, szablon) chodzą\n"
		                         "i pracują, pierścienie i inne obiekty dostają efekty i animacje ze skryptów.\n"
		                         "Pierwsze włączenie wczytuje klasy skryptów (kilka sekund). Wyłączenie wczytuje mapę od nowa.\n"
		                         "Włączona przy starcie (--no-simulate wyłącza). N: krasnal pod kursorem, Shift+N: wuker."));
		if (*simulate)
		{
			ImGui::SameLine();
			if (ImGui::SmallButton(tr("Od nowa"))) simRestart = true;
			if (!simInfo.empty())
			{
				ImGui::SameLine();
				ImGui::TextDisabled("%s", simInfo.c_str());
			}
			if (ImGui::Checkbox(tr("Dodaj postacie na mapie bez krasnali"), &populate)) simRestart = true;
			ImGui::SetItemTooltip(tr("Nie z danych gry: na mapie bez żadnego krasnala (np. sam szablon) stawia podaną liczbę\n"
			                         "krasnali i wukerów przy środku kadru. Zapisy i scenariusze mają własne postacie."));
			if (populate)
			{
				ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.f);
				if (ImGui::SliderInt(tr("krasnale##pop"), &populateGnomes, 0, 10)) simRestart = true;
				ImGui::SameLine();
				ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.f);
				if (ImGui::SliderInt(tr("wukery##pop"), &populateWukers, 0, 5)) simRestart = true;
			}
		}
	}
	ImGui::Checkbox(tr("Oświetlenie jak w grze"), &gameLighting_);
	ImGui::SameLine();
	ImGui::Checkbox(tr("Lampa podglądu"), &previewLamp_);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(tr("Dodatkowa lampa nad środkiem kadru (nie z gry), działa z oświetleniem jak w grze.\n"
		                     "Domyślnie włączona dla szablonów bez własnych świateł (%zu świateł w scenie)."),
		    allLights_.size());
	ImGui::SameLine();
	ImGui::BeginDisabled(!hasOpenAir_);
	ImGui::Checkbox(tr("Panorama nieba"), &showPanorama_);
	ImGui::SameLine();
	ImGui::Checkbox(tr("Dywan łąki"), &showCarpet_);
	ImGui::EndDisabled();
	if (borderMask)
	{
		ImGui::SameLine();
		ImGui::Checkbox(tr("Ramka ekranu"), borderMask);
		ImGui::SetItemTooltip(tr("Czarna ramka z zaokrąglonymi rogami z Data/GUI/bordermask.tga, jak w grze z BorderMask=1"));
	}
	ImGui::BeginDisabled(!hasOpenAir_);
	{
		// Podgląd nieodkrytego terenu (wymaga oświetlenia gry).
		const char* modes[] = {tr("wyłączona"), tr("całość odkryta, niewidoczna (szara)"), tr("kadr widoczny, reszta szara"),
		    tr("kadr widoczny, reszta czarna"), tr("z zapisu gry (gracz 0)")};
		// Odkryj mapę: wyłącza mgłę, odznaczenie przywraca poprzedni tryb (dla zapisu: mgła z zapisu).
		// Wybór zostaje przy wczytywaniu kolejnych zapisów.
		bool reveal = fogMode_ == FogMode::Off;
		if (ImGui::Checkbox(tr("Odkryj mapę"), &reveal))
		{
			revealMap_ = reveal;
			if (reveal)
			{
				revealPrev_ = fogMode_;
				setFog(FogMode::Off, fogRadius_, fogByte_);
			}
			else
			{
				FogMode back = revealPrev_ != FogMode::Off ? revealPrev_ : saveFog_.empty() ? FogMode::Explored
				                                                                            : FogMode::FromSave;
				setFog(back, fogRadius_, fogByte_);
			}
		}
		ImGui::SetItemTooltip(tr("Pokazuje cały teren i wszystkie obiekty, także te jeszcze nieodkryte."));
		ImGui::SameLine();
		int m = int(fogMode_);
		ImGui::SetNextItemWidth(260);
		if (ImGui::Combo(tr("Nieodkryty teren"), &m, modes, saveFog_.empty() ? 4 : 5)) setFog(FogMode(m), fogRadius_, fogByte_);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(tr("Prawo z docs/silnik/10_nieodkryty_teren.md: szara mgła (170,170,155) rośnie w głąb,\n"
			                     "lampy w niej nie świecą, czarne pola nieodkryte; obiekty bez viewinfog znikają."));
		if (fogMode_ == FogMode::ViewGrey || fogMode_ == FogMode::ViewBlack)
		{
			int r = fogRadius_, b = fogByte_;
			ImGui::SetNextItemWidth(120);
			bool ch = ImGui::SliderInt(tr("promień kadru"), &r, 1, 40);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(120);
			ch |= ImGui::SliderInt(tr("bajt w kadrze"), &b, 1, 255);
			if (ImGui::IsItemHovered()) ImGui::SetTooltip(tr("255 = widoczne; 0xE2..0x80 = wygasanie; < 0x80 = szara mgła"));
			if (ch) setFog(fogMode_, r, b);
		}
	}
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip(tr("6 warstw z paralaksą (Panorama_layer_0..5) i dywan łąki (Panorama_layer_6).\n"
		                     "Tylko gdy scenariusz ustawia niebo (set_light_begin) albo set_ground_begin."));
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(tr("Prawo z gry: jasność bazowa z warstwy 2, stałe słońce z lewej góry,\n"
		                     "lampy class_light (%zu w scenie, rysowane do %d najbliższych)."),
		    allLights_.size(), kMaxGameLights);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(tr("Kolor tła w otworach tylnej ściany (typ pola 5) i zanikanie modeli za skałą.\n%s"),
		    zone_                      ? zoneClass_.c_str()
		        : zoneObjects_.empty() ? tr("Szablon nie ma obiektu Info_Fog_*.")
		                               : tr("Kamera poza zasięgiem (size) wszystkich obiektów Info_Fog_*."));


	if (ImGui::CollapsingHeader(tr("Siatka 2D"), ImGuiTreeNodeFlags_DefaultOpen)) uiGrid();
	if (ImGui::CollapsingHeader(tr("Filtr obiektów"), ImGuiTreeNodeFlags_DefaultOpen))
	{
		const char* names[] = {"high", "medium", "low", "off", tr("bez poziomu")};
		for (int i = 0; i < 5; ++i)
		{
			if (i) ImGui::SameLine();
			ImGui::Checkbox(names[i], &showDetail_[i]);
		}
		ImGui::Checkbox(tr("Pokaż obiekty bez modelu (czerwone na siatce)"), &showUnresolved_);
		// znaczniki edytora, w grze niewidoczne
		ImGui::Checkbox(tr("Źródła świateł i efektów"), &showMarker_[1]);
		ImGui::Checkbox(tr("Obiekty Info"), &showMarker_[2]);
		ImGui::SameLine();
		ImGui::Checkbox(tr("Bryły kolizji"), &showMarker_[3]);
	}
	if (ImGui::CollapsingHeader(tr("Lista obiektów")))
	{
		ImGui::BeginChild("obj_list", ImVec2(0, 220), ImGuiChildFlags_Borders);
		for (const auto& o : objects_)
		{
			std::string model = o.slot ? o.cls->object + "." + o.cls->animation : "—";
			ImGui::Text("%-28s %-6s (%.2f %.2f %.2f) %s", o.src.className.c_str(), o.src.detail.c_str(), o.src.pos[0],
			    o.src.pos[1], o.src.pos[2], model.c_str());
		}
		ImGui::EndChild();
	}
	ImGui::EndChild();
	return recenter;
}
