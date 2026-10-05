#include "lib/messages.h"

#include <algorithm>

namespace ow
{

namespace
{

std::vector<std::string> splitLines(const std::vector<uint8_t>& data)
{
	std::vector<std::string> lines(1);
	for (uint8_t c : data)
	{
		if (c == '\n') lines.emplace_back();
		else if (c != '\r') lines.back() += char(c);
	}
	return lines;
}

// Tekst między pierwszym a ostatnim cudzysłowem (wewnętrzne cudzysłowy zostają: ""erfuellt"" -> "erfuellt").
bool quoted(const std::string& s, size_t from, std::string& out)
{
	size_t a = s.find('"', from), b = s.rfind('"');
	if (a == std::string::npos || b == a) return false;
	out = s.substr(a + 1, b - a - 1);
	return true;
}

} // namespace

bool Messages::load(const std::vector<uint8_t>& data)
{
	entries_.clear();
	languages_.clear();
	std::unordered_map<std::string, std::string>* cur = nullptr;
	for (const auto& line : splitLines(data))
	{
		std::string text;
		if (line.compare(0, 3, "## ") == 0)
		{
			cur = quoted(line, 3, text) ? &entries_[text] : nullptr;
			continue;
		}
		size_t sp = line.find(' ');
		if (!cur || sp == std::string::npos || sp == 0 || line[0] == '#' || !quoted(line, sp, text)) continue;
		std::string lang = line.substr(0, sp);
		(*cur)[lang] = text;
		if (std::find(languages_.begin(), languages_.end(), lang) == languages_.end()) languages_.push_back(lang);
	}
	return !entries_.empty();
}

const std::string* Messages::find(const std::string& key, const std::string& lang) const
{
	auto e = entries_.find(key);
	if (e == entries_.end()) return nullptr;
	auto t = e->second.find(lang);
	return t == e->second.end() ? nullptr : &t->second;
}

std::vector<Quote> parseQuotes(const std::vector<uint8_t>& data)
{
	std::vector<Quote> out;
	for (const auto& line : splitLines(data))
	{
		size_t a = line.find('\t'), b = a == std::string::npos ? a : line.find('\t', a + 1);
		if (b == std::string::npos) continue;
		out.push_back({line.substr(0, a), line.substr(a + 1, b - a - 1), line.substr(b + 1)});
	}
	return out;
}

} // namespace ow
