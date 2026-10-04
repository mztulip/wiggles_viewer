#include "lib/script/engine.h"

#include "lib/vfs.h"

#include <tcl.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fnmatch.h>
#include <fstream>
#include <optional>
#include <iterator>
#include <sstream>

namespace ow::script {

namespace {

Engine* gEngine = nullptr;

using Args = std::vector<std::string>;
using Fn = int (*)(Engine&, Tcl_Interp*, Args&);

void setResult(Tcl_Interp* in, const std::string& s) { Tcl_SetObjResult(in, Tcl_NewStringObj(s.data(), int(s.size()))); }

std::string fmt(double v) {
	char b[64];
	std::snprintf(b, sizeof b, "%.6g", v);
	return b;
}

// Tekst skryptu (CP1252/CP1250) -> UTF-8 dla Tcl 8.6. 🔶 Znaki 0x80..0x9F jak Latin-1.
std::string toUtf8(const std::string& s) {
	std::string o;
	o.reserve(s.size() + s.size() / 16);
	for (unsigned char c : s) {
		if (c < 0x80) o += char(c);
		else o += char(0xC0 | (c >> 6)), o += char(0x80 | (c & 0x3F));
	}
	return o;
}

std::vector<std::string> listItems(Tcl_Interp* in, const std::string& list) {
	std::vector<std::string> out;
	int n = 0;
	const char** items = nullptr;
	if (Tcl_SplitList(in, list.c_str(), &n, &items) != TCL_OK) return out;
	for (int i = 0; i < n; ++i) out.push_back(items[i]);
	Tcl_Free(reinterpret_cast<char*>(items));
	return out;
}

bool parseVec(Tcl_Interp* in, const std::string& s, float v[3]) {
	auto it = listItems(in, s);
	if (it.size() < 2) return false;
	for (size_t k = 0; k < 3; ++k) v[k] = k < it.size() ? std::strtof(it[k].c_str(), nullptr) : 0.f;
	return true;
}

// Opcje -nazwa wartość (timer_event, set_event).
std::map<std::string, std::string> options(const Args& a, size_t from) {
	std::map<std::string, std::string> o;
	for (size_t i = from; i + 1 < a.size(); i += 2)
		if (!a[i].empty() && a[i][0] == '-') o[a[i].substr(1)] = a[i + 1];
	return o;
}

const char* kCaptureProcs = R"TCL(
set __r ""
foreach __p [info procs] {
	if {$__p eq "unknown"} continue
	set __a {}
	foreach __x [info args $__p] {
		if {[info default $__p $__x __d]} {lappend __a [list $__x $__d]} else {lappend __a $__x}
	}
	append __r [list proc $__p $__a [info body $__p]] "\n"
}
set __r
)TCL";

const char* kCaptureGlobals = R"TCL(
set __r ""
foreach __v [info globals] {
	if {[string match tcl_* $__v] || $__v in {env errorInfo errorCode auto_index auto_path __r __v __p __a __x __d argv argv0 argc}} continue
	if {[array exists ::$__v]} {
		append __r [list array set $__v [array get ::$__v]] "\n"
	} elseif {[info exists ::$__v]} {
		append __r [list set $__v [set ::$__v]] "\n"
	}
}
set __r
)TCL";

// --- komendy ---

int cmdNop(Engine&, Tcl_Interp* in, Args&) {
	setResult(in, "");
	return TCL_OK;
}
int cmdZero(Engine&, Tcl_Interp* in, Args&) {
	setResult(in, "0");
	return TCL_OK;
}
int cmdLog(Engine&, Tcl_Interp* in, Args&) {
	setResult(in, "");
	return TCL_OK;
}
int cmdEcho(Engine&, Tcl_Interp* in, Args& a) { // lmsg x -> x
	setResult(in, a.size() > 1 ? a[1] : "");
	return TCL_OK;
}
int cmdStub(Engine& e, Tcl_Interp* in, Args& a) {
	if (a.size() > 1) e.stub(a[1]);
	setResult(in, "0");
	return TCL_OK;
}
int cmdCall(Engine& e, Tcl_Interp* in, Args& a) {
	if (a.size() < 2) return TCL_ERROR;
	return e.callFile(in, a[1]) ? TCL_OK : TCL_ERROR;
}
int cmdCd(Engine& e, Tcl_Interp* in, Args& a) {
	std::string p = a.size() > 1 ? a[1] : "";
	std::string cur = e.cwd_;
	std::istringstream ss(p);
	std::string part;
	std::vector<std::string> parts;
	if (p.find('/') != 0) {
		std::istringstream cs(cur);
		while (std::getline(cs, part, '/'))
			if (!part.empty()) parts.push_back(part);
	}
	while (std::getline(ss, part, '/')) {
		if (part.empty() || part == ".") continue;
		if (part == "..") {
			if (!parts.empty()) parts.pop_back();
		} else
			parts.push_back(part);
	}
	std::string r;
	for (auto& x : parts) r += (r.empty() ? "" : "/") + x;
	e.cwd_ = r;
	setResult(in, "");
	return TCL_OK;
}
int cmdLocale(Engine&, Tcl_Interp* in, Args&) { // język danych gry (pliki *_pl.tcl, *_de.tcl)
	setResult(in, "pl");
	return TCL_OK;
}
int cmdInClassDef(Engine& e, Tcl_Interp* in, Args&) {
	setResult(in, e.classContext(in) ? "1" : "0");
	return TCL_OK;
}
// open: zapis do plików gry trafia do katalogu pamięci podręcznej, odczyt najpierw stamtąd, potem z gry.
int cmdRedirect(Engine& e, Tcl_Interp* in, Args& a) {
	if (a.size() < 2) return TCL_ERROR;
	std::string mode = a.size() > 2 ? a[2] : "r";
	std::string game = e.resolvePath(a[1]);
	std::string cache = e.cachePath(game);
	bool write = mode.find_first_of("wa+") != std::string::npos;
	if (write) {
		std::error_code ec;
		std::filesystem::create_directories(std::filesystem::path(cache).parent_path(), ec);
		setResult(in, cache);
	} else if (std::filesystem::exists(cache))
		setResult(in, cache);
	else if (auto p = e.vfs_.resolve(game))
		setResult(in, p->string());
	else
		setResult(in, a[1]);
	return TCL_OK;
}
// get_class_name/type/category/era/flags [klasa]: bez argumentu klasa w definicji albo obiektu.
int cmdClassInfo(Engine& e, Tcl_Interp* in, Args& a) {
	const ScriptClass* c = nullptr;
	if (a.size() > 1) c = e.findClass(a[1]);
	if (!c) c = e.classContext(in);
	if (!c)
		if (ScriptObject* o = e.currentObject(in)) c = o->cls;
	std::string r;
	if (c) {
		if (a[0] == "get_class_name") r = c->name;
		else if (a[0] == "get_class_type") r = c->type;
		else if (a[0] == "get_class_category") r = c->category;
		else if (a[0] == "get_class_era") r = std::to_string(c->epoch);
		else if (a[0] == "get_class_flags") r = c->flags;
	}
	setResult(in, r);
	return TCL_OK;
}
int cmdPwd(Engine& e, Tcl_Interp* in, Args&) {
	setResult(in, e.cwd_);
	return TCL_OK;
}
int cmdGlob(Engine& e, Tcl_Interp* in, Args& a) {
	std::string out;
	for (size_t i = 1; i < a.size(); ++i) {
		if (a[i][0] == '-') continue;
		std::string pat = Vfs::lower(a[i]);
		for (const auto& f : e.vfs_.list(e.cwd_))
			if (fnmatch(pat.c_str(), Vfs::lower(f).c_str(), 0) == 0) out += (out.empty() ? "" : " ") + f;
	}
	setResult(in, out);
	return TCL_OK;
}
int cmdFile(Engine& e, Tcl_Interp* in, Args& a) {
	if (a.size() >= 3 && a[1] == "exists") {
		setResult(in, e.vfs_.exists(e.resolvePath(a[2])) || e.vfs_.exists(a[2]) ? "1" : "0");
		return TCL_OK;
	}
	std::string cmd = "::_ow_file";
	for (size_t i = 1; i < a.size(); ++i) {
		Tcl_Obj* o = Tcl_NewStringObj(a[i].c_str(), -1);
		Tcl_IncrRefCount(o);
		cmd += " ";
		cmd += Tcl_GetString(Tcl_NewListObj(1, &o));
		Tcl_DecrRefCount(o);
	}
	return Tcl_EvalEx(in, cmd.c_str(), -1, 0);
}
int cmdDefClass(Engine& e, Tcl_Interp* in, Args& a) {
	if (a.size() < 7) {
		setResult(in, "def_class: za mało argumentów");
		return TCL_ERROR;
	}
	e.defineClass(in, a);
	setResult(in, "");
	return TCL_OK;
}

