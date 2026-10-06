// Symulacja świata wspólna dla gry (openwiggles) i przeglądarki (wigview): skrypty Tcl obiektów na scenie mapy,
// krasnale chodzące po siatce nawigacji (chód, wspinanie, spadanie, kopanie), płynne ruchy obiektów zlecane
// przez skrypty. Most skrypty ↔ scena: ID obiektu skryptu = ID z zapisu albo kolejne od 100000, idx(id) to
// indeks obiektu MapScene. Zasady: docs/silnik/11_obiekty_tcl_i_petla.md, docs/silnik/14_gra.md.
//
// Użycie: scena wczytuje mapę, potem initUnits() i startScripts(); w każdej klatce scena.update(dt)
// i simUpdate(dt). Gra dziedziczy po WorldSim i dokłada gracza (zaznaczenie, rozkazy, zapis).
#pragma once

#include "lib/map_scene.h"
#include "lib/nav.h"
#include "lib/script/engine.h"

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ow
{

constexpr float kGnomeZ = 8.0f;    // głębokość krasnala w skali skryptu (świat 11 bloków), jak w scenariuszach
// ✅ Idąca postać trzyma się co najmniej 1,5 przed tylną ścianą pola (skala skryptu: wartość siatki 8×8 BlockDB).
// W korytarzu GNG_004 (ściana 11) krasnal postawiony na z = 8 po pierwszych krokach ma w grze z = 12,5;
// 🔶 dalej od ściany zostaje na swojej głębokości (krasnale z zapisu mają w jednym polu z 5,2 … 13,8). BAZA_WIEDZY §8.12.
constexpr float kWallGap = 1.5f;
constexpr float kWalkSpeed = 1.6f; // pola na sekundę (🔶)
constexpr float kClimbSpeed = 0.9f;
constexpr float kDigTime = 2.0f; // sekundy kopania jednego pola (🔶)
constexpr float kSimPi = 3.14159265f;

struct SimUnit
{
	int obj = -1;
	float x = 0, y = 0; // stopy: x środka, y dołu pola (bloki)
	int tx = 0, ty = 0; // bieżące pole
	std::vector<std::pair<int, int>> path;
	size_t step = 0;
	enum State
	{
		Idle,
		Walk,
		Dig
	} state = Idle;
	int digX = -1, digY = -1;
	float digT = 0;
	float facing = -kSimPi * 0.5f;      // −π/2 = w prawo, +π/2 = w lewo (obserwacja modelu mann)
	float z = kGnomeZ;                  // głębokość (skala skryptu), z zapisu albo scenariusza
	std::function<void(bool)> onArrive; // akcja walk zlecona przez skrypt
	bool digJob = false;                // przydzielony do kopania zaznaczonych pól (gracz)
	float goalX = -1, goalZ = -1;       // dokładny cel chodu skryptu (x w polu, głębokość), −1 = środek pola
	// Rodzaj postaci i jej animacje ruchu (krasnal: mann.3db; wuker: model klasy).
	bool gnome = true;
	std::string walkAnim = "gehen_loop", climbUpAnim = "kletter_hoch_loop", climbDownAnim = "kletter_runter_loop";
	std::string standAnim = "standard", climbStandAnim = "kletterstand";
	bool controlled = false; // prowadzony ręcznie w przeglądarce (skrypt postaci wstrzymany)
};

// Klasy chodzące po siatce nawigacji: krasnal (Zwerg) i wukery. Nazwa klasy małymi literami.
bool isWalkerClass(const std::string& lowerClass);

class WorldSim : public script::Host
{
public:
	explicit WorldSim(MapScene& scene) : scene_(scene) {}

	// Silnik skryptów: utworzenie i start (klasy, 2684 w wersji polskiej; z pamięci podręcznej ok. 0,2 s).
	// locale: język tekstów gry (pusty = język wydania). false: błąd w err (silnik i tak działa).
	bool bootScripts(const Vfs& vfs, const std::string& locale, std::string& err);
	std::unique_ptr<script::Engine> scripts;
	bool scriptGnomes = true; // krasnale sterowane skryptami (inaczej tylko rozkazami gracza)
	bool revealFog = true;    // krasnale odkrywają mgłę (rozdział 10)

	// Po wczytaniu mapy: siatka nawigacji, krasnale z obiektów Zwerg, identyfikatory skryptów.
	void initUnits();
	// Po initUnits: świat skryptów od zera i obj_init wszystkich obiektów mapy (zmienne Tcl z zapisu).
	void startScripts();
	// Krok symulacji: tura logiki 10 Hz, ruch krasnali, mgła, płynne ruchy. Scena (scene.update) osobno.
	void simUpdate(float dt);

	// Identyfikatory skryptów
	int idOf(int index);
	bool valid(int id) const { return idToIdx.count(id) != 0; }
	int idx(int id) const;
	std::unordered_map<int, int> idToIdx;
	std::vector<int> idxToId;
	int nextScriptId = 100000;

	// Krasnale i nawigacja
	NavGrid nav;
	std::vector<SimUnit> units;
	int selected = -1;  // zaznaczony krasnal (gra), −1 = brak
	std::string status; // ostatni komunikat (znalezisko, brak drogi)
	float floorY(int x, int ty) const;
	// Wolna część pola w poziomie (bloki, [x0, x1]): kolumny siatki 8×8 z komórką < 13 w dolnej połowie, jak
	// NavGrid::openBlock. Pola przy krawędzi jaskini bywają otwarte tylko z jednej strony (kris_caves_1_b,
	// blok 43: lewa połowa półka, prawa lita skała), więc postać nie może stawać na środku pola.
	bool openSpan(int x, int ty, float& x0, float& x1) const;
	float tileCenterX(int x, int ty) const; // środek wolnej części pola
	int tileOfFeet(float y) const { return int(std::floor(y - 0.6f)); }
	void snapToFloor(SimUnit& u);
	// Ręczne sterowanie postacią (dodatek przeglądarki, nie z gry): skrypt postaci wstrzymany, zlecenia chodu
	// i animacji od skryptu ignorowane. controlStep: krok o pole (dx ±1 w bok, dy −1 w górę, +1 w dół), od celu
	// bieżącego chodu; controlGoTo: chód do pola. false: nie ma drogi.
	void setControlled(int unit, bool on);
	bool controlStep(int unit, int dx, int dy);
	bool controlGoTo(int unit, int tx, int ty);
	// Najmniejsza głębokość chodu w polu (x, ty): tylna ściana w kolumnie x + kWallGap (skala skryptu).
	float walkZ(float x, int ty) const;
	SimUnit* unitOf(int obj);
	int addUnit(int obj); // krasnal albo wuker (wg klasy obiektu)
	int gnomeCount() const;
	// Nowa postać (krasnal albo wuker) w najbliższym miejscu do stania przy (x, y) w blokach (y: stopy), ze
	// skryptem klasy, gdy silnik skryptów działa. Zwraca indeks w units.
	int spawnUnit(float x, float y, bool gnome = true);
	// Zaludnienie mapy bez krasnali (przeglądarka): krasnale i wukery na wolnych, suchych polach z podłożem
	// najbliżej punktu (cx, cy), co najmniej 2 pola od siebie. Przed startScripts (obiekty dostaną wtedy skrypty).
	void populate(float cx, float cy, int gnomes, int wukers);
	void updateUnit(SimUnit& u, float dt);
	// Kopanie
	std::set<std::pair<int, int>> digMarks; // pola zaznaczone do kopania
	void dig(int x, int y);
	bool nextDigJob(SimUnit& u);
	void markDig(int x0, int y0, int x1, int y1);

	struct Glide
	{
		int obj;
		float from[3], to[3], t, dur;
	};
	std::vector<Glide> glides; // płynne ruchy obiektów zlecone przez skrypty

	// script::Host
	int createObject(const std::string& cls, const std::string&, const float pos[3], const float rot[3]) override;
	void deleteObject(int id) override;
	bool getPos(int id, float pos[3]) override;
	void setPos(int id, const float pos[3]) override;
	void setRotY(int id, float r) override;
	float getRotY(int id) override;
	void setAnim(int id, const std::string& anim, int, int mode) override;
	double animLength(int id, const std::string& anim) override;
	bool walkTo(int id, const float target[3], std::function<void(bool)> done) override;
	float hmap(float x, float y) override;
	bool underwater(const float p[3]) override;
	bool findPlace(const float c[3], float r, float out[3]) override;
	int gnomePosition(int id) override;
	void glide(int id, const float to[3], float seconds) override;
	void link(int child, int parent, int link) override;
	bool linkOffset(int id, int link, float out[3]) override;
	void changeParticleSource(int id, int slot, const ParticleSource& s) override;
	void setParticleSource(int id, int slot, bool on) override;
	void setVisible(int id, bool v) override;

protected:
	void attachScripts();
	const Pmp* simGrid() const { return scene_.grid(); }
	MapScene& scene_;
	double scriptAcc_ = 0;
	float simTime_ = 0; // czas symulacji (ślad OW_TRACE_SIM=2)
	float fogTimer_ = 0;
};

} // namespace ow
