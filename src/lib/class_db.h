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
	std::vector<std::string> variants; // obiekty z listy Set*Classes; gra używa pierwszego (= object), ❓ reszta
	bool editorOnly = false;           // model tylko w edytorze map: if {[get_mapedit]} {class_defaultanim ...}
	// Tryb animacji z class_defaultanim <obiekt>.<anim> <tryb> (ANIMINIT.TCL): 0 ANIM_STILL (klatka 0),
	// 1 ANIM_ONCE, 2 ANIM_LOOP. Dekoracje z list Set*Classes: 2 tylko z opcją "anim", inaczej 0.
	int animMode = 0;
	bool animRandom = false; // opcja "animrandom": losowa klatka startowa
	// Widoczność w szarej mgle wojny (class_viewinfog; dekoracje domyślnie 1, parametr "noviewinfog").
	bool viewInFog = false;
};

// Strefa horyzontu (klasy Info_Fog_* w Scripts/classes/items/infoobjs.tcl):
// horizon_set <pozycja> r g b głębokość zasięg. Kolor wypełnia otwory w tylnej ścianie
// (typ pola 5), a modele za skałą (z < 0 bloków) przechodzą w ten kolor ∝ |z|/głębokość.
// Opis: Zimennik/Diggles-Unity (MIT), Docs/BACKDROP_PARALLAX_RE.md §3–4.
struct HorizonZone {
	float r = 0, g = 0, b = 0;
	float depth = 32, size = 15;
};

// Źródło światła klasy: class_light {ox oy oz} R {r g b} (przesunięcie względem obiektu, promień
// w blokach, kolor 0..1). Klasa może mieć kilka.
struct ClassLight {
	float offset[3] = {0, 0, 0};
	float radius = 0;
	float color[3] = {0, 0, 0};
};

class ClassDb {
public:
	void load(const Vfs& vfs);
	// Klucz bez rozróżniania wielkości liter. nullptr, gdy nie znamy klasy.
	const ClassModel* find(const std::string& className) const;
	size_t size() const { return classes_.size(); }
	const HorizonZone* horizon(const std::string& className) const;
	const std::vector<ClassLight>* lights(const std::string& className) const;

private:
	void parseScript(const std::string& text, const std::string& modelHint);
	std::unordered_map<std::string, ClassModel> classes_;
	std::unordered_map<std::string, HorizonZone> horizons_;
	std::unordered_map<std::string, std::vector<ClassLight>> lights_;
	std::unordered_map<std::string, bool> viewInFog_; // class_viewinfog z ciał def_class
};

} // namespace ow
