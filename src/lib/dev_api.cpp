#include "lib/dev_api.h"

#include "lib/map_scene.h"
#include "lib/map_template.h"
#include "lib/png_write.h"
#include "lib/script/engine.h"
#include "lib/text.h"
#include "lib/world_sim.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <thread>

namespace ow
{
namespace
{

std::string num(double v)
{
	char b[48];
	std::snprintf(b, sizeof b, "%.4g", v);
	return b;
}

// Tekst z danych gry (CP1250) jako UTF-8; poprawny UTF-8 zostaje bez zmian.
std::string utf8(const std::string& s)
{
	for (unsigned char c : s)
		if (c >= 0x80) return cp1250ToUtf8(s);
	return s;
}

const char* kApiHelp = R"TXT(Serwer deweloperski OpenWiggles (docs/SERWER_HTTP.md). Tylko 127.0.0.1.

Zgodne z WebTraq oryginału (port 8082 w grze):
  GET  /tclshell?script=…        Tcl w głównym interpreterze: "Result:", pusta linia, "Ok." albo "Error:", wynik
  GET  /index                    program, mapa, czas gry
  GET  /objlist                  obiekty ze skryptami: ID klasa nazwa x y z
  GET  /objinfo:<ID|nazwa>       karta obiektu (klasa, pozycja, stan, właściciel, zadania, okno pracy)
  GET  /classlist                klasy skryptów
  GET  /classinfo:<klasa>        metody, zdarzenia, stany klasy

API (JSON):
  POST /api/eval  (treść: skrypt)  albo  GET /api/eval?script=…   {"ok":…,"result":…}
  GET  /api/state                program, mapa, czas, pauza, symulacja, prowadzona postać
  GET  /api/objects              obiekty sceny: indeks, id, klasa, model, animacja, pozycja, obrót
  GET  /api/units                postacie: indeks, obiekt, id, krasnal, x, y, pole, z, minz, stan, ścieżka
  POST /api/simulate?on=1|0      wigview: symulacja (skrypty) włączona/wyłączona
  POST /api/pause  /api/resume   pauza mapy
  POST /api/step?seconds=S       przewinięcie świata o S s (domyślnie 0,1 = jedna tura logiki), także w pauzie
  POST /api/control?unit=K       przejęcie postaci K (−1 albo /api/control/release: puść)
  POST /api/control/step?dx=1&dy=0      krok o pole (dx ±1 w bok, dy −1 w górę, +1 w dół)
  POST /api/control/goto?x=X&y=Y        idź do pola
  GET  /api/screenshot           bieżąca klatka PNG
  GET  /api/wait?cond=TCL&timeout=S     czekaj, aż warunek Tcl da 1 (sprawdzany co 0,1 s)
  GET  /                         strona podglądu w przeglądarce
)TXT";

const char* kPage = R"HTML(<!doctype html><html lang="pl"><head><meta charset="utf-8"><title>OpenWiggles: serwer deweloperski</title>
<style>body{font:14px sans-serif;margin:16px;background:#1d2128;color:#ddd}textarea{width:100%;height:90px;background:#111;color:#ddd}
pre{background:#111;padding:8px;white-space:pre-wrap}table{border-collapse:collapse;font-size:12px}td,th{border:1px solid #444;padding:2px 6px}
button{margin:4px 4px 4px 0}a{color:#8cf}</style></head><body>
<h3>OpenWiggles: serwer deweloperski</h3><div id="st"></div>
<p><textarea id="tcl">list [gettime] [gethours]</textarea><br><button onclick="ev()">Wykonaj Tcl (Ctrl+Enter)</button>
<button onclick="post('/api/pause')">Pauza</button><button onclick="post('/api/resume')">Graj</button>
<button onclick="post('/api/step?seconds=1')">Krok 1 s</button><a href="/api/screenshot" target="_blank">zrzut</a> <a href="/api">API</a></p>
<pre id="out"></pre><h4>Postacie</h4><div id="units"></div><h4>Obiekty ze skryptami</h4><pre id="objs"></pre>
<script>
async function ev(){const r=await fetch('/api/eval',{method:'POST',body:document.getElementById('tcl').value});const j=await r.json();
document.getElementById('out').textContent=(j.ok?'':'BŁĄD: ')+(j.result!==undefined?j.result:j.error);}
async function post(u){await fetch(u,{method:'POST'});refresh();}
document.getElementById('tcl').addEventListener('keydown',e=>{if(e.ctrlKey&&e.key=='Enter')ev();});
async function refresh(){try{const s=await (await fetch('/api/state')).json();document.getElementById('st').textContent=
s.program+' | mapa: '+s.map+' | czas gry '+s.time.toFixed(1)+' s | '+(s.paused?'pauza':'gra')+' | symulacja '+(s.simulation?'tak':'nie');
const u=await (await fetch('/api/units')).json();document.getElementById('units').innerHTML='<table><tr><th>#</th><th>rodzaj</th><th>x</th><th>y</th><th>pole</th><th>z</th><th>min z</th><th>stan</th><th></th></tr>'+
u.units.map(x=>`<tr><td>${x.index}</td><td>${x.gnome?'krasnal':'wuker'}</td><td>${x.x.toFixed(2)}</td><td>${x.y.toFixed(2)}</td><td>${x.tx},${x.ty}</td><td>${x.z.toFixed(2)}</td><td>${x.minz.toFixed(2)}</td><td>${x.state}</td><td><button onclick="post('/api/control?unit=${x.index}')">steruj</button></td></tr>`).join('')+'</table>';
document.getElementById('objs').textContent=await (await fetch('/objlist')).text();}catch(e){}}
refresh();setInterval(refresh,1000);
</script></body></html>)HTML";

const char* stateName(int s)
{
	return s == 1 ? "walk" : s == 2 ? "dig"
	                                : "idle";
}

} // namespace

void registerDevApi(DevServer& srv, DevHooks hooks)
{
	auto H = std::make_shared<DevHooks>(std::move(hooks));
	auto engine = [H]() -> script::Engine* {
		WorldSim* s = H->sim ? H->sim() : nullptr;
		return s && s->scripts ? s->scripts.get() : nullptr;
	};
	auto noEngine = [H]() {
		return std::string("brak silnika skryptów") + (H->program == "wigview" ? " (wigview: włącz symulację, POST /api/simulate?on=1)" : "");
	};
	auto evalTcl = [engine, noEngine](const std::string& script, bool& ok) {
		script::Engine* e = engine();
		if (!e)
		{
			ok = false;
			return noEngine();
		}
		std::string r;
		ok = e->eval(script, &r);
		return utf8(r);
	};
	auto scriptOf = [](const DevRequest& r) { return r.body.empty() ? r.param("script") : r.body; };

	srv.on("GET", "/api", [](const DevRequest&) { return DevReply::text(kApiHelp); });
	srv.on("GET", "/", [](const DevRequest&) { return DevReply{200, "text/html; charset=utf-8", kPage}; });

	// --- zgodne z WebTraq ---
	srv.on("GET", "/tclshell", [evalTcl](const DevRequest& r) {
		bool ok;
		std::string s = r.param("script"), res = evalTcl(s, ok);
		return DevReply::text("Script:\n" + s + "\nResult:\n\n" + (ok ? "Ok.\n" : "Error:\n") + res + "\n\n" + s + "\n");
	});
	srv.on("GET", "/index", [H, engine](const DevRequest&) {
		script::Engine* e = engine();
		std::string t = "OpenWiggles " + H->program + "\nmapa: " + (H->mapName ? H->mapName() : "") + "\n";
		t += e ? "czas gry: " + num(e->time()) + " s (" + num(e->time() / script::kHourSec) + " h)\n" : "symulacja wyłączona\n";
		return DevReply::text(t);
	});
	srv.on("GET", "/objlist", [engine, noEngine](const DevRequest&) {
		script::Engine* e = engine();
		if (!e) return DevReply::text(noEngine() + "\n", 409);
		std::map<int, std::string> lines;
		for (const auto& [id, o] : e->objects_)
		{
			if (!o->alive) continue;
			float p[3] = {0, 0, 0};
			if (e->host_) e->host_->getPos(id, p);
			lines[id] = std::to_string(id) + " " + (o->cls ? o->cls->name : "?") + " " + utf8(o->name) + " " + num(p[0]) + " " + num(p[1]) + " " + num(p[2]);
		}
		std::string t;
		for (auto& [id, l] : lines) t += l + "\n";
		return DevReply::text(t);
	});
	srv.on("GET", "/objinfo:", [engine, noEngine](const DevRequest& r) {
		script::Engine* e = engine();
		if (!e) return DevReply::text(noEngine() + "\n", 409);
		script::ScriptObject* o = nullptr;
		if (!r.tail.empty() && std::isdigit(static_cast<unsigned char>(r.tail[0]))) o = e->object(std::atoi(r.tail.c_str()));
		for (auto& [id, x] : e->objects_)
			if (!o && x->alive && (x->name == r.tail || utf8(x->name) == r.tail)) o = x.get();
		if (!o) return DevReply::text("nie ma obiektu: " + r.tail + "\n", 404);
		float p[3] = {0, 0, 0};
		if (e->host_) e->host_->getPos(o->id, p);
		std::string t = "ID: " + std::to_string(o->id) + "\nKlasa: " + (o->cls ? o->cls->name : "?") + "\nNazwa: " + utf8(o->name) +
		    "\nPozycja: " + num(p[0]) + " " + num(p[1]) + " " + num(p[2]) + "\nStan: " + o->state + " (licznik " +
		    std::to_string(o->stateCounter) + ")\nWłaściciel: " + std::to_string(o->owner) + "\nOkno pracy: " + num(o->workStart) + " " +
		    num(o->workLen) + "\nWidoczny: " + (o->visible ? "1" : "0") + "\nSterowany z zewnątrz: " + (o->external ? "1" : "0") + "\n";
		t += "Zadania (" + std::to_string(o->tasks.size()) + "):\n";
		for (const auto& task : o->tasks) t += "  " + utf8(task) + "\n";
		t += "Atrybuty:\n";
		for (const auto& [k, v] : o->attribs) t += "  " + k + " = " + num(v) + "\n";
		return DevReply::text(t);
	});
	srv.on("GET", "/classlist", [engine, noEngine](const DevRequest&) {
		script::Engine* e = engine();
		if (!e) return DevReply::text(noEngine() + "\n", 409);
		std::string t;
		for (const auto& [k, c] : e->classes_) t += c->name + " " + c->type + "\n";
		return DevReply::text(t);
	});
	srv.on("GET", "/classinfo:", [engine, noEngine](const DevRequest& r) {
		script::Engine* e = engine();
		if (!e) return DevReply::text(noEngine() + "\n", 409);
		const script::ScriptClass* c = e->findClass(r.tail);
		if (!c) return DevReply::text("nie ma klasy: " + r.tail + "\n", 404);
		std::string t = "Klasa: " + c->name + "\nTyp: " + c->type + "\nMetody:";
		for (const auto& [k, m] : c->methods) t += " " + k;
		t += "\nZdarzenia:";
		for (const auto& [k, m] : c->events) t += " " + k;
		t += "\nStany:";
		for (const auto& [k, m] : c->states) t += " " + k;
		return DevReply::text(t + "\nAnimacja domyślna: " + c->defaultAnim + "\n");
	});

	// --- API JSON ---
	auto evalRoute = [evalTcl, scriptOf](const DevRequest& r) {
		bool ok;
		std::string res = evalTcl(scriptOf(r), ok);
		return DevReply::json(std::string("{\"ok\":") + (ok ? "true" : "false") + ",\"result\":" + jsonStr(res) + "}");
	};
	srv.on("POST", "/api/eval", evalRoute);
	srv.on("GET", "/api/eval", evalRoute);
	srv.on("GET", "/api/state", [H, engine](const DevRequest&) {
		script::Engine* e = engine();
		return DevReply::json("{\"program\":" + jsonStr(H->program) + ",\"map\":" + jsonStr(H->mapName ? H->mapName() : "") +
		    ",\"time\":" + num(e ? e->time() : 0.0) + ",\"paused\":" + (H->paused && H->paused() ? "true" : "false") +
		    ",\"simulation\":" + (e ? "true" : "false") + ",\"controlled\":" + std::to_string(H->controlled ? H->controlled() : -1) + "}");
	});
	srv.on("GET", "/api/objects", [H](const DevRequest&) {
		MapScene* s = H->scene ? H->scene() : nullptr;
		WorldSim* w = H->sim ? H->sim() : nullptr;
		std::string j = "{\"objects\":[";
		for (int i = 0; s && i < s->objectCount(); ++i)
		{
			const auto& src = s->objectSource(i);
			const float* p = s->objectPos(i);
			int id = w && i < int(w->idxToId.size()) ? w->idxToId[size_t(i)] : 0;
			j += std::string(i ? "," : "") + "{\"index\":" + std::to_string(i) + ",\"id\":" + std::to_string(id) +
			    ",\"class\":" + jsonStr(src.className) + ",\"name\":" + jsonStr(utf8(src.instanceName)) +
			    ",\"model\":" + jsonStr(s->objectModelObject(i)) + ",\"anim\":" + jsonStr(s->objectAnimation(i)) + ",\"pos\":[" +
			    num(p[0]) + "," + num(p[1]) + "," + num(p[2]) + "],\"roty\":" + num(s->objectRotY(i)) + "}";
		}
		return DevReply::json(j + "]}");
	});
	srv.on("GET", "/api/units", [H](const DevRequest&) {
		WorldSim* w = H->sim ? H->sim() : nullptr;
		std::string j = "{\"units\":[";
		for (size_t k = 0; w && k < w->units.size(); ++k)
		{
			const SimUnit& u = w->units[k];
			int id = u.obj < int(w->idxToId.size()) ? w->idxToId[size_t(u.obj)] : 0;
			std::string path;
			for (size_t s = u.step; s < u.path.size(); ++s)
				path += std::string(path.empty() ? "" : ",") + "[" + std::to_string(u.path[s].first) + "," + std::to_string(u.path[s].second) + "]";
			j += std::string(k ? "," : "") + "{\"index\":" + std::to_string(k) + ",\"obj\":" + std::to_string(u.obj) + ",\"id\":" +
			    std::to_string(id) + ",\"gnome\":" + (u.gnome ? "true" : "false") + ",\"x\":" + num(u.x) + ",\"y\":" + num(u.y) +
			    ",\"tx\":" + std::to_string(u.tx) + ",\"ty\":" + std::to_string(u.ty) + ",\"z\":" + num(u.z) + ",\"minz\":" +
			    num(w->walkZ(u.x, u.ty)) + ",\"state\":\"" + stateName(int(u.state)) + "\",\"controlled\":" +
			    (u.controlled ? "true" : "false") + ",\"path\":[" + path + "]}";
		}
		return DevReply::json(j + "]}");
	});
	srv.on("POST", "/api/simulate", [H](const DevRequest& r) {
		if (!H->setSimulate) return DevReply::json("{\"ok\":false,\"error\":\"program nie przełącza symulacji\"}", 400);
		bool ok = H->setSimulate(r.param("on", "1") != "0");
		return DevReply::json(std::string("{\"ok\":") + (ok ? "true" : "false") + "}");
	});
	srv.on("POST", "/api/pause", [H](const DevRequest&) {
		if (H->setPaused) H->setPaused(true);
		return DevReply::json("{\"ok\":true}");
	});
	srv.on("POST", "/api/resume", [H](const DevRequest&) {
		if (H->setPaused) H->setPaused(false);
		return DevReply::json("{\"ok\":true}");
	});
	srv.on("POST", "/api/step", [H, engine](const DevRequest& r) {
		double s = std::atof(r.param("seconds", "0.1").c_str());
		if (!H->advance || s < 0 || s > 600) return DevReply::json("{\"ok\":false,\"error\":\"seconds 0..600\"}", 400);
		H->advance(s);
		script::Engine* e = engine();
		return DevReply::json("{\"ok\":true,\"time\":" + num(e ? e->time() : 0.0) + "}");
	});
	auto unitOk = [H](int k) {
		WorldSim* w = H->sim ? H->sim() : nullptr;
		return w && k >= 0 && k < int(w->units.size());
	};
	srv.on("POST", "/api/control", [H, unitOk](const DevRequest& r) {
		int k = std::atoi(r.param("unit", "-1").c_str());
		if (k >= 0 && !unitOk(k)) return DevReply::json("{\"ok\":false,\"error\":\"nie ma takiej postaci\"}", 404);
		if (H->setControlled) H->setControlled(k);
		return DevReply::json("{\"ok\":true,\"controlled\":" + std::to_string(H->controlled ? H->controlled() : k) + "}");
	});
	srv.on("POST", "/api/control/release", [H](const DevRequest&) {
		if (H->setControlled) H->setControlled(-1);
		return DevReply::json("{\"ok\":true}");
	});
	srv.on("POST", "/api/control/step", [H, unitOk](const DevRequest& r) {
		int k = H->controlled ? H->controlled() : -1;
		if (!unitOk(k)) return DevReply::json("{\"ok\":false,\"error\":\"żadna postać nie jest prowadzona (POST /api/control?unit=K)\"}", 409);
		bool ok = H->sim()->controlStep(k, std::atoi(r.param("dx", "0").c_str()), std::atoi(r.param("dy", "0").c_str()));
		return DevReply::json(std::string("{\"ok\":") + (ok ? "true" : "false") + "}");
	});
	srv.on("POST", "/api/control/goto", [H, unitOk](const DevRequest& r) {
		int k = H->controlled ? H->controlled() : -1;
		if (!unitOk(k)) return DevReply::json("{\"ok\":false,\"error\":\"żadna postać nie jest prowadzona (POST /api/control?unit=K)\"}", 409);
		bool ok = H->sim()->controlGoTo(k, std::atoi(r.param("x").c_str()), std::atoi(r.param("y").c_str()));
		return DevReply::json(std::string("{\"ok\":") + (ok ? "true" : "false") + "}");
	});
	srv.on("GET", "/api/screenshot", [H](const DevRequest&) {
		int w = 0, h = 0;
		std::vector<uint8_t> px;
		if (!H->screenshot || !H->screenshot(w, h, px) || px.size() != size_t(w) * size_t(h) * 4)
			return DevReply::text("zrzut niedostępny\n", 500);
		auto png = encodePng(w, h, px);
		return DevReply{200, "image/png", std::string(png.begin(), png.end())};
	});
	// Czekanie w wątku serwera: warunek sprawdzany w wątku głównym co 0,1 s.
	srv.onServer("GET", "/api/wait", [&srv, evalTcl](const DevRequest& r) {
		std::string cond = r.param("cond");
		double timeout = std::atof(r.param("timeout", "10").c_str());
		auto t0 = std::chrono::steady_clock::now();
		std::string last;
		for (;;)
		{
			bool ok = false;
			DevReply rep = srv.main([&] {
				std::string res = evalTcl("expr {" + cond + "}", ok);
				return DevReply::text(res);
			});
			last = rep.body;
			if (ok && last != "0" && !last.empty())
				return DevReply::json("{\"ok\":true,\"result\":" + jsonStr(last) + "}");
			if (!ok && rep.status == 200)
				return DevReply::json("{\"ok\":false,\"error\":" + jsonStr(last) + "}", 400);
			double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
			if (el >= timeout) return DevReply::json("{\"ok\":false,\"timeout\":true,\"result\":" + jsonStr(last) + "}", 408);
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	});
}

} // namespace ow
