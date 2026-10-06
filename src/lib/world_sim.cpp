#include "lib/world_sim.h"

#include "lib/blockdb.h"
#include "lib/i18n.h"
#include "lib/map_template.h"
#include "lib/vfs.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

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
	if (const Pmp* g = simGrid()) nav.build(*g, scene_.blockDb());
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

void WorldSim::simUpdate(float dt)
{
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
			    u.gnome ? "krasnal" : "wuker", u.x, u.y, u.tx, u.ty, u.z, walkZ(u.x, u.ty), int(u.state));
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
		scene_.setObjectPose(it->obj, p[0], p[1], p[2], std::abs(dx) > 1e-3f ? (dx > 0 ? -kSimPi * 0.5f : kSimPi * 0.5f) : scene_.objectRotY(it->obj));
		it = it->t >= it->dur ? glides.erase(it) : it + 1;
	}
}

// Wysokość stóp w polu (x, ty): podłoże z bloku pod spodem (pierwszy wiersz siatki 8×8 o wysokości ≥ 13
// w środkowej kolumnie), np. na łące 0,5 pola niżej niż dół pola. Na ścianie: dół pola.
float WorldSim::floorY(int x, int ty) const
{
	if (!nav.standable(x, ty) || !simGrid()) return float(ty) + 1.f;
	const Pmp* g = simGrid();
	const BlockRecord* r = scene_.blockDb().record(g->blocks[size_t(ty + 1) * g->width + size_t(x)]);
	if (!r) return float(ty) + 1.f;
	int col = std::clamp(int((tileCenterX(x, ty) - float(x)) * 8.f), 0, 7); // kolumna środka wolnej części
	for (int row = 0; row < 8; ++row)
		if (r->depth[size_t(row * 8 + col)] >= 13) return float(ty + 1) + float(row) / 8.f;
	return float(ty) + 1.f;
}

bool WorldSim::openSpan(int x, int ty, float& x0, float& x1) const
{
	x0 = float(x), x1 = float(x) + 1.f;
	const Pmp* g = simGrid();
	if (!g || x < 0 || ty < 0 || x >= int(g->width) || ty >= int(g->height)) return false;
	uint32_t block = g->blocks[size_t(ty) * g->width + size_t(x)];
	const BlockRecord* r = block == 59 ? nullptr : scene_.blockDb().record(block); // 59: puste pole
	if (!r) return true;
	int c0 = 8, c1 = -1;
	for (int c = 0; c < 8; ++c)
		for (int row = 4; row < 8; ++row)
			if (r->depth[size_t(row * 8 + c)] < 13)
			{
				c0 = std::min(c0, c), c1 = std::max(c1, c);
				break;
			}
	if (c1 < 0) return false;
	x0 = float(x) + float(c0) / 8.f, x1 = float(x) + float(c1 + 1) / 8.f;
	return true;
}

float WorldSim::tileCenterX(int x, int ty) const
{
	float x0, x1;
	openSpan(x, ty, x0, x1);
	return (x0 + x1) * 0.5f;
}

void WorldSim::snapToFloor(SimUnit& u)
{
	int x = int(std::floor(u.x)), y = tileOfFeet(u.y);
	for (int d = 0; d < 40; ++d)
	{
		for (int yy : {y + d, y - d})
			if (nav.standable(x, yy) || (d > 0 && nav.climbable(x, yy)))
			{
				u.tx = x, u.ty = yy;
				u.x = tileCenterX(x, yy), u.y = floorY(x, yy);
				return;
			}
	}
	u.tx = x, u.ty = y;
}

