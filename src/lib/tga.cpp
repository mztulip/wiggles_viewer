#include "lib/tga.h"
#include "lib/i18n.h"

#include "lib/binreader.h"

namespace ow
{

bool loadTga(const std::vector<uint8_t>& data, Image& out, std::string* err)
{
	auto fail = [&](const std::string& m) {
		if (err) *err = m;
		return false;
	};
	try
	{
		BinReader r(data);
		uint8_t idLen = r.u8();
		uint8_t cmapType = r.u8();
		uint8_t type = r.u8();
		r.skip(5); // specyfikacja palety
		r.skip(4); // x/y origin
		int w = r.u16();
		int h = r.u16();
		uint8_t bpp = r.u8();
		uint8_t desc = r.u8();
		r.skip(idLen);

		if (cmapType != 0) return fail(tr("TGA z paletą nieobsługiwane"));
		bool rle = type == 10 || type == 11;
		bool gray = type == 3 || type == 11;
		if (type != 2 && type != 3 && type != 10 && type != 11)
			return fail(strf(tr("nieobsługiwany typ TGA %d"), int(type)));
		if (w <= 0 || h <= 0) return fail(tr("pusty obraz"));
		int bytes = bpp / 8;
		if (gray ? (bpp != 8 && bpp != 16) : (bpp != 16 && bpp != 24 && bpp != 32))
			return fail(strf(tr("nieobsługiwana głębia %d"), int(bpp)));

		out.width = w;
		out.height = h;
		out.hasAlpha = (bpp == 32) || (gray && bpp == 16) || (bpp == 16 && (desc & 0x0f));
		out.rgba.assign(size_t(w) * h * 4, 255);

		auto decode = [&](const uint8_t* p, uint8_t* d) {
			if (gray)
			{
				d[0] = d[1] = d[2] = p[0];
				d[3] = bpp == 16 ? p[1] : 255;
			}
			else if (bpp == 16)
			{ // ARRRRRGG GGGBBBBB
				uint16_t v = uint16_t(p[0] | (p[1] << 8));
				d[0] = uint8_t(((v >> 10) & 31) * 255 / 31);
				d[1] = uint8_t(((v >> 5) & 31) * 255 / 31);
				d[2] = uint8_t((v & 31) * 255 / 31);
				d[3] = (desc & 0x0f) ? ((v & 0x8000) ? 255 : 0) : 255;
			}
			else
			{ // BGR(A)
				d[0] = p[2];
				d[1] = p[1];
				d[2] = p[0];
				d[3] = bpp == 32 ? p[3] : 255;
			}
		};

		std::vector<uint8_t> px(size_t(w) * h * 4);
		size_t n = size_t(w) * h, i = 0;
		uint8_t raw[4];
		while (i < n)
		{
			if (rle)
			{
				uint8_t hdr = r.u8();
				size_t count = (hdr & 0x7f) + 1;
				if (i + count > n) return fail(tr("uszkodzone dane RLE"));
				if (hdr & 0x80)
				{
					for (int b = 0; b < bytes; ++b) raw[b] = r.u8();
					for (size_t k = 0; k < count; ++k) decode(raw, &px[(i++) * 4]);
				}
				else
				{
					for (size_t k = 0; k < count; ++k)
					{
						for (int b = 0; b < bytes; ++b) raw[b] = r.u8();
						decode(raw, &px[(i++) * 4]);
					}
				}
			}
			else
			{
				for (int b = 0; b < bytes; ++b) raw[b] = r.u8();
				decode(raw, &px[(i++) * 4]);
			}
		}

		// Bit 5 deskryptora: początek u góry. Domyślnie TGA jest zapisany od dołu.
		bool topDown = desc & 0x20;
		bool rightToLeft = desc & 0x10;
		for (int y = 0; y < h; ++y)
		{
			int sy = topDown ? y : h - 1 - y;
			for (int x = 0; x < w; ++x)
			{
				int sx = rightToLeft ? w - 1 - x : x;
				const uint8_t* s = &px[(size_t(sy) * w + sx) * 4];
				uint8_t* d = &out.rgba[(size_t(y) * w + x) * 4];
				d[0] = s[0];
				d[1] = s[1];
				d[2] = s[2];
				d[3] = s[3];
			}
		}
		return true;
	}
	catch (const ParseError& e)
	{
		return fail(std::string("uszkodzony TGA: ") + e.what());
	}
}

} // namespace ow
