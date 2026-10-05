// Symulacja płynów: przepływ między polami 10 razy na sekundę (docs/silnik/13_plyny.md §13.5).
//
// Prawo za Zimennik/Diggles-Unity (MIT, Docs/WATER_PHASE1_RE.md §4, FLUID_SYSTEM_RE.md „Flow contract”):
// ciśnienie z wysokości słupa w ósmych częściach pola, przepuszczalność z deskryptora BlockDB pola
// „dolnego/prawego”, transfer T = ΔP · 0,125² · A · relaksacja + praca · pęd, przycięty do zawartości.
// 🔶 Uproszczenia: pełny przebieg po prostokącie z płynem zamiast rzadkiej listy pracy, praca kierunkowa
// = ostatni transfer bez starzenia, brak zatyczek (Wasserstopper) i opcji -height/-ventil.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ow
{

class BlockDb;

struct FluidSource
{
	int x = 0, y = 0;     // pole źródła (obcięte ku zeru: obiekt + {0 −1 0})
	uint8_t type = 1;     // 1 woda, 2 siarka, 3 lawa
	float perTick = 0;    // vpf · 128 (jednostki objętości na takt)
	float budget = -1;    // volume · 128; −1 = bez końca
	float delay = 1.f;    // gra uruchamia źródło sekundę po utworzeniu obiektu
};

// Źródło z obiektu szablonu: klasy Wasser / Sulfur / Lava i Editor_Set_Info {volume V} {vpf R}.
bool fluidSourceFromObject(const std::string& className, const std::string& info, const float pos[3], FluidSource& out);

class FluidSim
{
public:
	// blocks: ID bloków pola po polu (np. Pmp::blocks); muszą żyć dłużej niż symulacja.
	void init(int width, int height, const std::vector<uint32_t>* blocks, const BlockDb* db);
	void setState(const std::vector<uint8_t>& type, const std::vector<float>& fill);
	void addSource(const FluidSource& s) { sources_.push_back(s); }
	void update(float dt); // takty po 0,1 s
	void tick();
	const std::vector<uint8_t>& type() const { return type_; }
	const std::vector<float>& fill() const { return fill_; }
	bool changed()
	{
		bool c = changed_;
		changed_ = false;
		return c;
	}
	bool empty() const { return sources_.empty() && !any_; }

private:
	void transfer(int n, int c, bool vertical);
	int w_ = 0, h_ = 0;
	const std::vector<uint32_t>* blocks_ = nullptr;
	const BlockDb* db_ = nullptr;
	std::vector<uint8_t> type_;
	std::vector<float> fill_, snap_, workX_, workY_;
	std::vector<FluidSource> sources_;
	float acc_ = 0.f;
	bool changed_ = false, any_ = false;
};

} // namespace ow
