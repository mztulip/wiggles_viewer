// Planista pracy krasnali (gra: CProdPlanner, docs/silnik/20_planista.md): pula bezczynnych krasnali (gnome_idle),
// zadania ze świata (kopanie, zbiory, produkcja, wynalazki, pakowanie i rozpakowanie, dowóz materiałów),
// dopasowanie krasnala i wysyłka zdarzenia evt_autoprod_* (część zadań jako plany krokowe), które dalej prowadzą
// niezmienione skrypty gry. Prawa i stałe za Zimennik/Diggles-Unity (MIT), Docs/WORK_DISPATCHER_NATIVE.md i
// Runtime/Engine/Sim/ProdDispatcher.cs (adresy FUN_… gry tam cytowane); 🔶 o ile nie sprawdzone w laboratorium.
#pragma once

#include <array>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ow
{

class WorldSim;

class ProdPlanner
{
public:
	explicit ProdPlanner(WorldSim& w) : w_(w) {}
	void reset();
	void tick(double dt); // krok symulacji: co 0,5 s przegląd jednego gracza (po kolei 0..7)
	void logicTick();     // tura logiki 0,1 s: odliczanie blokad, priorytety zamówień

	// Komendy skryptów: gnome_idle obj 1|0, gnome_failed_work / gnome_stopped_work obj, gnome_announce_dig obj poz.
	void gnomeIdle(int id, bool idle);
	void releaseGnome(int id);
	void announceDig(int tx, int ty);

	// Diagnostyka (testy, /api)
	bool pooled(int id) const;
	size_t digRequests(int player) const { return reqs_[size_t(clampPlayer(player))].size(); }
	int dispatched = 0;                  // wysłane zadania od startu
	std::map<std::string, int> sentKind; // liczba wysłanych zadań wg rodzaju (dig, harvest, workat, invent, pack, unpack, haul)
	int claimOf(int place) const;        // krasnal pracujący przy budynku (0 = nikt)
	void sweepNow(int player) { sweep(clampPlayer(player)); }

private:
	static constexpr int kPlayers = 8;
	static int clampPlayer(int p) { return p >= 0 && p < kPlayers ? p : 0; }
	enum Kind
	{
		Dig,
		Harvest,
		Production,
		Pack,
		Unpack,
		Haul
	};
	struct Job
	{
		Kind kind = Dig;
		int target = 0;  // obiekt (zasób, budynek, skrzynia, cel dowozu)
		std::string cls; // klasa (zasób, przedmiot; wynalazek z przedrostkiem Bp)
		int item = 0;    // dowóz: przedmiot do przyniesienia
		int tx = 0, ty = 0;
		float pos[3] = {0, 0, 0};
		float need = 0;
		bool asBox = false;
	};
	struct Plan // plan krokowy krasnala (pakowanie, rozpakowanie, dowóz): krok co zgłoszenie bezczynności
	{
		Kind kind = Pack;
		int item = 0, dest = 0;
		std::string cls;
		float orderPos[3] = {0, 0, 0};
		bool asBox = false;
		int stage = 0;
	};
	struct DigRequest
	{
		int x = 0, y = 0;
		float timer = 0, age = 0;
		bool hadJob = false;
	};
	struct Reopen
	{
		float pos[3];
		int ticks;
	};
	WorldSim& w_;
	double accum_ = 0;
	int cursor_ = 0;
	long logicTick_ = 0;
	std::array<std::set<int>, kPlayers> idle_;
	std::array<std::vector<DigRequest>, kPlayers> reqs_;
	std::array<int, kPlayers> scanX_{}, scanY_{};
	std::array<int, kPlayers> harvestCooldown_{};
	std::map<int, int> busy_;    // obiekt → tury blokady (krasnal po przydziale, przedmiot w drodze)
	std::map<int, int> breathe_; // krasnal → tury wstrzymania po porażce (5 s)
	std::vector<Reopen> reopen_; // miejsca zadań po porażce (2 s)
	std::map<int, int> claim_;   // budynek → krasnal, który przy nim pracuje
	std::map<int, Plan> plans_;  // krasnal → plan krokowy
	std::map<std::pair<int, std::string>, std::pair<float, long>> slotPrio_; // (budynek, przedmiot) → priorytet zamówienia
	std::map<std::string, std::vector<std::string>> materials_;              // klasa budynku + przedmiot → materiały

	void sweep(int player);
	void collectDigJobs(int player, std::vector<Job>& jobs);
	void maintainDigRequests(int player);
	void collectPlaceJobs(int player, std::vector<Job>& jobs);
	void collectInventionJobs(int place, int player, std::vector<Job>& jobs);
	void collectProductionJobs(int place, int player, std::vector<Job>& jobs);
	void collectHarvestJobs(int player, std::vector<Job>& jobs);
	bool placeAvailable(int place);
	bool activeDigger(int id, int player) const;
	bool digPos(int id, float out[3]) const;
	bool workTime(int id) const;
	bool flag(int id, const char* prop, bool def) const;
	bool jobOnCooldown(const float p[3]) const;
	bool planTargets(int item) const;
	float slotPriority(int place, const std::string& item, int count);
	double ownerAttrib(int player, const std::string& name);
	std::vector<std::string> materialsFor(int place, const std::string& item);
	std::vector<std::string> missingMaterials(int place, const std::string& item);
	std::vector<std::pair<std::string, float>> inventRequirements(int place, const std::string& item);
	bool qualifies(int gnome, const std::vector<std::pair<std::string, float>>& req);
	int haulSource(int place, const std::string& cls, int player, const std::set<int>& exclude);
	bool match(int id, const Job& j);
	float matchCost(int id, const Job& j, int player) const;
	bool reserve(const Job& j, int id);
	void assign(int id, const Job& j, int player);
	void releaseClaim(int place);
	void advancePlan(int id, Plan& p);
	void dropPlan(int id, bool rearm);
	bool emit(int id, const std::string& cmd, const char* kind);
};

} // namespace ow
