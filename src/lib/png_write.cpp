#include "lib/png_write.h"

#include <fstream>
#include <zlib.h>

namespace ow {

namespace {

void be32(std::vector<uint8_t>& v, uint32_t x) {
	v.push_back(uint8_t(x >> 24));
	v.push_back(uint8_t(x >> 16));
	v.push_back(uint8_t(x >> 8));
	v.push_back(uint8_t(x));
}

void chunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data) {
	be32(out, uint32_t(data.size()));
	size_t start = out.size();
	out.insert(out.end(), type, type + 4);
	out.insert(out.end(), data.begin(), data.end());
	uLong crc = crc32(0L, out.data() + start, uInt(out.size() - start));
	be32(out, uint32_t(crc));
}

} // namespace

std::vector<uint8_t> encodePng(int w, int h, const std::vector<uint8_t>& rgba) {
	std::vector<uint8_t> out = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};

	std::vector<uint8_t> ihdr;
	be32(ihdr, uint32_t(w));
	be32(ihdr, uint32_t(h));
	ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0}); // 8 bit, RGBA
	chunk(out, "IHDR", ihdr);

	// Każdy wiersz poprzedzony bajtem filtra 0 (brak filtra).
	std::vector<uint8_t> raw;
	raw.reserve(size_t(h) * (size_t(w) * 4 + 1));
	for (int y = 0; y < h; ++y) {
		raw.push_back(0);
		raw.insert(raw.end(), rgba.begin() + size_t(y) * w * 4, rgba.begin() + size_t(y + 1) * w * 4);
	}
	uLongf zlen = compressBound(uLong(raw.size()));
	std::vector<uint8_t> z(zlen);
	compress2(z.data(), &zlen, raw.data(), uLong(raw.size()), 6);
	z.resize(zlen);
	chunk(out, "IDAT", z);
	chunk(out, "IEND", {});
	return out;
}

bool writePng(const std::string& path, int w, int h, const std::vector<uint8_t>& rgba) {
	auto png = encodePng(w, h, rgba);
	std::ofstream f(path, std::ios::binary);
	if (!f) return false;
	f.write(reinterpret_cast<const char*>(png.data()), std::streamsize(png.size()));
	return bool(f);
}

} // namespace ow
