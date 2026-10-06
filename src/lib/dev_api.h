// Trasy serwera deweloperskiego dla świata gry: Tcl jak WebTraq oryginału (/tclshell, /objlist, /objinfo:,
// /classlist, /classinfo:, /index) i API JSON (/api/…). Wspólne dla wigview i openwiggles; to, co zna tylko
// program (pauza, przewijanie, zrzut, sterowanie postacią), przychodzi przez DevHooks. Opis: docs/SERWER_HTTP.md.
#pragma once

#include "lib/dev_server.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ow
{

class WorldSim;
class MapScene;

struct DevHooks
{
	std::string program;                         // "wigview" albo "openwiggles"
	std::function<WorldSim*()> sim;              // symulacja (skrypty, postacie); nullptr = wyłączona
	std::function<MapScene*()> scene;            // scena mapy; nullptr = brak wczytanej mapy
	std::function<bool(bool)> setSimulate;       // wigview: włączenie symulacji (true = udało się)
	std::function<bool()> paused;                // pauza mapy (animacje, symulacja)
	std::function<void(bool)> setPaused;
	std::function<void(double)> advance;         // przewinięcie świata o S sekund (jak upływ czasu w klatkach)
	std::function<bool(int&, int&, std::vector<uint8_t>&)> screenshot; // bieżąca klatka RGBA
	std::function<int()> controlled;             // prowadzona postać (indeks w WorldSim::units), −1 = brak
	std::function<void(int)> setControlled;      // przejęcie postaci (−1 = puść); program pokazuje napis
	std::function<std::string()> mapName;        // nazwa wczytanej mapy (szablon, zapis)
};

void registerDevApi(DevServer& server, DevHooks hooks);

} // namespace ow
