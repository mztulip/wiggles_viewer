#include "lib/fluid_sim.h"

#include "lib/blockdb.h"
#include "lib/vfs.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace ow
{

namespace
{

struct Medium
{
	float relaxation, momentum, minCapacity, minAperture;
};
// woda / lawa z wodą, siarka, lawa (WATER_PHASE1_RE.md §4, tabela współczynników)
Medium medium(uint8_t type)
{
	if (type == 2) return {1.4f, 0.3f, 32.f, 8.f};
	if (type == 3) return {1.3f, 0.125f, 128.f, 16.f};
	return {1.5f, 0.5f, 32.f, 8.f};
}

float pressure(float h)
{
	return h <= 8.f ? h : (h - 8.f) * 8.f + 8.f;
}

} // namespace

bool fluidSourceFromObject(const std::string& className, const std::string& info, const float pos[3], FluidSource& out)
{
	std::string c = Vfs::lower(className);
	uint8_t type = c == "wasser" ? 1 : c == "sulfur" ? 2
	    : c == "lava"                                ? 3
	                                                 : 0;
	if (!type) return false;
	// {volume 1000} {vpf 5}: pary w nawiasach klamrowych
	float volume = -1, vpf = 0;
	std::string flat;
	for (char ch : info) flat += (ch == '{' || ch == '}') ? ' ' : ch;
	std::istringstream in(flat);
	std::string k, v;
	while (in >> k >> v)
	{
		if (k == "volume") volume = std::strtof(v.c_str(), nullptr);
		else if (k == "vpf") vpf = std::strtof(v.c_str(), nullptr);
	}
	if (vpf <= 0) return false;
	out.type = type;
	out.x = int(pos[0]);         // rzut C (obcięcie), jak w grze
	out.y = int(pos[1] - 1.f);   // -pos {0 -1 0}
	out.perTick = vpf * 128.f;
	out.budget = volume > 0 ? volume * 128.f : -1.f;
	return true;
}

void FluidSim::init(int width, int height, const std::vector<uint32_t>* blocks, const BlockDb* db)
{
	w_ = width, h_ = height;
	blocks_ = blocks, db_ = db;
	size_t n = size_t(w_) * h_;
	type_.assign(n, 0);
	fill_.assign(n, 0.f);
	workX_.assign(n, 0.f);
	workY_.assign(n, 0.f);
	sources_.clear();
	acc_ = 0.f;
	any_ = false;
}

void FluidSim::setState(const std::vector<uint8_t>& type, const std::vector<float>& fill)
{
	if (type.size() != type_.size() || fill.size() != fill_.size()) return;
	type_ = type, fill_ = fill;
	any_ = std::any_of(fill_.begin(), fill_.end(), [](float f) { return f > 0.001f; });
}

void FluidSim::update(float dt)
{
	if (empty()) return;
	acc_ += dt;
	int n = 0;
	while (acc_ >= 0.1f && n++ < 3)
	{
		acc_ -= 0.1f;
		tick();
	}
	if (n > 3) acc_ = 0.f;
}

// Transfer z sąsiada n (pole wyżej albo z lewej) do bieżącego c. Deskryptor należy do c.
void FluidSim::transfer(int n, int c, bool vertical)
{
	uint8_t t = type_[size_t(n)] ? type_[size_t(n)] : type_[size_t(c)];
	if (!t) return;
	FluidShape dn = db_->fluid((*blocks_)[size_t(n)]), dc = db_->fluid((*blocks_)[size_t(c)]);
	if (!dn.valid() || !dc.valid() || dn.capacity == 0 || dc.capacity == 0) return;
	float A = float(vertical ? dc.vertical : dc.horizontal);
	if (A <= 0) return;
	Medium m = medium(t);
	A = std::max(A, m.minAperture);
	float capN = std::max(float(dn.capacity), m.minCapacity), capC = std::max(float(dc.capacity), m.minCapacity);
	float hn = float(dn.upper - dn.lower) * snap_[size_t(n)] + float(8 + (vertical ? 8 : 0) - dn.upper);
	float hc = float(dc.upper - dc.lower) * snap_[size_t(c)] + float(8 - dc.upper);
	float& work = vertical ? workY_[size_t(c)] : workX_[size_t(c)];
	float T = (pressure(hn) - pressure(hc)) * 0.125f * 0.125f * A * m.relaxation + work * m.momentum;
	float an = fill_[size_t(n)] * capN, ac = fill_[size_t(c)] * capC;
	T = std::clamp(T, -ac, an);
	work = std::abs(T) > 0.004f ? T : 0.f;
	if (T == 0.f) return;
	an -= T, ac += T;
	if (T > 0 && !type_[size_t(c)]) type_[size_t(c)] = type_[size_t(n)];
	if (T < 0 && !type_[size_t(n)]) type_[size_t(n)] = type_[size_t(c)];
	fill_[size_t(n)] = an / capN < 0.001f ? 0.f : an / capN;
	fill_[size_t(c)] = ac / capC < 0.001f ? 0.f : ac / capC;
	changed_ = true;
}

void FluidSim::tick()
{
	if (!db_ || !blocks_ || w_ == 0) return;
	// Źródła: dolewają vpf·128 do swojego pola, do wyczerpania objętości.
	for (auto& s : sources_)
	{
		if (s.delay > 0)
		{
			s.delay -= 0.1f;
			continue;
		}
		if (s.x < 0 || s.y < 0 || s.x >= w_ || s.y >= h_ || s.budget == 0) continue;
		size_t i = size_t(s.y) * w_ + s.x;
		FluidShape d = db_->fluid((*blocks_)[i]);
		if (!d.valid() || d.capacity == 0) continue;
		float amount = s.budget < 0 ? s.perTick : std::min(s.perTick, s.budget);
		if (s.budget > 0) s.budget -= amount;
		float cap = std::max(float(d.capacity), medium(s.type).minCapacity);
		fill_[i] += amount / cap;
		if (!type_[i]) type_[i] = s.type;
		workY_[i] = std::max(workY_[i], 0.05f);
		changed_ = any_ = true;
	}
	if (!any_) return;
	// Prostokąt z płynem (+1 pole), migawka wypełnień, jeden przebieg w kolejności wierszy.
	int x0 = w_, y0 = h_, x1 = -1, y1 = -1;
	for (int y = 0; y < h_; ++y)
		for (int x = 0; x < w_; ++x)
			if (fill_[size_t(y) * w_ + x] > 0.f) x0 = std::min(x0, x), x1 = std::max(x1, x), y0 = std::min(y0, y), y1 = std::max(y1, y);
	if (x1 < 0)
	{
		any_ = false;
		return;
	}
	x0 = std::max(0, x0 - 1), y0 = std::max(0, y0 - 1), x1 = std::min(w_ - 1, x1 + 1), y1 = std::min(h_ - 1, y1 + 1);
	snap_ = fill_;
	for (int y = y0; y <= y1; ++y)
		for (int x = x0; x <= x1; ++x)
		{
			int c = y * w_ + x;
			if (y > 0 && (fill_[size_t(c - w_)] > 0.f || fill_[size_t(c)] > 0.f)) transfer(c - w_, c, true);
			if (x > 0 && (fill_[size_t(c - 1)] > 0.f || fill_[size_t(c)] > 0.f)) transfer(c - 1, c, false);
		}
}

} // namespace ow
