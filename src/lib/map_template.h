// Szablony map (Data/Templates): .PMP (siatka bloków), .l2m (warstwa 2) i skrypt .tcl
// z procedurą MapTemplateSet, który nakłada teren i ustawia obiekty dekoracyjne.
// Opis formatów: docs/formaty/szablony.md, składanie świata: docs/silnik/03_swiat_z_szablonow.md.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ow {

// .PMP: siatka w × h bloków. Struktura pewna, znaczenie części pól jeszcze nie (🔶).
struct Pmp {
	uint32_t width = 0, height = 0;
	std::vector<uint8_t> types;     // w*h, 🔶 rodzaj pola (02 korytarz, 04 jaskinia, …)
	std::vector<uint32_t> blocks;   // w*h, ID bloku w BlockDB (0..118687)
	std::vector<uint16_t> mask;     // 🔶 maska kodowana przebiegami (format nieznany)
	std::vector<uint8_t> layer2;    // w*h, 🔶 wartości 0..8
	uint8_t type(int x, int y) const { return types[size_t(y) * width + size_t(x)]; }
	uint32_t block(int x, int y) const { return blocks[size_t(y) * width + size_t(x)]; }
};

// .l2m ("M2L\0"): druga warstwa, u16 na blok. 🔶 Znaczenie wartości nieznane. Plik może być pusty.
struct L2m {
	uint32_t width = 0, height = 0;
	std::vector<uint16_t> values;
};

// Obiekt utworzony przez szablon: new <klasa> "<nazwa>" "<x y z>" "<rx ry rz>".
// Pozycja w blokach, względem lewego górnego rogu szablonu (bez xoffset/yoffset).
struct TemplateObject {
	std::string className;
	std::string name;
	float pos[3] = {0, 0, 0};
	float rot[3] = {0, 0, 0}; // radiany
	std::string detail;       // loading_objdetail: high, medium, low, off (pusty = brak)
	bool physic = false;      // set_physic
	std::string info;         // call_method … Editor_Set_Info {…}: parametry obiektu z edytora, np. {volume 1000} {vpf 5}
	int owner = -1;           // z zapisu: gracz 0..7, −1 = świat
	int linkParent = -1;      // z zapisu: ID obiektu-rodzica przypięcia (nazwa obiektu = ID)
	int linkPoint = 0;
};

struct TemplateScript {
	std::string pmpFile; // jak w skrypcie, np. "gng_021.pmp"
	std::string l2mFile; // np. "urw_gng_021_a.l2m"
	std::vector<TemplateObject> objects;
};

// Rzucają ParseError przy uszkodzonych danych.
Pmp parsePmp(const std::vector<uint8_t>& data);
L2m parseL2m(const std::vector<uint8_t>& data);
// Parser dopasowany do skryptów szablonów gry (nie jest to ogólny interpreter Tcl).
TemplateScript parseTemplateScript(const std::string& text);

} // namespace ow
