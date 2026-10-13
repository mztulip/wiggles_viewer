// Wspólne pomocniki komend natywnych gry (tylko dla plików src/lib/script/*.cpp). Komendy podzielone na moduły
// według kategorii oficjalnej pomocy komend (docs/tcl_komendy_stan.md): engine.cpp (rdzeń: klasy, obiekty,
// zdarzenia, stany, akcje), cmd_object.cpp (właściwości obiektów i graczy), cmd_misc.cpp (wektory, listy, statystyki, rozmowy, rodzina), cmd_inventory.cpp
// (ekwipunek, zawieranie), cmd_ui.cpp (zaznaczenie, kamera), cmd_dig.cpp (kopanie), cmd_trigger.cpp (wyzwalacze), cmd_prod.cpp (stan produkcji), cmd_work.cpp (planista pracy) …
#pragma once

#include "lib/script/engine.h"

#include <map>
#include <string>
#include <vector>

struct Tcl_Interp;

namespace ow::script
{

using Args = std::vector<std::string>;
using CmdFn = int (*)(Engine&, Tcl_Interp*, Args&);
struct CmdEntry
{
	const char* name;
	CmdFn fn;
};

void setResult(Tcl_Interp* in, const std::string& s);
std::string fmt(double v);           // liczba jak w wynikach gry (%.6g)
std::string vecStr(const float v[3]); // "x y z"
std::vector<std::string> listItems(Tcl_Interp* in, const std::string& list);
bool parseVec(Tcl_Interp* in, const std::string& s, float v[3]);
std::map<std::string, std::string> options(const Args& a, size_t from); // -nazwa wartość
bool truthy(const std::string& s);                                      // wartość logiczna Tcl (1, true, yes, on, liczba ≠ 0)

// Moduły komend (rejestrowane w Engine::registerCommands).
const std::vector<CmdEntry>& objectPropCommands(); // cmd_object.cpp
const std::vector<CmdEntry>& miscCommands();       // cmd_misc.cpp
const std::vector<CmdEntry>& inventoryCommands();  // cmd_inventory.cpp
const std::vector<CmdEntry>& uiCommands();         // cmd_ui.cpp
const std::vector<CmdEntry>& digCommands();        // cmd_dig.cpp
const std::vector<CmdEntry>& triggerCommands();    // cmd_trigger.cpp
const std::vector<CmdEntry>& prodCommands();       // cmd_prod.cpp
const std::vector<CmdEntry>& workCommands();       // cmd_work.cpp

} // namespace ow::script