// Komendy w ciele klasy.
ScriptClass* needClass(Engine& e, Tcl_Interp* in) { return e.classContext(in); }
int cmdMember(Engine& e, Tcl_Interp* in, Args& a) {
	if (auto* c = needClass(e, in))
		for (size_t i = 1; i + 1 < a.size(); i += 2) c->members.push_back({a[i], a[i + 1]});
	setResult(in, "");
	return TCL_OK;
}
int cmdMethod(Engine& e, Tcl_Interp* in, Args& a) {
	if (auto* c = needClass(e, in); c && a.size() >= 4) c->methods[a[1]] = {a[2], a[3]};
	setResult(in, "");
	return TCL_OK;
}
int cmdObjInit(Engine& e, Tcl_Interp* in, Args& a) {
	if (auto* c = needClass(e, in); c && a.size() >= 2) {
		if (a[0] == "obj_init") c->initBody += a[1] + "\n";
		else c->exitBody += a[1] + "\n";
	}
	setResult(in, "");
	return TCL_OK;
}
int cmdDefEvent(Engine& e, Tcl_Interp* in, Args& a) {
	if (auto* c = needClass(e, in); c && a.size() >= 2) c->eventNames.insert(a[1]);
	setResult(in, "");
	return TCL_OK;
}
int cmdHandleEvent(Engine& e, Tcl_Interp* in, Args& a) {
	if (auto* c = needClass(e, in); c && a.size() >= 3) c->events[a[1]] = a[2];
	setResult(in, "");
	return TCL_OK;
}
int cmdState(Engine& e, Tcl_Interp* in, Args& a) {
	if (auto* c = needClass(e, in); c && a.size() >= 3) {
		if (a[0] == "state") c->states[a[1]] = a[2];
		else if (a[0] == "state_enter") c->stateEnter[a[1]] = a[2];
		else c->stateLeave[a[1]] = a[2];
	}
	setResult(in, "");
	return TCL_OK;
}
int cmdInherit(Engine& e, Tcl_Interp* in, Args& a) {
	auto* c = needClass(e, in);
	for (size_t i = 1; c && i < a.size(); ++i) {
		const ScriptClass* b = e.findClass(a[i]);
		if (!b) continue;
		for (auto& m : b->members) c->members.push_back(m);
		for (auto& [k, v] : b->methods) c->methods[k] = v;
		for (auto& [k, v] : b->events) c->events[k] = v;
		for (auto& [k, v] : b->states) c->states[k] = v;
		for (auto& [k, v] : b->stateEnter) c->stateEnter[k] = v;
		for (auto& [k, v] : b->stateLeave) c->stateLeave[k] = v;
		c->eventNames.insert(b->eventNames.begin(), b->eventNames.end());
		c->initBody = b->initBody + c->initBody;
		c->procScript = b->procScript + c->procScript;
		c->globalsScript = b->globalsScript + c->globalsScript;
		for (auto& [k, v] : b->classAnims) c->classAnims[k] = v;
	}
	setResult(in, "");
	return TCL_OK;
}
int cmdClassFlag(Engine& e, Tcl_Interp* in, Args& a) {
	if (auto* c = needClass(e, in)) {
		if (a[0] == "class_disablescripting") c->scripting = false;
		else if (a[0] == "set_class_anim" && a.size() >= 3) c->classAnims[a[1]] = a[2];
		else if (a[0] == "class_defaultanim" && a.size() >= 2) c->defaultAnim = a[1];
	}
	setResult(in, "");
	return TCL_OK;
}

// Czas, zdarzenia, timery.
int cmdGettime(Engine& e, Tcl_Interp* in, Args&) {
	setResult(in, fmt(e.time_));
	return TCL_OK;
}
int cmdTimerEvent(Engine& e, Tcl_Interp* in, Args& a) {
	if (a.size() < 3) return TCL_ERROR;
	Timer t;
	t.object = e.resolveObj(in, a[1]);
	t.event = a[2];
	auto o = options(a, 3);
	t.fireTime = o.count("attime") ? std::atof(o["attime"].c_str()) : e.time_;
	t.interval = o.count("interval") ? std::atof(o["interval"].c_str()) : 0;
	t.repeat = o.count("repeat") ? std::atoi(o["repeat"].c_str()) : 0;
	t.userid = o.count("userid") ? std::atoi(o["userid"].c_str()) : 0;
	if (t.object) e.timers_.push_back(t);
	setResult(in, "");
	return TCL_OK;
}
int cmdTimerUnset(Engine& e, Tcl_Interp* in, Args& a) {
	int obj = a.size() > 1 ? e.resolveObj(in, a[1]) : 0;
	int uid = a.size() > 2 ? std::atoi(a[2].c_str()) : 0;
	e.timers_.erase(std::remove_if(e.timers_.begin(), e.timers_.end(),
	                               [&](const Timer& t) { return t.object == obj && t.userid == uid; }),
	                e.timers_.end());
	setResult(in, "");
	return TCL_OK;
}
int cmdSetEvent(Engine& e, Tcl_Interp* in, Args& a) {
	if (a.size() < 3) return TCL_ERROR;
	Event ev;
	ev.name = a[2];
	auto o = options(a, 3);
	int target = o.count("target") ? e.resolveObj(in, o["target"]) : e.resolveObj(in, a[1]);
	ev.params = o;
	ev.params["origin"] = std::to_string(e.resolveObj(in, a[1]));
	double delay = o.count("delay") ? std::atof(o["delay"].c_str()) : 0;
	if (delay > 0) {
		Timer t;
		t.object = target, t.event = ev.name, t.fireTime = e.time_ + delay;
		e.timers_.push_back(t);
	} else if (target)
		e.postEvent(target, ev);
	setResult(in, "");
	return TCL_OK;
}
int cmdEventGet(Engine& e, Tcl_Interp* in, Args& a) {
	ScriptObject* o = e.currentObject(in);
	std::string key = a.size() > 2 ? a[2] : "";
	if (!key.empty() && key[0] == '-') key = key.substr(1);
	std::string v = "0";
	if (o) {
		if (key == "timestamp") v = fmt(o->current.time);
		else if (auto it = o->current.params.find(key); it != o->current.params.end()) v = it->second;
	}
	setResult(in, v);
	return TCL_OK;
}

// Maszyna stanów.
int cmdStateCmd(Engine& e, Tcl_Interp* in, Args& a) {
	ScriptObject* o = a.size() > 1 ? e.object(e.resolveObj(in, a[1])) : nullptr;
	std::string r;
	if (o) {
		const std::string& c = a[0];
		if (c == "state_trigger" || c == "state_triggerfresh") e.setState(*o, a.size() > 2 ? a[2] : "", c == "state_triggerfresh");
		else if (c == "state_enable") ++o->stateCounter;
		else if (c == "state_disable") --o->stateCounter;
		else if (c == "state_reset") o->stateCounter = 1;
		else if (c == "state_get") r = o->state;
	}
	setResult(in, r);
	return TCL_OK;
}

