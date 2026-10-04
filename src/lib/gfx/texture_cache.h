// Tekstury wczytywane z Vfs i wyszukiwane po nazwie materiału (findTexture), na urządzeniu rhi.
#pragma once

#include "lib/rhi/rhi.h"
#include "lib/texture_find.h"
#include "lib/tga.h"

#include <optional>
#include <string>
#include <unordered_map>

namespace ow {

class Vfs;

class TextureCache {
public:
	explicit TextureCache(const Vfs& vfs) : vfs_(vfs) {}
	// Urządzenie, na którym powstają tekstury (ustawić przed pierwszym get/getPath).
	void setDevice(rhi::Device* d) { device_ = d; }
	rhi::Device* device() const { return device_; }
	~TextureCache() { clear(); }

	struct Entry {
		rhi::Handle id = rhi::kNull;
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
	// Wysyła obraz jako teksturę (z rozlaniem koloru w przezroczyste piksele); własność ma wołający.
	rhi::Handle upload(const Image& img, const rhi::TextureDesc& desc);

private:
	const Entry& checker();
	const Vfs& vfs_;
	rhi::Device* device_ = nullptr;
	std::unordered_map<std::string, Entry> byName_, byPath_;
	Entry checker_;
};

} // namespace ow
