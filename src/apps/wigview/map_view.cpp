#include "apps/wigview/map_view.h"

#include "lib/i18n.h"
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
	if (simulate)
	{
		ImGui::Checkbox(tr("Symulacja (skrypty)"), simulate);
		ImGui::SetItemTooltip(tr("Wykonuje skrypty obiektów jak w grze: postacie z mapy (zapis, scenariusz, szablon) chodzą\n"
		                         "i pracują, pierścienie i inne obiekty dostają efekty i animacje ze skryptów.\n"
		                         "Pierwsze włączenie wczytuje klasy skryptów (kilka sekund). Wyłączenie wczytuje mapę od nowa."));
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
		ImGui::Checkbox(tr("Znaczniki edytora (światła, obiekty Info)"), &showEditorOnly_);
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