// Obiekty.
int cmdGetRef(Engine& e, Tcl_Interp* in, Args& a) {
	setResult(in, std::to_string(a.size() > 1 ? e.resolveObj(in, a[1]) : 0));
	return TCL_OK;
}
int cmdObjInfo(Engine& e, Tcl_Interp* in, Args& a) {
	ScriptObject* o = a.size() > 1 ? e.object(e.resolveObj(in, a[1])) : nullptr;
	std::string r = "0";
	if (a[0] == "obj_valid") r = o && o->alive ? "1" : "0";
	else if (a[0] == "get_objclass") r = o && o->cls ? o->cls->name : "";
	else if (a[0] == "get_objname") r = o ? o->name : "";
	setResult(in, r);
	return TCL_OK;
}
int cmdRef(Engine& e, Tcl_Interp* in, Args& a) {
	ScriptObject* o = a.size() > 2 ? e.object(e.resolveObj(in, a[1])) : nullptr;
	if (!o || !o->interp) {
		setResult(in, a[0] == "ref_checkvar" ? "0" : "");
		return TCL_OK;
	}
	if (a[0] == "ref_set" && a.size() > 3) Tcl_SetVar(o->interp, a[2].c_str(), a[3].c_str(), TCL_GLOBAL_ONLY);
	const char* v = Tcl_GetVar(o->interp, a[2].c_str(), TCL_GLOBAL_ONLY);
	setResult(in, a[0] == "ref_checkvar" ? (v ? "1" : "0") : (v ? v : ""));
	return TCL_OK;
}
int cmdCallMethod(Engine& e, Tcl_Interp* in, Args& a) {
	if (a.size() < 3) return TCL_ERROR;
	ScriptObject* o = e.object(e.resolveObj(in, a[1]));
	if (!o || !o->cls || !o->interp) {
		setResult(in, "");
		return TCL_OK;
	}
	auto it = o->cls->methods.find(a[2]);
	if (it == o->cls->methods.end()) {
		setResult(in, "");
		return TCL_OK;
	}
	auto names = listItems(o->interp, it->second.args);
	for (size_t i = 0; i < names.size(); ++i) {
		auto nd = listItems(o->interp, names[i]);
		std::string name = nd.empty() ? names[i] : nd[0];
		if (name == "args") {
			std::string rest;
			for (size_t k = 3 + i; k < a.size(); ++k) rest += (rest.empty() ? "{" : " {") + a[k] + "}";
			Tcl_SetVar(o->interp, "args", rest.c_str(), TCL_GLOBAL_ONLY);
			break;
		}
		std::string v = 3 + i < a.size() ? a[3 + i] : (nd.size() > 1 ? nd[1] : "");
		Tcl_SetVar(o->interp, name.c_str(), v.c_str(), TCL_GLOBAL_ONLY);
	}
	int rc = Tcl_EvalEx(o->interp, it->second.body.c_str(), -1, TCL_EVAL_GLOBAL);
	if (rc == TCL_ERROR) {
		e.error(o->cls->name + "::" + a[2], o->interp);
		setResult(in, "");
		return TCL_OK;
	}
	setResult(in, Tcl_GetStringResult(o->interp));
	return TCL_OK;
}
int cmdCheckMethod(Engine& e, Tcl_Interp* in, Args& a) {
	const ScriptClass* c = a.size() > 2 ? e.findClass(a[1]) : nullptr;
	setResult(in, c && c->methods.count(a[2]) ? "1" : "0");
	return TCL_OK;
}
int cmdCallMethodStatic(Engine& e, Tcl_Interp* in, Args& a) {
	const ScriptClass* c = a.size() > 2 ? e.findClass(a[1]) : nullptr;
	if (!c || !c->methods.count(a[2])) {
		setResult(in, "");
		return TCL_OK;
	}
	const Method& m = c->methods.at(a[2]);
	// apply {argumenty ciało} arg… : lambda jako prawdziwa lista Tcl (puste argumenty też)
	std::vector<Tcl_Obj*> objv;
	Tcl_Obj* lam[2] = {Tcl_NewStringObj(m.args.c_str(), -1), Tcl_NewStringObj(m.body.c_str(), -1)};
	objv.push_back(Tcl_NewStringObj("apply", -1));
	objv.push_back(Tcl_NewListObj(2, lam));
	for (size_t k = 3; k < a.size(); ++k) objv.push_back(Tcl_NewStringObj(a[k].c_str(), -1));
	for (auto* o : objv) Tcl_IncrRefCount(o);
	if (Tcl_EvalObjv(e.main_, int(objv.size()), objv.data(), TCL_EVAL_GLOBAL) == TCL_ERROR) e.error(c->name + "::" + a[2] + " (static)", e.main_);
	for (auto* o : objv) Tcl_DecrRefCount(o);
	setResult(in, Tcl_GetStringResult(e.main_));
	return TCL_OK;
}
int cmdNew(Engine& e, Tcl_Interp* in, Args& a) {
	if (a.size() < 2) return TCL_ERROR;
	float pos[3] = {0, 0, 0}, rot[3] = {0, 0, 0};
	if (a.size() > 3) parseVec(in, a[3], pos);
	if (a.size() > 4) parseVec(in, a[4], rot);
	std::string name = a.size() > 2 ? a[2] : "";
	int id = e.host_ ? e.host_->createObject(a[1], name, pos, rot) : e.nextId_++;
	if (id <= 0) {
		setResult(in, "0");
		return TCL_OK;
	}
	e.attach(id, a[1], name, true);
	setResult(in, std::to_string(id));
	return TCL_OK;
}
int cmdDel(Engine& e, Tcl_Interp* in, Args& a) {
	int id = a.size() > 1 ? e.resolveObj(in, a[1]) : 0;
	if (id) {
		e.detach(id);
		if (e.host_) e.host_->deleteObject(id);
	}
	setResult(in, "");
	return TCL_OK;
}
int cmdGetPos(Engine& e, Tcl_Interp* in, Args& a) {
	float p[3] = {0, 0, 0};
	int id = a.size() > 1 ? e.resolveObj(in, a[1]) : 0;
	if (e.host_ && id) e.host_->getPos(id, p);
	setResult(in, fmt(p[0]) + " " + fmt(p[1]) + " " + fmt(p[2]));
	return TCL_OK;
}
int cmdSetPos(Engine& e, Tcl_Interp* in, Args& a) {
	float p[3];
	int id = a.size() > 2 ? e.resolveObj(in, a[1]) : 0;
	if (e.host_ && id && parseVec(in, a[2], p)) e.host_->setPos(id, p);
	setResult(in, "");
	return TCL_OK;
}
int cmdSetAnim(Engine& e, Tcl_Interp* in, Args& a) {
	int id = a.size() > 2 ? e.resolveObj(in, a[1]) : 0;
	if (e.host_ && id)
		e.host_->setAnim(id, a[2], a.size() > 3 ? std::atoi(a[3].c_str()) : 0, a.size() > 4 ? std::atoi(a[4].c_str()) : 2);
	setResult(in, "");
	return TCL_OK;
}
int cmdAnimLength(Engine& e, Tcl_Interp* in, Args& a) {
	setResult(in, fmt(e.host_ && a.size() > 1 ? e.host_->animLength(a[1]) * 10.0 : 10.0)); // w klatkach
	return TCL_OK;
}
// action <obj> wait <s> [kod] | anim <anim> [kod] | walk … : 🔶 wait i anim czekają, reszta kończy się od razu.
int cmdAction(Engine& e, Tcl_Interp* in, Args& a) {
	ScriptObject* o = a.size() > 2 ? e.object(e.resolveObj(in, a[1])) : nullptr;
	if (!o) {
		setResult(in, "0");
		return TCL_OK;
	}
	const std::string& kind = a[2];
	double dur = 0;
	std::string done;
	if (kind == "wait" && a.size() > 3) dur = std::atof(a[3].c_str()), done = a.size() > 4 ? a[4] : "";
	else if (kind == "walk" && a.size() > 3) {
		auto p = options(listItems(in, a[3]), 0);
		float t[3];
		done = a.size() > 4 ? a[4] : "";
		if (p.count("target") && parseVec(in, p["target"], t) && e.host_) {
			o->actionDone = done;
			o->actionUntil = -1;
			o->walking = true;
			int id = o->id;
			bool ok = e.host_->walkTo(id, t, [&e, id](bool reached) {
				if (ScriptObject* x = e.object(id)) e.finishAction(*x, reached);
			});
			if (ok) {
				setResult(in, "1");
				return TCL_OK;
			}
			o->walking = false;
			o->walkResult = 3;
		}
		dur = 0.1;
	} else if (kind == "fly" && a.size() > 3) { // fly "-target {p} | -around {p} …": lot do punktu (🔶 prosta trasa)
		auto p = options(listItems(in, a[3]), 0);
		float t[3], cur[3] = {0, 0, 0};
		if (e.host_) e.host_->getPos(o->id, cur);
		bool ok = false;
		if (p.count("target")) ok = parseVec(in, p["target"], t);
		else if (p.count("around") && parseVec(in, p["around"], t)) {
			float r = 1.5f * float(std::rand()) / float(RAND_MAX), ang = 6.283f * float(std::rand()) / float(RAND_MAX);
			t[0] += std::cos(ang) * r, t[1] += std::sin(ang) * r * 0.5f;
			ok = true;
		}
		dur = 1.0;
		if (ok && e.host_) {
			dur = std::max(0.5, double(std::hypot(t[0] - cur[0], t[1] - cur[1])) / 0.8);
			e.host_->glide(o->id, t, float(dur));
		}
		done = a.size() > 4 ? a[4] : "";
	} else if (kind == "move" && a.size() > 3) { // move "{x y z} prędkość": krótki ruch bez szukania drogi
		auto v = listItems(in, a[3]);
		float p[3];
		if (e.host_ && !v.empty() && parseVec(in, v[0], p)) e.host_->setPos(o->id, p);
		dur = 0.3;
		done = a.size() > 4 ? a[4] : "";
	} else if (kind == "rotate" && a.size() > 3) {
		auto v = listItems(in, a[3]);
		if (e.host_ && !v.empty()) e.host_->setRotY(o->id, std::strtof(v[0].c_str(), nullptr));
		dur = 0.3;
		done = a.size() > 4 ? a[4] : "";
	} else if (kind == "anim" && a.size() > 3) {
		if (e.host_) e.host_->setAnim(o->id, a[3], 0, 1);
		dur = e.host_ ? e.host_->animLength(a[3]) : 1.0;
		done = a.size() > 4 ? a[4] : "";
	} else {
		e.stub("action " + kind);
		done = a.size() > 3 ? a.back() : "";
		dur = 0.5;
	}
	o->actionUntil = e.time_ + dur;
	o->actionDone = done;
	setResult(in, "1");
	return TCL_OK;
}
int cmdTasklist(Engine& e, Tcl_Interp* in, Args& a) {
	ScriptObject* o = a.size() > 1 ? e.object(e.resolveObj(in, a[1])) : nullptr;
	std::string r;
	if (o) {
		auto& t = o->tasks;
		const std::string& c = a[0];
		if (c == "tasklist_add" && a.size() > 2) t.push_back(a[2]);
		else if (c == "tasklist_addfront" && a.size() > 2) t.insert(t.begin(), a[2]);
		else if (c == "tasklist_get" && a.size() > 2) {
			size_t i = size_t(std::atoi(a[2].c_str()));
			r = i < t.size() ? t[i] : "";
		} else if (c == "tasklist_rem" && a.size() > 2) {
			size_t i = size_t(std::atoi(a[2].c_str()));
			if (i < t.size()) t.erase(t.begin() + long(i));
		} else if (c == "tasklist_cnt") r = std::to_string(t.size());
		else if (c == "tasklist_clear") t.clear();
	}
	setResult(in, r.empty() && a[0] == "tasklist_cnt" ? "0" : r);
	return TCL_OK;
}
int cmdDefAttrib(Engine& e, Tcl_Interp* in, Args& a) {
	if (a.size() >= 5) e.attribDefs_[a[1]] = {std::atof(a[2].c_str()), std::atof(a[3].c_str()), std::atof(a[4].c_str())};
	setResult(in, "");
	return TCL_OK;
}
// get/set/add_attrib: wartość domyślna i zakres z def_attrib.
int cmdAttrib(Engine& e, Tcl_Interp* in, Args& a) {
	ScriptObject* o = a.size() > 2 ? e.object(e.resolveObj(in, a[1])) : nullptr;
	double v = 0;
	if (o) {
		auto d = e.attribDefs_.find(a[2]);
		auto it = o->attribs.find(a[2]);
		if (it == o->attribs.end()) it = o->attribs.emplace(a[2], d != e.attribDefs_.end() ? d->second.def : 0.0).first;
		double& x = it->second;
		if (a[0] == "set_attrib" && a.size() > 3) x = std::atof(a[3].c_str());
		else if (a[0] == "add_attrib" && a.size() > 3) x += std::atof(a[3].c_str());
		if (d != e.attribDefs_.end()) x = std::clamp(x, d->second.min, d->second.max);
		v = x;
	}
	setResult(in, fmt(v));
	return TCL_OK;
}
int cmdRandom(Engine&, Tcl_Interp* in, Args& a) {
	double n = a.size() > 1 ? std::atof(a[1].c_str()) : 1.0;
	double r = double(std::rand()) / (double(RAND_MAX) + 1.0);
	if (a[0] == "irandom") {
		int lo = 0, hi = int(n);
		if (a.size() > 2) lo = int(n), hi = std::atoi(a[2].c_str());
		setResult(in, std::to_string(hi > lo ? lo + int(r * (hi - lo)) : lo));
	} else
		setResult(in, fmt(r * n));
	return TCL_OK;
}

