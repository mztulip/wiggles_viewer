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
	std::vector<std::string> placedScripts;
	std::vector<std::string> errors;
};

// margin: ile bloków domyślnego świata zostawić wokół szablonów.
BuiltWorld buildWorld(const Vfs& vfs, const Scenario& scenario, int margin = 2);

} // namespace ow
