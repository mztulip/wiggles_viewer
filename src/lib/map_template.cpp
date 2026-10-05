#include "lib/map_template.h"

#include "lib/binreader.h"

#include <cstdlib>
#include <sstream>

namespace ow
{

Pmp parsePmp(const std::vector<uint8_t>& data)
{
	BinReader r(data);
	Pmp p;
	p.width = r.u32();
	p.height = r.u32();
	if (p.width == 0 || p.height == 0 || p.width > 4096 || p.height > 4096)
		throw ParseError("nieprawidłowy rozmiar PMP");
	size_t n = size_t(p.width) * p.height;
	r.readArray(p.types, n);
	r.readArray(p.blocks, n);
	r.readArray(p.mask, r.u32());
	r.readArray(p.layer2, n);
	if (!r.atEnd()) throw ParseError("nadmiarowe dane na końcu PMP: " + std::to_string(r.remaining()) + " B");
	return p;
}

L2m parseL2m(const std::vector<uint8_t>& data)
{
	if (data.empty()) return {}; // w grze są puste pliki .l2m (brak warstwy)
	BinReader r(data);
	uint32_t magic = r.u32();
	if (magic != 0x004C324D) throw ParseError("brak nagłówka M2L"); // "M2L\0"
	L2m l;
	l.width = r.u32();
	l.height = r.u32();
	if (l.width > 4096 || l.height > 4096) throw ParseError("nieprawidłowy rozmiar L2M");
	r.readArray(l.values, size_t(l.width) * l.height);
	if (!r.atEnd()) throw ParseError("nadmiarowe dane na końcu L2M");
	return l;
}

namespace
{

// Dzieli tekst na słowa Tcl z uwzględnieniem cudzysłowów i nawiasów [ ] { }.
std::vector<std::string> tclWords(const std::string& line)
{
	std::vector<std::string> out;
	size_t i = 0;
	while (i < line.size())
	{
		while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
		if (i >= line.size() || line[i] == ';') break;
		std::string w;
		if (line[i] == '"')
		{
			++i;
			int depth = 0;
			while (i < line.size() && !(line[i] == '"' && depth == 0))
			{
				if (line[i] == '[') ++depth;
				if (line[i] == ']') --depth;
				w += line[i++];
			}
			++i;
		}
		else
		{
			int depth = 0;
			while (i < line.size() && (depth > 0 || (line[i] != ' ' && line[i] != '\t')))
			{
				if (line[i] == '[' || line[i] == '{') ++depth;
				if (line[i] == ']' || line[i] == '}') --depth;
				w += line[i++];
			}
		}
		out.push_back(w);
	}
	return out;
}

// Wartość składowej pozycji: "15.01", "[expr {5.99 + $xoffset}]", "[expr 2.5 + $yoffset]".
// Pomijamy $xoffset/$yoffset, bo pozycja ma być względem szablonu.
float component(const std::string& s)
{
	std::string t;
	for (char c : s)
		if (c != '[' && c != ']' && c != '{' && c != '}') t += c;
	size_t e = t.find("expr");
	if (e != std::string::npos) t = t.substr(e + 4);
	return float(std::strtod(t.c_str(), nullptr));
}

// "a b c" z możliwymi wyrażeniami [expr ...] -> trzy liczby.
void vec3(const std::string& s, float out[3])
{
	auto parts = tclWords(s);
	for (int i = 0; i < 3; ++i) out[i] = i < int(parts.size()) ? component(parts[size_t(i)]) : 0.f;
}

} // namespace

TemplateScript parseTemplateScript(const std::string& text)
{
	TemplateScript t;
	std::istringstream in(text);
	std::string line, detail;
	TemplateObject* last = nullptr;
	while (std::getline(in, line))
	{
		if (!line.empty() && line.back() == '\r') line.pop_back();
		auto w = tclWords(line);
		if (w.empty()) continue;
		if (w[0] == "map_template" && w.size() >= 2) t.pmpFile = w[1];
		else if (w[0] == "map_setlayer2" && w.size() >= 4) t.l2mFile = w[3];
		else if (w[0] == "loading_objdetail" && w.size() >= 2) detail = w[1];
		else if (w[0] == "set" && w.size() >= 3 && w[2].rfind("[new ", 0) == 0)
		{
			// set objhandle [new Klasa "nazwa" "x y z" "rx ry rz"]
			auto inner = w[2].substr(5, w[2].size() - 6);
			auto a = tclWords(inner);
			if (a.empty()) continue;
			TemplateObject o;
			o.className = a[0];
			if (a.size() > 1) o.name = a[1];
			if (a.size() > 2) vec3(a[2], o.pos);
			if (a.size() > 3) vec3(a[3], o.rot);
			o.detail = detail;
			t.objects.push_back(o);
			last = &t.objects.back();
		}
		else if (w[0] == "call_method" && w.size() >= 4 && w[2] == "Editor_Set_Info" && last)
			last->info = w[3];
		else if (w[0] == "set_physic" && w.size() >= 3 && last)
			last->physic = w[2] == "true" || w[2] == "1";
	}
	return t;
}

} // namespace ow