// --- wektory, listy, liczby (pomoc gry: ENGINE_TCL_API, misc) ---
std::string vecStr(const float v[3]) { return fmt(v[0]) + " " + fmt(v[1]) + " " + fmt(v[2]); }
int cmdVector(Engine&, Tcl_Interp* in, Args& a) {
	float p[3] = {0, 0, 0}, q[3] = {0, 0, 0}, r[3];
	if (a.size() > 1) parseVec(in, a[1], p);
	if (a.size() > 2) parseVec(in, a[2], q);
	const std::string& c = a[0];
	if (c == "vector_add") for (int k = 0; k < 3; ++k) r[k] = p[k] + q[k];
	else if (c == "vector_sub") for (int k = 0; k < 3; ++k) r[k] = p[k] - q[k];
	else if (c == "vector_mul") {
		float f = a.size() > 2 ? std::strtof(a[2].c_str(), nullptr) : 1.f;
		for (int k = 0; k < 3; ++k) r[k] = p[k] * f;
	} else if (c == "vector_normalize") {
		float l = std::sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
		for (int k = 0; k < 3; ++k) r[k] = l > 0 ? p[k] / l : 0.f;
	} else if (c == "vector_dist") {
		setResult(in, fmt(std::hypot(p[0] - q[0], p[1] - q[1])));
		return TCL_OK;
	} else if (c == "vector_dist3d") {
		setResult(in, fmt(std::sqrt((p[0] - q[0]) * (p[0] - q[0]) + (p[1] - q[1]) * (p[1] - q[1]) + (p[2] - q[2]) * (p[2] - q[2]))));
		return TCL_OK;
	} else if (c == "vector_angle") { // kąt w płaszczyźnie x–y od p do q (🔶 konwencja)
		setResult(in, fmt(std::atan2(q[1] - p[1], q[0] - p[0])));
		return TCL_OK;
	} else if (c == "vector_pack") {
		for (int k = 0; k < 3; ++k) r[k] = a.size() > size_t(k + 1) ? std::strtof(a[size_t(k + 1)].c_str(), nullptr) : 0.f;
	} else if (c == "vector_unpackx" || c == "vector_unpacky" || c == "vector_unpackz") {
		setResult(in, fmt(p[c.back() - 'x']));
		return TCL_OK;
	} else if (c == "vector_inbox") {
		float n[3], m[3];
		bool ok = a.size() > 3 && parseVec(in, a[2], n) && parseVec(in, a[3], m);
		bool inside = ok;
		for (int k = 0; k < 3 && ok; ++k) inside = inside && p[k] >= n[k] && p[k] <= m[k];
		setResult(in, inside ? "1" : "0");
		return TCL_OK;
	} else
		for (int k = 0; k < 3; ++k) r[k] = p[k];
	setResult(in, vecStr(r));
	return TCL_OK;
}
int cmdFincr(Engine&, Tcl_Interp* in, Args& a) { // fincr zmienna liczba: incr na liczbach rzeczywistych
	if (a.size() < 2) return TCL_ERROR;
	const char* v = Tcl_GetVar(in, a[1].c_str(), 0);
	double x = (v ? std::atof(v) : 0.0) + (a.size() > 2 ? std::atof(a[2].c_str()) : 1.0);
	std::string s = fmt(x);
	Tcl_SetVar(in, a[1].c_str(), s.c_str(), 0);
	setResult(in, s);
	return TCL_OK;
}
int cmdHminmax(Engine&, Tcl_Interp* in, Args& a) {
	std::vector<double> v;
	for (size_t i = 1; i < a.size(); ++i)
		for (auto& x : listItems(in, a[i])) v.push_back(std::atof(x.c_str()));
	if (v.empty()) v.push_back(0);
	setResult(in, fmt(a[0] == "hmin" ? *std::min_element(v.begin(), v.end()) : *std::max_element(v.begin(), v.end())));
	return TCL_OK;
}
int cmdListOp(Engine&, Tcl_Interp* in, Args& a) { // lnand l1 l2 = elementy l2 spoza l1; land = część wspólna; lor = suma
	auto l1 = a.size() > 1 ? listItems(in, a[1]) : Args{};
	auto l2 = a.size() > 2 ? listItems(in, a[2]) : Args{};
	std::set<std::string> s1(l1.begin(), l1.end());
	Args out;
	if (a[0] == "lnand") {
		for (auto& x : l2)
			if (!s1.count(x)) out.push_back(x);
	} else if (a[0] == "land") {
		for (auto& x : l2)
			if (s1.count(x)) out.push_back(x);
	} else {
		out = l1;
		for (auto& x : l2)
			if (!s1.count(x)) out.push_back(x);
	}
	std::vector<Tcl_Obj*> objs;
	for (auto& x : out) objs.push_back(Tcl_NewStringObj(x.c_str(), -1));
	Tcl_SetObjResult(in, Tcl_NewListObj(int(objs.size()), objs.data()));
	return TCL_OK;
}
int cmdConst(Engine&, Tcl_Interp* in, Args& a) { // proste odpowiedzi do czasu prawdziwych systemów (🔶)
	static const std::map<std::string, std::string> kValues = {
		{"get_remaining_sparetime", "1.0"}, // zawsze czas wolny (brak harmonogramu pracy)
		{"get_prodautoschedule", "0"}, {"is_contained", "0"}, {"is_wearing_pannier", "0"}, {"get_max_fow", "255"},
		{"inv_find", "-1"}, {"inv_find_obj", "-1"}, {"inv_cnt", "0"}, {"inv_list", ""}, {"get_bestexp", ""},
		{"get_linked_to", "-1"}, {"sm_get_event", "0"}, {"tasklist_find", "-1"}};
	auto it = kValues.find(a[0]);
	setResult(in, it != kValues.end() ? it->second : "");
	return TCL_OK;
}
int cmdPosAxis(Engine& e, Tcl_Interp* in, Args& a) { // get_posx/y/z obj
	float p[3] = {0, 0, 0};
	int id = a.size() > 1 ? e.resolveObj(in, a[1]) : 0;
	if (e.host_ && id) e.host_->getPos(id, p);
	setResult(in, fmt(p[a[0].back() - 'x']));
	return TCL_OK;
}
int cmdLinkPos(Engine& e, Tcl_Interp* in, Args& a) { // 🔶 punkt modelu = pozycja obiektu (bez punktów zaczepienia .3db)
	float p[3] = {0, 0, 0};
	int id = a.size() > 1 ? e.resolveObj(in, a[1]) : 0;
	if (e.host_ && id) e.host_->getPos(id, p);
	setResult(in, a[0] == "get_linkpos" ? "0 -0.5 0" : vecStr(p));
	return TCL_OK;
}
int cmdVectorFix(Engine& e, Tcl_Interp* in, Args& a) { // najbliższa osiągalna pozycja
	float p[3] = {0, 0, 0}, out[3];
	if (a.size() > 1) parseVec(in, a[1], p);
	if (e.host_ && e.host_->findPlace(p, 2.f, out)) setResult(in, vecStr(out));
	else setResult(in, vecStr(p));
	return TCL_OK;
}
int cmdLrep(Engine&, Tcl_Interp* in, Args& a) { // lrep zmienna indeks wartość: zamiana elementu listy w zmiennej
	if (a.size() < 4) return TCL_ERROR;
	const char* v = Tcl_GetVar(in, a[1].c_str(), 0);
	auto items = listItems(in, v ? v : "");
	size_t i = size_t(std::max(0, std::atoi(a[2].c_str())));
	if (i < items.size()) items[i] = a[3];
	else items.push_back(a[3]);
	std::vector<Tcl_Obj*> objs;
	for (auto& x : items) objs.push_back(Tcl_NewStringObj(x.c_str(), -1));
	Tcl_Obj* l = Tcl_NewListObj(int(objs.size()), objs.data());
	Tcl_SetVar2Ex(in, a[1].c_str(), nullptr, l, 0);
	Tcl_SetObjResult(in, l);
	return TCL_OK;
}

