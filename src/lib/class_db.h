// Mapowanie klas obiektów gry na modele .3db.
//
// Klasy dekoracji Dummy_* są zdefiniowane w Data/Scripts/classes/deco/dummy_<plik>.tcl:
//  - listy SetDummyClassesAll / SetDummyClassesNoZ / SetFrontDummyClassesNoPhys / SetCaveSkinClasses
//    { {Sufiks obiekt} ... } tworzą klasę Dummy_<Sufiks> z animacją <obiekt>.standard
//    (SetWeaponClasses w items/waffen.tcl tworzy klasy bez prefiksu),
//  - def_class <Klasa> ... { ... class_defaultanim <obiekt>.<animacja> ... }.
// Plik dummy_<plik>.tcl wskazuje model Data/3db/<plik>.3db.
// Dla innych klas zbieramy class_defaultanim ze wszystkich skryptów w Data/Scripts/classes.
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace ow {

class Vfs;

struct ClassModel {
	std::string object;    // nazwa obiektu w .3db
	std::string animation; // nazwa animacji (domyślnie "standard")
	std::string modelHint; // plik .3db, w którym najpewniej leży obiekt (może być pusty)
	std::vector<std::string> variants; // warianty wyglądu (gra losuje jeden); object = pierwszy
	bool editorOnly = false;           // model tylko w edytorze map: if {[get_mapedit]} {class_defaultanim ...}
};

// Strefa horyzontu (klasy Info_Fog_* w Scripts/classes/items/infoobjs.tcl):
// horizon_set <pozycja> r g b głębokość zasięg. Kolor wypełnia otwory w tylnej ścianie
// (typ pola 5), a modele za skałą (z < 0 bloków) przechodzą w ten kolor ∝ |z|/głębokość.
// Opis: Zimennik/Diggles-Unity (MIT), Docs/BACKDROP_PARALLAX_RE.md §3–4.
struct HorizonZone {
	float r = 0, g = 0, b = 0;
	float depth = 32, size = 15;
};

class ClassDb {
public:
	void load(const Vfs& vfs);
	// Klucz bez rozróżniania wielkości liter. nullptr, gdy nie znamy klasy.
	const ClassModel* find(const std::string& className) const;
	size_t size() const { return classes_.size(); }
	const HorizonZone* horizon(const std::string& className) const;

private:
	void parseScript(const std::string& text, const std::string& modelHint);
	std::unordered_map<std::string, ClassModel> classes_;
	std::unordered_map<std::string, HorizonZone> horizons_;
};

} // namespace ow
