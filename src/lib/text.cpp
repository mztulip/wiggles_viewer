#include "lib/text.h"

namespace ow
{

static void appendUtf8(std::string& out, unsigned cp)
{
	if (cp < 0x80) out += char(cp);
	else if (cp < 0x800)
	{
		out += char(0xC0 | (cp >> 6));
		out += char(0x80 | (cp & 0x3F));
	}
	else
	{
		out += char(0xE0 | (cp >> 12));
		out += char(0x80 | ((cp >> 6) & 0x3F));
		out += char(0x80 | (cp & 0x3F));
	}
}

std::string cp1252ToUtf8(const std::string& s)
{
	// Zakres 0x80..0x9F różni się od Latin-1; reszta odpowiada kodom Unicode 1:1.
	static const unsigned k80[32] = {0x20AC, 0xFFFD, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
	    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0xFFFD, 0x017D, 0xFFFD,
	    0xFFFD, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
	    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0xFFFD, 0x017E, 0x0178};
	std::string out;
	out.reserve(s.size());
	for (unsigned char c : s) appendUtf8(out, (c >= 0x80 && c < 0xA0) ? k80[c - 0x80] : c);
	return out;
}

} // namespace ow