// --- świat (przez Host) ---
int cmdOwner(Engine& e, Tcl_Interp* in, Args& a) {
	ScriptObject* o = a.size() > 1 ? e.object(e.resolveObj(in, a[1])) : nullptr;
	if (o && a[0] == "set_owner" && a.size() > 2) o->owner = std::atoi(a[2].c_str());
	setResult(in, std::to_string(o ? o->owner : -1));
	return TCL_OK;
}
int cmdRot(Engine& e, Tcl_Interp* in, Args& a) {
	int id = a.size() > 1 ? e.resolveObj(in, a[1]) : 0;
	if (a[0] == "set_roty" && a.size() > 2 && e.host_ && id) e.host_->setRotY(id, std::strtof(a[2].c_str(), nullptr));
	setResult(in, fmt(e.host_ && id ? e.host_->getRotY(id) : 0.f));
	return TCL_OK;
}
int cmdHmap(Engine& e, Tcl_Interp* in, Args& a) {
	float x = a.size() > 1 ? std::strtof(a[1].c_str(), nullptr) : 0.f, y = a.size() > 2 ? std::strtof(a[2].c_str(), nullptr) : 0.f;
	setResult(in, fmt(e.host_ ? e.host_->hmap(x, y) : 0.f));
	return TCL_OK;
}
int cmdUnderwater(Engine& e, Tcl_Interp* in, Args& a) {
	float p[3];
	setResult(in, e.host_ && a.size() > 1 && parseVec(in, a[1], p) && e.host_->underwater(p) ? "1" : "0");
	return TCL_OK;
}
int cmdGnomePos(Engine& e, Tcl_Interp* in, Args& a) {
	int id = a.size() > 1 ? e.resolveObj(in, a[1]) : 0;
	setResult(in, std::to_string(e.host_ && id ? e.host_->gnomePosition(id) : 0));
	return TCL_OK;
}
int cmdWalkResult(Engine& e, Tcl_Interp* in, Args& a) {
	ScriptObject* o = a.size() > 1 ? e.object(e.resolveObj(in, a[1])) : nullptr;
	setResult(in, std::to_string(o ? o->walkResult : 0));
	return TCL_OK;
}
// get_place -center {x y z} [-circle r | -rect x0 y0 x1 y1] …: wolne miejsce albo {0 0 0}.
int cmdGetPlace(Engine& e, Tcl_Interp* in, Args& a) {
	auto o = options(a, 1);
	float c[3] = {0, 0, 0}, r = 2.f, out[3] = {0, 0, 0};
	if (o.count("center")) parseVec(in, o["center"], c);
	if (o.count("circle")) r = std::strtof(o["circle"].c_str(), nullptr);
	if (!(e.host_ && e.host_->findPlace(c, r, out))) out[0] = out[1] = out[2] = 0;
	setResult(in, vecStr(out));
	return TCL_OK;
}
// placelock_set pos promień id / placelock_check pos promień [idwyjątek] / placelock_rem id
int cmdPlacelock(Engine& e, Tcl_Interp* in, Args& a) {
	float p[3];
	const std::string& c = a[0];
	if (c == "placelock_rem" && a.size() > 1) e.placeLocks_.erase(std::atoi(a[1].c_str()));
	else if (c == "placelock_set" && a.size() > 3 && parseVec(in, a[1], p))
		e.placeLocks_[std::atoi(a[3].c_str())] = {{p[0], p[1], p[2]}, std::strtof(a[2].c_str(), nullptr)};
	else if (c == "placelock_check" && a.size() > 2 && parseVec(in, a[1], p)) {
		float r = std::strtof(a[2].c_str(), nullptr);
		int except = a.size() > 3 ? std::atoi(a[3].c_str()) : -1;
		for (auto& [id, l] : e.placeLocks_)
			if (id != except && std::hypot(l.first[0] - p[0], l.first[1] - p[1]) < r + l.second) {
				setResult(in, "1");
				return TCL_OK;
			}
		setResult(in, "0");
		return TCL_OK;
	}
	setResult(in, "");
	return TCL_OK;
}
// obj_query obj -class {…} -type t -range r -limit n -owner o -pos {…} -boundingbox {…}: lista ID albo 0.
int cmdObjQuery(Engine& e, Tcl_Interp* in, Args& a) {
	Args args;
	for (size_t i = 2; i < a.size(); ++i) // parametry bywają jednym napisem: "-class Pilz -range 10"
		for (auto& x : listItems(in, a[i])) args.push_back(x);
	args.insert(args.begin(), {"obj_query", "x"});
	auto o = options(args, 2);
	int self = a.size() > 1 ? e.resolveObj(in, a[1]) : 0;
	float c[3] = {0, 0, 0};
	if (o.count("pos")) parseVec(in, o["pos"], c);
	else if (e.host_ && self) e.host_->getPos(self, c);
	std::set<std::string> classes;
	if (o.count("class"))
		for (auto& x : listItems(in, o["class"])) classes.insert(Vfs::lower(x));
	std::string type = o.count("type") ? o["type"] : "";
	float range = o.count("range") ? std::strtof(o["range"].c_str(), nullptr) : 1e9f;
	size_t limit = o.count("limit") ? size_t(std::atoi(o["limit"].c_str())) : SIZE_MAX;
	std::string owner = o.count("owner") ? o["owner"] : "any";
	int selfOwner = e.object(self) ? e.object(self)->owner : -1;
	float bb[6];
	bool box = o.count("boundingbox") && listItems(in, o["boundingbox"]).size() >= 6;
	if (box) {
		auto b = listItems(in, o["boundingbox"]);
		for (int k = 0; k < 6; ++k) bb[k] = std::strtof(b[size_t(k)].c_str(), nullptr);
	}
	std::vector<std::pair<float, int>> hits;
	for (auto& [id, ob] : e.objects_) {
		if (!ob->alive || !ob->cls) continue;
		if (!classes.empty() && !classes.count(Vfs::lower(ob->cls->name))) continue;
		if (!type.empty() && ob->cls->type != type) continue;
		if (owner == "own" && ob->owner != selfOwner) continue;
		if (owner == "world" && ob->owner != -1) continue;
		if ((owner == "enemy" || owner == "other") && (ob->owner == selfOwner || ob->owner < 0)) continue;
		if (std::isdigit(static_cast<unsigned char>(owner[0])) && ob->owner != std::atoi(owner.c_str())) continue;
		float p[3];
		if (!e.host_ || !e.host_->getPos(id, p)) continue;
		float d = std::hypot(p[0] - c[0], p[1] - c[1]);
		if (d > range) continue;
		if (box && (p[0] - c[0] < bb[0] || p[1] - c[1] < bb[1] || p[0] - c[0] > bb[3] || p[1] - c[1] > bb[4])) continue;
		hits.push_back({d, id});
	}
	std::sort(hits.begin(), hits.end());
	std::string r;
	for (size_t i = 0; i < hits.size() && i < limit; ++i) r += (r.empty() ? "" : " ") + std::to_string(hits[i].second);
	setResult(in, r.empty() ? "0" : r);
	return TCL_OK;
}

