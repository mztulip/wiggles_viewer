// Planista pracy krasnali (gra: CProdPlanner, docs/silnik/20_planista.md): pula bezczynnych krasnali (gnome_idle),
// zadania z świata (kopanie; dalej zbiory, produkcja, dowóz), dopasowanie krasnala i wysyłka zdarzenia evt_autoprod_*,
// które dalej prowadzą niezmienione skrypty gry. Prawa i stałe za Zimennik/Diggles-Unity (MIT),
// Docs/WORK_DISPATCHER_NATIVE.md i Runtime/Engine/Sim/ProdDispatcher.cs (adresy FUN_… gry tam cytowane);
// 🔶 o ile nie sprawdzone w laboratorium.
#pragma once

#include <array>
#include <map>
#include <set>
#include <string>
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
	void logicTick();     // tura logiki 0,1 s: odliczanie blokad

	// Komendy skryptów: gnome_idle obj 1|0, gnome_failed_work / gnome_stopped_work obj, gnome_announce_dig obj poz.
	void gnomeIdle(int id, bool idle);
	void releaseGnome(int id);
	void announceDig(int tx, int ty);

	// Diagnostyka (testy, /api)
	bool pooled(int id) const;
	size_t digRequests(int player) const { return reqs_[clampPlayer(player)].size(); }
	int dispatched = 0; // wysłane zadania od startu
	void sweepNow(int player) { sweep(clampPlayer(player)); }

private:
	static constexpr int kPlayers = 8;
	static int clampPlayer(int p) { return p >= 0 && p < kPlayers ? p : 0; }
	struct Job
	{
		int kind = 0; // 0 kopanie
		int tx = 0, ty = 0;
		float pos[3] = {0, 0, 0};
		float need = 0;
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
	std::array<std::set<int>, kPlayers> idle_;
	std::array<std::vector<DigRequest>, kPlayers> reqs_;
	std::array<int, kPlayers> scanX_{}, scanY_{};
	std::map<int, int> busy_;    // krasnal → tury do ponownego przydziału (gnome+0x3e8)
	std::map<int, int> breathe_; // krasnal → tury wstrzymania po porażce (5 s)
	std::vector<Reopen> reopen_; // miejsca zadań po porażce (2 s)

	void sweep(int player);
	void collectDigJobs(int player, std::vector<Job>& jobs);
	void maintainDigRequests(int player);
	bool activeDigger(int id, int player) const;
	bool digPos(int id, float out[3]) const;
	bool workTime(int id) const;
	bool autoSchedule(int id) const;
	bool jobOnCooldown(const float p[3]) const;
	float matchCost(int id, const Job& j, int player) const;
	bool reserve(const Job& j, int id);
	void raise(int id, const Job& j);
};

} // namespace ow
