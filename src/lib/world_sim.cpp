#include "lib/world_sim.h"

#include "lib/blockdb.h"
#include "lib/i18n.h"
#include "lib/map_template.h"
#include "lib/vfs.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>

namespace ow
{

bool isWalkerClass(const std::string& c)
{
	return c == "zwerg" || c == "wuker" || c == "schwefelwuker";
}

int WorldSim::gnomeCount() const
{
	int n = 0;
	for (const auto& u : units) n += u.gnome;
	return n;
}

bool WorldSim::bootScripts(const Vfs& vfs, const std::string& locale, std::string& err)
{
	scripts = std::make_unique<script::Engine>(vfs);
	scripts->setHost(this);
	const auto& langs = scripts->messages().languages();
	if (!locale.empty() && std::find(langs.begin(), langs.end(), locale) != langs.end()) scripts->setLocale(locale);
	return scripts->boot(err);
}

void WorldSim::initUnits()
{
	units.clear();
	idToIdx.clear(), idxToId.clear(), nextScriptId = 100000;
	digMarks.clear(), glides.clear();
	for (int i = 0; i < scene_.objectCount(); ++i) idOf(i);
	selected = -1;
	if (const Pmp* g = simGrid()) nav.build(*g, scene_.blockDb()), walk.build(*g, scene_.blockDb()), digTerrain.init(*g);
	for (int i = 0; i < scene_.objectCount(); ++i)
		if (isWalkerClass(Vfs::lower(scene_.objectClass(i)))) addUnit(i);
	for (size_t k = 0; k < units.size(); ++k)
		if (units[k].gnome)
		{
			selected = int(k);
			break;
		}
}

void WorldSim::startScripts()
{
	if (!scripts) return;
	scripts->resetWorld();
	attachScripts();
}

// Zaznaczenie: gra zmienia selected (indeks postaci), skrypty selection_ (uchwyty). Wygrywa strona, która
// zmieniła się od ostatniej synchronizacji.
void WorldSim::syncSelection()
{
	if (!scripts) return;
	int mine = selected >= 0 && selected < int(units.size()) ? idOf(units[size_t(selected)].obj) : 0;
	auto& s = scripts->selection_;
	int theirs = s.empty() ? 0 : s.front();
	if (mine != selSync_) scripts->selectOnly(mine), selSync_ = mine;
	else if (theirs != selSync_)
	{
		selected = -1;
		for (size_t k = 0; k < units.size(); ++k)
			if (theirs && idOf(units[k].obj) == theirs) selected = int(k);
		selSync_ = selected >= 0 ? theirs : 0;
	}
}

void WorldSim::simUpdate(float dt)
{
	syncSelection();
	if (scripts)
	{ // tura logiki 10 razy na sekundę (rozdział 11.7)
		scriptAcc_ += dt;
		while (scriptAcc_ >= 0.1)
		{
			scriptAcc_ -= 0.1;
			scripts->tick(0.1);
		}
	}
	for (auto& u : units) updateUnit(u, dt);
	static const bool trace = std::getenv("OW_TRACE_SIM") && std::atoi(std::getenv("OW_TRACE_SIM")) >= 2;
	if (trace && int((simTime_ + dt) * 2.f) != int(simTime_ * 2.f)) // OW_TRACE_SIM=2: postacie co 0,5 s
		for (const auto& u : units)
			std::fprintf(stderr, "[sim %.1f] %s x %.2f y %.2f pole (%d, %d) z %.2f (min %.2f) stan %d\n", simTime_ + dt,
			    u.gnome ? "krasnal" : "wuker", u.x, u.y, u.tx, u.ty, u.z, minDepth(u.x, u.y), int(u.state));
	simTime_ += dt;
	fogTimer_ += dt;
	if (revealFog && fogTimer_ >= 0.4f)
	{ // mgła co 4 tury logiki (rozdział 10.2)
		fogTimer_ = 0;
		for (auto& u : units) scene_.revealFog(u.tx, u.ty, 6);
	}
	for (auto it = glides.begin(); it != glides.end();)
	{
		it->t = std::min(it->t + dt, it->dur);
		float k = it->dur > 0 ? it->t / it->dur : 1.f, p[3];
		for (int i = 0; i < 3; ++i) p[i] = it->from[i] + (it->to[i] - it->from[i]) * k;
		float dx = it->to[0] - it->from[0];
		scene_.setObjectPose(it->obj, p[0], p[1], p[2],
		    it->face && std::abs(dx) > 1e-3f ? (dx > 0 ? -kSimPi * 0.5f : kSimPi * 0.5f) : scene_.objectRotY(it->obj));
		it = it->t >= it->dur ? glides.erase(it) : it + 1;
	}
}


namespace
{
int sub8(float v)
{ // podpole współrzędnej (bloki · 8, zaokrąglenie jak w grze)
	return int(std::nearbyint(v * 8.f));
}
float wrapAngle(float a)
{
	while (a > kSimPi) a -= 2.f * kSimPi;
	while (a < -kSimPi) a += 2.f * kSimPi;
	return a;
}
// Kierunek patrzenia na ruch (dx w bok, dz w głąb w jednostkach świata): w prawo −π/2, do przodu 0.
float facingOf(float dx, float dzWorld)
{
	return std::atan2(-dx, dzWorld);
}
} // namespace

bool WorldSim::floorSpot(float x, float y, float r, float z, float out[3]) const
{
	int cx = sub8(x), cy = sub8(y), R = std::max(1, int(std::ceil(r * 8.f)));
	long bd = -1;
	int bx = 0, by = 0;
	for (int dy = -R; dy <= R; ++dy)
		for (int dx = -R; dx <= R; ++dx)
		{
			int sx = cx + dx, sy = cy + dy;
			// bliżej w bok i w dół (stopy spadają na podłogę pod spodem) niż w górę
			long d = long(dx) * dx + long(dy) * dy * (dy < 0 ? 4 : 1);
			if (bd >= 0 && d >= bd) continue;
			float z0, z1;
			if (!walk.floor(sx, sy) || !walk.walkable(sx, sy) || walk.baseCost(sx >> 3, sy >> 3) == -1 || !floorBand(walk, sx, sy, z0, z1)) continue;
			bd = d, bx = sx, by = sy;
		}
	if (bd < 0) return false;
	out[0] = float(bx) / 8.f, out[1] = float(by) / 8.f, out[2] = fitDepth(out[0], out[1], z);
	return true;
}

// Na podłodze: z w pasie podłogi; za tylną ścianą (z przed pasem, np. nowa postać): głębokość stania
// min(podparcie + 1, 14,5) jak w grze. Poza podłogą (ściana): co najmniej wolne od skały.
float WorldSim::fitDepth(float x, float y, float z) const
{
	int sx = sub8(x), sy = sub8(y);
	float z0, z1;
	if (walk.floor(sx, sy) && floorBand(walk, sx, sy, z0, z1))
	{
		if (z < z0) return std::clamp(floorLineDepth(walk, sx, sy), z0, z1);
		return std::min(z, z1);
	}
	return std::max(z, airDepth(walk, x, y));
}

float WorldSim::minDepth(float x, float y) const
{
	float z0, z1;
	if (walk.floor(sub8(x), sub8(y)) && floorBand(walk, sub8(x), sub8(y), z0, z1)) return z0;
	return airDepth(walk, x, y);
}

void WorldSim::updateTile(SimUnit& u) const
{ // pole wolnego podpola nad stopą
	u.tx = std::clamp(sub8(u.x) >> 3, 0, std::max(0, walk.width() - 1));
	u.ty = std::clamp((sub8(u.y) - 1) >> 3, 0, std::max(0, walk.height() - 1));
}

void WorldSim::snapToFloor(SimUnit& u)
{ // najpierw podłoga w tej samej kolumnie (x bez zmian: bez przeskoku przy przejęciu postaci), pod stopami
	// albo trochę wyżej; potem najbliższa w promieniu; bez podłogi: ściana w kolumnie (postać wisi)
	int sx = sub8(u.x), sy = sub8(u.y);
	for (int d = 0; d < 24; ++d)
		for (int yy : {sy + d, sy - d / 2})
		{
			float z0, z1;
			if (!walk.floor(sx, yy) || walk.baseCost(sx >> 3, yy >> 3) == -1 || !floorBand(walk, sx, yy, z0, z1)) continue;
			u.y = float(yy) / 8.f, u.z = fitDepth(u.x, u.y, u.z);
			updateTile(u);
			return;
		}
	float p[3];
	if (floorSpot(u.x, u.y, 6.f, u.z, p))
		u.x = p[0], u.y = p[1], u.z = p[2];
	else
		for (int d = 0; d < 48; ++d)
			if (walk.walkable(sx, sy + d) || walk.walkable(sx, sy - d))
			{
				u.y = float(walk.walkable(sx, sy + d) ? sy + d : sy - d) / 8.f;
				u.z = fitDepth(u.x, u.y, u.z);
				break;
			}
	updateTile(u);
}

SimUnit* WorldSim::unitOf(int obj)
{
	for (auto& u : units)
		if (u.obj == obj) return &u;
	return nullptr;
}

int WorldSim::addUnit(int obj)
{
	bool gnome = Vfs::lower(scene_.objectClass(obj)) == "zwerg";
	if (gnome)
	{ // płeć: zmienna gnome_gender z zapisu; dodany krasnal: tak, żeby kobiet i mężczyzn było po równo
		// (auto_choose_gender). Kobieta: model frau z tymi samymi animacjami (set_alternateanimdb, z_methods.tcl).
		std::string gender;
		for (const auto& [k, v] : scene_.objectSource(obj).tclVars)
			if (k == "gnome_gender") gender = v;
		if (gender.empty())
		{
			int women = 0, men = 0;
			for (const auto& x : units)
				if (x.gnome) (scene_.objectModelObject(x.obj) == "frau" ? women : men)++;
			gender = women < men ? "female" : "male";
		}
		bool female = gender == "female";
		scene_.setObjectModel(obj, female ? "frau.3db" : "mann.3db", female ? "frau" : "mann");
	}
	SimUnit u;
	u.obj = obj;
	u.gnome = gnome;
	if (!gnome && isWalkerClass(Vfs::lower(scene_.objectClass(obj))))
	{ // wuker: animacje z modelu wuker.3db (obj_init: set_anim this wuker.…); klasa nie ma class_defaultanim
		if (scene_.objectModelObject(obj).empty()) scene_.setObjectModel(obj, "", "wuker");
		u.runStart = u.runLoop = u.runStop = "";
		u.turnLeft = "drehen_links_a", u.turnRight = "drehen_rechts_a", u.turnLeft180 = u.turnRight180 = "drehen_ganz";
		u.climbUpAnim = "kletter_hoch", u.climbDownAnim = "kletter_runter", u.standAnim = "stand_atmen_a";
		u.fallAnim = "kletter_runterfallen";
		u.climbRightAnim = "kletter_rechts", u.climbLeftAnim = "kletter_links";
		for (auto& c : u.climbFast) c.clear();
	}
	else if (!gnome)
	{ // inne stworzenie (adoptUnit): chód i stanie z animacji jego modelu, bez faz, obrotów i wspinania
		auto anims = scene_.objectAnimations(obj);
		u.walkLoop = creatureAnim(anims, true), u.standAnim = creatureAnim(anims, false);
		u.walkStart = u.walkStop = u.runStart = u.runLoop = u.runStop = "";
		u.turnLeft = u.turnRight = u.turnLeft180 = u.turnRight180 = "";
		u.climbUpAnim = u.climbDownAnim = u.walkLoop, u.climbStandAnim = u.fallAnim = u.standAnim;
		u.canClimb = false;
		u.climbRightAnim = u.climbLeftAnim = u.mountAnim = u.dismountAnim = "";
		for (auto& c : u.climbFast) c.clear();
	}
	// Prędkości z ruchu zapisanego w animacjach (|move| · 0,05 bloku na sekundę; za Diggles-Unity, odczyt
	// z pamięci gry: mann gehen_loop 16,22 → 0,811, laufen_schnell 34,29 → 1,714); wspinanie: ¼ sumy czterech.
	auto speed = [&](const std::string& a) {
		Vec3f m{}, r{};
		int f = 0;
		return !a.empty() && scene_.animMotion(obj, a, m, r, f) ? std::sqrt(m.x * m.x + m.y * m.y + m.z * m.z) * 0.05f : 0.f;
	};
	if (float v = speed(u.walkLoop); v > 0.01f) u.walkSpeed = v;
	if (float v = speed(u.runLoop); v > 0.01f) u.runSpeed = v;
	u.canRun = !u.runLoop.empty() && speed(u.runLoop) > 0.01f;
	if (u.canClimb)
	{
		float c = 0.25f * (speed(u.climbUpAnim) + speed(u.climbDownAnim) + speed(u.climbRightAnim) + speed(u.climbLeftAnim));
		if (c > 0.01f) u.climbSpeed = c;
		u.climbFastSpeed = 0.25f * (speed(u.climbFast[0]) + speed(u.climbFast[1]) + speed(u.climbFast[2]) + speed(u.climbFast[3]));
	}
	const float* p = scene_.objectPos(obj);
	u.x = p[0], u.y = p[1], u.z = p[2];
	snapToFloor(u);
	static const bool trace = std::getenv("OW_TRACE_SIM") && std::atoi(std::getenv("OW_TRACE_SIM")) >= 2;
	if (trace)
	{
		auto rot = [&](const std::string& a) {
			Vec3f m{}, r{};
			int f = 0;
			return !a.empty() && scene_.animMotion(obj, a, m, r, f) ? std::max({std::abs(r.x), std::abs(r.y), std::abs(r.z)}) : 0.f;
		};
		std::fprintf(stderr, "[sim] postać %s: chód %.3f, bieg %.3f, wspinanie %.3f bloku/s; obrót %.3f / %.3f rad na 0,1 s\n",
		    gnome ? "krasnal" : "wuker", u.walkSpeed, u.canRun ? u.runSpeed : 0.f, u.climbSpeed, rot(u.turnLeft), rot(u.turnRight180));
	}
	units.push_back(u);
	return int(units.size()) - 1;
}

// Animacja chodu (walk) albo stania stworzenia: pierwsza istniejąca z listy, potem dowolna z „geh”/„lauf”
// (chód) albo „stand” (stanie) w nazwie. Pusta: model nie ma takiej animacji.
std::string WorldSim::creatureAnim(const std::vector<std::string>& anims, bool walk)
{
	auto has = [&](const std::string& n) { return std::find(anims.begin(), anims.end(), n) != anims.end(); };
	static const std::vector<std::string> kWalk = {"gehen_loop", "laufen_loop", "lauf_a_loop", "stehen_gehen"},
	                                      kStand = {"standard", "stand_anim", "standanim", "stand"};
	for (const auto& n : walk ? kWalk : kStand)
		if (has(n)) return n;
	for (const auto& a : anims)
	{
		std::string l = Vfs::lower(a);
		if (walk ? (l.find("geh") != std::string::npos || l.find("lauf") != std::string::npos) && l.find("schwert") == std::string::npos
		         : l.find("stand") != std::string::npos)
			return a;
	}
	return walk ? std::string() : (anims.empty() ? std::string() : anims.front());
}

int WorldSim::adoptUnit(int obj)
{
	if (obj < 0 || obj >= scene_.objectCount()) return -1;
	for (size_t k = 0; k < units.size(); ++k)
		if (units[k].obj == obj) return int(k);
	if (creatureAnim(scene_.objectAnimations(obj), true).empty()) return -1;
	idOf(obj);
	return addUnit(obj);
}

void WorldSim::controlMove(int unit, float dx, float dz)
{
	if (unit < 0 || unit >= int(units.size())) return;
	units[size_t(unit)].moveX = std::clamp(dx, -1.f, 1.f), units[size_t(unit)].moveZ = std::clamp(dz, -1.f, 1.f);
}

// Ruch ręczny o jeden krok symulacji z prędkością marszu: w bok po linii podłogi (w sąsiedniej kolumnie
// podpól podłoga najwyżej 3 podpola wyżej albo niżej, inaczej ściana), w głąb w pasie głębokości podłogi.
void WorldSim::manualMove(SimUnit& u, float dt)
{
	float sp = u.walkSpeed;
	int sy = sub8(u.y);
	if (u.moveX != 0 && hasFloorSupport(walk, u.x, u.y))
	{
		float nx = u.x + u.moveX * sp * dt;
		int sx = sub8(nx);
		for (int dy : {0, -1, 1, -2, 2, -3, 3})
		{
			float z0, z1;
			if (!walk.floor(sx, sy + dy) || !floorBand(walk, sx, sy + dy, z0, z1)) continue;
			u.x = nx, u.y = float(sy + dy) / 8.f;
			break;
		}
	}
	if (u.moveZ != 0) u.z += u.moveZ * 2.f * sp * dt; // z skryptu = 2 · z świata
	u.z = std::clamp(fitDepth(u.x, u.y, u.z), 0.f, 15.75f);
	u.facing = facingOf(u.moveX, u.moveZ); // przodem w stronę ruchu, także po skosie
	scene_.playAnim(u.obj, u.walkLoop, true);
}

int WorldSim::spawnUnit(float x, float y, bool gnome)
{
	const char* cls = gnome ? "Zwerg" : "Wuker";
	float p[3] = {x, y, kGnomeZ};
	floorSpot(x, y, 3.f, kGnomeZ, p);
	int o = scene_.addObject(cls, p[0], p[1], p[2]);
	int u = addUnit(o);
	int id = idOf(o);
	if (scripts && (!gnome || scriptGnomes)) scripts->attach(id, cls, "");
	if (gnome) selected = u;
	return u;
}

bool WorldSim::carveNode(int nx, int ny, int owner, int& dugId, bool rebuild)
{
	Pmp* g = scene_.mutableGrid();
	if (!g || !digTerrain.ready()) return false;
	std::vector<std::pair<int, int>> changed;
	bool ok = digTerrain.carveStep(*g, scene_.blockDb(), nx, ny, owner, dugId, changed);
	if (changed.empty()) return ok;
	for (auto [x, y] : changed)
	{
		size_t i = size_t(y) * g->width + size_t(x);
		scene_.setBlock(x, y, g->blocks[i], g->types[i]); // też bez starego detalu .l2m
		nav.updateTile(*g, scene_.blockDb(), x, y);
		walk.revalidateTile(x, y);
	}
	if (rebuild) scene_.rebuildTerrain(), terrainDirty = false;
	else terrainDirty = true;
	return ok;
}

void WorldSim::digMark(int owner, int x, int y, int mode, int cnt)
{
	const Pmp* g = simGrid();
	if (!g || !digTerrain.ready()) return;
	if (mode == 2) digTerrain.clearMark(x, y, owner);
	else if (digTerrain.diggable(x, y) && digTerrain.markRimGate(*g, scene_.blockDb(), x, y, mode == 1 ? 3 : (cnt | 8)))
		digTerrain.setMark(x, y, owner, cnt, mode == 1); // znacznik tylko na materiale 1/2 i gdy jest jeszcze co kopać
}

bool WorldSim::isDigMarked(int x1, int y1, int x2, int y2)
{
	for (int y = y1; y < y2; ++y)
		for (int x = x1; x < x2; ++x)
			if (digTerrain.marked(x, y, 0)) return true;
	return false;
}

bool WorldSim::digNext(const float pos[3], int owner, int gnomeId, int tryN, bool matchId, float out[3])
{
	const Pmp* g = simGrid();
	return g && digTerrain.ready() && digTerrain.next(*g, scene_.blockDb(), pos, owner, gnomeId, tryN, matchId, out);
}

void WorldSim::dig(int x, int y)
{
	// Tunel jak w grze (tryb 1 dig_mark, cel 11): węzły pola (narożniki NW, NE, SW, SE) wycinane, dopóki krawędź
	// sięga celu. Węzły wspólne z wykopanym już sąsiadem odpadają na bramce krawędzi (już wykopane).
	if (const Pmp* g = simGrid(); g && digTerrain.ready())
		for (auto [nx, ny] : {std::pair{x, y}, {x + 1, y}, {x, y + 1}, {x + 1, y + 1}})
		{
			if (!digTerrain.markRimGate(*g, scene_.blockDb(), nx, ny, 3)) continue;
			digTerrain.setMark(nx, ny, 0, 0, true);
			int id;
			for (int k = 0; k < 4 && carveNode(nx, ny, 0, id, false); ++k)
			{
			}
		}
	if (terrainDirty) scene_.rebuildTerrain(), terrainDirty = false;
	digMarks.erase({x, y});
	// Znalezisko jak w dig_execute (z_dignwalk.tcl): 4% szans; rodzaj losowany (🔶 bez stref kampanii).
	if (float(std::rand()) / float(RAND_MAX) < 0.04f)
	{
		static const char* kFinds[] = {"Stein", "Eisenerz", "Kohle", "Golderz", "Kristallerz"};
		const char* cls = kFinds[std::rand() % 5];
		float pos[3] = {float(x) + 0.5f, float(y) + 1.f, kGnomeZ}, rot[3] = {0, 0, 0};
		floorSpot(float(x) + 0.5f, float(y) + 1.f, 1.f, kGnomeZ, pos);
		int id = createObject(cls, "", pos, rot);
		if (scripts) scripts->attach(id, cls, "");
		status = std::string(tr("Znalezisko: ")) + cls;
	}
}

// Następne zaznaczone pole dla krasnala z zadaniem kopania: najbliższe, do którego da się dojść. 🔶 Stanowisko:
// punkt wyjścia sąsiedniego pola (z boku, nad, pod); dojście do krawędzi jak w grze to etap P3 (rozdział 17).
// Czy węzeł pola (x, y) jest jeszcze do wykopania jak tunel: materiał piasek albo ziemia, a krawędź węzła sięga
// celu tunelu (jak bramka malowania znaczników w grze). Bez danych kopania: pole lite dla ruchu.
bool WorldSim::digNeeded(int x, int y) const
{
	const Pmp* g = simGrid();
	if (!g || !digTerrain.ready()) return !nav.open(x, y);
	return digTerrain.diggable(x, y) && digTerrain.markRimGate(*g, scene_.blockDb(), x, y, 3);
}

bool WorldSim::nextDigJob(SimUnit& u)
{
	std::vector<std::pair<int, std::pair<int, int>>> cand;
	for (auto& m : digMarks) cand.push_back({std::abs(m.first - u.tx) + std::abs(m.second - u.ty), m});
	std::sort(cand.begin(), cand.end());
	for (size_t i = 0; i < cand.size() && i < 12; ++i)
	{
		auto [mx, my] = cand[i].second;
		if (!digNeeded(mx, my))
		{
			digMarks.erase({mx, my});
			continue;
		}
		if (digAt(u, mx, my)) return true;
	}
	return false;
}

bool WorldSim::digAt(SimUnit& u, int mx, int my)
{
	int ex, ey;
	static const bool trace = std::getenv("OW_TRACE_SIM") && std::atoi(std::getenv("OW_TRACE_SIM")) >= 2;
	if (!digEdgeTile(mx, my, int(std::nearbyint(u.x + 0.5f)), int(std::nearbyint(u.y + 0.5f)), ex, ey))
	{
		if (trace) std::fprintf(stderr, "[sim %.1f] kopanie pola (%d, %d): brak krawędzi (get_digedge)\n", simTime_, mx, my);
		return false;
	}
	// miejsce przy polu startu; z = 14 jak w grze (lrep walkpos 2 originalz), w pas podłogi wpasuje trasa
	float edge[3] = {float(ex), float(ey), 14.f}, g[3] = {u.x, u.y, u.z}, w[3];
	if (!fixDigPos(edge, g, w))
	{
		if (trace) std::fprintf(stderr, "[sim %.1f] kopanie pola (%d, %d): brak miejsca (vector_fixdig)\n", simTime_, mx, my);
		return false;
	}
	w[2] = 14.f;
	if (trace)
		std::fprintf(stderr, "[sim %.1f] kopanie pola (%d, %d): start (%d, %d), miejsce (%.2f, %.2f)\n", simTime_, mx, my, ex, ey, w[0], w[1]);
	float d = std::sqrt((w[0] - u.x) * (w[0] - u.x) + (w[1] - u.y) * (w[1] - u.y) + (w[2] - u.z) * (w[2] - u.z));
	if (d > 0.5f && !walkUnit(u, w[0], w[1], w[2], true)) return false;
	if (d <= 0.5f) u.state = SimUnit::Dig, u.digT = 0; // nonewwalk 0,5: kopie z miejsca
	u.digX = mx, u.digY = my;
	u.onArrive = nullptr;
	return true;
}

bool WorldSim::digEdgeTile(int sx, int sy, int rx, int ry, int& ex, int& ey) const
{
	ex = ey = -1;
	const Pmp* g = simGrid();
	if (!g) return false;
	int W = int(g->width), H = int(g->height);
	if (sx < 1 || sy < 1 || sx >= W - 1 || sy >= H - 1) return false; // pas jednego pola przy brzegu świata
	const auto& fog = scene_.fogBytes();
	auto open = [&](int tx, int ty) {
		const BlockRecord* r = scene_.blockDb().record(g->blocks[size_t(ty) * g->width + size_t(tx)]);
		return r && r->depth[0] < 12;
	};
	std::vector<uint8_t> seen(size_t(W) * H, 0);
	std::deque<std::pair<int, int>> q{{sx, sy}};
	seen[size_t(sy) * W + sx] = 1;
	long best = -1;
	while (!q.empty())
	{
		auto [tx, ty] = q.front();
		q.pop_front();
		if (tx < 1 || ty < 1 || tx >= W - 1 || ty >= H - 1) continue;
		bool explored = fog.empty() || fog[size_t(ty) * W + tx] != 0;
		if (explored && open(tx, ty))
		{
			long d = long(tx - rx) * (tx - rx) + long(ty - ry) * (ty - ry);
			if (best < 0 || d < best) best = d, ex = tx, ey = ty;
		}
		if (!(tx == sx && ty == sy) && !digMarks.count({tx, ty})) continue; // dalej z pola startu (znacznik) i pól zaznaczonych do kopania
		for (auto [nx, ny] : {std::pair{tx + 1, ty}, {tx - 1, ty}, {tx, ty + 1}, {tx, ty - 1}})
		{
			if (nx < 0 || ny < 0 || nx >= W || ny >= H || seen[size_t(ny) * W + nx]) continue;
			seen[size_t(ny) * W + nx] = 1;
			q.push_back({nx, ny});
		}
	}
	return ex >= 0;
}

bool WorldSim::fixDigPos(const float wall[3], const float gnome[3], float out[3]) const
{
	// 1. jeden blok od punktu w stronę postaci (bliżej niż 1: pozycja postaci)
	float fx = wall[0], fy = wall[1], dx = gnome[0] - fx, dy = gnome[1] - fy, d = std::sqrt(dx * dx + dy * dy);
	if (d >= 1.f) fx += dx / d, fy += dy / d;
	else
		fx = gnome[0], fy = gnome[1];
	// 2. najbliższe podpole do chodzenia (euklidesowo, do 50 podpól) wokół punktu jedno pole niżej (stopy
	// pod kopaną ścianą)
	int px = sub8(fx), py = sub8(fy) + 8, bx = px, by = py;
	long bd = walk.walkable(px, py) ? 0 : -1;
	for (int r = 1; r <= 50 && bd != 0; ++r)
	{
		if (bd > 0 && long(r) * r > bd) break;
		for (int yy = -r; yy <= r; ++yy)
			for (int xx = -r; xx <= r; ++xx)
			{
				if (std::abs(xx) != r && std::abs(yy) != r) continue;
				if (!walk.walkable(px + xx, py + yy)) continue;
				long dd = long(xx) * xx + long(yy) * yy;
				if (bd < 0 || dd < bd) bd = dd, bx = px + xx, by = py + yy;
			}
	}
	if (bd < 0) return false;
	// 3. podłoga do 8 podpól niżej; bez podłogi zostaje na ścianie (kopie wisząc)
	for (int k = 0; k < 8; ++k)
		if (walk.floor(bx, by + k))
		{
			by += k;
			break;
		}
	// 4. głębokość: co najmniej podpole 5 wierszy wyżej + 1, najwyżej 14,5
	float z = std::max(gnome[2], float(walk.detail(bx, by - 5) + 1));
	out[0] = float(bx) / 8.f, out[1] = float(by) / 8.f, out[2] = std::min(z, 14.5f);
	return true;
}

bool WorldSim::digEdge(const float start[3], int id, float out[3])
{
	float ref[2] = {start[0], start[1]};
	if (valid(id))
	{
		const float* p = scene_.objectPos(idx(id));
		ref[0] = p[0], ref[1] = p[1];
	}
	int ex, ey;
	if (!digEdgeTile(int(std::nearbyint(start[0])), int(std::nearbyint(start[1])), int(std::nearbyint(ref[0] + 0.5f)),
	        int(std::nearbyint(ref[1] + 0.5f)), ex, ey))
		return false;
	out[0] = float(ex), out[1] = float(ey), out[2] = 14.f;
	return true;
}

// Zaznaczenie prostokąta pól skały do kopania; zadanie dostaje zaznaczony krasnal.
void WorldSim::markDig(int x0, int y0, int x1, int y1)
{
	int n = 0;
	for (int y = std::min(y0, y1); y <= std::max(y0, y1); ++y)
		for (int x = std::min(x0, x1); x <= std::max(x0, x1); ++x)
			if (simGrid() && x >= 0 && y >= 0 && x < int(simGrid()->width) && y < int(simGrid()->height) && digNeeded(x, y))
				n += digMarks.insert({x, y}).second;
	if (selected >= 0 && selected < int(units.size())) units[size_t(selected)].digJob = true;
	status = std::string(tr("Do kopania: ")) + std::to_string(digMarks.size());
	(void)n;
}

bool WorldSim::walkUnit(SimUnit& u, float x, float y, float z, bool keepGoal, std::string* why)
{
	float s[3] = {u.x, u.y, u.z}, g[3] = {x, y, z};
	auto r = planWalk(walk, s, g, u.canClimb, keepGoal, why);
	if (r.empty()) return false;
	u.route = std::move(r), u.next = 0;
	u.started = u.turning = false;
	u.speedtype = 0, u.walkMayRun = true; // opcje action walk ustawia walkTo
	// na ścianie (wisi): wspinanie bez wejścia; przerwane wejście albo zejście nie wraca
	u.climbPhase = SimUnit::NoClimb, u.climbClip.clear();
	u.climbing = !hasFloorSupport(walk, u.x, u.y) && walk.walkable(sub8(u.x), sub8(u.y));
	u.state = SimUnit::Walk;
	return true;
}

void WorldSim::finishWalk(SimUnit& u, bool ok)
{
	u.route.clear(), u.next = 0;
	u.started = u.turning = u.running = false;
	u.stride = 0;
	u.state = ok && u.digX >= 0 ? SimUnit::Dig : SimUnit::Idle;
	u.digT = 0;
	if (!ok) u.digX = u.digY = -1;
	if (u.onArrive)
	{
		auto f = std::move(u.onArrive);
		u.onArrive = nullptr;
		f(ok);
	}
}

// Wykonawca chodu (za Diggles-Unity MetaWalkAction, odczyt gry FUN_00418280/00418850/004185a0): postać zużywa
// w każdym kroku prędkość · dt drogi wzdłuż punktów trasy (odległość w świecie: dx, dy, dz · 0,5). Przed
// ruchem i na załamaniach: błąd kierunku ≥ π/4 → obrót w miejscu animacją drehen (|rotate| animacji rad na
// 0,1 s), potem od nowa start. W marszu dryf kierunku 0,1 rad na 0,1 s (poniżej 0,051 rad od razu).
// Fazy: start (animacja raz, ruch już z pełną prędkością), pętla, stop (animacja raz w miejscu), stanie.
// Odcinki ściany: 🔶 wspinanie uproszczone (etap P4).
void WorldSim::stepWalk(SimUnit& u, float dt)
{
	auto& R = u.route;
	auto metric = [](float dx, float dy, float dz) { return std::sqrt(dx * dx + dy * dy + dz * dz * 0.25f); };
	if (!u.started)
	{ // pierwszy punkt dalej niż 0,2 w bok i w głąb (na ścianie też w pionie); żaden: od razu na miejscu
		u.next = 0;
		while (u.next < R.size())
		{
			const auto& p = R[u.next];
			float dy = p.wall ? p.y - u.y : 0.f;
			if (metric(p.x - u.x, dy, p.z - u.z) > 0.2f) break;
			++u.next;
		}
		if (u.next >= R.size())
		{
			finishWalk(u, true);
			return;
		}
		u.started = true, u.phase = SimUnit::Start, u.entered = false, u.stride = 0;
		u.running = u.speedtype == 2 && u.canRun && u.walkMayRun;
	}
	if (u.next >= R.size() && u.phase != SimUnit::Stop) u.phase = SimUnit::Stop, u.entered = false;
	bool climb = u.next < R.size() && (R[u.next].wall || (u.next > 0 && R[u.next - 1].wall));
	// Głębokość punktu, do którego idzie ciało: na ścianie 0,25 bloku (0,5 skali z) przed punktem podparcia
	// i zapas na stopniu ściany (Diggles-Unity ClimbAction: gra wiesza ciało pozą animacji, której nie mamy).
	auto zOf = [](const WalkPoint& p) { return p.wall ? p.z + 0.5f + p.clear : p.z; };
	// Zużycie drogi wzdłuż punktów; w marszu kierunek dryfuje w stronę ruchu.
	auto advance = [&](float dist) {
		for (int guard = 0; dist > 0 && u.next < R.size() && guard < 256; ++guard)
		{
			const auto& p = R[u.next];
			float pz = zOf(p), dx = p.x - u.x, dy = p.y - u.y, dz = pz - u.z, seg = metric(dx, dy, dz);
			if (seg <= 1e-4f)
			{
				++u.next;
				continue;
			}
			if (!climb && std::hypot(dx, dz * 0.5f) > 1e-3f)
			{
				float err = wrapAngle(facingOf(dx, dz * 0.5f) - u.facing);
				float mx = 0.1f * dt * 10.f;
				u.facing = wrapAngle(u.facing + (std::abs(err) <= 0.051f ? err : std::clamp(err, -mx, mx)));
			}
			if (dist >= seg)
			{
				u.x = p.x, u.y = p.y, u.z = pz;
				++u.next, dist -= seg;
			}
			else
			{
				float k = dist / seg;
				u.x += dx * k, u.y += dy * k, u.z += dz * k;
				dist = 0;
			}
		}
	};
	// Animacja raz (wejście, zejście): czas trwania, 0 = brak animacji.
	auto once = [&](const std::string& a) {
		Vec3f m{}, r{};
		int f = 0;
		if (a.empty() || !scene_.animMotion(u.obj, a, m, r, f) || f <= 0) return 0.f;
		scene_.playAnim(u.obj, a, false);
		return float(f) / 10.f;
	};
	auto faceWall = [&]() { // twarzą do ściany (roty = π), obrót 540°/s
		float err = wrapAngle(kSimPi - u.facing), mx = 9.42f * dt;
		u.facing = wrapAngle(u.facing + std::clamp(err, -mx, mx));
	};
	if (u.climbPhase == SimUnit::Mount)
	{ // stand_zu_kletterstand w miejscu
		faceWall();
		u.phaseRem -= dt;
		if (u.phaseRem <= 0) u.climbPhase = SimUnit::Advance;
		return;
	}
	if (u.climbPhase == SimUnit::Dismount)
	{ // kletterstand_zu_stand w miejscu, potem marsz od startu
		u.phaseRem -= dt;
		if (u.phaseRem > 0) return;
		u.climbPhase = SimUnit::NoClimb, u.climbing = false, u.climbClip.clear();
		u.phase = SimUnit::Start, u.entered = false;
		if (u.next >= R.size())
		{
			scene_.playAnim(u.obj, u.standAnim, true);
			finishWalk(u, true);
		}
		return;
	}
	if (climb)
	{ // wspinanie (CAct_Climb, Diggles-Unity ClimbAction)
		if (!u.climbing)
		{
			u.climbing = true, u.turning = false, u.climbClip.clear();
			bool fromFloor = !(u.next > 0 && R[u.next - 1].wall) && hasFloorSupport(walk, u.x, u.y);
			if (fromFloor && (u.phaseRem = once(u.mountAnim)) > 0)
			{
				u.climbPhase = SimUnit::Mount;
				return;
			}
			u.climbPhase = SimUnit::Advance;
		}
		faceWall();
		bool fast = u.running && u.speedtype == 2 && u.climbFastSpeed > 0.01f;
		const auto& p = R[u.next];
		float dx = p.x - u.x, dy = p.y - u.y;
		int dir = std::abs(dx) < 1e-4f && std::abs(dy) < 1e-4f ? -1 : std::abs(dy) >= std::abs(dx) ? (dy > 0 ? 1 : 0)
		                                                                                           : (dx > 0 ? 2 : 3);
		const std::string* clips[4] = {&u.climbUpAnim, &u.climbDownAnim, &u.climbRightAnim, &u.climbLeftAnim};
		const std::string& clip = dir < 0 ? u.climbStandAnim : fast ? u.climbFast[dir]
		                                                            : *clips[dir];
		if (clip != u.climbClip && !clip.empty() && scene_.playAnim(u.obj, clip, true)) u.climbClip = clip;
		advance((fast ? u.climbFastSpeed : u.climbSpeed) * dt);
		bool stillClimb = u.next < R.size() && (R[u.next].wall || (u.next > 0 && R[u.next - 1].wall));
		if (stillClimb) return;
		if (u.next >= R.size() && R.back().wall)
		{ // koniec na ścianie: wisi
			scene_.playAnim(u.obj, u.climbStandAnim, true);
			u.climbPhase = SimUnit::NoClimb, u.climbClip.clear();
			finishWalk(u, true);
			return;
		}
		u.climbPhase = SimUnit::Dismount; // zejście na podłogę w miejscu
		u.phaseRem = once(u.dismountAnim);
		return;
	}
	if (u.phase != SimUnit::Stop && u.next < R.size())
	{ // obrót w miejscu
		const auto& p = R[u.next];
		float dx = p.x - u.x, dz = (p.z - u.z) * 0.5f;
		if (std::hypot(dx, dz) > 1e-3f)
		{
			float want = facingOf(dx, dz), err = wrapAngle(want - u.facing), a = std::abs(err);
			if (!u.turning && a >= kSimPi * 0.25f)
			{
				u.turning = true;
				bool big = a > kSimPi * 0.75f, left = err < 0; // ku malejącemu kątowi = w lewo postaci (🔶 znak)
				const std::string& clip = left ? (big ? u.turnLeft180 : u.turnLeft) : (big ? u.turnRight180 : u.turnRight);
				Vec3f m{}, r{};
				int f = 0;
				u.turnRate = 0;
				if (!clip.empty() && scene_.animMotion(u.obj, clip, m, r, f))
				{
					scene_.playAnim(u.obj, clip, true);
					u.turnRate = std::max({std::abs(r.x), std::abs(r.y), std::abs(r.z)});
				}
				if (u.turnRate <= 1e-4f) u.turnRate = big ? 0.15f : 0.1f;
			}
			if (u.turning)
			{
				float step = u.turnRate * dt * 10.f;
				if (a <= step + 1e-4f || a <= u.turnRate * 0.05f + 0.001f)
				{
					u.facing = want, u.turning = false;
					u.phase = SimUnit::Start, u.entered = false;
				}
				else
					u.facing = wrapAngle(u.facing + (err > 0 ? step : -step));
				return;
			}
		}
		else
			u.turning = false;
	}
	float cruise = u.running ? u.runSpeed : u.walkSpeed;
	switch (u.phase)
	{
	case SimUnit::Start:
		if (!u.entered)
		{
			u.entered = true;
			const std::string& a = u.running ? u.runStart : u.walkStart;
			Vec3f m{}, r{};
			int f = 0;
			if (a.empty() || !scene_.animMotion(u.obj, a, m, r, f) || f <= 0)
			{
				u.phase = SimUnit::Loop, u.entered = false;
				return;
			}
			scene_.playAnim(u.obj, a, false);
			u.phaseRem = float(f) / 10.f;
		}
		advance(cruise * dt);
		u.stride += dt, u.phaseRem -= dt;
		if (u.next >= R.size()) u.phase = SimUnit::Stop, u.entered = false;
		else if (u.phaseRem <= 0)
			u.phase = SimUnit::Loop, u.entered = false;
		break;
	case SimUnit::Loop:
		if (!u.entered) u.entered = true, scene_.playAnim(u.obj, u.running ? u.runLoop : u.walkLoop, true);
		advance(cruise * dt);
		u.stride += dt;
		if (!u.running && u.speedtype == 0 && u.canRun && u.walkMayRun && u.stride >= 2.f)
			u.running = true, u.phase = SimUnit::Start, u.entered = false; // po 2 s marszu bieg (start biegu, pętla)
		if (u.next >= R.size()) u.phase = SimUnit::Stop, u.entered = false;
		break;
	case SimUnit::Stop:
		if (!u.entered)
		{
			u.entered = true;
			const std::string& a = u.running ? u.runStop : u.walkStop;
			Vec3f m{}, r{};
			int f = 0;
			u.phaseRem = 0;
			if (!a.empty() && scene_.animMotion(u.obj, a, m, r, f))
			{
				scene_.playAnim(u.obj, a, false);
				u.phaseRem = float(f) / 10.f;
			}
		}
		u.phaseRem -= dt;
		if (u.phaseRem <= 0)
		{
			scene_.playAnim(u.obj, u.standAnim, true);
			finishWalk(u, true);
		}
		break;
	}
}

void WorldSim::updateUnit(SimUnit& u, float dt)
{
	if (u.controlled && (u.moveX != 0 || u.moveZ != 0))
	{ // prowadzona ręcznie, klawisz wciśnięty: ruch zamiast trasy
		if (u.state == SimUnit::Walk) finishWalk(u, false);
		u.state = SimUnit::Idle;
		manualMove(u, dt);
		updateTile(u);
		scene_.setObjectPose(u.obj, u.x, u.y, u.z, u.facing);
		return;
	}
	if (u.state == SimUnit::Walk) stepWalk(u, dt);
	if (u.state == SimUnit::Dig)
	{ // kierunek do środka pola od tułowia (0,8 nad stopami, jak dig_check); na ścianie kopie wisząc
		float dx = float(u.digX) + 0.5f - u.x, dy = float(u.digY) + 0.5f - (u.y - 0.8f);
		bool side = std::abs(dx) >= std::abs(dy), wall = !hasFloorSupport(walk, u.x, u.y);
		const char* anim = wall ? (side ? (dx > 0 ? "kletter_hack_rechts" : "kletter_hack_links") : dy > 0 ? "kletter_hack_unten"
		                                                                                                   : "kletter_hack_oben")
		                        : (side ? "buddeln_vorn" : dy > 0 ? "buddeln_unten"
		                                                          : "buddeln_oben_normal");
		if (wall) u.facing = kSimPi;
		else if (side)
			u.facing = dx > 0 ? -kSimPi * 0.5f : kSimPi * 0.5f;
		scene_.playAnim(u.obj, anim, true);
		u.digT += dt;
		if (u.digT >= kDigTime)
		{
			dig(u.digX, u.digY);
			u.digX = u.digY = -1;
			u.state = SimUnit::Idle; // pod postacią mogła zniknąć podłoga: spadanie niżej
		}
	}
	if (u.state == SimUnit::Idle && u.digJob)
	{
		if (digMarks.empty() || !nextDigJob(u)) u.digJob = false; // nic osiągalnego: koniec zadania
	}
	int sx = sub8(u.x);
	if (u.state == SimUnit::Idle)
	{
		bool onFloor = hasFloorSupport(walk, u.x, u.y);
		if (!onFloor && !walk.walkable(sx, sub8(u.y))) u.state = SimUnit::Fall, u.climbing = false, u.climbPhase = SimUnit::NoClimb;
		else if (!scripts || u.controlled) // ze skryptami animację w miejscu ustawia skrypt (set_anim); prowadzonej: my
			scene_.playAnim(u.obj, onFloor ? u.standAnim : u.climbStandAnim, true);
	}
	if (u.state == SimUnit::Fall)
	{ // 🔶 spadanie w pionie do pierwszej podłogi pod stopami
		int sy = sub8(u.y), land = -1;
		for (int d = 0; d < walk.subH() && land < 0; ++d)
			if (walk.floor(sx, sy + d)) land = sy + d;
		scene_.playAnim(u.obj, u.fallAnim, true);
		float ly = land < 0 ? u.y : float(land) / 8.f;
		u.y = std::min(ly, u.y + kFallSpeed * dt);
		if (u.y >= ly - 1e-4f)
		{
			u.y = ly, u.state = SimUnit::Idle;
			u.z = fitDepth(u.x, u.y, u.z);
		}
	}
	// Nigdy w skale (też po set_pos i glide ze skryptu): co najmniej wolne od skały w punkcie stóp.
	u.z = std::max(u.z, airDepth(walk, u.x, u.y));
	updateTile(u);
	scene_.setObjectPose(u.obj, u.x, u.y, u.z, u.facing);
}

// Skrypty obiektów świata (obj_init). Krasnale na razie steruje gracz (C++), nie skrypty.
void WorldSim::attachScripts()
{
	if (!scripts) return;
	int n = 0;
	for (int i = 0; i < scene_.objectCount(); ++i)
	{
		bool gnome = Vfs::lower(scene_.objectClass(i)) == "zwerg";
		if (gnome && !scriptGnomes) continue;
		auto* o = scripts->attach(idOf(i), scene_.objectClass(i), scene_.objectInstanceName(i), false);
		if (!o) continue;
		o->owner = scene_.objectOwner(i); // z zapisu
		if (gnome && o->owner < 0) o->owner = 0; // krasnale ze scenariusza: gracz 0
		n += o->interp != nullptr;
	}
	// obj_init dopiero po rejestracji wszystkich obiektów (obj_query widzi cały świat)
	// Potem zmienne Tcl z zapisu (stan skryptów w chwili zapisu; timery ustawił już obj_init).
	for (int i = 0; i < scene_.objectCount(); ++i)
	{
		auto* o = scripts->object(idOf(i));
		if (!o || !o->interp || !o->cls) continue;
		scripts->runIn(*o, o->cls->initBody, "obj_init");
		for (auto& [k, v] : scene_.objectTclVars(i)) scripts->setVar(*o, k, v);
	}
	std::printf("skrypty: %d obiektów ze skryptami\n", n);
}

int WorldSim::idOf(int index)
{
	if (index >= int(idxToId.size())) idxToId.resize(size_t(index) + 1, 0);
	int& id = idxToId[size_t(index)];
	if (!id)
	{
		const std::string& n = scene_.objectName(index);
		char* end = nullptr;
		long v = n.empty() ? 0 : std::strtol(n.c_str(), &end, 10);
		id = (v > 0 && end && !*end && !idToIdx.count(int(v))) ? int(v) : nextScriptId++;
		idToIdx[id] = index;
	}
	return id;
}

int WorldSim::idx(int id) const
{
	auto it = idToIdx.find(id);
	return it == idToIdx.end() ? -1 : it->second;
}

int WorldSim::createObject(const std::string& cls, const std::string&, const float pos[3], const float rot[3])
{
	int i = scene_.addObject(cls, pos[0], pos[1], pos[2]);
	scene_.setObjectPose(i, pos[0], pos[1], pos[2], rot[1]);
	if (isWalkerClass(Vfs::lower(cls))) addUnit(i);
	return idOf(i);
}
void WorldSim::deleteObject(int id)
{ // 🔶 brak usuwania obiektów sceny: wynosimy poza świat
	if (valid(id)) scene_.setObjectPose(idx(id), -1000.f, -1000.f, 0.f, 0.f);
}
bool WorldSim::getPos(int id, float pos[3])
{
	if (!valid(id)) return false;
	const float* p = scene_.objectPos(idx(id));
	pos[0] = p[0], pos[1] = p[1], pos[2] = p[2];
	return true;
}
void WorldSim::setPos(int id, const float pos[3])
{
	if (!valid(id)) return;
	if (SimUnit* u = unitOf(idx(id)))
	{
		u->x = pos[0], u->y = pos[1], u->z = pos[2];
		u->route.clear(), u->started = false, u->state = SimUnit::Idle;
		snapToFloor(*u);
		return;
	}
	scene_.setObjectPose(idx(id), pos[0], pos[1], pos[2], scene_.objectRotY(idx(id)));
}
void WorldSim::setRotY(int id, float r)
{
	if (!valid(id)) return;
	if (SimUnit* u = unitOf(idx(id))) u->facing = r;
	const float* p = scene_.objectPos(idx(id));
	scene_.setObjectPose(idx(id), p[0], p[1], p[2], r);
}
float WorldSim::getRotY(int id)
{
	return valid(id) ? scene_.objectRotY(idx(id)) : 0.f;
}
void WorldSim::setAnim(int id, const std::string& anim, int, int mode)
{
	if (!valid(id)) return;
	if (SimUnit* u = unitOf(idx(id)); u && (u->state != SimUnit::Idle || u->controlled)) return; // chód i kopanie mają własne animacje
	size_t dot = anim.find('.');
	std::string name = dot == std::string::npos ? anim : anim.substr(dot + 1);
	// obiekt.anim innego obiektu .3db niż bieżący (albo obiekt bez modelu): podpięcie modelu po nazwie obiektu;
	// mann.* u kobiety zostaje przy frau (alternatywna baza animacji, set_alternateanimdb)
	std::string cur = Vfs::lower(scene_.objectModelObject(idx(id))), want = dot == std::string::npos ? "" : Vfs::lower(anim.substr(0, dot));
	if (!want.empty() && want != cur && !(want == "mann" && cur == "frau")) scene_.setObjectModel(idx(id), "", anim.substr(0, dot));
	scene_.playAnim(idx(id), name, mode == 2);
}
double WorldSim::animLength(int id, const std::string& anim)
{ // klatki animacji modelu / 10 kl./s
	size_t dot = anim.find('.');
	std::string name = dot == std::string::npos ? anim : anim.substr(dot + 1);
	if (valid(id))
	{
		double s = scene_.animSeconds(idx(id), name);
		if (s > 0) return s;
	}
	return 1.5; // 🔶 nieznana animacja albo db_animlength bez obiektu
}
bool WorldSim::walkTo(int id, const float t[3], const WalkOpts& o, std::function<void(bool)> done)
{
	SimUnit* u = unitOf(idx(id));
	if (!u || u->digJob || u->controlled) return false; // krasnal pracuje dla gracza albo jest prowadzony ręcznie
	std::string why;
	bool ok = walkUnit(*u, t[0], t[1], t[2], o.dig, &why);
	static const bool trace = std::getenv("OW_TRACE_SIM") && std::atoi(std::getenv("OW_TRACE_SIM")) >= 2;
	if (trace)
		std::fprintf(stderr, "[sim %.1f] walk %s (%.2f, %.2f, %.2f) → (%.2f, %.2f, %.2f): %s\n", simTime_, u->gnome ? "krasnal" : "wuker", u->x,
		    u->y, u->z, t[0], t[1], t[2], ok ? (std::to_string(u->route.size()) + " punktów").c_str() : why.c_str());
	if (!ok) return false;
	u->speedtype = o.speedtype, u->walkMayRun = o.canRun;
	u->digX = u->digY = -1;
	u->onArrive = std::move(done);
	return true;
}
float WorldSim::hmap(float x, float y)
{ // głębokość tylnej ściany w punkcie: wartość siatki 8×8 (= z skryptu)
	const Pmp* p = simGrid();
	if (!p || x < 0 || y < 0 || x >= float(p->width) || y >= float(p->height)) return 0.f;
	const BlockRecord* r = scene_.blockDb().record(p->blocks[size_t(int(y)) * p->width + size_t(int(x))]);
	if (!r) return 0.f;
	int fx = std::clamp(int((x - std::floor(x)) * 8.f), 0, 7), fy = std::clamp(int((y - std::floor(y)) * 8.f), 0, 7);
	return float(r->depth[size_t(fy * 8 + fx)]);
}
bool WorldSim::underwater(const float p[3])
{
	return scene_.fluidFill(int(std::floor(p[0])), int(std::floor(p[1]))) > 0.5f;
}
bool WorldSim::findPlace(const float c[3], float r, float out[3])
{
	return floorSpot(c[0], c[1], std::max(r, 1.f), c[2], out);
}
void WorldSim::glide(int id, const float to[3], float seconds, bool face)
{
	if (!valid(id) || unitOf(idx(id))) return;
	const float* p = scene_.objectPos(idx(id));
	Glide gl{idx(id), {p[0], p[1], p[2]}, {to[0], to[1], to[2]}, 0.f, seconds, face};
	for (auto& x : glides)
		if (x.obj == gl.obj)
		{
			x = gl;
			return;
		}
	glides.push_back(gl);
}

void WorldSim::link(int child, int parent, int link)
{
	if (valid(child)) scene_.linkObject(idx(child), valid(parent) ? idx(parent) : -1, link);
}
bool WorldSim::linkOffset(int id, int link, float out[3])
{
	return valid(id) && scene_.linkOffset(idx(id), link, out);
}

int WorldSim::gnomePosition(int id)
{
	SimUnit* u = unitOf(idx(id));
	return u && !hasFloorSupport(walk, u->x, u->y) ? 1 : 0;
}

void WorldSim::populate(float cx, float cy, int gnomes, int wukers)
{
	const Pmp* g = simGrid();
	if (!g) return;
	std::vector<std::pair<int, int>> spots; // podpola: punkty wyjścia pól z podłogą
	int x0 = int(std::floor(cx)), y0 = int(std::floor(cy)), want = gnomes + wukers;
	for (int r = 0; r < 40 && int(spots.size()) < want; ++r)
		for (int dy = -r; dy <= r && int(spots.size()) < want; ++dy)
			for (int dx = -r; dx <= r && int(spots.size()) < want; ++dx)
			{
				int tx = x0 + dx, ty = y0 + dy;
				if (std::max(std::abs(dx), std::abs(dy)) != r || walk.baseCost(tx, ty) == -1 || !walk.tileFloor(tx, ty)) continue;
				uint8_t e = walk.exitPair(tx, ty);
				int sx = tx * 8 + (e & 7), sy = ty * 8 + ((e >> 3) & 7);
				float z0, z1;
				if (!walk.floor(sx, sy) || !floorBand(walk, sx, sy, z0, z1)) continue;
				if (scene_.fluidFill((sx - 1) >> 3, (sy - 1) >> 3) > 0.05f) continue; // nie na dnie zalanej komory
				bool far = true;
				for (auto& s : spots) far = far && std::max(std::abs(s.first - sx), std::abs(s.second - sy)) >= 16;
				if (far) spots.push_back({sx, sy});
			}
	for (size_t i = 0; i < spots.size(); ++i)
	{
		auto [sx, sy] = spots[i];
		const char* cls = int(i) < gnomes ? "Zwerg" : "Wuker";
		float x = float(sx) / 8.f, y = float(sy) / 8.f;
		int o = scene_.addObject(cls, x, y, fitDepth(x, y, kGnomeZ));
		addUnit(o);
		idOf(o);
	}
	if (selected < 0 && gnomeCount() > 0) selected = 0;
}

void WorldSim::setControlled(int unit, bool on)
{
	if (unit < 0 || unit >= int(units.size())) return;
	SimUnit& u = units[size_t(unit)];
	if (u.controlled == on) return;
	u.controlled = on;
	if (on)
	{ // zlecony przez skrypt chód kończy się niepowodzeniem, żeby skrypt nie czekał bez końca
		u.digJob = false, u.digX = u.digY = -1;
		if (u.state == SimUnit::Walk) finishWalk(u, false);
		if (u.state == SimUnit::Dig) u.state = SimUnit::Idle;
	}
	else
	{ // puszczona: stoi tam, gdzie jest (idle, bez reszty trasy); dalej decyduje jej skrypt
		if (u.state == SimUnit::Walk) finishWalk(u, false);
		u.moveX = u.moveZ = 0;
	}
	if (scripts)
		if (script::ScriptObject* o = scripts->object(idOf(u.obj))) o->external = on;
}

bool WorldSim::controlGoTo(int unit, int tx, int ty)
{
	if (unit < 0 || unit >= int(units.size())) return false;
	SimUnit& u = units[size_t(unit)];
	// punkt wyjścia pola (najlepsze podpole do chodzenia); pole lite: jego środek, cel dociągnie wyszukiwanie
	float gx = float(tx) + 0.5f, gy = float(ty) + 0.5f;
	if (walk.baseCost(tx, ty) != -1)
	{
		uint8_t e = walk.exitPair(tx, ty);
		gx = float(tx * 8 + (e & 7)) / 8.f, gy = float(ty * 8 + ((e >> 3) & 7)) / 8.f;
	}
	if (u.state == SimUnit::Walk) finishWalk(u, false);
	u.digX = u.digY = -1;
	return walkUnit(u, gx, gy, u.z);
}

bool WorldSim::controlGoToRay(int unit, const glm::vec3& ro, const glm::vec3& rd)
{
	int tx, ty;
	glm::vec3 p;
	if (unit < 0 || unit >= int(units.size()) || !scene_.pickTerrain(ro, rd, tx, ty, &p)) return false;
	constexpr float B = MapScene::kBlock;
	SimUnit& u = units[size_t(unit)];
	// trafiony punkt terenu jako cel jak w grze: x, y (podłoga albo ściana), głębokość (z świata = z · 0,5 + 7)
	float gx = p.x / B, gy = p.y / B, gz = std::clamp((p.z / B - 7.f) * 2.f, 0.f, 15.75f);
	if (u.state == SimUnit::Walk) finishWalk(u, false);
	u.digX = u.digY = -1;
	return walkUnit(u, gx, gy, gz);
}

bool WorldSim::controlStep(int unit, int dx, int dy)
{
	if (unit < 0 || unit >= int(units.size())) return false;
	const SimUnit& u = units[size_t(unit)];
	int sx = u.tx, sy = u.ty;
	if (u.state == SimUnit::Walk && !u.route.empty()) // kolejny krok od celu bieżącego chodu
		sx = sub8(u.route.back().x) >> 3, sy = (sub8(u.route.back().y) - 1) >> 3;
	if (dx != 0) // w bok: to samo pole wysokości, stopień w górę albo w dół
		for (int d : {0, -1, 1})
			if (walk.baseCost(sx + dx, sy + d) != -1 && controlGoTo(unit, sx + dx, sy + d)) return true;
	if (dy != 0 && walk.baseCost(sx, sy + dy) != -1) return controlGoTo(unit, sx, sy + dy);
	return false;
}

void WorldSim::changeParticleSource(int id, int slot, const ParticleSource& s)
{
	if (valid(id)) scene_.setObjectParticle(idx(id), slot, s);
}

void WorldSim::setVisible(int id, bool v)
{
	if (valid(id)) scene_.setObjectVisible(idx(id), v);
}

void WorldSim::setParticleSource(int id, int slot, bool on)
{
	if (valid(id)) scene_.enableObjectParticle(idx(id), slot, on);
}

} // namespace ow