int cmdXZ(Engine& e, Tcl_Interp* in, Args& a) {
	const std::string& c = a[0];
	if (c == "get_vectorxz") { // get_vectorxz kąt długość -> wektor w płaszczyźnie x–z (🔶 kierunek kąta)
		float ang = a.size() > 1 ? std::strtof(a[1].c_str(), nullptr) : 0.f, len = a.size() > 2 ? std::strtof(a[2].c_str(), nullptr) : 1.f;
		float v[3] = {std::cos(ang) * len, 0.f, std::sin(ang) * len};
		setResult(in, vecStr(v));
	} else if (c == "get_anglexz") {
		float p[3] = {0, 0, 0}, q[3] = {0, 0, 0};
		if (a.size() > 1) parseVec(in, a[1], p);
		if (a.size() > 2) parseVec(in, a[2], q);
		setResult(in, fmt(std::atan2(q[2] - p[2], q[0] - p[0])));
	} else if (c == "lcount") {
		int n = 0;
		if (a.size() > 2)
			for (auto& x : listItems(in, a[1])) n += x == a[2];
		setResult(in, std::to_string(n));
	} else if (c == "dist_between") {
		float p[3] = {0, 0, 0}, q[3] = {0, 0, 0};
		if (e.host_ && a.size() > 2) e.host_->getPos(e.resolveObj(in, a[1]), p), e.host_->getPos(e.resolveObj(in, a[2]), q);
		setResult(in, fmt(std::hypot(p[0] - q[0], p[1] - q[1])));
	}
	return TCL_OK;
}
// sparetime obj: godziny czasu wolnego (🔶 zawsze 1).
// sparetime obj announce typ | disannounce: obiekt oferuje miejsce czasu wolnego (eat, sleep, home, bath, fun, sex).
// sparetime obj queryrect typ x0 y0 x1 y1 | queryrange typ r: oferujące obiekty w prostokącie (względem obj) / promieniu.
int cmdSparetime(Engine& e, Tcl_Interp* in, Args& a) {
	int self = a.size() > 1 ? e.resolveObj(in, a[1]) : 0;
	std::string sub = a.size() > 2 ? a[2] : "";
	if (sub.empty()) {
		setResult(in, "1.0");
		return TCL_OK;
	}
	if (sub == "announce" && a.size() > 3) e.sparetimeOffers_[self].insert(a[3]);
	else if (sub == "disannounce") {
		if (a.size() > 3) e.sparetimeOffers_[self].erase(a[3]);
		else e.sparetimeOffers_.erase(self);
	} else if ((sub == "queryrect" || sub == "queryrange") && a.size() > 3 && e.host_) {
		float c[3] = {0, 0, 0};
		e.host_->getPos(self, c);
		std::vector<std::pair<float, int>> hits;
		for (auto& [id, offers] : e.sparetimeOffers_) {
			ScriptObject* o = e.object(id);
			float p[3];
			if (!o || !o->alive || !offers.count(a[3]) || !e.host_->getPos(id, p)) continue;
			float dx = p[0] - c[0], dy = p[1] - c[1];
			if (sub == "queryrect" && a.size() > 7) {
				float x0 = std::strtof(a[4].c_str(), nullptr), y0 = std::strtof(a[5].c_str(), nullptr);
				float x1 = std::strtof(a[6].c_str(), nullptr), y1 = std::strtof(a[7].c_str(), nullptr);
				if (dx < x0 || dx > x1 || dy < y0 || dy > y1) continue;
			} else if (sub == "queryrange" && a.size() > 4 && std::hypot(dx, dy) > std::strtof(a[4].c_str(), nullptr))
				continue;
			hits.push_back({std::hypot(dx, dy), id});
		}
		std::sort(hits.begin(), hits.end());
		std::string r;
		for (auto& h : hits) r += (r.empty() ? "" : " ") + std::to_string(h.second);
		setResult(in, r);
		return TCL_OK;
	}
	setResult(in, "");
	return TCL_OK;
}
// net localid -> 0 (gra jednoosobowa: gracz lokalny 0); inne podkomendy sieci: 0.
int cmdNet(Engine&, Tcl_Interp* in, Args&) {
	setResult(in, "0");
	return TCL_OK;
}
int cmdOwnerAttrib(Engine& e, Tcl_Interp* in, Args& a) {
	if (a.size() < 3) return TCL_ERROR;
	int owner = std::atoi(a[1].c_str());
	auto& m = e.ownerAttribs_[owner];
	auto d = e.attribDefs_.find(a[2]);
	auto it = m.find(a[2]);
	if (it == m.end()) it = m.emplace(a[2], d != e.attribDefs_.end() ? d->second.def : 0.0).first;
	if (a[0] == "set_owner_attrib" && a.size() > 3) it->second = std::atof(a[3].c_str());
	else if (a[0] == "add_owner_attrib" && a.size() > 3) it->second += std::atof(a[3].c_str());
	setResult(in, fmt(it->second));
	return TCL_OK;
}

struct Entry {
	const char* name;
	Fn fn;
};
const Entry kCommands[] = {
	{"//", cmdNop}, {"log", cmdLog}, {"logdebug", cmdLog}, {"logmod", cmdLog}, {"print", cmdLog}, {"load_info", cmdLog},
	{"set_run_info", cmdLog}, {"show_loading", cmdLog}, {"lmsg", cmdEcho}, {"__ow_stub", cmdStub},
	{"minimalrun", cmdZero}, {"get_mapedit", cmdZero}, {"perfoptions", cmdZero}, {"startcache", cmdZero},
	{"call", cmdCall}, {"callnc", cmdCall}, {"cd", cmdCd}, {"pwd", cmdPwd}, {"get_class_name", cmdClassInfo}, {"get_class_type", cmdClassInfo},
	{"get_class_category", cmdClassInfo}, {"get_class_era", cmdClassInfo}, {"get_class_flags", cmdClassInfo}, {"__ow_redirect", cmdRedirect}, {"in_class_def", cmdInClassDef}, {"locale", cmdLocale}, {"glob", cmdGlob}, {"file", cmdFile},
	{"def_class", cmdDefClass}, {"member", cmdMember}, {"method", cmdMethod}, {"method_static", cmdMethod},
	{"method_const", cmdMethod}, {"obj_init", cmdObjInit}, {"obj_exit", cmdObjInit}, {"def_event", cmdDefEvent},
	{"handle_event", cmdHandleEvent}, {"state", cmdState}, {"state_enter", cmdState}, {"state_leave", cmdState},
	{"inherit", cmdInherit}, {"class_disablescripting", cmdClassFlag}, {"set_class_anim", cmdClassFlag},
	{"class_defaultanim", cmdClassFlag}, {"gettime", cmdGettime}, {"timer_event", cmdTimerEvent},
	{"timer_unset", cmdTimerUnset}, {"set_event", cmdSetEvent}, {"event_get", cmdEventGet},
	{"state_trigger", cmdStateCmd}, {"state_triggerfresh", cmdStateCmd}, {"state_enable", cmdStateCmd},
	{"state_disable", cmdStateCmd}, {"state_reset", cmdStateCmd}, {"state_get", cmdStateCmd}, {"get_ref", cmdGetRef},
	{"obj_valid", cmdObjInfo}, {"get_objclass", cmdObjInfo}, {"get_objname", cmdObjInfo}, {"ref_get", cmdRef},
	{"ref_set", cmdRef}, {"ref_checkvar", cmdRef}, {"call_method", cmdCallMethod}, {"check_method", cmdCheckMethod},
	{"call_method_static", cmdCallMethodStatic}, {"new", cmdNew}, {"qnew", cmdNew}, {"del", cmdDel},
	{"get_pos", cmdGetPos}, {"set_pos", cmdSetPos}, {"set_anim", cmdSetAnim}, {"db_animlength", cmdAnimLength},
	{"action", cmdAction}, {"tasklist_add", cmdTasklist}, {"tasklist_addfront", cmdTasklist},
	{"tasklist_get", cmdTasklist}, {"tasklist_rem", cmdTasklist}, {"tasklist_cnt", cmdTasklist},
	{"tasklist_clear", cmdTasklist}, {"get_attrib", cmdAttrib}, {"set_attrib", cmdAttrib}, {"add_attrib", cmdAttrib},
	{"random", cmdRandom}, {"irandom", cmdRandom}, {"sel", cmdNop},
	{"vector_add", cmdVector}, {"vector_sub", cmdVector}, {"vector_mul", cmdVector}, {"vector_normalize", cmdVector},
	{"vector_dist", cmdVector}, {"vector_dist3d", cmdVector}, {"vector_angle", cmdVector}, {"vector_pack", cmdVector},
	{"vector_unpackx", cmdVector}, {"vector_unpacky", cmdVector}, {"vector_unpackz", cmdVector}, {"vector_inbox", cmdVector},
	{"fincr", cmdFincr}, {"hmin", cmdHminmax}, {"hmax", cmdHminmax}, {"lnand", cmdListOp}, {"land", cmdListOp},
	{"lor", cmdListOp}, {"get_remaining_sparetime", cmdConst}, {"get_prodautoschedule", cmdConst},
	{"is_contained", cmdConst}, {"is_wearing_pannier", cmdConst}, {"get_max_fow", cmdConst}, {"inv_find", cmdConst},
	{"inv_find_obj", cmdConst}, {"inv_cnt", cmdConst}, {"inv_list", cmdConst}, {"get_bestexp", cmdConst},
	{"get_linked_to", cmdConst}, {"sm_get_event", cmdConst}, {"tasklist_find", cmdConst}, {"get_posx", cmdPosAxis},
	{"get_posy", cmdPosAxis}, {"get_posz", cmdPosAxis}, {"get_linkpos", cmdLinkPos}, {"vector_fix", cmdVectorFix},
	{"lrep", cmdLrep}, {"enable_digbrushes", cmdNop}, {"get_vectorxz", cmdXZ}, {"get_anglexz", cmdXZ},
	{"lcount", cmdXZ}, {"dist_between", cmdXZ}, {"sparetime", cmdSparetime}, {"net", cmdNet}, {"get_owner_attrib", cmdOwnerAttrib},
	{"set_owner_attrib", cmdOwnerAttrib}, {"add_owner_attrib", cmdOwnerAttrib},
	{"gnome_idle", cmdNop}, {"get_owner", cmdOwner}, {"set_owner", cmdOwner}, {"get_roty", cmdRot}, {"set_roty", cmdRot},
	{"get_hmap", cmdHmap}, {"isunderwater", cmdUnderwater}, {"get_gnomeposition", cmdGnomePos},
	{"get_walkresult", cmdWalkResult}, {"get_place", cmdGetPlace}, {"placelock_set", cmdPlacelock},
	{"placelock_check", cmdPlacelock}, {"placelock_rem", cmdPlacelock}, {"obj_query", cmdObjQuery}, {"def_attrib", cmdDefAttrib},
};

