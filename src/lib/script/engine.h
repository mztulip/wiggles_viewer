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
struct ParticleSource; // lib/particles.h

class Vfs;

namespace script
{

// Tekst skryptu gry (bajty CP1252) -> UTF-8 dla Tcl 8.6.
std::string toUtf8(const std::string& s);

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
	// Opcje action walk: -speedtype (0 marsz, po 2 s bieg; 1 sam marsz; 2 od razu bieg), -animsets {chód bieg}
	// (bieg −1: bez biegu), -dig 1 (cel na ścianie zostaje).
	struct WalkOpts
	{
		int speedtype = 0;
		bool canRun = true;
		bool dig = false;
	};
	// Akcja chodzenia: true = przyjęta; done(true) po dojściu, done(false) gdy nie ma drogi.
	virtual bool walkTo(int, const float[3], const WalkOpts&, std::function<void(bool)>) { return false; }
	virtual float getRotY(int) { return 0.f; }
	virtual float hmap(float, float) { return 0.f; }          // get_hmap: głębokość tylnej ściany (skala skryptu)
	virtual bool underwater(const float[3]) { return false; }
	virtual bool findPlace(const float c[3], float /*promień*/, float out[3])
	{ // wolne miejsce do stania
		out[0] = c[0], out[1] = c[1], out[2] = c[2];
		return true;
	}
	virtual int gnomePosition(int) { return 0; } // 0 stoi, 1 wisi na ścianie
	// get_digedge: pole, z którego zacząć kopanie oznaczonej skały w start (out: x, y pola); false = brak.
	virtual bool digEdge(const float /*start*/[3], int /*id kopiącego*/, float /*out*/[3]) { return false; }
	// vector_fixdig: miejsce do stania przy kopanym punkcie wall, od strony postaci w gnome.
	virtual bool fixDig(const float /*wall*/[3], const float /*gnome*/[3], float /*out*/[3]) { return false; }
	// Płynny lot/przesunięcie obiektu do punktu w podanym czasie (action fly, move).
	// face: obiekt zwrócony w stronę ruchu w bok (action fly); action move nie obraca.
	virtual void glide(int id, const float to[3], float /*sekundy*/, bool /*face*/ = true) { setPos(id, to); }
	virtual void link(int /*dziecko*/, int /*rodzic, 0 = odepnij*/, int /*punkt*/) {}
	virtual bool linkOffset(int, int, float[3]) { return false; } // get_linkpos
	virtual bool linkEuler(int, int, float[3]) { return false; }  // get_linkrot
	// Źródła cząsteczek obiektu (change_particlesource / set_particlesource; docs/silnik/12_czasteczki.md §12.1).
	virtual void changeParticleSource(int /*id*/, int /*slot*/, const ParticleSource&) {}
	virtual void setParticleSource(int /*id*/, int /*slot*/, bool /*włączone*/) {}
	virtual void setVisible(int /*id*/, bool) {} // set_visibility: obiekt rysowany albo ukryty (przedmioty w rękach, półki)
	// Kamera gry (get_view / set_view): x y zoom dx dy. getView false = brak kamery (wartości bez zmian).
	// Kopanie (cmd_dig.cpp, lib/dig_terrain): materiał pola, znaczniki, kolejny węzeł, krok wycinania.
	virtual int material(const float /*poz*/[3]) { return 1; }
	virtual void digMark(int /*gracz*/, int /*x*/, int /*y*/, int /*tryb 0 jaskinia, 1 tunel, 2 zdjęcie*/, int /*głębokość*/) {}
	virtual bool isDigMarked(int /*x1*/, int /*y1*/, int /*x2*/, int /*y2*/) { return false; }
	virtual bool digNext(const float /*poz*/[3], int /*gracz*/, int /*id kopiącego*/, int /*próba*/, bool /*tylko id*/, float /*out*/[3]) { return false; }
	virtual bool digApply(int /*nx*/, int /*ny*/, int /*gracz*/, int& /*id znacznika*/) { return false; }
	virtual void incDigMarkId(int /*gracz*/) {}
	// Planista pracy (cmd_work.cpp, lib/prod_planner): gnome_idle, gnome_failed_work/gnome_stopped_work, gnome_announce_dig.
	virtual void gnomeIdle(int /*id*/, bool /*bezczynny*/) {}
	virtual void gnomeFailedWork(int /*id*/) {}
	virtual void announceDig(int /*x*/, int /*y*/) {}
	virtual bool getView(float /*v*/[5]) { return false; }
	virtual void setView(const float /*v*/[5], bool /*move: płynny przejazd*/) {}
};

