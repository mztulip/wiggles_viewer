// Tekstury GL wczytywane z Vfs i wyszukiwane po nazwie materiału (findTexture).
#pragma once

#include "lib/gfx/gl.h"
#include "lib/texture_find.h"
#include "lib/tga.h"

#include <optional>
#include <string>
#include <unordered_map>

namespace ow {

class Vfs;

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
	const Vfs& vfs() const { return vfs_; }

private:
	const Entry& checker();
	const Vfs& vfs_;
	std::unordered_map<std::string, Entry> byName_, byPath_;
	Entry checker_;
};

} // namespace ow
