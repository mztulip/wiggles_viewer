// Silnik skryptów: osadzony Tcl 8.6 z komendami gry (docs/silnik/02_start_i_skrypty.md, 11_obiekty_tcl_i_petla.md).
//
// - Główny interpreter wykonuje skrypty startowe (atrybuty, animacje, klasy…).
// - def_class wykonuje ciało klasy w tymczasowym interpreterze i zapamiętuje: pola, metody, obj_init,
//   zdarzenia, stany, procedury i zmienne globalne ciała (procedury z dołączonych plików, np. z_*.tcl).
// - Każdy obiekt ze skryptami dostaje własny interpreter: zmienne obiektu = jego zmienne globalne.
//   Przy tworzeniu kopiujemy do niego zmienne globalne głównego interpretera (stałe startowe),
//   procedury i zmienne klasy, a potem wykonujemy obj_init.
// - Komendy gry, których jeszcze nie mamy, są zaślepkami: zwracają "0" i trafiają do licznika (log).
// 🔶 Pierwsza wersja: brak trybu bezpiecznego, brak pamięci podręcznej klas, zgodność 8.3 → 8.6 nie
// sprawdzona w całości (różnice expr, kodowanie). Lista zaślepek: openwiggles_tcl.log.
#pragma once

#include "lib/messages.h"

#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

struct Tcl_Interp;

namespace ow
{

class Vfs;

namespace script
{

struct Method
{
	std::string args, body;
};

struct ScriptClass
{
	std::string name, category, type, flags;
	int epoch = 0;
	bool scripting = true; // class_disablescripting wyłącza
	std::string initBody, exitBody;
	std::vector<std::pair<std::string, std::string>> members;
	std::map<std::string, Method> methods;
	std::map<std::string, std::string> events, states, stateEnter, stateLeave;
	std::set<std::string> eventNames;
	std::string procScript;    // odtworzenie procedur ciała klasy (proc … ; proc …)
	std::string globalsScript; // odtworzenie zmiennych globalnych ciała klasy
	std::map<std::string, std::string> classAnims; // set_class_anim stan -> obiekt.animacja
	std::string defaultAnim;
};

struct Event
{
	std::string name;
	std::map<std::string, std::string> params; // -subject1, -num1, … (bez myślnika)
	double time = 0;
};

struct Timer
{
	int object = 0;
	std::string event;
	double fireTime = 0, interval = 0;
	int repeat = 0; // −1 bez końca, 0 = raz
	int userid = 0;
};

// Operacje na świecie, których silnik potrzebuje od gry (implementuje src/apps/game).
struct Host
{
	virtual ~Host() = default;
	virtual int createObject(const std::string& cls, const std::string& name, const float pos[3], const float rot[3]) = 0;
	virtual void deleteObject(int id) = 0;
	virtual bool getPos(int id, float pos[3]) = 0;
	virtual void setPos(int id, const float pos[3]) = 0;
	virtual void setRotY(int, float) {}
	virtual void setAnim(int, const std::string& /*obiekt.anim*/, int /*klatka*/, int /*tryb*/) {}
	virtual double animLength(int /*id*/, const std::string& /*obiekt.anim*/) { return 1.0; } // sekundy
	// Akcja chodzenia: true = przyjęta; done(true) po dojściu, done(false) gdy nie ma drogi.
	virtual bool walkTo(int, const float[3], std::function<void(bool)>) { return false; }
	virtual float getRotY(int) { return 0.f; }
	virtual float hmap(float, float) { return 0.f; }          // get_hmap: głębokość tylnej ściany (skala skryptu)
	virtual bool underwater(const float[3]) { return false; }
	virtual bool findPlace(const float c[3], float /*promień*/, float out[3])
	{ // wolne miejsce do stania
		out[0] = c[0], out[1] = c[1], out[2] = c[2];
		return true;
	}
	virtual int gnomePosition(int) { return 0; } // 0 stoi, 1 wisi na ścianie
	// Płynny lot/przesunięcie obiektu do punktu w podanym czasie (action fly, move).
	virtual void glide(int id, const float to[3], float /*sekundy*/) { setPos(id, to); }
	virtual void link(int /*dziecko*/, int /*rodzic, 0 = odepnij*/, int /*punkt*/) {}
	virtual bool linkOffset(int, int, float[3]) { return false; } // get_linkpos
};

struct ScriptObject
{
	int id = 0;
	ScriptClass* cls = nullptr;
	std::string name;
	Tcl_Interp* interp = nullptr;
	std::string state;
	int stateCounter = 1;
	std::deque<Event> queue;
	Event current;     // zdarzenie w obsłudze (event_get)
	bool alive = true;
	std::vector<std::string> tasks; // tasklist_*
	std::map<std::string, double> attribs;
	int owner = -1;           // gracz 0..7, −1 = świat
	int walkResult = 4;       // get_walkresult: 3 brak przejścia, 4 cel osiągnięty
	bool walking = false;     // trwa akcja walk (koniec zgłasza gra)
	double actionUntil = -1;  // akcja wait/anim: koniec w czasie gry
	std::string actionDone;   // kod do wykonania po akcji
};

class Engine
{
public:
	explicit Engine(const Vfs& vfs);
	~Engine();
	void setHost(Host* h) { host_ = h; }

	// Skrypty startowe gry (atrybuty, shader, txt, animacje, klasy, …) bez menu startowego.
	bool boot(std::string& err);
	bool loadedFromCache() const { return fromCache_; }
	// Wykonanie skryptu w głównym interpreterze.
	bool eval(const std::string& script, std::string* result = nullptr);
	bool callFile(Tcl_Interp* interp, const std::string& path);

