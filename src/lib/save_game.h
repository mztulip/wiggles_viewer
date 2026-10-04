// Zapis gry .sav (Data/Gamesave/*.sav): odczyt części potrzebnych do pokazania mapy.
//
// Format opisuje docs/formaty/sav.md. Wiedza o układzie pochodzi z Zimennik/Diggles-Unity (MIT,
// https://github.com/Zimennik/Diggles-Unity: Docs/ORIGINAL_SAVE_FORMAT_RE.md,
// Docs/save-re-reports/B4-fixed-engine-state.md, Runtime/Save/OriginalSaveObjectDocument.cs);
// kod jest nasz. ✅ Sprawdzone na zapisach polskiej wersji 1.0 build 844 (wersja formatu 0xA5).
//
// Czytamy: nagłówek, kamerę, teren (blok BlockDB, rodzaj gruntu, detal .l2m, mgła wojny),
// linie nieba i gruntu, listę obiektów z klasą, typem, pozycją i obrotem. Reszta zapisu
// (logika, akcje, fabuła) jest pomijana.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ow {

struct SaveObject {
	int id = 0;
	std::string className;
	int type = 0;            // typ obiektu silnika (−1..12; 0 = dekoracja, 1 = postać, 5 = budynek…)
	float pos[3] = {0, 0, 0}; // pozycja w świecie: x, y w blokach, z = głębokość świata (7..14,5 = teren)
	float rot[3] = {0, 0, 0}; // obrót w radianach
	int owner = -1;           // gracz 0..7, −1 = świat
	int linkParent = -1;      // obiekt, do którego jest przypięty (link_obj), −1 = brak
	int linkPoint = 0;        // punkt zaczepienia rodzica (0..31)
	std::vector<std::pair<std::string, std::string>> tclVars; // zmienne Tcl obiektu (nazwa, wartość)
};

// Dekoracja spoza listy obiektów: lekki obiekt zastępczy („tabela klas”) albo „nasiono” dekoracji
// w kubełku przestrzennym (docs/formaty/sav.md, sekcje po ciałach obiektów).
struct SaveDeco {
	std::string className;
	float pos[3] = {0, 0, 0}; // jak SaveObject::pos (z w skali skryptu)
	float rot[3] = {0, 0, 0};
	int link = -1;            // obiekt zastępczy: ID obiektu, którego jest kopią (−1 = brak)
	bool proxy = false;       // true = tabela klas, false = kubełek przestrzenny
};

struct SaveTile {
	uint32_t block = 0;   // ID kształtu w BlockDB
	uint8_t material = 0; // rodzaj gruntu: 0 A skała, 1 B ziemia, 2 C piasek, 3 otwór do tła
	uint8_t flags = 0;    // młodszy bajt stanu terenu (bit 0x80 jak w typie pola .pmp)
	uint8_t variant = 0;  // starszy bajt stanu terenu (🔶 wariant tekstury)
	uint16_t cell = 0;    // wartość detali .l2m
	uint8_t fluidType = 0; // płyn: 0 brak, 1 woda, 2 siarka, 3 lawa, 4 lawa z wodą
	float fluidFill = 0;   // wypełnienie pola (> 1 = ciśnienie od słupa płynu nad nim)
	uint8_t fog[8] = {};  // bajt mgły wojny dla każdego gracza
};

struct SaveGame {
	int version = 0;
	std::string header; // np. "V1.0.844r"
	double gameTime = 0;
	// Kamera w chwili zapisu (jak set_view x y zoom dx dy).
	float viewX = 0, viewY = 0, viewZoom = 1.38f, viewDx = 0, viewDy = 0;
	int width = 0, height = 0;
	std::vector<SaveTile> tiles; // width·height, wiersz po wierszu
	int fowBegin[8] = {};
	int lightBegin = -10000, groundBegin = -10000;
	std::vector<SaveObject> objects;
	std::vector<SaveDeco> decos;
	bool decoError = false; // sekcje dekoracji nie dały się odczytać (reszta zapisu jest poprawna)
};

// Rozpakowanie gzip (zapisy są strumieniem gzip).
std::optional<std::vector<uint8_t>> gunzip(const std::vector<uint8_t>& data, std::string& err);
// Odczyt zapisu z danych pliku (spakowanych). Rzuca ParseError przy błędnej strukturze.
SaveGame parseSaveGame(const std::vector<uint8_t>& fileData);

} // namespace ow
