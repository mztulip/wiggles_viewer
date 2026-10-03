// Wirtualny system plików nad katalogiem gry.
//
// Gra pochodzi z Windows, a nazwy plików mają mieszaną wielkość liter
// (np. "3db/BABY.3DB" i "3db/baby.3db" to ten sam plik dla gry), dlatego
// wyszukiwanie ignoruje wielkość liter. Separatorami mogą być '/' lub '\'.
// Kolejne korzenie (addRoot) mają wyższy priorytet. Na tym oprzemy później
// Data_HD i mody.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ow {

class Vfs {
public:
	// Dodaje katalog (np. ".../Wiggles"). Ścieżki w grze są względne do niego, np. "Data/3db/baby.3db".
	void addRoot(const std::filesystem::path& dir);
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
	std::optional<std::filesystem::path> resolveIn(const std::filesystem::path& root, const std::string& gamePath) const;

	std::vector<std::filesystem::path> roots_;
	mutable std::unordered_map<std::string, std::unordered_map<std::string, std::string>> cache_;
};

} // namespace ow
