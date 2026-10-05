// Zakładka Pliki: drzewo całego Data/ (z nakładkami modów), statystyka typów plików w katalogu,
// podgląd tekstu (CP1250/CP1252 -> UTF-8) albo zrzut szesnastkowy oraz przejście do zakładki,
// która dany typ umie pokazać (modele, tekstury, dźwięki, szablony, scenariusze, zapisy).
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace ow
{
class Vfs;
}
struct ImFont;

class FilesView
{
public:
	void init(const ow::Vfs* vfs) { vfs_ = vfs; }
	void ui();
	void select(const std::string& gamePath); // np. "Data/Scripts/init/out.tcl"; rozwija drzewo do pliku

	// Otwarcie pliku w innej zakładce (ustawia Viewer). canOpen: czy jest zakładka dla tego pliku.
	std::function<bool(const std::string& gamePath)> canOpen, open;
	ImFont* mono = nullptr; // font o stałej szerokości (hex, tekst); nullptr = domyślny

private:
	struct Entry
	{
		std::string name;
		bool dir = false;
		uint64_t size = 0;
	};
	struct DirStats
	{
		std::map<std::string, std::pair<int, uint64_t>> byExt; // rozszerzenie -> (liczba, bajty)
		int files = 0, dirs = 0;
		uint64_t bytes = 0;
	};
	const std::vector<Entry>& listDir(const std::string& gameDir);
	void treeNode(const std::string& gameDir);
	void collectAll(const std::string& gameDir, std::vector<std::string>& out);
	const DirStats& stats(const std::string& gameDir);
	void load(const std::string& gamePath);
	void uiDetails();
	void uiText();
	void uiHex();

	const ow::Vfs* vfs_ = nullptr;
	std::map<std::string, std::vector<Entry>> dirs_; // katalog gry -> zawartość (katalogi przed plikami)
	std::map<std::string, DirStats> stats_;
	std::vector<std::string> all_; // wszystkie pliki (do filtra); budowane przy pierwszym użyciu filtra
	bool allBuilt_ = false;
	char filter_[96] = "";

	std::string sel_;       // zaznaczona ścieżka gry (plik albo katalog)
	bool selDir_ = false;
	bool openTo_ = false;   // rozwinąć drzewo do sel_ w najbliższej klatce
	std::string loaded_;    // ścieżka wczytanego pliku
	std::vector<uint8_t> data_;
	bool truncated_ = false; // wczytano tylko początek pliku
	bool isText_ = false;
	int view_ = 0;           // 0 tekst, 1 hex
	int encoding_ = 0;       // 0 CP1250 (wersja polska), 1 CP1252
	std::string text_;       // tekst w UTF-8 (dla bieżącego kodowania)
	int textEncoding_ = -1;
};
