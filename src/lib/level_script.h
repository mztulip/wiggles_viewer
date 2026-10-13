// Przebieg wstępny skryptu poziomu: skrypt scenariusza wykonany w osobnym interpreterze Tcl, który
// zapisuje, co skrypt stawia w świecie (docs/silnik/21_start_kampanii.md).
//
// Kampania nie ma stałej listy szablonów: single_Campaign.tcl tworzy StoryMgr i woła jego metodę
// load_level Campaign, a Campaign.tcl losuje układ jaskiń w procedurze first_level (irandom). Zamiast
// czytać takie skrypty wiersz po wierszu (parseScenario), wykonujemy je naprawdę, z komendami, które
// tylko zapisują wynik:
//   map create W H            -> rozmiar świata
//   map_template PLIK X Y     -> nałożenie .pmp (kolejno, jak w grze: późniejszy szablon wygrywa)
//   map_setlayer2 X Y PLIK    -> nałożenie warstwy 2 (.l2m)
//   new / qnew KLASA ...      -> obiekt (set_owner, set_physic, set_pos, del, obj_query -class)
//   call_method OBJ M ...     -> powtórzone po starcie świata (np. FogRemover fog_remove)
//   call_method SM load_level POZIOM: tu wykonuje tylko scripts/gameplay/POZIOM.tcl (mapa);
//                                    CampaignInit.tcl (wydarzenia fabuły) wykonuje skrypt StoryMgr w grze
//   set_view, set_light_begin, set_fow_begin, set_ground_begin
// Pozostałe komendy gry nic nie robią (unknown). split_template jak w STORY.TCL przy ustawionym temppos.
// 🔶 Bez sm_draw_stone (granitowa ramka mapy, lejek) i generate_color_variation.
#pragma once

#include "lib/map_world.h"

#include <string>

namespace ow
{

class Vfs;

// gamePath: np. "Data/Gamesave/single_Campaign.tcl". seed: ziarno irandom (0 = losowe).
// Błędy skryptu trafiają do scenario.levelErrors.
Scenario runLevelScript(const Vfs& vfs, const std::string& gamePath, unsigned seed = 0);

} // namespace ow
