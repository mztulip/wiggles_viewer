#include "lib/i18n.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <unordered_map>

namespace ow::i18n
{

namespace
{

struct Catalog
{
	Language info;
	std::unordered_map<std::string, std::string> strings; // źródło -> tłumaczenie
	std::unordered_map<std::string, std::string> withId;  // "źródło##id" -> "tłumaczenie##id"
};

struct State
{
	std::vector<std::unique_ptr<Catalog>> catalogs; // [0] = polski (źródło)
	std::vector<Language> list;
	Catalog* current = nullptr;
	State()
	{
		auto pl = std::make_unique<Catalog>();
		pl->info = {"pl", "Polski", 0};
		catalogs.push_back(std::move(pl));
		current = catalogs[0].get();
		list.push_back(current->info);
	}
};

State& state()
{
	static State s;
	return s;
}

// Znaczniki printf w kolejności (np. "%s %zu %.1f" -> "s zu f"); %% pomijane.
std::string formatSignature(const std::string& s)
{
	std::string sig;
	for (size_t i = 0; i < s.size(); ++i)
	{
		if (s[i] != '%') continue;
		if (i + 1 < s.size() && s[i + 1] == '%')
		{
			++i;
			continue;
		}
		size_t j = i + 1;
		while (j < s.size() && std::strchr("-+ #0123456789.*", s[j])) ++j;
		std::string len;
		while (j < s.size() && std::strchr("hljztL", s[j])) len += s[j++];
		if (j < s.size()) sig += len + s[j] + ' ';
		i = j;
	}
	return sig;
}

// Napis w cudzysłowie z pliku .po (z sekwencjami \n \t \" \\).
bool parseQuoted(const std::string& line, size_t pos, std::string& out)
{
	pos = line.find('"', pos);
	if (pos == std::string::npos) return false;
	for (size_t i = pos + 1; i < line.size(); ++i)
	{
		char c = line[i];
		if (c == '"') return true;
		if (c == '\\' && i + 1 < line.size())
		{
			char e = line[++i];
			out += e == 'n' ? '\n' : e == 't' ? '\t'
			                                  : e;
		}
		else
			out += c;
	}
	return false;
}

std::string headerField(const std::string& header, const char* name)
{
	std::string key = std::string(name) + ":";
	size_t p = 0;
	while (p < header.size())
	{
		size_t e = header.find('\n', p);
		if (e == std::string::npos) e = header.size();
		std::string line = header.substr(p, e - p);
		if (line.compare(0, key.size(), key) == 0)
		{
			std::string v = line.substr(key.size());
			while (!v.empty() && v.front() == ' ') v.erase(v.begin());
			while (!v.empty() && (v.back() == ' ' || v.back() == '\r')) v.pop_back();
			return v;
		}
		p = e + 1;
	}
	return {};
}

struct BuiltinLanguage
{
	const char* file;
	const char* text;
};
#include "ow_lang.inc" // kBuiltinLanguages[] = {{"en.po", R"(…)"}, …, {nullptr, nullptr}}

} // namespace

std::string addPo(const std::string& text, std::string& err, std::vector<std::string>* warnings)
{
	auto cat = std::make_unique<Catalog>();
	std::string header;
	bool fuzzy = false;
	std::string id, str, *target = nullptr;
	bool haveEntry = false;
	auto flush = [&]() {
		if (haveEntry)
		{
			if (id.empty()) header = str;
			else if (!str.empty() && !fuzzy)
			{
				if (formatSignature(id) != formatSignature(str))
				{
					if (warnings) warnings->push_back("niezgodne znaczniki printf: \"" + id + "\"");
				}
				else
					cat->strings[id] = str;
			}
		}
		id.clear();
		str.clear();
		target = nullptr;
		haveEntry = false;
		fuzzy = false;
	};
	size_t p = 0;
	int lineNo = 0;
	bool pendingFuzzy = false;
	while (p <= text.size())
	{
		size_t e = text.find('\n', p);
		if (e == std::string::npos) e = text.size();
		std::string line = text.substr(p, e - p);
		p = e + 1;
		++lineNo;
		if (!line.empty() && line.back() == '\r') line.pop_back();
		size_t s = line.find_first_not_of(" \t");
		if (s == std::string::npos) continue;
		if (line[s] == '#')
		{
			if (line.compare(s, 2, "#,") == 0 && line.find("fuzzy") != std::string::npos) pendingFuzzy = true;
			continue;
		}
		if (line.compare(s, 7, "msgctxt") == 0)
		{
			err = "linia " + std::to_string(lineNo) + ": msgctxt nie jest obsługiwane";
			return {};
		}
		if (line.compare(s, 5, "msgid") == 0 && line.compare(s, 12, "msgid_plural") != 0)
		{
			flush();
			fuzzy = pendingFuzzy;
			pendingFuzzy = false;
			haveEntry = true;
			target = &id;
		}
		else if (line.compare(s, 6, "msgstr") == 0)
			target = &str;
		else if (line[s] == '"')
		{
			// kontynuacja poprzedniego napisu
		}
		else
		{
			err = "linia " + std::to_string(lineNo) + ": nieznana składnia";
			return {};
		}
		if (!target || !parseQuoted(line, s, *target))
		{
			err = "linia " + std::to_string(lineNo) + ": brak napisu w cudzysłowie";
			return {};
		}
	}
	flush();
	cat->info.code = headerField(header, "Language");
	cat->info.name = headerField(header, "X-Language-Name");
	if (cat->info.code.empty())
	{
		err = "brak pola Language w nagłówku";
		return {};
	}
	if (cat->info.name.empty()) cat->info.name = cat->info.code;
	cat->info.translated = int(cat->strings.size());
	State& st = state();
	std::string code = cat->info.code;
	for (const auto& l : st.list)
		if (l.code == code)
		{
			err = "język " + code + " jest już wczytany";
			return {};
		}
	st.catalogs.push_back(std::move(cat));
	st.list.push_back(st.catalogs.back()->info);
	return code;
}

void loadBuiltin(std::vector<std::string>* warnings)
{
	for (const BuiltinLanguage* b = kBuiltinLanguages; b->file; ++b)
	{
		std::string err;
		if (addPo(b->text, err, warnings).empty() && warnings) warnings->push_back(std::string(b->file) + ": " + err);
	}
}

const std::vector<Language>& languages()
{
	return state().list;
}

bool setLanguage(const std::string& code)
{
	State& st = state();
	for (size_t i = 0; i < st.list.size(); ++i)
		if (st.list[i].code == code)
		{
			st.current = st.catalogs[i].get();
			return true;
		}
	return false;
}

const std::string& language()
{
	return state().current->info.code;
}

std::string systemLanguage()
{
	for (const char* var : {"LC_ALL", "LC_MESSAGES", "LANG"})
	{
		const char* v = std::getenv(var);
		if (!v || !*v || !std::strcmp(v, "C") || !std::strcmp(v, "POSIX")) continue;
		std::string code(v);
		code = code.substr(0, code.find_first_of("_.@"));
		for (const auto& l : languages())
			if (l.code == code) return code;
		break;
	}
	return "en";
}

const char* tr(const char* source)
{
	State& st = state();
	Catalog* c = st.current;
	if (c == st.catalogs[0].get() || !source) return source;
	const char* hash = std::strstr(source, "##");
	if (!hash)
	{
		auto it = c->strings.find(source);
		return it != c->strings.end() ? it->second.c_str() : source;
	}
	auto w = c->withId.find(source);
	if (w != c->withId.end()) return w->second.c_str();
	std::string text(source, hash);
	auto it = c->strings.find(text);
	std::string out = (it != c->strings.end() ? it->second : text) + hash;
	return c->withId.emplace(source, std::move(out)).first->second.c_str();
}

std::string strf(const char* fmt, ...)
{
	va_list a, b;
	va_start(a, fmt);
	va_copy(b, a);
	int n = std::vsnprintf(nullptr, 0, fmt, a);
	va_end(a);
	std::string out(size_t(std::max(n, 0)), '\0');
	if (n > 0) std::vsnprintf(out.data(), size_t(n) + 1, fmt, b);
	va_end(b);
	return out;
}

} // namespace ow::i18n
