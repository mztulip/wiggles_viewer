#include "lib/class_db.h"

#include "lib/vfs.h"

#include <cctype>
#include <sstream>

namespace ow {

namespace {

std::string trim(const std::string& s) {
	size_t a = s.find_first_not_of(" \t\r");
	size_t b = s.find_last_not_of(" \t\r");
	return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

bool isIdent(char c) { return std::isalnum((unsigned char)c) || c == '_'; }

} // namespace

const ClassModel* ClassDb::find(const std::string& className) const {
	auto it = classes_.find(Vfs::lower(className));
	return it == classes_.end() ? nullptr : &it->second;
}

const HorizonZone* ClassDb::horizon(const std::string& className) const {
	auto it = horizons_.find(Vfs::lower(className));
	return it == horizons_.end() ? nullptr : &it->second;
}

void ClassDb::parseScript(const std::string& text, const std::string& modelHint) {
	std::istringstream in(text);
	std::string line, currentClass, listPrefix;
	bool inDummyList = false;
	while (std::getline(in, line)) {
		std::string t = trim(line);
		if (t.empty() || t.rfind("//", 0) == 0 || t[0] == '#') continue;

		// Listy Set<...>Classes { {Klasa obiekt} ... }: SetDummyClassesAll, SetDummyClassesNoZ,
		// SetFrontDummyClassesNoPhys, SetCaveSkinClasses (klasy Dummy_<Klasa>), SetWeaponClasses (bez prefiksu).
		if (t.rfind("Set", 0) == 0 && t.find("Classes") != std::string::npos && t.find('{') != std::string::npos) {
			std::string listName = t.substr(0, t.find_first_of(" \t{"));
			bool dummy = listName.find("Dummy") != std::string::npos || listName.find("CaveSkin") != std::string::npos;
			listPrefix = dummy ? "Dummy_" : "";
			inDummyList = true;
			continue;
		}
		if (inDummyList) {
			// { Sufiks obiekt }  albo zamknięcie listy "}"
			if (t[0] == '{') {
				// { Sufiks obiekt }  albo  { Sufiks {wariant_a wariant_b ...} }
				std::string flat;
				for (char c : t.substr(1))
					if (c != '{' && c != '}') flat += c;
				std::istringstream ws(flat);
				std::string suffix, object;
				ws >> suffix;
				ClassModel cm;
				cm.animation = "standard";
				cm.modelHint = modelHint;
				while (ws >> object) cm.variants.push_back(object);
				if (!suffix.empty() && !cm.variants.empty()) {
					cm.object = cm.variants.front();
					classes_[Vfs::lower(listPrefix + suffix)] = cm;
				}
			} else if (t[0] == '}')
				inDummyList = false;
			continue;
		}

		if (t.rfind("def_class ", 0) == 0) {
			std::istringstream ws(t.substr(10));
			ws >> currentClass;
			continue;
		}
		// horizon_set [get_pos this] r g b głębokość zasięg (tylko wartości liczbowe)
		size_t hp = t.find("horizon_set [get_pos this]");
		if (hp != std::string::npos && t.rfind("log", 0) != 0 && !currentClass.empty()) {
			std::istringstream ws(t.substr(hp + 26));
			HorizonZone z;
			if (ws >> z.r >> z.g >> z.b >> z.depth >> z.size) horizons_.emplace(Vfs::lower(currentClass), z);
		}
		size_t p = t.find("class_defaultanim ");
		if (p != std::string::npos && !currentClass.empty()) {
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
			if (!cm.object.empty() && cm.object[0] != '$') {
				auto key = Vfs::lower(currentClass);
				if (!classes_.count(key)) classes_[key] = cm;
			}
		}
	}
}

void ClassDb::load(const Vfs& vfs) {
	classes_.clear();
	horizons_.clear();
	// Najpierw dekoracje: tu nazwa pliku wskazuje model.
	for (const auto& f : vfs.list("Data/Scripts/classes/deco")) {
		auto low = Vfs::lower(f);
		if (low.rfind("dummy_", 0) != 0 || low.size() < 10 || low.substr(low.size() - 4) != ".tcl") continue;
		std::string hint = low.substr(6, low.size() - 10) + ".3db";
		if (auto data = vfs.read("Data/Scripts/classes/deco/" + f))
			parseScript(std::string(data->begin(), data->end()), hint);
	}
	// Pozostałe klasy (bez wskazówki pliku modelu).
	for (const auto& dir : vfs.list("Data/Scripts/classes")) {
		std::string d = "Data/Scripts/classes/" + dir;
		if (Vfs::lower(dir) == "deco") continue;
		for (const auto& f : vfs.list(d)) {
			auto low = Vfs::lower(f);
			if (low.size() < 4 || low.substr(low.size() - 4) != ".tcl") continue;
			if (auto data = vfs.read(d + "/" + f)) parseScript(std::string(data->begin(), data->end()), "");
		}
	}
}

} // namespace ow
