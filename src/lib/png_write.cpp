#include "lib/png_write.h"

#include <fstream>

namespace ow {

namespace {

uint32_t crc32(const uint8_t* p, size_t n, uint32_t crc = 0) {
	static uint32_t table[256];
	static bool init = false;
	if (!init) {
		for (uint32_t i = 0; i < 256; ++i) {
			uint32_t c = i;
			for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
			table[i] = c;
		}
		init = true;
	}
	crc = ~crc;
	for (size_t i = 0; i < n; ++i) crc = table[(crc ^ p[i]) & 0xff] ^ (crc >> 8);
	return ~crc;
}

void be32(std::vector<uint8_t>& v, uint32_t x) {
	v.push_back(uint8_t(x >> 24));
	v.push_back(uint8_t(x >> 16));
	v.push_back(uint8_t(x >> 8));
	v.push_back(uint8_t(x));
}

void chunk(std::ofstream& f, const char* type, const std::vector<uint8_t>& data) {
	std::vector<uint8_t> buf;
	be32(buf, uint32_t(data.size()));
	buf.insert(buf.end(), type, type + 4);
	buf.insert(buf.end(), data.begin(), data.end());
	be32(buf, crc32(buf.data() + 4, buf.size() - 4));
	f.write(reinterpret_cast<const char*>(buf.data()), std::streamsize(buf.size()));
}

} // namespace

bool writePng(const std::string& path, int w, int h, const std::vector<uint8_t>& rgba) {
	std::ofstream f(path, std::ios::binary);
	if (!f) return false;
	static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
	f.write(reinterpret_cast<const char*>(sig), 8);

	std::vector<uint8_t> ihdr;
	be32(ihdr, uint32_t(w));
	be32(ihdr, uint32_t(h));
	ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0}); // 8 bit, RGBA
	chunk(f, "IHDR", ihdr);

	// Surowe dane: każdy wiersz poprzedzony filtrem 0.
	std::vector<uint8_t> raw;
	raw.reserve(size_t(h) * (w * 4 + 1));
	for (int y = 0; y < h; ++y) {
		raw.push_back(0);
		raw.insert(raw.end(), rgba.begin() + size_t(y) * w * 4, rgba.begin() + size_t(y + 1) * w * 4);
	}
	// zlib: nagłówek, bloki "stored" po max 65535 B, adler32.
	std::vector<uint8_t> z = {0x78, 0x01};
	size_t pos = 0;
	do {
		size_t n = std::min<size_t>(65535, raw.size() - pos);
		bool last = pos + n == raw.size();
		z.push_back(last ? 1 : 0);
		z.push_back(uint8_t(n));
		z.push_back(uint8_t(n >> 8));
		z.push_back(uint8_t(~n));
		z.push_back(uint8_t(~n >> 8));
		z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
		pos += n;
	} while (pos < raw.size());
	uint32_t a = 1, b = 0;
	for (uint8_t c : raw) {
		a = (a + c) % 65521;
		b = (b + a) % 65521;
	}
	be32(z, (b << 16) | a);
	chunk(f, "IDAT", z);
	chunk(f, "IEND", {});
	return bool(f);
}

} // namespace ow
