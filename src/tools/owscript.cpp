// owscript: test silnika skryptów bez okna (docs/silnik/11_obiekty_tcl_i_petla.md §11.9).
//
//   owscript [--game <katalog gry>] [KLASA [SEKUNDY]] [-o dziennik.log]
// Wczytuje skrypty startowe i klasy, tworzy jeden obiekt KLASY (domyślnie Zwerg), przewija logikę
// o SEKUNDY (domyślnie 30) i zapisuje dziennik: zaślepki komend natywnych i błędy skryptów.
#include "lib/script/engine.h"
#include "lib/vfs.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace ow;

int main(int argc, char** argv)
{
	std::string game, cls = "Zwerg", log = "owscript.log";
	double seconds = 30;
	int pos = 0;
	for (int i = 1; i < argc; ++i)
	{
		std::string a = argv[i];
		if (a == "--game" && i + 1 < argc) game = argv[++i];
		else if (a == "-o" && i + 1 < argc) log = argv[++i];
		else if (pos++ == 0) cls = a;
		else seconds = std::atof(a.c_str());
	}
	if (game.empty())
	{
		const char* e = std::getenv("OW_GAME");
		const char* h = std::getenv("HOME");
		game = e ? e : std::string(h ? h : ".") + "/.wine/drive_c/Program Files (x86)/Innonics/Wiggles";
	}
	Vfs vfs;
	vfs.addRoot(game);
	script::Engine e(vfs);
	auto t0 = std::chrono::steady_clock::now();
	std::string err;
	bool ok = e.boot(err);
	auto ms = [&](auto t) { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t).count(); };
	std::printf("start %s%s: klasy %zu, błędy %zu, %.0f ms\n", ok ? "OK" : "BŁĄD ", err.c_str(), e.classCount(), e.errors().size(), ms(t0));
	auto t1 = std::chrono::steady_clock::now();
	auto* o = e.attach(1, cls, "test");
	std::printf("obiekt %s: %s, %.0f ms, stan '%s'\n", cls.c_str(), o && o->interp ? "ze skryptami" : "bez skryptów", ms(t1),
	    o ? o->state.c_str() : "");
	for (double t = 0; t < seconds; t += 0.1) e.tick(0.1);
	std::printf("po %.0f s: stan '%s', błędy %zu, zaślepki %zu\n", seconds, o ? o->state.c_str() : "", e.errors().size(), e.stubCalls().size());
	e.writeLog(log);
	std::printf("dziennik: %s\n", log.c_str());
	return ok ? 0 : 1;
}
