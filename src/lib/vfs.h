// Wirtualny system plików nad katalogiem gry.
//
// Gra pochodzi z Windows, a nazwy plików mają mieszaną wielkość liter
// (np. "3db/BABY.3DB" i "3db/baby.3db" to ten sam plik dla gry), dlatego
// wyszukiwanie ignoruje wielkość liter. Separatorami mogą być '/' lub '\'.
// Kolejne korzenie (addRoot) mają wyższy priorytet: nakładki (mody, tekstury HD) przykrywają pliki gry.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ow
{

class Vfs
{
public:
	// Dodaje katalog (np. ".../Wiggles"). Ścieżki w grze są względne do niego, np. "Data/3db/baby.3db".
	// dataDir: katalog grający rolę "Data/" w tym korzeniu (np. "Data_de" w wariancie językowym moda).
	void addRoot(const std::filesystem::path& dir, const std::string& dataDir = "Data");
	// Nakładka w układzie modów (DigglesHDmod, DigglesModManager): katalog z "Data/" przykrywa grę, a jeśli
	// istnieje "Data_<lang>", ten przykrywa jeszcze "Data/" nakładki (np. tekstury z napisami w języku).
	// Pliki łatek change_*.tcl nie są stosowane (TODO, faza 7). false: brak "Data/" ani "Data_<lang>".
	bool addOverlay(const std::filesystem::path& dir, const std::string& lang);
	bool empty() const { return roots_.empty(); }

	// Rzeczywista ścieżka pliku lub katalogu, jeśli istnieje.
	std::optional<std::filesystem::path> resolve(const std::string& gamePath) const;
	bool exists(const std::string& gamePath) const { return resolve(gamePath).has_value(); }

	std::optional<std::vector<uint8_t>> read(const std::string& gamePath) const;

	// Nazwy plików w katalogu (bez duplikatów między korzeniami), posortowane bez rozróżniania wielkości liter.
	std::vector<std::string> list(const std::string& gameDir) const;

	static std::string lower(std::string s);

private:
	// Indeks katalogu: nazwa małymi literami -> prawdziwa nazwa.
	const std::unordered_map<std::string, std::string>& dirIndex(const std::filesystem::path& dir) const;
	struct Root
	{
		std::filesystem::path dir;
		std::string dataDir; // zamiennik pierwszego członu "Data" ścieżki gry
	};
	std::optional<std::filesystem::path> resolveIn(const Root& root, const std::string& gamePath) const;

	std::vector<Root> roots_;
	mutable std::unordered_map<std::string, std::unordered_map<std::string, std::string>> cache_;
};

} // namespace ow