float WorldSim::walkZ(float x, int ty) const
{
	const Pmp* g = simGrid();
	int bx = int(std::floor(x));
	if (!g || bx < 0 || ty < 0 || bx >= int(g->width) || ty >= int(g->height)) return kGnomeZ;
	const BlockRecord* r = scene_.blockDb().record(g->blocks[size_t(ty) * g->width + size_t(bx)]);
	if (!r) return kGnomeZ;
	int col = std::clamp(int((x - float(bx)) * 8.f), 0, 7), wall = 15;
	for (int row = 0; row < 8; ++row) wall = std::min(wall, int(r->depth[size_t(row * 8 + col)]));
	return std::min(float(wall) + kWallGap, 14.5f);
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
	if (!gnome)
	{ // wuker: animacje bez przyrostka _loop przy wspinaniu; model nadaje obj_init (set_anim this wuker.…),
		// a klasa ($wukerclass w WUKER.TCL) nie ma class_defaultanim
		u.climbUpAnim = "kletter_hoch", u.climbDownAnim = "kletter_runter", u.standAnim = "stand_atmen_a";
		if (scene_.objectModelObject(obj).empty()) scene_.setObjectModel(obj, "", "wuker");
	}
	const float* p = scene_.objectPos(obj);
	u.x = p[0], u.y = p[1], u.z = p[2];
	snapToFloor(u);
	// za tylną ścianą albo przed przednią płaszczyzną (dodana postać, stała kGnomeZ): na głębokość chodu
	float wz = walkZ(u.x, u.ty);
	if (u.z < wz - kWallGap + 0.5f || u.z > 14.5f) u.z = wz;
	units.push_back(u);
	return int(units.size()) - 1;
}

void WorldSim::spawnUnit(float x, float y)
{
	int o = scene_.addObject("Zwerg", x, y, kGnomeZ);
	selected = addUnit(o);
}

void WorldSim::dig(int x, int y)
{
	// 🔶 Wykopane pole: tunel z tylną ścianą (blok 4523: siatka 8×8 o wysokości 7), bez zszywania z sąsiadami.
	// Rodzaj gruntu bez zmian, ale nie „otwór do tła” (typ 5): wtedy grunt pola niżej albo skała A (4).
	uint8_t type = simGrid()->types[size_t(y) * simGrid()->width + size_t(x)];
	if ((type & 0x7F) == 5)
	{
		uint8_t below = y + 1 < int(simGrid()->height) ? simGrid()->types[size_t(y + 1) * simGrid()->width + size_t(x)] : 4;
		type = uint8_t((below & 0x7F) == 5 || (below & 0x7F) == 0 ? 4 : (below & 0x7F));
	}
	scene_.setBlock(x, y, 4523, type);
	nav.updateTile(*simGrid(), scene_.blockDb(), x, y);
	scene_.rebuildTerrain();
	digMarks.erase({x, y});
	// Znalezisko jak w dig_execute (z_dignwalk.tcl): 4% szans; rodzaj losowany (🔶 bez stref kampanii).
	if (float(std::rand()) / float(RAND_MAX) < 0.04f)
	{
		static const char* kFinds[] = {"Stein", "Eisenerz", "Kohle", "Golderz", "Kristallerz"};
		const char* cls = kFinds[std::rand() % 5];
		float pos[3] = {tileCenterX(x, y), floorY(x, y), kGnomeZ}, rot[3] = {0, 0, 0};
		int id = createObject(cls, "", pos, rot);
		if (scripts) scripts->attach(id, cls, "");
		status = std::string(tr("Znalezisko: ")) + cls;
	}
}

// Następne zaznaczone pole dla krasnala z zadaniem kopania: najbliższe, do którego da się dojść.
bool WorldSim::nextDigJob(SimUnit& u)
{
	std::vector<std::pair<int, std::pair<int, int>>> cand;
	for (auto& m : digMarks) cand.push_back({std::abs(m.first - u.tx) + std::abs(m.second - u.ty), m});
	std::sort(cand.begin(), cand.end());
	for (size_t i = 0; i < cand.size() && i < 12; ++i)
	{
		auto [mx, my] = cand[i].second;
		if (nav.open(mx, my))
		{
			digMarks.erase({mx, my});
			continue;
		}
		const int st[4][2] = {{mx - 1, my}, {mx + 1, my}, {mx, my - 1}, {mx, my + 1}};
		for (auto& c : st)
		{
			if (!(nav.standable(c[0], c[1]) || nav.climbable(c[0], c[1]))) continue;
			auto p = nav.path(u.tx, u.ty, c[0], c[1]);
			if (p.empty()) continue;
			u.path = std::move(p), u.step = 0, u.state = SimUnit::Walk, u.digX = mx, u.digY = my;
			u.goalX = u.goalZ = -1;
			u.onArrive = nullptr;
			return true;
		}
	}
	return false;
}

