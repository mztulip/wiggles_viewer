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

#include <optional>
#include <string>
#include <vector>

namespace ow {

class Vfs;
class BlockDb;

bool isTransparentTile(const Pmp& tpl, int x, int y);
Pmp makeWorldGrid(int width, int height);
void blitTemplate(Pmp& world, L2m& worldLayer, const Pmp& tpl, const L2m* tplLayer, int ox, int oy);

// Plik szablonu wskazany w skrypcie: "gng_021.pmp" albo "data/templates/gng_021.pmp".
std::optional<std::vector<uint8_t>> readTemplateData(const Vfs& vfs, const std::string& name);

struct ScenarioPlacement {
	std::string script; // plik w Data/Templates, np. "urw_gng_021_a.tcl"
	int x = 0, y = 0;   // w blokach
};

// Kamera gry: set_view x y zoom dx dy (pierwsze wywołanie w skrypcie). Prawo z exe za
// Zimennik/Diggles-Unity (MIT, Runtime/Engine/DigglesCameraController.cs): cel = (x, y, 14) bloków,
// R = zoom·16 − 10, oko = cel + R·(cos dx·sin(−dy), sin dx, cos dx·cos dy),
// góra = (sin dx·sin(−dy), −cos dx, sin dx·cos dy), poziomy pół-kąt widzenia = 0,3·proporcje.
struct GameView {
	float x = 0, y = 0, zoom = 1.38f, dx = -0.355f, dy = 0.f;
};

struct Scenario {
	int width = 0, height = 0; // z map create (0 = nie podano)
	std::optional<GameView> view;
	std::vector<ScenarioPlacement> placements;
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

struct BuiltWorld {
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