	const ScriptClass* findClass(const std::string& name) const;
	size_t classCount() const { return classes_.size(); }
	// Obiekt świata już istniejący w grze (np. z szablonu): rejestracja i obj_init.
	ScriptObject* attach(int id, const std::string& cls, const std::string& name, bool runInit = true);
	friend struct ScriptHostAccess;
	ScriptObject* object(int id);
	void setVar(ScriptObject& o, const std::string& name, const std::string& value);
	double attrib(int id, const std::string& name); // get_attrib z wartością domyślną z def_attrib
	// Zapis stanu gry: zmienne globalne obiektu różne od startowych (skrypt Tcl: set/array set …).
	std::string captureVars(ScriptObject& o);
	// Usunięcie wszystkich obiektów, timerów i rezerwacji (klasy zostają); czas gry od zera.
	void resetWorld();
	void setTime(double t) { time_ = t; }
	std::map<std::string, std::string> mainValues_; // wartości zmiennych głównego interpretera po starcie
	void detach(int id);

	// Język tekstów gry (komenda locale, lmsg). Domyślnie język pierwszego hasła MESSAGES.TXT (język
	// wydania, w polskiej wersji "pl"). Ustawić przed boot(): wyniki lmsg trafiają do pamięci podręcznej klas.
	void setLocale(const std::string& l) { locale_ = l; }
	const std::string& locale() const { return locale_; }
	const Messages& messages() const { return messages_; }
	std::string lmsg(const std::string& key); // tekst w bieżącym języku albo klucz (brak zapisywany w dzienniku)

	double time() const { return time_; }
	void tick(double dt); // tura logiki (10 Hz): timery, zdarzenia, stany, akcje

	// Statystyka zaślepek (nazwa -> liczba wywołań) i błędów skryptów.
	const std::map<std::string, int>& stubCalls() const { return stubs_; }
	const std::vector<std::string>& errors() const { return errors_; }
	void writeLog(const std::string& path) const;

	// --- użytek wewnętrzny (komendy natywne) ---
	struct Ctx;
	ScriptObject* currentObject(Tcl_Interp* in);
	int resolveObj(Tcl_Interp* in, const std::string& word); // "this" albo liczba; 0 = brak
	ScriptClass* classContext(Tcl_Interp* in);
	void registerCommands(Tcl_Interp* in);
	Tcl_Interp* newInterp();
	std::string preprocess(const std::string& text) const;
	std::string resolvePath(const std::string& p) const;
	void error(const std::string& where, Tcl_Interp* in);
	void stub(const std::string& name) { stubs_[name]++; }
	bool runIn(ScriptObject& o, const std::string& code, const char* what);
	void postEvent(int target, Event e);
	void defineClass(Tcl_Interp* caller, const std::vector<std::string>& args);
	ScriptObject* createScripted(int id, ScriptClass* cls, const std::string& name, bool runInit);
	void setState(ScriptObject& o, const std::string& st, bool fresh);

	const Vfs& vfs_;
	Host* host_ = nullptr;
	Tcl_Interp* main_ = nullptr;
	std::map<std::string, std::unique_ptr<ScriptClass>> classes_; // klucz małymi literami
	std::unordered_map<int, std::unique_ptr<ScriptObject>> objects_;
	std::unordered_map<Tcl_Interp*, ScriptObject*> byInterp_;
	std::unordered_map<Tcl_Interp*, ScriptClass*> classInterp_;
	std::vector<Timer> timers_;
	std::map<std::string, int> stubs_;
	std::vector<std::string> errors_;
	std::string cwd_; // katalog bieżący względem katalogu gry (cd/pwd/glob), na starcie katalog gry
	std::string mainGlobals_; // migawka zmiennych głównego interpretera dla interpreterów obiektów
	int mainGlobalCount_ = -1;
	struct AttribDef
	{
		double min = 0, max = 1e9, def = 0;
	};
	std::map<std::string, AttribDef> attribDefs_; // def_attrib nazwa min max domyślna
	std::map<int, std::map<std::string, double>> ownerAttribs_; // get/set_owner_attrib
	std::map<int, std::set<std::string>> sparetimeOffers_;        // sparetime obj announce eat|sleep|… (miejsca czasu wolnego)
	std::map<int, std::pair<std::array<float, 3>, float>> placeLocks_; // placelock: id -> (pozycja, koniec rezerwacji)
	void finishAction(ScriptObject& o, bool ok);
	bool placeFree(Tcl_Interp* in, std::map<std::string, std::string>& opts, const float p[3]);
	// Nazwa logiczna z set_class_anim (np. standloopa) -> obiekt.animacja; pełne nazwy bez zmian.
	std::string resolveAnim(const ScriptObject* o, const std::string& anim) const;
	std::string cacheDir_; // pliki zapisywane przez skrypty (np. gen_tt.tcl): ~/.cache/openwiggles, nie katalog gry
	std::string cachePath(const std::string& gamePath) const;
	// Pamięć podręczna klas (~/.cache/openwiggles/classes.cache): start bez ponownego wykonywania skryptów.
	std::string scriptsSignature() const;
	bool loadClassCache(const std::string& sig);
	void writeClassCache(const std::string& sig);
	std::string mainProcs_;
	bool fromCache_ = false;
	std::set<std::string> nativeNames_;
	double time_ = 0;
	Messages messages_;
	std::string locale_ = "en";
	std::map<std::string, int> msgMisses_; // lmsg bez hasła (gra dopisuje je do Data/msgmisses.txt)
	int nextId_ = 100000; // obiekty tworzone przez skrypty bez gry
};

} // namespace script
} // namespace ow
