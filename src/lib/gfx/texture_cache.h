// Tekstury GL wczytywane z Vfs i wyszukiwane po nazwie materiału.
//
// Kolejność katalogów (od największej rozdzielczości) pochodzi ze skryptu
// przeglądarki w djunkles/Diggles3db (https://github.com/djunkles/Diggles3db):
// m256, m128, m064, m032, Gray, Misc, ClassIcons.
#pragma once

#include "lib/gfx/gl.h"
#include "lib/tga.h"

#include <optional>
#include <string>
#include <unordered_map>

namespace ow {

class Vfs;

// Ścieżka w grze do tekstury o danej nazwie (bez rozszerzenia) albo nullopt.
std::optional<std::string> findTexture(const Vfs& vfs, const std::string& name);

GLuint uploadTexture(const Image& img, bool mipmaps = true);

class TextureCache {
public:
	explicit TextureCache(const Vfs& vfs) : vfs_(vfs) {}
	~TextureCache() { clear(); }

	struct Entry {
		GLuint id = 0;
		int width = 0, height = 0;
		bool hasAlpha = false;
		std::string path; // pusty, gdy nie znaleziono
	};

	// Tekstura dla nazwy materiału. Gdy jej brak, zwraca szachownicę (Entry::path pusty).
	const Entry& get(const std::string& name);
	// Tekstura dla pełnej ścieżki w grze, np. "Data/Texture/m256/foo.tga".
	const Entry& getPath(const std::string& gamePath);
	void clear();

private:
	const Entry& checker();
	const Vfs& vfs_;
	std::unordered_map<std::string, Entry> byName_, byPath_;
	Entry checker_;
};

} // namespace ow
