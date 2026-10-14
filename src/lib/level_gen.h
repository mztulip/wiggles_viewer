// Generator świata kampanii (gra: CLevelGenerator, komendy lg_*): wzrost jednego klastra szablonów wokół przebicia
// kopania (docs/silnik/21_start_kampanii.md §21.8). Prawa i stałe za Zimennik/Diggles-Unity (MIT),
// Runtime/World/LevelGenerator.cs i Docs/LEVEL_GENERATOR.md, WORLDGEN_ORIGINAL_RE.md (adresy FUN_… to ich źródło):
//  - rejestr: szablony z grup lginit.tcl (lg_set_templategroup), wymiary z .pmp albo nagłówka split_template,
//    drzwi z .ent (map_entry 1 Xpx Ypx: kwantyzacja do 4 pól i +2 w połowie krawędzi, kierunek z krawędzi),
//    obiekty klas filtra (lg_addfilterclass) z wierszy `new` skryptu szablonu;
//  - lg_start: elementy wymuszone (start, szablon unikalny) w zapisanych miejscach, potem dokowanie drzwi czoła
//    frontu: pula posortowana kosztem, krawędzie przeciwne drzwiom, pierwsze udane TryPlace; nie da się
//    zadokować czoła → cała generacja się nie udaje (prawo pełnego zamknięcia);
//  - TryPlace: wyrównanie do 4, w obszarze, bez nakładania, przełączanie drzwi frontu (MergeEdges), margines 4
//    pól po stronach bez drzwi, walidator frontu (≥ 2 od brzegu obszaru, jeden z 4 narożników wolny).
// 🔶 Bez routera korytarzy (FUN_00587480, tylko szablony unikalne z -connect) i bez przycinania ślepych odnóg
// (FUN_00585390). Losowanie: wspólny generator 0..0x7fff (gra: MT19937 całej gry).
#pragma once

#include <climits>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ow
{

class Vfs;

class LevelGen
{
public:
	struct Edge
	{
		int lx, ly, dir; // drzwi: pole na krawędzi; kierunek 0 lewo (x = 0), 1 góra (y = 0), 2 prawo (x = W), 3 dół (y = H)
	};
	struct Piece
	{
		std::string tcl;     // np. "urw_gng_050_a.tcl"
		std::string pmpBase; // np. "gng_050"
		int w = 0, h = 0;
		std::vector<Edge> edges;
		int levelCap = INT_MAX, gameCap = INT_MAX, tunnelFlag = 0;
		float value = 1.f;
		bool loaded = false, split = false, objectBearing = false;
		int placedThisLevel = 0, emittedThisSession = 0;
		int openSides = 0; // bity: 1 góra, 2 prawo, 4 dół, 8 lewo (strony z drzwiami)
		std::map<std::string, int> objCounts; // klasy filtra → liczba obiektów w szablonie
	};
	struct Placement
	{
		std::string tcl;
		int x, y, w, h; // pola świata
	};

	explicit LevelGen(const Vfs& vfs) : vfs_(vfs) {}

	std::function<int()> rnd; // 0..0x7fff
	std::function<bool(int, int)> tileTemplated; // pole świata już pod szablonem (gra: tile+0x13 bit 0)
	int mapW = 0, mapH = 0;

	// --- konfiguracja (lg_*) ---
	void addFilterClass(const std::string& cls);
	void setTemplateGroup(const std::string& name, const std::vector<std::string>& tcls);
	void setTemplateGroupProps(const std::string& name, int levelCount);
	void setTemplateProps(const std::string& tcl, int levelCount, int gameCount, int tunnel); // INT_MIN = bez zmiany
	void setTemplateGroupValue(const std::string& name, float v);
	void setTemplateValue(const std::string& tcl, float v);
	void poolClear();
	void poolAddTemplateSets(const std::string& spec);
	void poolSetFilter(const std::string& group);
	void poolObjFilter(const std::vector<std::string>& classes);
	void setArea(int x0, int y0, int x1, int y1) { ax0_ = x0, ay0_ = y0, ax1_ = x1, ay1_ = y1; }
	void setTemplateRatio(float a, float opt, float max) { ratioA_ = a, ratioOpt_ = opt, ratioMax_ = max; }
	void setObjCount(const std::string& cls, float min, float max, float stride) { objCons_[cls] = {min, max, stride}; }
	void clearObjCounts() { objCons_.clear(); }
	void clearForced() { forced_.clear(); }
	void addForced(const std::string& tcl, int gx, int gy) { forced_.push_back({trimTcl(tcl), gx, gy}); }
	size_t forcedCount() const { return forced_.size(); }

	// Element startowy przy przebiciu (FUN_00585d80): z puli elementy 4 × 4; przy kopaniu w dół w trybie 1 dwoje
	// drzwi lewo+prawo (korytarz poprzeczny), inaczej jedne drzwi w stronę side. Pusty = brak.
	std::string pickStartPiece(int side, bool downThrough);
	// Miejsce elementu wymuszonego (FUN_0058a510): wyrównanie do 4, ≥ 8 pól od brzegów mapy, bez pól pod szablonem.
	bool probeForcedPose(const std::string& tcl, int wx, int wy);
	bool tempSize(const std::string& tcl, int& w, int& h); // wymiary szablonu w polach

	// --- wzrost (lg_start, lg_get_level) ---
	bool start();
	const std::vector<Placement>& level() const { return level_; }
	void commitSessionEmission(); // obiektowe warianty raz na sesję (lg_get_level)
	std::string lastFail;

	static std::string trimTcl(const std::string& s); // bez ścieżki i nawiasów, z .tcl

private:
	struct Door
	{
		int x, y, dir; // pole obszaru
	};
	struct ObjCon
	{
		float min = 0, max = 1e30f, stride = -1;
	};
	Piece* resolve(const std::string& tcl, bool create = false);
	void loadMeta(Piece& p);
	bool tryPlace(Piece& p, int gx, int gy, int margin);
	bool forcePlace(Piece& p, int gx, int gy);
	void mergeEdges(const Piece& p, int dx, int dy);
	bool validateFrontier() const;
	long long cost(const Piece& p, const Door& d);
	bool seedOrAdvance(bool anyForced);
	bool fail();

	const Vfs& vfs_;
	std::map<std::string, Piece> pieces_; // klucz: nazwa małymi literami
	struct Group
	{
		std::vector<std::string> tcls;
	};
	std::map<std::string, Group> groups_; // klucz małymi literami
	std::vector<std::string> groupOrder_;  // kolejność rejestracji grup (kolejność puli)
	std::vector<std::string> filterClasses_, pool_;
	std::map<std::string, ObjCon> objCons_;
	float ratioA_ = 0, ratioOpt_ = 0.1f, ratioMax_ = 1;
	int ax0_ = 0, ay0_ = 0, ax1_ = 0, ay1_ = 0;
	struct Forced
	{
		std::string tcl;
		int gx, gy;
	};
	std::vector<Forced> forced_;
	// stan przebiegu
	std::vector<int> grid_;
	int aw_ = 0, ah_ = 0, filled_ = 0, seedState_ = -1;
	std::vector<Door> frontier_;
	struct Placed
	{
		Piece* p;
		int gx, gy;
	};
	std::vector<Placed> placed_;
	std::map<std::string, float> objPlaced_;
	struct ObjPiece
	{
		int x, y;
		float r;
	};
	std::vector<ObjPiece> objPieces_;
	std::vector<Placement> level_;
	std::vector<Piece*> levelPieces_;
};

} // namespace ow
