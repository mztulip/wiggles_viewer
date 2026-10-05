#include "lib/sound_refs.h"

#include "lib/i18n.h"
#include "lib/vfs.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <unordered_set>

namespace ow
{

namespace
{

bool isWordChar(char c)
{
	return std::isalnum((unsigned char)c) || c == '_';
}

// Słowa Tcl w kawałku linii: rozdzielone białymi znakami, bez klamer, cudzysłowów i ';' na brzegach.
std::vector<std::string> words(const std::string& s)
{
	std::vector<std::string> out;
	std::string cur;
	auto flush = [&]() {
		while (!cur.empty() && std::strchr("{}\"\\;", cur.back())) cur.pop_back();
		size_t b = 0;
		while (b < cur.size() && std::strchr("{}\"\\", cur[b])) ++b;
		if (b < cur.size()) out.push_back(cur.substr(b));
		cur.clear();
	};
	for (char c : s)
	{
		if (c == ' ' || c == '\t' || c == ';')
		{
			flush();
			if (c == ';') out.push_back(";");
		}
		else cur += c;
	}
	flush();
	return out;
}

bool isVariable(const std::string& w)
{
	return w.find('$') != std::string::npos || w.find('[') != std::string::npos;
}

std::string stemOf(const std::string& key)
{
	size_t s = key.rfind('/');
	std::string f = s == std::string::npos ? key : key.substr(s + 1);
	size_t d = f.rfind('.');
	return d == std::string::npos ? f : f.substr(0, d);
}

bool parseFloat(const std::string& s, float& out)
{
	char* end = nullptr;
	float v = std::strtof(s.c_str(), &end);
	if (end == s.c_str() || *end) return false;
	out = v;
	return true;
}

struct ScriptFile
{
	std::string path; // względem Data/
	std::vector<std::string> lines;
};

void collectScripts(const Vfs& vfs, std::vector<ScriptFile>& out)
{
	std::vector<std::string> dirs = {"Data/Scripts"};
	std::vector<std::string> files = {"Data/systeminit.tcl"};
	while (!dirs.empty())
	{
		std::string d = dirs.back();
		dirs.pop_back();
		for (const auto& f : vfs.list(d))
		{
			std::string path = d + "/" + f, low = Vfs::lower(f);
			size_t dot = low.rfind('.');
			if (dot == std::string::npos) dirs.push_back(path);
			else if (low.substr(dot) == ".tcl" || low.substr(dot) == ".txt") files.push_back(path);
		}
	}
	for (const auto& path : files)
	{
		auto data = vfs.read(path);
		if (!data) continue;
		ScriptFile sf;
		sf.path = path.substr(5); // bez "Data/"
		std::string line;
		for (uint8_t c : *data)
		{
			if (c == '\n')
			{
				sf.lines.push_back(line);
				line.clear();
			}
			else if (c != '\r') line += char(c);
		}
		sf.lines.push_back(line);
		out.push_back(std::move(sf));
	}
}

// Linia zakomentowana (// albo #); zwraca resztę linii. Plik sekwencji może zaczynać linię od '-'
// (pomiń, gdy gracz przerwał sekwencję) albo '+' (wykonaj także wtedy), sequencer.tcl parse_next.
std::string stripComment(const std::string& t, bool& commented, bool& seqSkippable)
{
	commented = seqSkippable = false;
	size_t p = t.find_first_not_of(" \t");
	if (p == std::string::npos) return std::string();
	for (;;)
	{
		if (t.compare(p, 2, "//") == 0) p += 2;
		else if (t[p] == '#') ++p;
		else break;
		commented = true;
		p = t.find_first_not_of(" \t", p);
		if (p == std::string::npos) return std::string();
	}
	if (t[p] == '-' || t[p] == '+')
	{
		seqSkippable = t[p] == '-';
		++p;
	}
	return t.substr(p);
}

} // namespace

std::vector<std::string> listSoundFiles(const Vfs& vfs)
{
	std::vector<std::string> out, dirs = {"Data/Music", "Data/Sound"};
	while (!dirs.empty())
	{
		std::string d = dirs.back();
		dirs.pop_back();
		for (const auto& f : vfs.list(d))
		{
			std::string path = d + "/" + f, low = Vfs::lower(f);
			size_t dot = low.rfind('.');
			if (dot == std::string::npos) dirs.push_back(path);
			else if (low.substr(dot) == ".wav" || low.substr(dot) == ".mp3") out.push_back(path);
		}
	}
	std::sort(out.begin(), out.end(), [](const std::string& a, const std::string& b) { return Vfs::lower(a) < Vfs::lower(b); });
	return out;
}

std::string soundFileKey(std::string path)
{
	path = Vfs::lower(path);
	std::replace(path.begin(), path.end(), '\\', '/');
	while (path.compare(0, 2, "./") == 0) path.erase(0, 2);
	if (path.compare(0, 5, "data/") == 0) path.erase(0, 5);
	return path;
}

const char* soundRefKindName(SoundRef::Kind k)
{
	switch (k)
	{
	case SoundRef::Bind: return N_("nazwa (sound bind)");
	case SoundRef::Anim: return N_("klatka animacji");
	case SoundRef::AnimNoSeq: return N_("klatka animacji (poza sekwencjami)");
	case SoundRef::Play: return N_("sound play");
	case SoundRef::PlayGlobal: return N_("sound playglobal");
	case SoundRef::Music: return N_("muzyka (adaptive_sound)");
	case SoundRef::Clip: return N_("dialog / sekwencja");
	default: return N_("wzmianka");
	}
}

SoundIndex scanSoundRefs(const Vfs& vfs, const std::vector<std::string>& knownFiles)
{
	SoundIndex idx;
	std::vector<ScriptFile> scripts;
	collectScripts(vfs, scripts);

	std::unordered_set<std::string> stems;
	for (const auto& k : knownFiles) stems.insert(stemOf(k));

	auto add = [&](SoundRef r) {
		int i = int(idx.refs.size());
		if (r.kind == SoundRef::Bind)
		{
			idx.byFile[r.file].push_back(i);
			idx.fileOfName[r.name] = r.file;
		}
		else if (r.kind == SoundRef::Music) idx.byFile[r.file].push_back(i);
		else if (r.kind == SoundRef::Clip) idx.byStem[stemOf(r.file)].push_back(i);
		else if (!r.name.empty() && !isVariable(r.name)) idx.byName[r.name].push_back(i);
		if ((r.kind == SoundRef::Anim || r.kind == SoundRef::AnimNoSeq) && !r.anim.empty()) idx.byAnim[r.anim].push_back(i);
		idx.refs.push_back(std::move(r));
	};

	// Linie bez rozpoznanego polecenia: tu szukamy później wzmianek nazw.
	struct Pending
	{
		int script, line;
	};
	std::vector<Pending> rest;
	std::vector<int> variableNames; // sound play c4_beep$b: dopasowanie po prefiksie po zebraniu nazw

	for (int si = 0; si < int(scripts.size()); ++si)
	{
		const auto& sf = scripts[size_t(si)];
		for (int li = 0; li < int(sf.lines.size()); ++li)
		{
			bool commented, seqSkippable;
			std::string body = stripComment(sf.lines[size_t(li)], commented, seqSkippable);
			if (body.empty()) continue;
			std::string text = sf.lines[size_t(li)];
			text.erase(0, text.find_first_not_of(" \t"));
			auto base = [&](SoundRef::Kind k) {
				SoundRef r;
				r.kind = k;
				r.script = sf.path;
				r.line = li + 1;
				r.text = text;
				r.commented = commented;
				r.seqSkippable = seqSkippable;
				return r;
			};
			bool found = false;
			std::vector<std::string> w = words(body);
			for (size_t i = 0; i < w.size(); ++i)
			{
				std::string cmd = Vfs::lower(w[i]);
				auto arg = [&](size_t k) { return i + k < w.size() ? w[i + k] : std::string(); };
				if (cmd == "sound")
				{
					std::string sub = Vfs::lower(arg(1));
					if (sub == "bind" && !arg(2).empty() && !arg(3).empty())
					{
						SoundRef r = base(SoundRef::Bind);
						r.name = Vfs::lower(arg(2));
						r.file = soundFileKey(arg(3));
						add(std::move(r));
						found = true;
					}
					else if ((sub == "anim" || sub == "animnoseq") && !arg(4).empty())
					{
						SoundRef r = base(sub == "anim" ? SoundRef::Anim : SoundRef::AnimNoSeq);
						r.anim = Vfs::lower(arg(2));
						r.frame = std::atoi(arg(3).c_str());
						r.name = Vfs::lower(arg(4));
						parseFloat(arg(7), r.volume); // materiał, pomieszczenie, głośność
						add(std::move(r));
						found = true;
					}
					else if ((sub == "play" || sub == "playglobal") && !arg(2).empty())
					{
						SoundRef r = base(sub == "play" ? SoundRef::Play : SoundRef::PlayGlobal);
						r.name = Vfs::lower(arg(2));
						parseFloat(arg(3), r.volume);
						if (isVariable(r.name)) variableNames.push_back(int(idx.refs.size()));
						add(std::move(r));
						found = true;
					}
				}
				else if (cmd == "adaptive_sound")
				{
					for (size_t k = i + 1; k < w.size() && w[k] != ";"; ++k)
						if (Vfs::lower(w[k]).find(".mp3") != std::string::npos)
						{
							SoundRef r = base(SoundRef::Music);
							r.file = soundFileKey(w[k]);
							add(std::move(r));
							found = true;
						}
				}
				else if (cmd == "soundfile" || cmd == "soundfile_open" || (cmd == "seq_audiostream" && Vfs::lower(arg(1)) == "open"))
				{
					std::string f = cmd == "seq_audiostream" ? arg(2) : arg(1);
					if (!f.empty() && !isVariable(f) && f != "filename" && f != "{filename}")
					{
						SoundRef r = base(SoundRef::Clip);
						r.file = soundFileKey(f);
						add(std::move(r));
						found = true;
					}
				}
			}
			if (!found) rest.push_back({si, li});
		}
	}

	// sound play z nazwą zawierającą zmienną (np. c4_beep$b): wszystkie nazwy o tym prefiksie.
	for (int i : variableNames)
	{
		const std::string& n = idx.refs[size_t(i)].name;
		std::string prefix = n.substr(0, std::min(n.find('$'), n.find('[')));
		if (prefix.size() < 3) continue;
		for (const auto& [name, file] : idx.fileOfName)
			if (name.compare(0, prefix.size(), prefix) == 0) idx.byName[name].push_back(i);
	}

	// Wzmianki: słowa równe nazwie logicznej albo nazwie pliku (bez rozszerzenia) w pozostałych liniach,
	// np. listy {klatka dźwięk} w snd_animlist, zmienne z nazwą dźwięku, ścieżki w komentarzach.
	// Słowa występujące bardzo często (anim, hamster, labor…) to zwykłe słowa skryptów, a nie odwołania.
	constexpr int kMaxMentions = 12;
	std::vector<SoundRef> mentions;
	std::unordered_map<std::string, int> count;
	for (const auto& p : rest)
	{
		const std::string& line = scripts[size_t(p.script)].lines[size_t(p.line)];
		bool commented, seqSkippable;
		stripComment(line, commented, seqSkippable);
		std::string low = Vfs::lower(line);
		std::unordered_set<std::string> seen;
		for (size_t a = 0; a < low.size();)
		{
			if (!isWordChar(low[a]))
			{
				++a;
				continue;
			}
			size_t b = a;
			while (b < low.size() && isWordChar(low[b])) ++b;
			std::string tok = low.substr(a, b - a);
			a = b;
			if (tok.size() < 4 || !seen.insert(tok).second) continue;
			bool isName = idx.fileOfName.count(tok) > 0;
			if (!isName && !stems.count(tok)) continue;
			SoundRef r;
			r.kind = SoundRef::Mention;
			r.script = scripts[size_t(p.script)].path;
			r.line = p.line + 1;
			r.text = line.substr(std::min(line.size(), line.find_first_not_of(" \t")));
			r.commented = commented;
			r.seqSkippable = seqSkippable;
			r.name = tok;
			r.file = isName ? std::string() : tok; // pusty: wzmianka nazwy logicznej
			count[tok]++;
			mentions.push_back(std::move(r));
		}
	}
	for (auto& r : mentions)
	{
		if (count[r.name] > kMaxMentions) continue;
		int i = int(idx.refs.size());
		if (r.file.empty()) idx.byName[r.name].push_back(i);
		else
		{
			idx.byStem[r.file].push_back(i);
			r.name.clear();
			r.file.clear();
		}
		idx.refs.push_back(std::move(r));
	}
	return idx;
}

std::vector<std::string> SoundIndex::namesForFile(const std::string& fileKey) const
{
	std::vector<std::string> out;
	auto it = byFile.find(fileKey);
	if (it == byFile.end()) return out;
	for (int i : it->second)
	{
		const auto& r = refs[size_t(i)];
		if (r.kind == SoundRef::Bind && std::find(out.begin(), out.end(), r.name) == out.end()) out.push_back(r.name);
	}
	return out;
}

std::vector<int> SoundIndex::refsForFile(const std::string& fileKey) const
{
	std::vector<int> out;
	auto append = [&](const std::unordered_map<std::string, std::vector<int>>& m, const std::string& k) {
		auto it = m.find(k);
		if (it != m.end()) out.insert(out.end(), it->second.begin(), it->second.end());
	};
	append(byFile, fileKey);
	for (const auto& n : namesForFile(fileKey)) append(byName, n);
	append(byStem, stemOf(fileKey));
	std::sort(out.begin(), out.end());
	out.erase(std::unique(out.begin(), out.end()), out.end());
	// Najpierw powiązania (bind), potem użycia, na końcu wzmianki; w ramach rodzaju kolejność skryptów.
	std::stable_sort(out.begin(), out.end(), [&](int a, int b) { return refs[size_t(a)].kind < refs[size_t(b)].kind; });
	return out;
}

} // namespace ow
