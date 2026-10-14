#include "lib/level_script.h"

#include "lib/script/engine.h"
#include "lib/vfs.h"

#include <tcl.h>

#include <cstdlib>
#include <random>
#include <set>
#include <sstream>

namespace ow
{

namespace
{

struct Recorder
{
	const Vfs& vfs;
	Scenario& sc;
	std::mt19937 rng;
	std::vector<int> objectIds;  // id obiektu Tcl -> indeks w sc.objects (−1 = usunięty)
	std::set<int> storyMgrs;     // obiekty klasy StoryMgr
	std::string detail;          // loading_objdetail
	int depth = 0;
};

constexpr int kFirstId = 100; // identyfikatory obiektów w przebiegu wstępnym: kFirstId + indeks

void result(Tcl_Interp* in, const std::string& s)
{
	Tcl_SetObjResult(in, Tcl_NewStringObj(s.data(), int(s.size())));
}

std::vector<std::string> words(Tcl_Interp* in, const std::string& list)
{
	std::vector<std::string> out;
	int n = 0;
	const char** v = nullptr;
	if (Tcl_SplitList(in, list.c_str(), &n, &v) != TCL_OK) return out;
	for (int i = 0; i < n; ++i) out.push_back(v[i]);
	Tcl_Free(reinterpret_cast<char*>(v));
	return out;
}

TemplateObject* object(Recorder& r, const std::string& word)
{
	int i = std::atoi(word.c_str()) - kFirstId;
	if (i < 0 || i >= int(r.objectIds.size()) || r.objectIds[size_t(i)] < 0) return nullptr;
	return &r.sc.objects[size_t(r.objectIds[size_t(i)])];
}

void vec3(Tcl_Interp* in, const std::string& s, float out[3])
{
	auto w = words(in, s);
	for (int k = 0; k < 3; ++k) out[k] = k < int(w.size()) ? float(std::atof(w[size_t(k)].c_str())) : 0.f;
}

bool callFile(Recorder& r, Tcl_Interp* in, const std::string& path)
{
	std::string p = path;
	for (char& c : p)
		if (c == '\\') c = '/';
	std::string low = Vfs::lower(p);
	p = low.rfind("data/", 0) == 0 ? "Data/" + p.substr(5) : "Data/" + p;
	auto data = r.vfs.read(p);
	if (!data)
	{
		r.sc.levelErrors.push_back("call: brak pliku " + path);
		return true; // gra też idzie dalej
	}
	if (r.depth > 16) return true;
	++r.depth;
	std::string code = script::toUtf8(script::Engine::preprocess(std::string(data->begin(), data->end())));
	int rc = Tcl_EvalEx(in, code.c_str(), -1, TCL_EVAL_GLOBAL);
	--r.depth;
	if (rc == TCL_ERROR)
	{
		const char* info = Tcl_GetVar(in, "errorInfo", TCL_GLOBAL_ONLY);
		std::string msg = "call " + path + ": " + Tcl_GetStringResult(in);
		if (info) msg += "\n    " + std::string(info).substr(0, 400);
		r.sc.levelErrors.push_back(msg);
	}
	return true;
}

int cmd(ClientData cd, Tcl_Interp* in, int argc, const char** argv)
{
	Recorder& r = *static_cast<Recorder*>(cd);
	std::vector<std::string> a(argv, argv + argc);
	const std::string& c = a[0];
	result(in, "");
	auto num = [&](size_t i) { return i < a.size() ? std::atoi(a[i].c_str()) : 0; };
	if (c == "call" || c == "callnc")
	{
		if (a.size() > 1) callFile(r, in, a[1]);
	}
	else if (c == "map")
	{
		if (a.size() >= 4 && a[1] == "create") r.sc.width = num(2), r.sc.height = num(3);
		result(in, "0");
	}
	else if (c == "map_template")
	{
		if (a.size() >= 4) r.sc.stamps.push_back({false, a[1], num(2), num(3)});
	}
	else if (c == "map_setlayer2")
	{
		if (a.size() >= 4) r.sc.stamps.push_back({true, a[3], num(1), num(2)});
	}
	else if (c.rfind("sm_", 0) == 0 && c != "sm_draw_stone")
		r.sc.storyCalls.push_back(a);
	else if (c == "sm_draw_stone")
	{
		r.sc.storyCalls.push_back(a); // -border, -funnel: pasy granitu malowane przy składaniu świata (map_world.h drawStoneOption)
		if (a.size() > 1 && (a[1] == "-border" || a[1] == "-funnel"))
		{
			ScenarioStamp st;
			st.stone = a[1];
			r.sc.stamps.push_back(st);
		}
	}
	else if (c == "loading_objdetail")
	{
		if (a.size() > 1) r.detail = a[1];
	}
	else if (c == "new" || c == "qnew")
	{
		if (a.size() < 2) return TCL_OK;
		TemplateObject o;
		o.className = a[1];
		if (a.size() > 2) o.name = a[2];
		if (a.size() > 3) vec3(in, a[3], o.pos);
		if (a.size() > 4) vec3(in, a[4], o.rot);
		o.detail = r.detail;
		int id = kFirstId + int(r.objectIds.size());
		r.objectIds.push_back(int(r.sc.objects.size()));
		r.sc.objects.push_back(o);
		if (Vfs::lower(o.className) == "storymgr") r.storyMgrs.insert(id);
		result(in, std::to_string(id));
	}
	else if (c == "del")
	{
		int i = num(1) - kFirstId;
		if (i >= 0 && i < int(r.objectIds.size())) r.objectIds[size_t(i)] = -1;
	}
	else if (c == "obj_clear")
	{
		for (int& i : r.objectIds) i = -1;
	}
	else if (c == "obj_query")
	{ // obj_query OBJ -class {Klasy} …: obiekty podanych klas (bez -class: wszystkie), 0 = brak
		std::set<std::string> cls;
		for (size_t i = 1; i + 1 < a.size(); ++i)
			if (a[i] == "-class")
				for (auto& k : words(in, a[i + 1])) cls.insert(Vfs::lower(k));
		std::string out;
		for (size_t i = 0; i < r.objectIds.size(); ++i)
			if (r.objectIds[i] >= 0 && (cls.empty() || cls.count(Vfs::lower(r.sc.objects[size_t(r.objectIds[i])].className))))
				out += (out.empty() ? "" : " ") + std::to_string(kFirstId + int(i));
		result(in, out.empty() ? "0" : out);
	}
	else if (c == "set_owner")
	{
		if (auto* o = a.size() > 2 ? object(r, a[1]) : nullptr) o->owner = num(2);
	}
	else if (c == "set_physic")
	{
		if (auto* o = a.size() > 2 ? object(r, a[1]) : nullptr) o->physic = a[2] == "true" || a[2] == "1";
	}
	else if (c == "set_pos" || c == "set_posbottom")
	{
		if (auto* o = a.size() > 2 ? object(r, a[1]) : nullptr) vec3(in, a[2], o->pos);
	}
	else if (c == "set_objname")
	{
		if (auto* o = a.size() > 2 ? object(r, a[1]) : nullptr) o->instanceName = a[2];
	}
	else if (c == "call_method")
	{
		if (a.size() < 3) return TCL_OK;
		int id = num(1);
		if (r.storyMgrs.count(id) && a[2] == "load_level" && a.size() > 3)
		{
			r.sc.storyLevel = a[3];
			callFile(r, in, "scripts/gameplay/" + a[3] + ".tcl");
			return TCL_OK;
		}
		TemplateObject* o = object(r, a[1]);
		if (!o) return TCL_OK;
		if (a[2] == "Editor_Set_Info" && a.size() > 3) o->info = a[3];
		else
		{
			ScenarioCall call;
			call.object = r.objectIds[size_t(id - kFirstId)];
			call.method = a[2];
			call.args.assign(a.begin() + 3, a.end());
			r.sc.calls.push_back(call);
		}
	}
	else if (c == "irandom")
	{ // jak cmdRandom w silniku: irandom N: 0..N−1; irandom A B: A..B−1
		int lo = 0, hi = num(1);
		if (a.size() > 2) lo = num(1), hi = num(2);
		int v = hi > lo ? lo + int(r.rng() % unsigned(hi - lo)) : lo;
		result(in, std::to_string(v));
	}
	else if (c == "random")
	{
		double m = a.size() > 1 ? std::atof(a[1].c_str()) : 1.0;
		result(in, std::to_string(std::uniform_real_distribution<double>(0.0, m)(r.rng)));
	}
	else if (c == "lrem")
	{ // jak cmd_misc.cpp: zwraca usunięty element
		if (a.size() < 3) return TCL_OK;
		const char* v = Tcl_GetVar(in, a[1].c_str(), 0);
		if (!v) return TCL_OK;
		auto items = words(in, v);
		size_t i = size_t(num(2));
		if (i < items.size())
		{
			result(in, items[i]);
			std::string popped = items[i];
			items.erase(items.begin() + long(i));
			Tcl_Obj* l = Tcl_NewListObj(0, nullptr);
			for (auto& s : items) Tcl_ListObjAppendElement(in, l, Tcl_NewStringObj(s.data(), int(s.size())));
			Tcl_SetVar2Ex(in, a[1].c_str(), nullptr, l, 0);
			result(in, popped);
		}
	}
	else if (c == "set_view")
	{
		if (a.size() >= 6 && !r.sc.view)
		{
			GameView v;
			v.x = float(std::atof(a[1].c_str())), v.y = float(std::atof(a[2].c_str())), v.zoom = float(std::atof(a[3].c_str()));
			v.dx = float(std::atof(a[4].c_str())), v.dy = float(std::atof(a[5].c_str()));
			r.sc.view = v;
		}
	}
	else if (c == "set_light_begin")
		r.sc.lightBegin = num(1);
	else if (c == "set_ground_begin")
		r.sc.groundBegin = num(1);
	else if (c == "set_fow_begin")
		r.sc.fowBegin = num(1);
	return TCL_OK; // log i reszta: nic
}

// Pozostałe komendy gry w przebiegu wstępnym nic nie robią.
const char* kUnknown = "proc unknown {args} {return 0}\n"
                       // split_template jak w STORY.TCL (gałąź z ustawionym temppos): części szablonu od razu
                       "proc split_template {orgname orgpmp iW iH templist} {\n"
                       "  global temppos\n"
                       "  foreach item $templist {\n"
                       "    set x [expr {[lindex $item 2] + [lindex $temppos 0]}]\n"
                       "    set y [expr {[lindex $item 3] + [lindex $temppos 1]}]\n"
                       "    map_template \"data/templates/pmp/split/[lindex $item 1]\" $x $y\n"
                       "    call \"data/templates/tcl/split/[lindex $item 0]\"\n"
                       "    MapTemplateSet $x $y\n"
                       "  }\n"
                       "  set temppos 0\n"
                       "  proc MapTemplateSet {x y} {}\n"
                       "}\n"
                       "proc sel {args} {return \"\"}\n"
                       "set temppos 0\n";

} // namespace

Scenario runLevelScript(const Vfs& vfs, const std::string& gamePath, unsigned seed)
{
	Scenario sc;
	sc.levelSeed = seed ? seed : std::random_device{}();
	Recorder r{vfs, sc, std::mt19937(sc.levelSeed), {}, {}, {}, 0};
	Tcl_Interp* in = Tcl_CreateInterp();
	for (const char* c : {"call", "callnc", "map", "map_template", "map_setlayer2", "loading_objdetail", "new", "qnew", "del",
	         "obj_clear", "obj_query", "set_owner", "set_physic", "set_pos", "set_posbottom", "set_objname",
	         "call_method", "irandom", "random", "lrem", "set_view", "set_light_begin", "set_ground_begin",
	         "set_fow_begin", "log", "sm_draw_stone", "sm_set_temp", "sm_mark_temparea", "sm_map_set",
	         "sm_set_zone", "sm_force_zone", "sm_dig_message"})
		Tcl_CreateCommand(in, c, cmd, &r, nullptr);
	Tcl_EvalEx(in, kUnknown, -1, TCL_EVAL_GLOBAL);
	std::string p = gamePath;
	if (Vfs::lower(p).rfind("data/", 0) == 0) p = p.substr(5);
	callFile(r, in, p);
	Tcl_DeleteInterp(in);
	// Usunięte obiekty (del, obj_clear) wypadają; wywołania metod przenumerowane.
	std::vector<TemplateObject> kept;
	std::vector<int> remap(sc.objects.size(), -1);
	std::set<int> alive;
	for (int i : r.objectIds)
		if (i >= 0) alive.insert(i);
	for (size_t i = 0; i < sc.objects.size(); ++i)
		if (alive.count(int(i))) remap[i] = int(kept.size()), kept.push_back(sc.objects[i]);
	sc.objects = std::move(kept);
	std::vector<ScenarioCall> calls;
	for (auto& c : sc.calls)
		if (c.object >= 0 && remap[size_t(c.object)] >= 0) c.object = remap[size_t(c.object)], calls.push_back(c);
	sc.calls = std::move(calls);
	return sc;
}

} // namespace ow