// Wyzwalacz obiektu (trigger create …, cmd_trigger.cpp). Wartości domyślne jak w grze (FUN_0060da90).
struct TriggerRec
{
	int kind = 0;               // 0 martwy, 1 single_timer, 2 repeat_timer, 3 any_object, 4 unique_object, 5 callback
	int exec = 2;               // 1 never, 2 on_activate, 3 on_inactivate, 4 when_active, 5 when_inavtive, 6 always
	int owner = 0;              // obiekt właściciela
	std::string fire, callback; // skrypt wyzwalacza, warunek (callback)
	double lastCheck = 0, checkTimer = 3, checkIdle = 3, checkNear = 0.5;
	bool active = false;
	float range = 5.f;                // zasięg (any_object, unique_object)
	double fireTime = 0, interval = 1; // zegar
	int target = -1;                  // set_target_ref
	std::string cls;                  // set_target_class (małymi literami)
	std::vector<std::string> types;   // set_target_type
	std::string ownerSpec;            // set_target_owner
	bool hasOwnerSpec = false;
	int count = 0;                    // set_target_count
	std::vector<int> actuators;       // obiekty, które wyzwoliły
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
	// Okno pracy w godzinach doby (0..12): praca w [workStart, workStart + workLen), poza nim czas wolny.
	// Domyślnie 0 h (zawsze czas wolny); krasnal przy starcie: auto_choose_workingtime (zwerg.tcl).
	float workStart = 0, workLen = 0;
	bool visible = true; // set_visibility / get_visibility
	// Sterowany z zewnątrz (przeglądarka: gracz prowadzi postać strzałkami): bez akcji i ciała stanu;
	// zdarzenia (timery potrzeb) działają dalej.
	bool external = false;
	std::string gender = "neutral";            // set_objgender / auto_choose_gender
	std::map<std::string, std::string> props; // flagi i wartości set_X/get_X bez skutków w scenie (cmd_object.cpp)
	std::map<std::string, double> talkIssues; // talkissue: temat rozmowy → waga 0..50
	std::vector<int> inventory; // inv_add / inv_rem: odwołania przedmiotów w kolejności (cmd_inventory.cpp)
	int containedIn = -1;       // właściciel ekwipunku, w którym obiekt leży (−1 = w świecie)
	int digSetId = -1;          // id znacznika ostatnio kopanej żyły (dig_apply → dig_next)
	std::map<std::string, double> prodSlots; // set_prod_slot_cnt: zamówienia budynku (przedmiot → liczba, 10 = bez końca)
	std::array<int, 16> guestLinks = initLinks(), guests{}; // prod_guest: punkty miejsc (−1 brak) i goście
	std::array<double, 16> guestOrders{};
	static std::array<int, 16> initLinks()
	{
		std::array<int, 16> l{};
		l.fill(-1);
		return l;
	}
};

// ✅ Godzina gry trwa 150 s czasu gry (gethours = gettime / 150), doba 12 h (rozdział 11.4).
constexpr double kHourSec = 150.0, kDayHours = 12.0;

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
	static std::string preprocess(const std::string& text); // //#IF…, komentarze //
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
	double timeFactor_ = 1;    // gametime factor: tempo upływu czasu gry
	bool clockRunning_ = true; // gametime start/stop
	Messages messages_;
	std::string locale_ = "en";
	std::map<std::string, int> msgMisses_; // lmsg bez hasła (gra dopisuje je do Data/msgmisses.txt)
	int nextId_ = 100000; // obiekty tworzone przez skrypty bez gry
	bool storyMgr = false; // is_storymgr: kampania (gra ustawia przy wczytaniu świata)
	// Fabuła (cmd_story.cpp): wydarzenia sm_add_event (kolejność rejestracji, wyzwalacz i słowo komunikatu),
	// wartości sm_set_event, wywołanie zwrotne StoryMgr z sm_register.
	struct StoryEvent
	{
		std::string name, trigger, keyword;
	};
	std::vector<StoryEvent> storyEvents_;
	std::map<std::string, std::string> storyValues_;
	int storyCallbackObj_ = 0;
	std::string storyCallbackProc_;
	std::vector<TriggerRec> triggers_; // trigger: rekordy (miejsce = numer zwracany przez create)
	bool sequenceActive = false;       // set_sequence: przerywnik (wstrzymuje wyzwalacze)
	void scanTriggers();               // co turę logiki (cmd_trigger.cpp)
	// Czy właściciel owner pasuje do opisu słowami (any, own, other, world, player, enemy, friend, ally, neutral,
	// allplayers, numer) widzianego przez gracza selfOwner (obj_query -owner, trigger set_target_owner).
	bool ownerMatches(int owner, const std::string& spec, int selfOwner, Tcl_Interp* in);
	// Zaznaczenie gracza (cmd_ui.cpp): lista uchwytów, pierwszy = get_selectedobject. Prawa jak w grze:
	// canSelect (obiekt gracza 0, wybieralny; do niepustego zaznaczenia tylko krasnale i dzieci),
	// clickSelect (kliknięcie, additive = Shift), selectOnly (programowo, bez praw).
	std::vector<int> selection_;
	bool canSelect(int id);
	bool selectInclude(int id);
	void selectOnly(int id);
	void clickSelect(int id, bool additive);
	int viewBegin = 0; // set_view_begin
	std::map<std::string, std::vector<std::string>> smalltalk_; // smalltalk add/get: teksty pogawędek wg tematu
	std::array<int, 8> ownerRace_{};                            // set_ownerrace / get_ownerrace
	std::array<std::array<int, 8>, 8> diplomacy_ = initDiplomacy(); // kody own 0, ally 1, friend 2, neutral 3, enemy 4
	static std::array<std::array<int, 8>, 8> initDiplomacy()
	{
		std::array<std::array<int, 8>, 8> t{};
		for (size_t i = 0; i < 8; ++i)
			for (size_t j = 0; j < 8; ++j) t[i][j] = i == j ? 0 : 3;
		return t;
	}
};

} // namespace script
} // namespace ow