int dispatch(ClientData cd, Tcl_Interp* in, int objc, Tcl_Obj* const objv[]) {
	Fn fn = reinterpret_cast<Fn>(cd);
	Args a;
	a.reserve(size_t(objc));
	for (int i = 0; i < objc; ++i) a.emplace_back(Tcl_GetString(objv[i]));
	return fn(*gEngine, in, a);
}

} // namespace

Engine::Engine(const Vfs& vfs) : vfs_(vfs) {
	gEngine = this;
	const char* xdg = std::getenv("XDG_CACHE_HOME");
	const char* home = std::getenv("HOME");
	cacheDir_ = xdg && *xdg ? std::string(xdg) + "/openwiggles" : std::string(home ? home : ".") + "/.cache/openwiggles";
	Tcl_FindExecutable(nullptr);
	main_ = newInterp();
}

Engine::~Engine() {
	for (auto& [id, o] : objects_)
		if (o->interp) Tcl_DeleteInterp(o->interp);
	if (main_) Tcl_DeleteInterp(main_);
	if (gEngine == this) gEngine = nullptr;
}

Tcl_Interp* Engine::newInterp() {
	Tcl_Interp* in = Tcl_CreateInterp();
	Tcl_Init(in); // biblioteka Tcl (init.tcl) bywa niedostępna: wtedy działamy bez niej
	Tcl_EvalEx(in, "rename file ::_ow_file", -1, 0);
	registerCommands(in);
	Tcl_EvalEx(in, "proc unknown {args} { __ow_stub [lindex $args 0]; return 0 }", -1, 0);
	Tcl_EvalEx(in, "rename open ::_ow_open; proc open {name args} { ::_ow_open [__ow_redirect $name [lindex $args 0]] {*}$args }", -1, 0);
	return in;
}

void Engine::registerCommands(Tcl_Interp* in) {
	for (const auto& c : kCommands) Tcl_CreateObjCommand(in, c.name, dispatch, reinterpret_cast<ClientData>(c.fn), nullptr);
}

std::string Engine::resolvePath(const std::string& p) const {
	std::string s = p;
	std::replace(s.begin(), s.end(), '\\', '/');
	while (s.rfind("./", 0) == 0) s = s.substr(2);
	std::string low = Vfs::lower(s);
	if (low.rfind("data/", 0) != 0) s = "Data/" + s;
	else s = "Data/" + s.substr(5);
	return s;
}

std::string Engine::preprocess(const std::string& text) const {
	std::istringstream in(text);
	std::string line, out;
	std::vector<bool> active; // stos warunków
	auto on = [&]() { return std::all_of(active.begin(), active.end(), [](bool b) { return b; }); };
	auto defined = [](const std::string& s) { return s == "FULL"; };
	while (std::getline(in, line)) {
		if (!line.empty() && line.back() == '\r') line.pop_back();
		size_t s = line.find_first_not_of(" \t");
		if (s != std::string::npos && line.compare(s, 3, "//#") == 0) {
			std::istringstream ws(line.substr(s + 3));
			std::string d, sym;
			ws >> d >> sym;
			if (d == "IF") active.push_back(defined(sym));
			else if (d == "IFNOT" || d == "IFNDEF") active.push_back(!defined(sym));
			else if (d == "ELSE" && !active.empty()) active.back() = !active.back();
			else if (d == "ENDIF" && !active.empty()) active.pop_back();
			else if (d == "STOPIFNOT" && !defined(sym)) break;
			out += "\n";
			continue;
		}
		if (!on()) {
			out += "\n";
			continue;
		}
		// Komentarz // do końca linii (poza cudzysłowami).
		bool q = false;
		for (size_t i = 0; i + 1 < line.size(); ++i) {
			if (line[i] == '"' && (i == 0 || line[i - 1] != '\\')) q = !q;
			if (!q && line[i] == '/' && line[i + 1] == '/' && (i == 0 || line[i - 1] != ':')) {
				line.resize(i);
				break;
			}
		}
		out += line + "\n";
	}
	return out;
}

std::string Engine::cachePath(const std::string& gamePath) const { return cacheDir_ + "/" + Vfs::lower(gamePath); }

