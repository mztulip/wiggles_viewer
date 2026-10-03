#include "lib/gfx/texture_cache.h"

#include "lib/vfs.h"

namespace ow {

GLuint uploadTexture(const Image& img, bool mipmaps) {
	GLuint id = 0;
	gl::GenTextures(1, &id);
	gl::BindTexture(GL_TEXTURE_2D, id);
	gl::PixelStorei(GL_UNPACK_ALIGNMENT, 1);
	gl::TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, img.width, img.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.rgba.data());
	gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
	if (mipmaps) gl::GenerateMipmap(GL_TEXTURE_2D);
	return id;
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
		checker_.id = uploadTexture(img);
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
			e.id = uploadTexture(img);
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
	for (auto& [k, e] : byPath_)
		if (e.id) gl::DeleteTextures(1, &e.id);
	byPath_.clear();
	byName_.clear();
	if (checker_.id) gl::DeleteTextures(1, &checker_.id);
	checker_ = {};
}

} // namespace ow
