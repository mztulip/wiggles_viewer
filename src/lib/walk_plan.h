// Trasa chodu jak w grze: z drogi po polach (NavWorld::findPath, Calc1) punkty, którymi postać idzie, razem
// z głębokością. Prawa za Zimennik/Diggles-Unity (MIT): Runtime/Navigation/NavWorldService.cs (FindPath)
// i FloorPathMap.cs (odczyt działającej gry: FUN_004163a0 podział na odcinki, FUN_0041aac0/0041d8a0
// głębokość na podłodze, FUN_004238e0 głębokość podparcia na ścianie). Opis: docs/silnik/17_ruch.md §17.5.
//
// Układ jak NavWorld: x, y w blokach (y w dół, stopy postaci na linii podłogi), z w skali skryptu (głębokość
// 0..15,75, z świata = z · 0,5 + 7; podpole o wysokości h ma skałę przy z < h i wolne przy z ≥ h).
#pragma once

#include <string>
#include <vector>

namespace ow
{

class NavWorld;

struct WalkPoint
{
	float x = 0, y = 0, z = 0;
	bool wall = false; // punkt na ścianie (wspinanie), inaczej na podłodze
};

// Trasa od start do goal (x, y, z). Pierwszy punkt to start bez zmian; dalej odcinki podłogi z głębokością
// z FloorPathMap i odcinki ściany na głębokości podparcia. Pusta: brak drogi (why: powód, do śladu).
// keepGoal: cel na samej ścianie zostaje (bez zsuwania na podłogę niżej), jak action walk -dig 1.
std::vector<WalkPoint> planWalk(const NavWorld& nav, const float start[3], const float goal[3], bool canClimb,
    bool keepGoal = false, std::string* why = nullptr);

// Głębokość podparcia podpola (FUN_004238e0): wysokość wiersz wyżej; 15 (lita skała) → najmniejsza z czterech
// sąsiednich (sx ± 1 w wierszu wyżej, sy − 2 i sy).
int supportDepth(const NavWorld& nav, int sx, int sy);
// Głębokość stania na linii podłogi: min(podparcie + 1, 14,5).
float floorLineDepth(const NavWorld& nav, int sx, int sy);
// Dozwolone głębokości na podłodze w podpolu (pas FloorPathMap: wiersze [h(sy − 1) · 4 + 1, h(sy + 1) · 4 − 1)
// po 0,25): false, gdy pas pusty.
bool floorBand(const NavWorld& nav, int sx, int sy, float& z0, float& z1);
// Podłoga pod punktem (x, y): bit podłogi w podpolu stóp, wiersz niżej albo wyżej (get_gnomeposition).
bool hasFloorSupport(const NavWorld& nav, float x, float y);
// Najmniejsza głębokość wolna od skały w punkcie (min(h(sx, sy), h(sx, sy − 1)), AirClamp).
float airDepth(const NavWorld& nav, float x, float y);

} // namespace ow
