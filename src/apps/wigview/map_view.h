// Zakładki "Mapy" i "Zapisy" w wigview: panel ImGui nad sceną mapy (lib/map_scene.h).
//
// Listy szablonów (Data/Templates), scenariuszy i zapisów (Data/Gamesave), siatka .PMP w 2D
// (typ pola, ID bloku, warstwy, głębokość), przełączniki widoku sceny i lista obiektów.
#pragma once

#include "lib/map_scene.h"

#include <functional>
#include <string>
#include <vector>

class MapView : public ow::MapScene
{
public:
	void init(ow::rhi::Device& dev, const ow::Vfs* vfs, ow::TextureCache* textures, const std::vector<ow::Vec3f>* sphere);
	// Wczytanie ustawia też listę, na której zaznaczony jest plik (scenariusze albo zapisy).
	bool loadScenario(const std::string& file);
	bool loadSavePath(const std::string& path);
	void step(int d);                           // poprzedni/następny szablon (strzałki)
	void select(const std::string& file)
	{      // wczytaj (odroczone) i przewiń listę do szablonu
		list_ = ListMode::Templates;
		pending_ = [this, file] { return loadTemplate(file); };
		scrollToSel_ = true;
	}
	void selectScenario(const std::string& file)
	{
		bool sav = file.size() > 4 && (file.compare(file.size() - 4, 4, ".sav") == 0 || file.compare(file.size() - 4, 4, ".SAV") == 0);
		list_ = sav ? ListMode::Saves : ListMode::Scenarios;
		pending_ = [this, file] { return loadScenario(file); };
		scrollToSel_ = true;
	}
	// Wczytywanie odroczone: wybór w UI (w trakcie klatki ImGui) tylko zleca wczytanie, a pętla główna
	// wykonuje je między klatkami, rysując postęp (progress).
	bool hasPending() const { return bool(pending_); }
	const std::string& loadingName() const { return file_; }
	void runPending()
	{
		auto f = std::move(pending_);
		pending_ = nullptr;
		f();
	}
	const std::vector<std::string>& scenarios() const { return scenarios_; }
	const std::vector<std::string>& files() const { return files_; }

	// Rysuje panel UI (lista, siatka 2D, informacje). Zwraca true, gdy kamera powinna się wycentrować.
	// savesTab: zakładka Zapisy (lista zapisów gry) zamiast Mapy (szablony i scenariusze).
	bool ui(bool savesTab);
	bool showingSave() const { return list_ == ListMode::Saves; }
	// Katalog zapisów na dysku; pusty = Data/Gamesave gry (z nakładkami). Ustawienie odświeża listę.
	void setSavesDir(const std::string& dir);
	const std::string& savesDir() const { return savesDir_; }

	bool* borderMask = nullptr; // przełącznik ramki ekranu w wigview (rysuje ją Viewer); nullptr = bez przełącznika

private:
	enum class ListMode
	{
		Templates,
		Scenarios,
		Saves
	};
	ListMode list_ = ListMode::Templates;
	const std::vector<std::string>& currentList() const
	{
		return list_ == ListMode::Templates ? files_ : list_ == ListMode::Scenarios ? scenarios_
		                                                                            : saves_;
	}
	void uiGrid();
	void refreshSaves();
	bool loadListed(const std::string& f);
	void uiSaves(bool& recenter);

	std::function<bool()> pending_;
	std::vector<std::string> files_; // skrypty .tcl szablonów
	std::vector<std::string> scenarios_, saves_;
	char savePath_[512] = "";
	std::string savesDir_; // pusty: Data/Gamesave; inaczej saves_ to pełne ścieżki plików w tym katalogu
	char filter_[64] = "";
	bool scrollToSel_ = false;
	int gridLayer_ = 0; // 0 typ, 1 ID bloku, 2 warstwa2, 3 l2m
	float cellPx_ = 22.f;
	bool showUnresolved_ = true;
};
