#include "lib/gfx/texture_cache.h"

#include "lib/vfs.h"

#include <vector>

namespace ow {

namespace {

// Przezroczyste teksele (alfa < 128, np. tło liści) mają w plikach dowolny kolor, często szaroniebieski.
// Filtrowanie i mipmapy mieszają go z sąsiednimi nieprzezroczystymi, co daje ciemne, szare obwódki.
// Rozlewamy więc kolor sąsiednich nieprzezroczystych tekseli w przezroczyste (alfa bez zmian).
std::vector<uint8_t> bleedTransparent(const Image& img) {
	std::vector<uint8_t> px = img.rgba;
	const int W = img.width, H = img.height;
	std::vector<uint8_t> solid(size_t(W) * H);
	bool any = false;
	for (size_t i = 0; i < solid.size(); ++i) {
		solid[i] = px[i * 4 + 3] >= 128;
		any |= !solid[i];
	}
	if (!any) return px;
	std::vector<uint8_t> next = solid;
	for (int pass = 0; pass < 16; ++pass) {
		bool changed = false;
		for (int y = 0; y < H; ++y)
			for (int x = 0; x < W; ++x) {
				size_t i = size_t(y) * W + x;
				if (solid[i]) continue;
				int r = 0, g = 0, b = 0, n = 0;
				for (int dy = -1; dy <= 1; ++dy)
					for (int dx = -1; dx <= 1; ++dx) {
						int xx = (x + dx + W) % W, yy = (y + dy + H) % H; // tekstury się powtarzają
						size_t j = size_t(yy) * W + xx;
						if (!solid[j]) continue;
						r += px[j * 4], g += px[j * 4 + 1], b += px[j * 4 + 2], ++n;
					}
				if (!n) continue;
				px[i * 4] = uint8_t(r / n), px[i * 4 + 1] = uint8_t(g / n), px[i * 4 + 2] = uint8_t(b / n);
				next[i] = 1;
				changed = true;
			}
		solid = next;
		if (!changed) break;
	}
	return px;
}

} // namespace

rhi::Handle TextureCache::upload(const Image& img, const rhi::TextureDesc& desc) {
	if (!device_) return rhi::kNull;
	std::vector<uint8_t> px = bleedTransparent(img);
	rhi::TextureDesc d = desc;
	d.width = img.width;
	d.height = img.height;
	return device_->createTexture(d, px.data());
}

const TextureCache::Entry& TextureCache::checker() {
	if (!checker_.id) {
		Image img;
		img.width = img.height = 64;
		img.rgba.resize(64 * 64 * 4);
		for (int y = 0; y < 64; ++y)
			for (int x = 0; x < 64; ++x) {
				uint8_t* p = &img.rgba[(y * 64 + x) * 4];
				bool on = ((x / 8) + (y / 8)) & 1;
				p[0] = on ? 255 : 40;
				p[1] = 0;
				p[2] = on ? 255 : 40;
				p[3] = 255;
			}
		checker_.id = upload(img, rhi::TextureDesc{});
		checker_.width = checker_.height = 64;
	}
	return checker_;
}

const TextureCache::Entry& TextureCache::getPath(const std::string& gamePath) {
	std::string key = Vfs::lower(gamePath);
	auto it = byPath_.find(key);
	if (it != byPath_.end()) return it->second.id ? it->second : checker();
	Entry& e = byPath_[key];
	if (auto data = vfs_.read(gamePath)) {
		Image img;
		if (loadTga(*data, img)) {
			e.id = upload(img, rhi::TextureDesc{});
			e.width = img.width;
			e.height = img.height;
			e.hasAlpha = img.hasAlpha;
			e.path = gamePath;
		}
	}
	return e.id ? e : checker();
}

const TextureCache::Entry& TextureCache::get(const std::string& name) {
	std::string key = Vfs::lower(name);
	auto it = byName_.find(key);
	if (it != byName_.end()) return it->second;
	auto path = findTexture(vfs_, name);
	Entry e = path ? getPath(*path) : checker();
	return byName_[key] = e;
}

void TextureCache::clear() {
	if (device_) {
		for (auto& [k, e] : byPath_)
			if (e.id) device_->destroyTexture(e.id);
		if (checker_.id) device_->destroyTexture(checker_.id);
	}
	byPath_.clear();
	byName_.clear();
	checker_ = {};
}

} // namespace ow