// Zaznaczenie prostokąta pól skały do kopania; zadanie dostaje zaznaczony krasnal.
void WorldSim::markDig(int x0, int y0, int x1, int y1)
{
	int n = 0;
	for (int y = std::min(y0, y1); y <= std::max(y0, y1); ++y)
		for (int x = std::min(x0, x1); x <= std::max(x0, x1); ++x)
			if (!nav.open(x, y) && simGrid() && x >= 0 && y >= 0 && x < int(simGrid()->width) && y < int(simGrid()->height))
				n += digMarks.insert({x, y}).second;
	if (selected >= 0 && selected < int(units.size())) units[size_t(selected)].digJob = true;
	status = std::string(tr("Do kopania: ")) + std::to_string(digMarks.size());
	(void)n;
}

void WorldSim::updateUnit(SimUnit& u, float dt)
{
	if (u.state == SimUnit::Walk)
	{
		if (u.step >= u.path.size())
		{
			u.state = u.digX >= 0 ? SimUnit::Dig : SimUnit::Idle;
			u.digT = 0;
			if (u.onArrive)
			{
				auto f = std::move(u.onArrive);
				u.onArrive = nullptr;
				f(true);
			}
		}
		else
		{
			auto [wx, wy] = u.path[u.step];
			float sx0, sx1;
			openSpan(wx, wy, sx0, sx1);
			float gx = (sx0 + sx1) * 0.5f, gy = floorY(wx, wy);
			bool last = u.step + 1 == u.path.size();
			if (last && u.goalX >= 0) gx = std::clamp(u.goalX, std::min(sx0 + 0.1f, gx), std::max(sx1 - 0.1f, gx));
			{ // w głąb równolegle z chodem: cel skryptu albo bieżąca głębokość, ale nie bliżej ściany niż kWallGap
				// ściana pod bieżącym x (pole x może już być następnym polem ścieżki) i w polu docelowym kroku
				float wz = std::max(walkZ(u.x, u.ty), walkZ(gx, wy));
				float gz = std::max(u.goalZ >= 0 ? u.goalZ : u.z, wz);
				float dz = gz - u.z, mz = 2.f * kWalkSpeed * dt;
				u.z += std::abs(dz) <= mz ? dz : (dz > 0 ? mz : -mz);
			}
			bool vertical = wx == u.tx && wy != u.ty;
			float speed = vertical ? kClimbSpeed : kWalkSpeed;
			float ddx = gx - u.x, ddy = gy - u.y, len = std::hypot(ddx, ddy), mv = speed * dt;
			if (std::abs(ddx) > 1e-3f) u.facing = ddx > 0 ? -kSimPi * 0.5f : kSimPi * 0.5f;
			if (len <= mv)
			{
				u.x = gx, u.y = gy, u.tx = wx, u.ty = wy;
				++u.step;
			}
			else
				u.x += ddx / len * mv, u.y += ddy / len * mv;
			const std::string& anim = !vertical ? u.walkAnim : (ddy < 0 ? u.climbUpAnim : u.climbDownAnim);
			if (vertical) u.facing = kSimPi; // twarzą do ściany
			scene_.playAnim(u.obj, anim, true);
		}
	}
	if (u.state == SimUnit::Dig)
	{
		const char* anim = u.digY > u.ty ? "buddeln_unten" : u.digY < u.ty ? "buddeln_oben_normal"
		                                                                   : "buddeln_vorn";
		if (u.digY == u.ty) u.facing = u.digX > u.tx ? -kSimPi * 0.5f : kSimPi * 0.5f;
		scene_.playAnim(u.obj, anim, true);
		u.digT += dt;
		if (u.digT >= kDigTime)
		{
			dig(u.digX, u.digY);
			u.digX = u.digY = -1;
			u.state = SimUnit::Idle;
			snapToFloor(u); // pod krasnalem mogło zniknąć podłoże
		}
	}
	if (u.state == SimUnit::Idle && u.digJob)
	{
		if (digMarks.empty() || !nextDigJob(u)) u.digJob = false; // nic osiągalnego: koniec zadania
	}
	if (u.state == SimUnit::Idle)
	{
		if (!nav.standable(u.tx, u.ty) && !nav.climbable(u.tx, u.ty) && nav.open(u.tx, u.ty + 1))
		{
			u.path = {{u.tx, u.ty + 1}}, u.step = 0, u.state = SimUnit::Walk; // spada
		}
		else if (!scripts) // ze skryptami animację w miejscu ustawia skrypt (set_anim)
			scene_.playAnim(u.obj, nav.standable(u.tx, u.ty) ? u.standAnim : u.climbStandAnim, true);
	}
	// Nigdy za tylną ścianą (też po set_pos i glide ze skryptu): co najmniej 0,5 przed nią w kolumnie stóp.
	u.z = std::max(u.z, walkZ(u.x, u.ty) - kWallGap + 0.5f);
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
		u->path.clear(), u->state = SimUnit::Idle;
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
	if (SimUnit* u = unitOf(idx(id)); u && u->state != SimUnit::Idle) return; // chód i kopanie mają własne animacje
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
bool WorldSim::walkTo(int id, const float t[3], std::function<void(bool)> done)
{
	SimUnit* u = unitOf(idx(id));
	if (!u || u->digJob) return false; // krasnal pracuje dla gracza
	int tx = int(std::floor(t[0])), ty = tileOfFeet(t[1]);
	for (int d = 0; d < 4 && !nav.standable(tx, ty) && !nav.climbable(tx, ty); ++d) ++ty;
	auto path = nav.path(u->tx, u->ty, tx, ty);
	if (path.empty()) return false;
	u->path = std::move(path), u->step = 0, u->state = SimUnit::Walk, u->digX = -1;
	u->goalX = t[0], u->goalZ = t[2];
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
	int cx = int(std::floor(c[0])), cy = tileOfFeet(c[1]);
	if (nav.standable(cx, cy))
	{ // pole celu jest dobre: zostaje x i głębokość, y na podłożu
		float x0, x1;
		openSpan(cx, cy, x0, x1);
		out[0] = std::clamp(c[0], x0, x1), out[1] = floorY(cx, cy), out[2] = c[2];
		return true;
	}
	int rad = std::max(1, int(std::ceil(r)));
	for (int d = 0; d <= rad; ++d)
		for (int dy = -d; dy <= d; ++dy)
			for (int dx = -d; dx <= d; ++dx)
			{
				if (std::max(std::abs(dx), std::abs(dy)) != d || !nav.standable(cx + dx, cy + dy)) continue;
				out[0] = tileCenterX(cx + dx, cy + dy), out[1] = floorY(cx + dx, cy + dy), out[2] = c[2];
				return true;
			}
	return false;
}
void WorldSim::glide(int id, const float to[3], float seconds)
{
	if (!valid(id) || unitOf(idx(id))) return;
	const float* p = scene_.objectPos(idx(id));
	Glide gl{idx(id), {p[0], p[1], p[2]}, {to[0], to[1], to[2]}, 0.f, seconds};
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
	return u && !nav.standable(u->tx, u->ty) ? 1 : 0;
}

void WorldSim::populate(float cx, float cy, int gnomes, int wukers)
{
	const Pmp* g = simGrid();
	if (!g) return;
	std::vector<std::pair<int, int>> spots;
	int x0 = int(std::floor(cx)), y0 = int(std::floor(cy));
	for (int r = 0; r < 40 && int(spots.size()) < gnomes + wukers; ++r)
		for (int dy = -r; dy <= r && int(spots.size()) < gnomes + wukers; ++dy)
			for (int dx = -r; dx <= r && int(spots.size()) < gnomes + wukers; ++dx)
			{
				if (std::max(std::abs(dx), std::abs(dy)) != r || !nav.standable(x0 + dx, y0 + dy)) continue;
				if (scene_.fluidFill(x0 + dx, y0 + dy) > 0.05f) continue; // nie na dnie zalanej komory
				bool far = true;
				for (auto& s : spots) far = far && std::max(std::abs(s.first - (x0 + dx)), std::abs(s.second - (y0 + dy))) >= 2;
				if (far) spots.push_back({x0 + dx, y0 + dy});
			}
	for (size_t i = 0; i < spots.size(); ++i)
	{
		auto [x, y] = spots[i];
		const char* cls = int(i) < gnomes ? "Zwerg" : "Wuker";
		int o = scene_.addObject(cls, tileCenterX(x, y), floorY(x, y), kGnomeZ);
		addUnit(o);
		idOf(o);
	}
	if (selected < 0 && gnomeCount() > 0) selected = 0;
}

void WorldSim::changeParticleSource(int id, int slot, const ParticleSource& s)
{
	if (valid(id)) scene_.setObjectParticle(idx(id), slot, s);
}

void WorldSim::setParticleSource(int id, int slot, bool on)
{
	if (valid(id)) scene_.enableObjectParticle(idx(id), slot, on);
}

} // namespace ow
