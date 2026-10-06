#include "lib/class_db.h"

#include "lib/vfs.h"

#include <cctype>
#include <cstdlib>
#include <sstream>

namespace ow
{

namespace
{

std::string trim(const std::string& s)
{
	size_t a = s.find_first_not_of(" \t\r");
	size_t b = s.find_last_not_of(" \t\r");
	return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

bool isIdent(char c)
{
	return std::isalnum((unsigned char)c) || c == '_';
}

// Elementy listy Tcl najwyższego poziomu: "a {b c} d" -> ["a", "b c", "d"]. Zewnętrzne klamry
// całego tekstu (np. "{ a b }") traktuje jak jeden element.
std::vector<std::string> tclListItems(const std::string& s)
{
	std::vector<std::string> out;
	size_t i = 0;
	while (i < s.size())
	{
		while (i < s.size() && std::isspace((unsigned char)s[i])) ++i;
		if (i >= s.size()) break;
		std::string w;
		if (s[i] == '{')
		{
			int depth = 1;
			++i;
			while (i < s.size() && depth > 0)
			{
				if (s[i] == '{') ++depth;
				else if (s[i] == '}' && --depth == 0) break;
				w += s[i++];
			}
			++i;
		}
		else
			while (i < s.size() && !std::isspace((unsigned char)s[i])) w += s[i++];
		out.push_back(w);
	}
	return out;
}

} // namespace

const ClassModel* ClassDb::find(const std::string& className) const
{
	auto it = classes_.find(Vfs::lower(className));
	return it == classes_.end() ? nullptr : &it->second;
}

const HorizonZone* ClassDb::horizon(const std::string& className) const
{
	auto it = horizons_.find(Vfs::lower(className));
	return it == horizons_.end() ? nullptr : &it->second;
}

const std::vector<ClassLight>* ClassDb::lights(const std::string& className) const
{
	auto it = lights_.find(Vfs::lower(className));
	return it == lights_.end() ? nullptr : &it->second;
}

const std::vector<ParticleSource>* ClassDb::particles(const std::string& className) const
{
	auto it = particles_.find(Vfs::lower(className));
	return it == particles_.end() ? nullptr : &it->second;
}

void ClassDb::parseScript(const std::string& text, const std::string& modelHint)
{
	std::istringstream in(text);
	std::string line, currentClass, listPrefix;
	bool inDummyList = false, listViewInFog = true;
	while (std::getline(in, line))
	{
		std::string t = trim(line);
		if (t.empty() || t.rfind("//", 0) == 0 || t[0] == '#') continue;

		// Listy Set<...>Classes { {Klasa obiekt} ... }: SetDummyClassesAll, SetDummyClassesNoZ,
		// SetFrontDummyClassesNoPhys (klasy Dummy_<Klasa>); SetWeaponClasses i ✅ SetCaveSkinClasses bez prefiksu
		// (ANIMINIT.TCL: def_class $classname; „new Dummy_Kris_caves_1_b_a” z szablonu gra odrzuca: unknown class).
		// Okładziny jaskiń stawia dopiero kopanie jaskini ze stempla (Diggles-Unity DIGGING_MECHANIC, cave skin).
		if (t.rfind("Set", 0) == 0 && t.find("Classes") != std::string::npos && t.find('{') != std::string::npos)
		{
			std::string listName = t.substr(0, t.find_first_of(" \t{"));
			bool dummy = listName.find("Dummy") != std::string::npos;
			listPrefix = dummy ? "Dummy_" : "";
			// Parametry listy: SetDummyClassesAll {noviewinfog nophysic} { ... }
			listViewInFog = t.find("noviewinfog") == std::string::npos;
			inDummyList = true;
			continue;
		}
		if (inDummyList)
		{
			// { Sufiks obiekt }  albo zamknięcie listy "}"
			if (t[0] == '{')
			{
				// Pozycja listy (lista Tcl): { Nazwa obiekt [opcje…] } albo { Nazwa {obiekt_a obiekt_b …} [opcje…] }
				auto items = tclListItems(t);
				if (items.size() == 1) items = tclListItems(items[0]);
				if (items.size() >= 2)
				{
					ClassModel cm;
					cm.animation = "standard";
					cm.modelHint = modelHint;
					cm.viewInFog = listViewInFog;
					cm.variants = tclListItems(items[1]);
					for (size_t k = 2; k < items.size(); ++k)
					{
						std::string o = Vfs::lower(items[k]);
						if (o == "anim") cm.animMode = 2;
						else if (o == "animrandom") cm.animMode = 2, cm.animRandom = true;
					}
					if (!cm.variants.empty())
					{
						cm.object = cm.variants.front();
						classes_[Vfs::lower(listPrefix + items[0])] = cm;
					}
				}
			}
			else if (t[0] == '}')
				inDummyList = false;
			continue;
		}

		if (t.rfind("def_class ", 0) == 0)
		{
			std::istringstream ws(t.substr(10));
			ws >> currentClass;
			continue;
		}
		// class_viewinfog 0|1 (widoczność w szarej nieodkrytym terenie)
		size_t vp = t.find("class_viewinfog ");
		if (vp != std::string::npos && !currentClass.empty() && t.find('$') == std::string::npos)
			viewInFog_[Vfs::lower(currentClass)] = std::atoi(t.c_str() + vp + 16) != 0;
		// class_light {ox oy oz} R {r g b} [...]
		size_t lp = t.find("class_light ");
		if (lp != std::string::npos && !currentClass.empty() && t.find('$') == std::string::npos)
		{
			std::string flat;
			for (char c : t.substr(lp + 12))
				flat += (c == '{' || c == '}') ? ' ' : c;
			std::istringstream ws(flat);
			ClassLight l;
			if (ws >> l.offset[0] >> l.offset[1] >> l.offset[2] >> l.radius >> l.color[0] >> l.color[1] >> l.color[2])
				lights_[Vfs::lower(currentClass)].push_back(l);
		}
		// class_particle typ {relpos} {dir} max naKlatkę widocznyWMgle …
		size_t pp = t.find("class_particle ");
		if (pp != std::string::npos && !currentClass.empty())
		{
			ParticleSource ps;
			if (parseClassParticle(t.substr(pp + 15), ps)) particles_[Vfs::lower(currentClass)].push_back(ps);
		}
		// horizon_set [get_pos this] r g b głębokość zasięg (tylko wartości liczbowe)
		size_t hp = t.find("horizon_set [get_pos this]");
		if (hp != std::string::npos && t.rfind("log", 0) != 0 && !currentClass.empty())
		{
			std::istringstream ws(t.substr(hp + 26));
			HorizonZone z;
			if (ws >> z.r >> z.g >> z.b >> z.depth >> z.size) horizons_.emplace(Vfs::lower(currentClass), z);
		}
		// set standanim <obiekt>.<anim> (obj_init drzwi i krat, potem set_anim this $standanim): zastępcza
		// domyślna animacja dla klas bez class_defaultanim; model nadaje dopiero skrypt.
		size_t sp = t.find("set standanim ");
		if (sp != std::string::npos && !currentClass.empty())
		{
			std::string anim = trim(t.substr(sp + 14));
			size_t e = 0;
			while (e < anim.size() && (isIdent(anim[e]) || anim[e] == '.')) ++e;
			anim = anim.substr(0, e);
			if (!anim.empty()) standAnims_.emplace(Vfs::lower(currentClass), anim);
		}
		size_t p = t.find("class_defaultanim ");
		if (p != std::string::npos && !currentClass.empty())
		{
			std::string anim = trim(t.substr(p + 18));
			size_t e = 0;
			while (e < anim.size() && (isIdent(anim[e]) || anim[e] == '.')) ++e;
			anim = anim.substr(0, e);
			size_t dot = anim.find('.');
			ClassModel cm;
			cm.object = dot == std::string::npos ? anim : anim.substr(0, dot);
			cm.animation = dot == std::string::npos ? "standard" : anim.substr(dot + 1);
			cm.modelHint = modelHint;
			cm.editorOnly = t.find("get_mapedit") != std::string::npos;
			{
				// class_defaultanim <obiekt>.<anim> <tryb>
				std::istringstream ms(trim(t.substr(p + 18)));
				std::string a;
				int mode = 0;
				if (ms >> a >> mode) cm.animMode = mode;
			}
			if (!cm.object.empty() && cm.object[0] != '$')
			{
				auto key = Vfs::lower(currentClass);
				if (!classes_.count(key)) classes_[key] = cm;
			}
		}
	}
}

void ClassDb::load(const Vfs& vfs)
{
	classes_.clear();
	horizons_.clear();
	lights_.clear();
	particles_.clear();
	viewInFog_.clear();
	standAnims_.clear();
	// Najpierw dekoracje: tu nazwa pliku wskazuje model.
	for (const auto& f : vfs.list("Data/Scripts/classes/deco"))
	{
		auto low = Vfs::lower(f);
		if (low.rfind("dummy_", 0) != 0 || low.size() < 10 || low.substr(low.size() - 4) != ".tcl") continue;
		std::string hint = low.substr(6, low.size() - 10) + ".3db";
		if (auto data = vfs.read("Data/Scripts/classes/deco/" + f))
			parseScript(std::string(data->begin(), data->end()), hint);
	}
	// Pozostałe klasy (bez wskazówki pliku modelu).
	for (const auto& dir : vfs.list("Data/Scripts/classes"))
	{
		std::string d = "Data/Scripts/classes/" + dir;
		if (Vfs::lower(dir) == "deco") continue;
		for (const auto& f : vfs.list(d))
		{
			auto low = Vfs::lower(f);
			if (low.size() < 4 || low.substr(low.size() - 4) != ".tcl") continue;
			if (auto data = vfs.read(d + "/" + f)) parseScript(std::string(data->begin(), data->end()), "");
		}
	}
	for (const auto& [name, anim] : standAnims_)
	{
		if (classes_.count(name)) continue;
		size_t dot = anim.find('.');
		ClassModel cm;
		cm.object = anim.substr(0, dot);
		cm.animation = dot == std::string::npos ? "standard" : anim.substr(dot + 1);
		classes_[name] = cm; // ANIM_STILL jak set_anim this $standanim 0 $ANIM_STILL
	}
	for (auto& [name, v] : viewInFog_)
		if (auto it = classes_.find(name); it != classes_.end()) it->second.viewInFog = v;
}

} // namespace ow
