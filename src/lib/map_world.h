// Składanie świata gry z szablonów map (map create + MapTemplateSet).
//
// Prawo nakładania szablonu (FUN_005992b0 w Wiggles.exe), za Zimennik/Diggles-Unity (MIT,
// Runtime/World/BdbPmpWorldBuilder.cs BlitPmpIntoWorld/BlitL2mIntoWorld):
//  - pole szablonu jest PRZEZROCZYSTE, gdy w jego oknie 2×2 (pole, +x, +y, +x+y; pola poza
//    szablonem pomijane) wszystkie typy & 0x7F == 0; wtedy świat pod nim zostaje nietknięty,
//  - w przeciwnym razie nadpisywane są: ID bloku, typ pola i warstwa 2 (strefa),
//  - .l2m: wartości 0 nie nadpisują świata.
// Świat domyślny (map create): ID bloku 0 (lita skała), typ 0 (klasa gruntu jak typ 2).
#pragma once

#include "lib/map_template.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace ow
{

class Vfs;
class BlockDb;

bool isTransparentTile(const Pmp& tpl, int x, int y);
Pmp makeWorldGrid(int width, int height);
void blitTemplate(Pmp& world, L2m& worldLayer, const Pmp& tpl, const L2m* tplLayer, int ox, int oy);

// Plik szablonu wskazany w skrypcie: "gng_021.pmp" albo "data/templates/gng_021.pmp".
std::optional<std::vector<uint8_t>> readTemplateData(const Vfs& vfs, const std::string& name);

// Rozmiar szablonu w polach z jego skryptu (Data/Templates/<nazwa>.tcl albo tcl/<nazwa>.tcl, z .tcl albo bez):
// split_template … W H …, inaczej nagłówek pliku .pmp z map_template. false = nie znaleziono.
bool templateSize(const Vfs& vfs, const std::string& name, int& w, int& h);

struct ScenarioPlacement
{
	std::string script; // plik w Data/Templates, np. "urw_gng_021_a.tcl"
	int x = 0, y = 0;   // w blokach
};

// Nałożenie z przebiegu skryptu poziomu (level_script.h): map_template, map_setlayer2 albo sm_draw_stone.
struct ScenarioStamp
{
	bool layer = false; // false: .pmp (map_template), true: .l2m (map_setlayer2)
	std::string file;   // jak w skrypcie, np. "data/templates/pmp/split/splt_unq_start_tAA.pmp"
	int x = 0, y = 0;
	std::string stone;  // niepuste: sm_draw_stone z tą opcją (-border, -funnel) zamiast nałożenia pliku
};

// Malarz klasy kamienia StoryMgr (FUN_005fc9d0, sm_draw_stone), za Zimennik/Diggles-Unity (MIT,
// BdbPmpWorldBuilder.DrawStoneIntoWorld): prostokąt [x0,x1)×[y0,y1) przycięty do mapy (bez ostatniego wiersza
// i kolumny), klasa 1↔2 zamieniona; prawdopodobieństwo p = 1 minus spadek kwadratowy za środkiem pasa w
// kierunkach flag (4: x > środka, 8: x < środka, 1: y > środka, 2: y < środka), p −= (d/w)²/s', s' = siła /
// (min(w,h)·0,2); malowane tylko pola klasy 1 i 2, gdy los (0..32767) ≤ p·32768. Klasa w typie pola:
// 0 → 4 (granit), 1 → 3, 2 → 2, 3 → 5. rnd: generator 0..0x7fff.
void drawStone(Pmp& grid, int x0, int y0, int x1, int y1, unsigned flags, int cls, float strength, const std::function<int()>& rnd);
// Pasy sm_draw_stone dla opcji -border (granit wzdłuż góry, lewej i prawej krawędzi mapy W×H) i -funnel
// (lejek pod powierzchnią); gridOx/gridOy: lewy górny róg siatki w świecie.
void drawStoneOption(Pmp& grid, int gridOx, int gridOy, int mapW, int mapH, const std::string& option, const std::function<int()>& rnd);

// call_method obiektu ze skryptu poziomu, powtarzane po starcie świata (obiekt = indeks w Scenario::objects).
struct ScenarioCall
{
	int object = -1;
	std::string method;
	std::vector<std::string> args;
};

// Kamera gry: set_view x y zoom dx dy (pierwsze wywołanie w skrypcie). Prawo z exe za
// Zimennik/Diggles-Unity (MIT, Runtime/Engine/DigglesCameraController.cs): cel = (x, y, 14) bloków,
// R = zoom·16 − 10, oko = cel + R·(cos dx·sin(−dy), sin dx, cos dx·cos dy),
// góra = (sin dx·sin(−dy), −cos dx, sin dx·cos dy), poziomy pół-kąt widzenia = 0,3·proporcje.
struct GameView
{
	float x = 0, y = 0, zoom = 1.38f, dx = -0.355f, dy = 0.f;
};
// ✅ Zakres zoom w trakcie gry: set_view poza nim gra przycina (get_view po set_view 0,5 → 1,0; 3,0 i 6,0 → 2,5;
// docs/silnik/06_kamera.md §6.1). ❓ Czy przerywniki mają inny zakres.
constexpr float kGameZoomMin = 1.f, kGameZoomMax = 2.5f;

struct Scenario
{
	int width = 0, height = 0; // z map create (0 = nie podano)
	std::optional<GameView> view;
	std::vector<ScenarioPlacement> placements;
	std::vector<ScenarioStamp> stamps;      // z przebiegu skryptu poziomu (po placements)
	std::vector<ScenarioCall> calls;        // z przebiegu skryptu poziomu
	std::string storyLevel;                 // StoryMgr load_level POZIOM (np. "Campaign"), pusty = brak
	// Wywołania sm_* skryptu poziomu (sm_draw_stone, sm_set_temp, sm_mark_temparea, sm_map_set, strefy), do
	// powtórzenia w grze po CampaignInit.tcl (mapa fabuły StoryMgr).
	std::vector<std::vector<std::string>> storyCalls;
	std::vector<std::string> levelErrors;   // błędy przebiegu skryptu poziomu
	unsigned levelSeed = 0;                 // ziarno losowania przebiegu (irandom, sm_draw_stone)
	bool usesGenerator = false; // StoryMgr load_level/load_sandbox: świat proceduralny (nieobsługiwany)
	// Obiekty tworzone wprost w skrypcie: set_pos [qnew|new Klasa ...] {x y z} (np. krasnale), współrzędne świata.
	std::vector<TemplateObject> objects;
	int lightBegin = kNoSky;    // set_light_begin: wiersz świata, od którego w dół nie ma nieba
	static constexpr int kNoSky = -10000; // domyślnie: cały świat jest „pod ziemią”
	std::optional<int> groundBegin;       // set_ground_begin: wiersz gruntu panoramy (dywan łąki)
	std::optional<int> fowBegin;          // set_fow_begin: powyżej tego wiersza nieodkryty teren nie wraca
};

// Czyta skrypt scenariusza (np. "Data/Gamesave/single_Knockersdorf.tcl") i skrypty wołane przez
// call data/gamesave/... (rekurencyjnie): map create W H, call templates/X.tcl + MapTemplateSet x y.
Scenario parseScenario(const Vfs& vfs, const std::string& gamePath);

struct BuiltWorld
{
	Pmp grid;          // przycięty do użytego obszaru (+ margines)
	L2m layer;
	std::vector<TemplateObject> objects; // pozycje względem przyciętego świata
	int originX = 0, originY = 0;        // lewy górny róg przycięcia w blokach świata
	std::optional<GameView> view;        // set_view przeliczone na współrzędne przyciętego świata
	int skyTiles = 0;                    // ile pól dostało światło dzienne (reguła set_light_begin)
	std::vector<uint8_t> fluidType;      // płyn w polu (z zapisu; puste = brak)
	std::vector<float> fluidFill;
	std::vector<std::string> placedScripts;
	std::vector<std::string> errors;
};

// Światło nieba (FUN_005906a0, kanał „MapValidator:BASELIGHT”), za Zimennik/Diggles-Unity (MIT,
// Docs/TUNNEL_TORCH_LIGHTING_RE.md §6): pole w wierszu y < lightBegin dostaje bazowe światło 20
// (pełny dzień), gdy w jego kolumnie, w nim samym albo do 6 pól wyżej, jest otwarte powietrze
// (drobne pole bloku o wysokości < 13, z BlockDB; bez BlockDB tylko blok 59) albo skan wychodzi
// ponad górną krawędź świata. Pozostałe pola zachowują warstwę 2 szablonu. 🔶 Gra skanuje drobną
// siatkę 8×8 kolumna po kolumnie i liczy też ciemnienie w kątach tuneli; tu tylko gałąź nieba
// w skali pól.
constexpr int kDaylight = 20;
void applySkyLight(Pmp& grid, int originY, int lightBegin, const BlockDb* db = nullptr, int* litCount = nullptr);

// margin: ile bloków domyślnego świata zostawić wokół szablonów.
// db (opcjonalnie) dokładniej wyznacza otwarte pola dla reguły nieba.
BuiltWorld buildWorld(const Vfs& vfs, const Scenario& scenario, int margin = 2, const BlockDb* db = nullptr);

// Świat z zapisu gry (docs/formaty/sav.md): pełna mapa bez przycinania, typ pola odtworzony z rodzaju
// gruntu (0 A → 4, 1 B → 3, 2 C → 2, 3 otwór → 5, plus bit 0x80), detale .l2m, obiekty, kamera z zapisu,
// światło bazowe 4 i reguła nieba z linii nieba zapisu. scenarioOut dostaje linie nieba i gruntu.
struct SaveGame;
BuiltWorld buildWorldFromSave(const SaveGame& save, const BlockDb* db, Scenario& scenarioOut);

} // namespace ow