bool Engine::callFile(Tcl_Interp* in, const std::string& path) {
	std::string p = resolvePath(path);
	std::optional<std::vector<uint8_t>> data;
	if (std::ifstream f{cachePath(p), std::ios::binary}) data.emplace((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	else data = vfs_.read(p);
	if (!data) {
		setResult(in, "call: brak pliku " + path);
		errors_.push_back("call: brak pliku " + path);
		return false;
	}
	std::string code = toUtf8(preprocess(std::string(data->begin(), data->end())));
	int rc = Tcl_EvalEx(in, code.c_str(), -1, TCL_EVAL_GLOBAL);
	if (rc == TCL_ERROR) {
		error("call " + path, in);
		return false;
	}
	return true;
}

void Engine::error(const std::string& where, Tcl_Interp* in) {
	const char* info = Tcl_GetVar(in, "errorInfo", TCL_GLOBAL_ONLY);
	std::string msg = where + ": " + Tcl_GetStringResult(in);
	if (info) {
		std::string i = info;
		if (i.size() > 400) i = i.substr(0, 400) + "…";
		msg += "\n    " + i;
	}
	if (errors_.size() < 5000) errors_.push_back(msg);
}

bool Engine::eval(const std::string& script, std::string* result) {
	int rc = Tcl_EvalEx(main_, script.c_str(), -1, TCL_EVAL_GLOBAL);
	if (result) *result = Tcl_GetStringResult(main_);
	if (rc == TCL_ERROR) error("eval", main_);
	return rc != TCL_ERROR;
}

bool Engine::boot(std::string& err) {
	// Kolejność jak w systeminit.tcl (rozdział 2.4), bez menu startowego, dźwięku i SI.
	const char* files[] = {"scripts/misc/genattribs.tcl", "scripts/init/customcommands.tcl", "scripts/init/shader.tcl",
	                       "scripts/init/txtinit.tcl",     "scripts/init/animinit.tcl",     "scripts/init/classinit.tcl",
	                       "scripts/init/talkinit.tcl",    "scripts/init/claninit.tcl",     "scripts/init/fight.tcl"};
	for (const char* f : files) {
		if (vfs_.exists(resolvePath(f))) callFile(main_, f);
		// 🔶 makettree.tcl (generuje gen_tt.tcl) i init_techtree pomijamy: gra ma gotowy gen_tt.tcl (TODO)
	}
	std::string g;
	if (Tcl_EvalEx(main_, kCaptureGlobals, -1, TCL_EVAL_GLOBAL) == TCL_OK) mainGlobals_ = Tcl_GetStringResult(main_);
	if (classes_.empty()) {
		err = "nie wczytano żadnej klasy";
		return false;
	}
	return true;
}

ScriptClass* Engine::classContext(Tcl_Interp* in) {
	auto it = classInterp_.find(in);
	return it == classInterp_.end() ? nullptr : it->second;
}

const ScriptClass* Engine::findClass(const std::string& name) const {
	auto it = classes_.find(Vfs::lower(name));
	return it == classes_.end() ? nullptr : it->second.get();
}

void Engine::defineClass(Tcl_Interp* caller, const std::vector<std::string>& a) {
	(void)caller;
	auto c = std::make_unique<ScriptClass>();
	c->name = a[1], c->category = a[2], c->type = a[3], c->epoch = std::atoi(a[4].c_str()), c->flags = a[5];
	ScriptClass* cls = c.get();
	// Migawka zmiennych głównego interpretera (stałe z animinit.tcl itd.), odświeżana, gdy przybyły nowe.
	if (Tcl_EvalEx(main_, "llength [info globals]", -1, TCL_EVAL_GLOBAL) == TCL_OK) {
		int n = std::atoi(Tcl_GetStringResult(main_));
		if (n != mainGlobalCount_ && Tcl_EvalEx(main_, kCaptureGlobals, -1, TCL_EVAL_GLOBAL) == TCL_OK) {
			mainGlobals_ = Tcl_GetStringResult(main_);
			mainGlobalCount_ = n;
		}
	}
	Tcl_Interp* tmp = newInterp();
	classInterp_[tmp] = cls;
	// Stałe głównego interpretera (np. ANIM_LOOP) są widoczne także w ciele klasy.
	if (!mainGlobals_.empty()) Tcl_EvalEx(tmp, mainGlobals_.c_str(), -1, TCL_EVAL_GLOBAL);
	if (Tcl_EvalEx(tmp, a[6].c_str(), -1, TCL_EVAL_GLOBAL) == TCL_ERROR) error("def_class " + a[1], tmp);
	if (Tcl_EvalEx(tmp, kCaptureProcs, -1, TCL_EVAL_GLOBAL) == TCL_OK) cls->procScript += Tcl_GetStringResult(tmp);
	if (Tcl_EvalEx(tmp, kCaptureGlobals, -1, TCL_EVAL_GLOBAL) == TCL_OK) cls->globalsScript += Tcl_GetStringResult(tmp);
	classInterp_.erase(tmp);
	Tcl_DeleteInterp(tmp);
	classes_[Vfs::lower(a[1])] = std::move(c);
}

ScriptObject* Engine::object(int id) {
	auto it = objects_.find(id);
	return it == objects_.end() ? nullptr : it->second.get();
}

ScriptObject* Engine::currentObject(Tcl_Interp* in) {
	auto it = byInterp_.find(in);
	return it == byInterp_.end() ? nullptr : it->second;
}

int Engine::resolveObj(Tcl_Interp* in, const std::string& w) {
	if (w == "this") {
		ScriptObject* o = currentObject(in);
		return o ? o->id : 0;
	}
	return std::atoi(w.c_str());
}

ScriptObject* Engine::attach(int id, const std::string& cls, const std::string& name, bool runInit) {
	ScriptClass* c = nullptr;
	auto it = classes_.find(Vfs::lower(cls));
	if (it != classes_.end()) c = it->second.get();
	return createScripted(id, c, name, runInit);
}

ScriptObject* Engine::createScripted(int id, ScriptClass* cls, const std::string& name, bool runInit) {
	auto o = std::make_unique<ScriptObject>();
	o->id = id, o->cls = cls, o->name = name;
	ScriptObject* p = o.get();
	objects_[id] = std::move(o);
	if (!cls || !cls->scripting) return p;
	p->interp = newInterp();
	byInterp_[p->interp] = p;
	Tcl_EvalEx(p->interp, mainGlobals_.c_str(), -1, TCL_EVAL_GLOBAL);
	if (Tcl_EvalEx(p->interp, cls->globalsScript.c_str(), -1, TCL_EVAL_GLOBAL) == TCL_ERROR) error(cls->name + " (zmienne klasy)", p->interp);
	if (Tcl_EvalEx(p->interp, cls->procScript.c_str(), -1, TCL_EVAL_GLOBAL) == TCL_ERROR) error(cls->name + " (procedury klasy)", p->interp);
	for (auto& [k, v] : cls->members) Tcl_SetVar(p->interp, k.c_str(), v.c_str(), TCL_GLOBAL_ONLY);
	if (runInit && !cls->initBody.empty()) runIn(*p, cls->initBody, "obj_init");
	return p;
}

void Engine::detach(int id) {
	auto it = objects_.find(id);
	if (it == objects_.end()) return;
	ScriptObject& o = *it->second;
	if (o.interp && o.cls && !o.cls->exitBody.empty()) runIn(o, o.cls->exitBody, "obj_exit");
	o.alive = false;
	timers_.erase(std::remove_if(timers_.begin(), timers_.end(), [&](const Timer& t) { return t.object == id; }), timers_.end());
	// Interpreter usuwamy dopiero przy niszczeniu silnika: obiekt może usunąć sam siebie w trakcie kodu.
}

bool Engine::runIn(ScriptObject& o, const std::string& code, const char* what) {
	if (!o.interp || !o.alive) return false;
	int rc = Tcl_EvalEx(o.interp, code.c_str(), -1, TCL_EVAL_GLOBAL);
	if (rc == TCL_ERROR) {
		error((o.cls ? o.cls->name : std::string("?")) + " " + std::to_string(o.id) + " " + what, o.interp);
		return false;
	}
	return true;
}

void Engine::postEvent(int target, Event e) {
	ScriptObject* o = object(target);
	if (!o || !o->alive) return;
	e.time = time_;
	o->queue.push_back(std::move(e));
}

void Engine::setState(ScriptObject& o, const std::string& st, bool fresh) {
	if (!o.cls) return;
	if (!o.state.empty())
		if (auto it = o.cls->stateLeave.find(o.state); it != o.cls->stateLeave.end()) runIn(o, it->second, "state_leave");
	o.state = st;
	if (fresh) o.stateCounter = 1;
	if (!st.empty())
		if (auto it = o.cls->stateEnter.find(st); it != o.cls->stateEnter.end()) runIn(o, it->second, "state_enter");
}

void Engine::tick(double dt) {
	time_ += dt;
	// Timery -> zdarzenia.
	std::vector<Timer> due;
	for (auto it = timers_.begin(); it != timers_.end();) {
		if (it->fireTime <= time_) {
			due.push_back(*it);
			if (it->repeat != 0 && it->interval > 0) {
				it->fireTime += it->interval;
				if (it->repeat > 0) --it->repeat;
				++it;
			} else
				it = timers_.erase(it);
		} else
			++it;
	}
	for (auto& t : due) postEvent(t.object, Event{t.event, {}, time_});
	// Obiekty: akcje, zdarzenia, ciało stanu.
	std::vector<ScriptObject*> list;
	list.reserve(objects_.size());
	for (auto& [id, o] : objects_)
		if (o->alive && o->interp) list.push_back(o.get());
	for (ScriptObject* o : list) {
		if (!o->walking && o->actionUntil >= 0 && time_ >= o->actionUntil) {
			o->actionUntil = -1;
			std::string done;
			std::swap(done, o->actionDone);
			if (!done.empty()) runIn(*o, done, "akcja");
		}
		int guard = 0;
		while (!o->queue.empty() && guard++ < 16) {
			Event e = std::move(o->queue.front());
			o->queue.pop_front();
			auto it = o->cls->events.find(e.name);
			if (it == o->cls->events.end()) continue;
			o->current = e;
			runIn(*o, it->second, e.name.c_str());
		}
		if (!o->state.empty() && o->stateCounter > 0 && o->actionUntil < 0 && !o->walking)
			if (auto it = o->cls->states.find(o->state); it != o->cls->states.end()) runIn(*o, it->second, ("stan " + o->state).c_str());
	}
}

void Engine::finishAction(ScriptObject& o, bool ok) {
	o.walking = false;
	o.walkResult = ok ? 4 : 3;
	o.actionUntil = time_; // kod zakończenia wykona najbliższa tura logiki
}

void Engine::writeLog(const std::string& path) const {
	std::ofstream f(path);
	f << "# OpenWiggles: dziennik skryptów\n# klasy: " << classes_.size() << ", obiekty ze skryptami: " << objects_.size()
	  << ", błędy: " << errors_.size() << "\n\n## Zaślepki (komenda: liczba wywołań)\n";
	std::vector<std::pair<int, std::string>> s;
	for (auto& [k, v] : stubs_) s.push_back({v, k});
	std::sort(s.rbegin(), s.rend());
	for (auto& [v, k] : s) f << k << "\t" << v << "\n";
	f << "\n## Błędy\n";
	for (auto& e : errors_) f << e << "\n";
}

} // namespace ow::script
