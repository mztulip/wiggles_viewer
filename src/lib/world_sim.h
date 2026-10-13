// Symulacja świata wspólna dla gry (openwiggles) i przeglądarki (wigview): skrypty Tcl obiektów na scenie mapy,
// postacie chodzące jak w grze (NavWorld + walk_plan, rozdział 17: chód, wspinanie, spadanie, kopanie), płynne ruchy obiektów zlecane
// przez skrypty. Most skrypty ↔ scena: ID obiektu skryptu = ID z zapisu albo kolejne od 100000, idx(id) to
// indeks obiektu MapScene. Zasady: docs/silnik/11_obiekty_tcl_i_petla.md, docs/silnik/14_gra.md.
//
// Użycie: scena wczytuje mapę, potem initUnits() i startScripts(); w każdej klatce scena.update(dt)
// i simUpdate(dt). Gra dziedziczy po WorldSim i dokłada gracza (zaznaczenie, rozkazy, zapis).
#pragma once

#include "lib/dig_terrain.h"
#include "lib/map_scene.h"
#include "lib/nav.h"
#include "lib/nav_world.h"
#include "lib/prod_planner.h"
#include "lib/walk_plan.h"
#include "lib/script/engine.h"

#include <cmath>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ow
{

constexpr float kGnomeZ = 8.0f; // głębokość nowej postaci przed dopasowaniem do podłogi (skala skryptu)
constexpr float kDigTime = 2.0f; // sekundy kopania jednego pola (🔶)
constexpr float kFallSpeed = 3.0f; // spadanie, bloki na sekundę (🔶)
constexpr float kSimPi = 3.14159265f;

struct SimUnit
{
	int obj = -1;
	float x = 0, y = 0; // stopy (bloki): na linii podłogi albo na ścianie
	int tx = 0, ty = 0; // pole stóp (podpole round(x · 8), round(y · 8) >> 3)
	enum State
	{
		Idle,
		Walk,
		Dig,
		Fall
	} state = Idle;
	int digX = -1, digY = -1;
	float digT = 0;
	float facing = -kSimPi * 0.5f;      // −π/2 = w prawo, +π/2 = w lewo, 0 = do przodu, π = w głąb
	float z = kGnomeZ;                  // głębokość (skala skryptu)
	std::function<void(bool)> onArrive; // akcja walk zlecona przez skrypt
	bool digJob = false;                // przydzielony do kopania zaznaczonych pól (gracz)
	bool gnome = true;
	bool canClimb = true;
	// Trasa (walk_plan) i wykonawca chodu (MetaWalk, docs/silnik/17_ruch.md §17.6): faza start / pętla / stop,
	// obrót w miejscu przy błędzie kierunku ≥ π/4, po 2 s marszu bieg (speedtype 0).
	std::vector<WalkPoint> route;
	size_t next = 0; // indeks punktu, do którego idzie
	enum Phase
	{
		Start,
		Loop,
		Stop
	} phase = Start;
	bool entered = false, started = false, turning = false, running = false, canRun = true, climbing = false;
	int speedtype = 0;       // action walk -speedtype
	bool walkMayRun = true;  // action walk -animsets {… −1}: ten chód bez biegu
	float phaseRem = 0, turnRate = 0, stride = 0;
	// Animacje (krasnal: mann.3db, set_class_anim w ACTORS.TCL; wuker i stworzenia: z modelu).
	std::string walkStart = "gehen_start", walkLoop = "gehen_loop", walkStop = "gehen_end";
	std::string runStart = "laufen_schnell_start", runLoop = "laufen_schnell_loop", runStop = "laufen_schnell_end";
	std::string turnLeft = "drehen_links", turnRight = "drehen_rechts", turnLeft180 = "drehen_ganz_links", turnRight180 = "drehen_ganz";
	std::string climbUpAnim = "kletter_hoch_loop", climbDownAnim = "kletter_runter_loop", climbStandAnim = "kletterstand";
	std::string climbRightAnim = "kletter_rechts_loop", climbLeftAnim = "kletter_links_loop";
	std::string climbFast[4] = {"kletter_hoch_schnell", "kletter_runter_schnell", "kletter_rechts_schnell", "kletter_links_schnell"};
	std::string mountAnim = "stand_zu_kletterstand", dismountAnim = "kletterstand_zu_stand";
	std::string standAnim = "standard", fallAnim = "fallen_loop";
	float walkSpeed = 0.8111f, runSpeed = 1.7143f, climbSpeed = 0.4155f; // bloki na sekundę, z animacji (addUnit)
	float climbFastSpeed = 0;                                            // wspinanie biegiem (-speedtype 2), 0 = brak
	// Wspinanie (CAct_Climb, rozdział 17.8): wejście na ścianę, ruch po niej, zejście.
	enum ClimbPhase
	{
		NoClimb,
		Mount,
		Advance,
		Dismount
	} climbPhase = NoClimb;
	std::string climbClip; // bieżąca animacja ruchu po ścianie
	bool controlled = false; // prowadzony ręcznie w przeglądarce (skrypt postaci wstrzymany)
	// Prowadzony ręcznie: kierunek ruchu z wciśniętych klawiszy (−1, 0, +1): moveX w bok, moveZ w głąb (−1)
	// albo do przodu (+1). Ruch z prędkością marszu w każdym kroku symulacji; 0, 0 = stoi.
	float moveX = 0, moveZ = 0;
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

	// Postacie i ruch
	NavGrid nav;     // pola otwarte / lite (zaznaczanie kopania, podpowiedzi); ruch: walk
	NavWorld walk;   // dane ruchu jak w grze (rozdział 17)
	std::vector<SimUnit> units;
	int selected = -1;  // zaznaczony krasnal (gra), −1 = brak
	std::string status; // ostatni komunikat (znalezisko, brak drogi)
	// Najbliższe miejsce do stania przy (x, y) (bloki): podpole podłogi w promieniu r bloków, x i y na nim,
	// z wpasowane w pas głębokości podłogi. false: brak.
	bool floorSpot(float x, float y, float r, float z, float out[3]) const;
	// Głębokość z wpasowana w pas podłogi w punkcie (x, y); poza podłogą co najmniej wolne od skały.
	float fitDepth(float x, float y, float z) const;
	float minDepth(float x, float y) const; // najmniejsza dozwolona głębokość w punkcie (dev API, ślad)
	void snapToFloor(SimUnit& u);
	void updateTile(SimUnit& u) const;
	// Chód postaci do punktu (x, y, z): trasa walk_plan i wykonawca. false: brak drogi (why: powód).
	bool walkUnit(SimUnit& u, float x, float y, float z, bool keepGoal = false, std::string* why = nullptr);
	// Ręczne sterowanie postacią (dodatek przeglądarki, nie z gry): skrypt postaci wstrzymany, zlecenia chodu
	// i animacji od skryptu ignorowane. controlStep: krok o pole (dx ±1 w bok, dy −1 w górę, +1 w dół);
	// controlGoTo: chód do pola. false: nie ma drogi.
	void setControlled(int unit, bool on);
	bool controlStep(int unit, int dx, int dy);
	bool controlGoTo(int unit, int tx, int ty);
	// Idź w miejsce pod kursorem (promień w układzie sceny, jak MapScene::pickTerrain): trafiony punkt terenu
	// (x, y, głębokość) jako cel chodu jak w grze.
	bool controlGoToRay(int unit, const glm::vec3& ro, const glm::vec3& rd);
	// Kierunek ruchu prowadzonej postaci (trzymane klawisze): dx −1/0/+1 w bok, dz −1 w głąb, +1 do przodu.
	void controlMove(int unit, float dx, float dz);
	// Stworzenie z mapy (chomik, pająk, troll…) jako postać symulacji, jeśli jego model ma animację chodu:
	// indeks w units albo −1. Krasnale i wukery już są postaciami (initUnits).
	int adoptUnit(int obj);
	static std::string creatureAnim(const std::vector<std::string>& anims, bool walk);
	void manualMove(SimUnit& u, float dt); // krok ruchu ręcznego (controlMove)
	SimUnit* unitOf(int obj);
	int addUnit(int obj); // krasnal albo wuker (wg klasy obiektu)
	int gnomeCount() const;
	// Nowa postać (krasnal albo wuker) w najbliższym miejscu do stania przy (x, y) w blokach (y: stopy), ze
	// skryptem klasy, gdy silnik skryptów działa. Zwraca indeks w units.
	int spawnUnit(float x, float y, bool gnome = true);
	// Zaludnienie mapy bez krasnali (przeglądarka): krasnale i wukery na wolnych, suchych polach z podłogą
	// najbliżej punktu (cx, cy), co najmniej 2 pola od siebie. Przed startScripts (obiekty dostaną wtedy skrypty).
	void populate(float cx, float cy, int gnomes, int wukers);
	void updateUnit(SimUnit& u, float dt);
	void stepWalk(SimUnit& u, float dt); // wykonawca chodu (MetaWalk)
	void finishWalk(SimUnit& u, bool ok);
	// Kopanie
	std::set<std::pair<int, int>> digMarks; // pola zaznaczone do kopania
	// Kopanie pola (x, y) do końca, jak tunel w grze: znacznik tunelu (cel 11) i kroki wycinania w czterech węzłach
	// pola (narożniki), zszyte z sąsiadami grafem BlockDB (docs/silnik/19_kopanie.md).
	void dig(int x, int y);
	DigTerrain digTerrain; // znaczniki graczy i materiał pól (dig_mark, dig_next, dig_apply)
	ProdPlanner planner{*this}; // planista pracy: bezczynne krasnale dostają evt_autoprod_* (docs/silnik/20_planista.md)
	// Krasnale ze skryptami kopią przez planistę i skrypty gry (evt_autoprod_dig → z_dignwalk.tcl), inaczej naszą pętlą.
	bool scriptDigging() const { return scripts && scriptGnomes; }
	// Jeden krok wycinania węzła (dig_apply) z przebudową ruchu zmienionych pól i terenu (rebuild: od razu;
	// inaczej terrainDirty i jedna przebudowa później).
	bool carveNode(int nx, int ny, int owner, int& dugId, bool rebuild = true);
	bool terrainDirty = false;
	bool nextDigJob(SimUnit& u);
	bool digNeeded(int x, int y) const;
	// Kopanie pola (mx, my) jak dig_starttask gry: pole startu get_digedge, miejsce vector_fixdig, chód
	// z -dig 1 (bez chodu, gdy miejsce bliżej niż 0,5), potem kopanie.
	bool digAt(SimUnit& u, int mx, int my);
	// get_digedge (rozdział 17.7): przeszukanie wszerz od pola (sx, sy) przez pola zaznaczone do kopania;
	// wynik: otwarte pole (wysokość lewego górnego podpola bloku < 12), odkryte, najbliższe polu (rx, ry).
	bool digEdgeTile(int sx, int sy, int rx, int ry, int& ex, int& ey) const;
	// vector_fixdig (rozdział 17.7): miejsce do stania przy kopanym punkcie wall od strony postaci.
	bool fixDigPos(const float wall[3], const float gnome[3], float out[3]) const;
	void markDig(int x0, int y0, int x1, int y1);

	struct Glide
	{
		int obj;
		float from[3], to[3], t, dur;
		bool face = true; // obrót w stronę ruchu w bok
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
	bool walkTo(int id, const float target[3], const WalkOpts& o, std::function<void(bool)> done) override;
	float hmap(float x, float y) override;
	bool underwater(const float p[3]) override;
	bool findPlace(const float c[3], float r, float out[3]) override;
	int gnomePosition(int id) override;
	bool digEdge(const float start[3], int id, float out[3]) override;
	bool fixDig(const float wall[3], const float gnome[3], float out[3]) override { return fixDigPos(wall, gnome, out); }
	void glide(int id, const float to[3], float seconds, bool face = true) override;
	void link(int child, int parent, int link) override;
	bool linkOffset(int id, int link, float out[3]) override;
	bool linkEuler(int id, int link, float out[3]) override;
	void changeParticleSource(int id, int slot, const ParticleSource& s) override;
	void setParticleSource(int id, int slot, bool on) override;
	void setVisible(int id, bool v) override;
	int material(const float p[3]) override { return digTerrain.material(int(std::lround(p[0])), int(std::lround(p[1]))); }
	void digMark(int owner, int x, int y, int mode, int cnt) override;
	bool isDigMarked(int x1, int y1, int x2, int y2) override;
	bool digNext(const float pos[3], int owner, int gnomeId, int tryN, bool matchId, float out[3]) override;
	bool digApply(int nx, int ny, int owner, int& id) override { return carveNode(nx, ny, owner, id); }
	void incDigMarkId(int) override { ++digTerrain.playerDigMarkId; }
	void gnomeIdle(int id, bool idle) override { planner.gnomeIdle(id, idle); }
	void gnomeFailedWork(int id) override { planner.releaseGnome(id); }
	void announceDig(int x, int y) override { planner.announceDig(x, y); }
	bool getView(float v[5]) override { return viewGet && viewGet(v); }
	void setView(const float v[5], bool move) override
	{
		if (viewSet) viewSet(v, move);
	}
	// Kamera programu (gra, wigview) dla get_view / set_view ze skryptów.
	std::function<bool(float[5])> viewGet;
	std::function<void(const float[5], bool)> viewSet;

protected:
	void attachScripts();
	const Pmp* simGrid() const { return scene_.grid(); }
	MapScene& scene_;
	double scriptAcc_ = 0;
	float simTime_ = 0; // czas symulacji (ślad OW_TRACE_SIM=2)
	float fogTimer_ = 0;
	int selSync_ = 0; // zaznaczenie zsynchronizowane między grą (selected) a skryptami (selection_)
	void syncSelection();
};

} // namespace ow
