#include "lib/particles.h"

#include "lib/i18n.h"
#include "lib/vfs.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace ow
{

namespace
{

// Stałe wspólnego modelu ruchu (Diggles-Unity, CpuParticleEmitter.cs; 🔶 dobrane na oko przez tamtych
// autorów: grawitacja z tabeli ×2,5, wypór 0,003 bloku/klatkę², tłumienie błądzenia emitera 0,9).
constexpr float kGravityScale = 2.5f;
constexpr float kBuoyancy = 0.003f;
constexpr float kDriftDamping = 0.9f;
constexpr float kDriftCouple = 0.03f;
constexpr float kSwayAmp = 0.03f;
constexpr float kGroundDrop = 3.f; // podłoże cząsteczek „ziemnych” 3 bloki pod emiterem (bez terenu, 🔶)
constexpr float kPi = 3.14159265f;

std::vector<ParticleType> buildTypes()
{
	std::vector<ParticleType> t(43);
	auto T = [&](int id, const char* name, int tex, int mode, int alpha, int grid) -> ParticleType& {
		ParticleType& p = t[size_t(id)];
		p.id = id;
		p.name = name;
		p.tex = tex;
		p.renderMode = mode;
		p.alpha = alpha;
		p.grid = grid;
		return p;
	};
	auto set3 = [](float* d, float x, float y, float z) { d[0] = x, d[1] = y, d[2] = z; };
	enum
	{
		A128d = 0,
		B128d = 1,
		C256d = 2,
		E256 = 3,
		D256d = 4,
		A256d = 5
	};
	const uint32_t gold = 0xFFFFE060, yellow = 0xFFFFFF00, olive = 0xFF7F753F, grey = 0x7F7F7F7F;
	using S = ParticleShape;
	using A = ParticleAtlas;
	using C = ParticleColour;
	using Z = ParticleSizeCurve;
	using N = ParticleCount;
	using M = ParticleMotion;

	{
		auto& p = T(0, N_("żar kowala (kula ognia)"), E256, 0x10, 100, 4);
		p.shape = S::Sphere, p.radius = 0.25f, set3(p.jitter, .02f, .02f, .02f), p.rows = 1, p.colour = C::Warm;
		p.size = 2.6f, p.sizeJitter = .2f, p.sizeCurve = Z::Grow, p.buoyancyFrames = 12;
	}
	{
		auto& p = T(1, N_("iskry i bryzgi przy ziemi"), A256d, 0x0f, 255, 8);
		p.motion = M::Ground, p.shape = S::Box, set3(p.box, .1f, .1f, .1f), set3(p.jitter, .06f, .06f, .06f), p.rows = 1;
		p.size = 2.5f, p.sizeCurve = Z::Grow, p.gravity = -.01f, p.ageCap = 100;
	}
	{
		auto& p = T(2, N_("ciepły żar"), A128d, 0x10, 180, 16);
		p.shape = S::Sphere, p.radius = .06f, set3(p.jitter, .004f, .006f, .004f), p.atlas = A::Flipbook, p.frames = {6, 22, 38, 54};
		p.flipSquare = true;
		p.colour = C::Warm, p.size = 2.f, p.sizeJitter = .15f, p.sizeCurve = Z::Grow, p.buoyancyFrames = 6;
	}
	{
		auto& p = T(3, N_("ogień kowala"), A128d, 0x10, 170, 16);
		p.shape = S::Sphere, p.radius = .12f, set3(p.jitter, .006f, .008f, .006f), p.atlas = A::Flipbook, p.frames = {7, 23, 39, 55};
		p.flipSquare = true;
		p.colour = C::Warm, p.size = 2.8f, p.sizeJitter = .15f, p.sizeCurve = Z::Grow, p.buoyancyFrames = 16;
	}
	{
		auto& p = T(4, N_("litera Z nad śpiącym"), B128d, 0x0f, 255, 8);
		p.count = N::Gated, p.gate = .02f, p.atlas = A::Fixed, p.fixedU = 0, p.fixedV = 4, p.size = 1.5f, p.sizeCurve = Z::Grow;
		p.buoyancyFrames = 8, p.sway = true, p.ageCap = 64;
	}
	{
		auto& p = T(5, N_("nieużywany (brak funkcji w grze)"), A128d, 0x10, 255, 16);
		p.motion = M::Inert;
	}
	{
		auto& p = T(6, N_("dym z wierzchołków modelu"), A128d, 0x0f, 255, 16);
		p.shape = S::MeshVerts, p.count = N::Jitter, set3(p.jitter, .04f, .04f, .04f), p.row0 = 14, p.rows = 2, p.col0 = 4, p.cols = 4;
		p.size = 2.5f, p.sizeCurve = Z::Grow, p.drag = .9f, p.buoyancyFrames = 4, p.wander = true;
	}
	{
		auto& p = T(7, N_("spadające krople"), D256d, 0x0f, 200, 4);
		p.shape = S::Box, set3(p.box, .1f, .1f, .1f), p.count = N::Gated, p.gate = .12f, set3(p.jitter, .03f, .02f, .03f);
		p.atlas = A::Fixed, p.fixedV = 2, p.colour = C::Constant, p.argb = grey, p.size = 4.5f, p.gravity = -.015f;
	}
	{
		auto& p = T(8, N_("ciemnoczerwone krople (ziemia)"), B128d, 0x0f, 255, 16);
		p.motion = M::Ground, set3(p.jitter, .03f, 0, .03f), p.rows = 2, p.size = .5f, p.sizeCurve = Z::Grow;
		p.colour = C::Constant, p.argb = 0xFFC03028, p.gravity = -.01f, p.ageCap = 95;
	}
	{
		auto& p = T(9, N_("mała kula ognia"), B128d, 0x0f, 255, 16);
		p.shape = S::Sphere, p.radius = .2f, p.count = N::Jitter, p.rows = 2, p.colour = C::Warm, p.size = 2.f, p.sizeJitter = .2f;
		p.sizeCurve = Z::Grow, p.drag = .9f, p.buoyancyFrames = 8, p.ageCap = 111;
	}
	{
		auto& p = T(10, N_("szare krople (odbicie od ziemi)"), B128d, 0x0f, 255, 16);
		p.motion = M::Ground, p.count = N::Single, set3(p.jitter, .06f, 0, .06f), p.row0 = 4, p.rows = 2, p.size = .4f;
		p.sizeCurve = Z::Grow, p.colour = C::Constant, p.argb = 0xFF9098A8, p.gravity = -.008f, p.ageCap = 95;
	}
	{
		auto& p = T(11, N_("chmura dymu"), A128d, 0x0f, 255, 16);
		p.shape = S::Disc, p.radius = .35f, p.count = N::Jitter, set3(p.jitter, .04f, .04f, .04f), p.row0 = 14, p.rows = 2, p.col0 = 4;
		p.cols = 4, p.colour = C::Grey, p.size = 2.f, p.sizeCurve = Z::Grow, p.drag = .95f, p.buoyancyFrames = 24, p.ageCap = 48;
	}
	{
		auto& p = T(12, N_("mały obłok dymu"), A128d, 0x0f, 255, 16);
		p.count = N::Jitter, p.row0 = 14, p.rows = 2, p.col0 = 4, p.cols = 4, p.colour = C::Grey, p.size = 1.f, p.sizeCurve = Z::Grow;
		p.ageCap = 64;
	}
	{
		auto& p = T(13, N_("magiczne iskierki w dół"), C256d, 0x10, 180, 4);
		p.shape = S::Sphere, p.radius = .4f, p.count = N::Jitter, p.colour = C::Constant, p.argb = 0xFFB0A8FF, p.size = 1.2f;
		p.sizeCurve = Z::Shrink, p.gravity = -.01f, p.ageCap = 48;
	}
	{
		auto& p = T(14, N_("wybuch promienisty"), C256d, 0x10, 70, 4);
		p.shape = S::Sphere, p.radius = .5f, set3(p.jitter, 0, .05f, 0), p.rows = 1, p.colour = C::Grey, p.size = 2.f;
		p.sizeCurve = Z::Grow, p.drag = .92f, p.ageCap = 64;
	}
	{
		auto& p = T(15, N_("żółte krople"), B128d, 0x0f, 200, 16);
		p.motion = M::Ground, p.shape = S::Disc, p.radius = .1f, p.row0 = 4, p.rows = 2, p.colour = C::Constant, p.argb = yellow;
		p.size = .6f, p.gravity = -.012f, p.ageCap = 95;
	}
	{
		auto& p = T(16, N_("ciepły stożek w górę"), C256d, 0x10, 100, 4);
		p.shape = S::Cone, p.radius = .2f, set3(p.jitter, .04f, .1f, .04f), p.atlas = A::Fixed, p.fixedU = 2, p.fixedV = 3;
		p.colour = C::Warm, p.argb = gold, p.size = .4f, p.sizeCurve = Z::Grow, p.buoyancyFrames = 8, p.ageCap = 47;
	}
	{
		auto& p = T(17, N_("wióry (stolarz)"), B128d, 0x0f, 255, 8);
		p.motion = M::Ground, p.shape = S::Cone, p.radius = .15f, p.row0 = 5, p.rows = 2, p.size = .5f, p.gravity = -.01f, p.ageCap = 100;
	}
	{
		auto& p = T(18, N_("iskry (kuźnia)"), B128d, 0x10, 170, 8);
		p.motion = M::Ground, set3(p.jitter, .02f, .02f, .02f), p.atlas = A::Fixed, p.fixedU = 5, p.fixedV = 3, p.colour = C::Warm;
		p.argb = yellow, p.size = .6f, p.gravity = -.01f;
	}
	{
		auto& p = T(19, N_("pył"), E256, 0x0f, 100, 4);
		p.shape = S::Box, set3(p.box, .1f, .1f, .1f), set3(p.jitter, .05f, .05f, .05f), p.row0 = 1, p.rows = 1, p.size = .8f;
		p.gravity = -.005f, p.ageCap = 128;
	}
	{
		auto& p = T(20, N_("brązowa chmura ziemi"), C256d, 0x0f, 100, 4);
		p.shape = S::Box, set3(p.box, .15f, .15f, .15f), p.count = N::Jitter, set3(p.jitter, .05f, .05f, .05f), p.rows = 1;
		p.colour = C::Constant, p.argb = olive, p.size = 1.7f, p.sizeCurve = Z::Shrink, p.drag = .92f, p.wander = true;
	}
	{
		auto& p = T(21, N_("mgiełka i bryzgi (kryształy)"), D256d, 0x0f, 170, 8);
		p.shape = S::Sphere, p.radius = .3f, p.rows = 2, p.size = 2.5f, p.sizeJitter = .3f, p.buoyancyFrames = 6, p.gravity = -.02f;
	}
	{
		auto& p = T(22, N_("duże szare płatki"), D256d, 0x0f, 200, 4);
		p.shape = S::Box, set3(p.box, .2f, .2f, .2f), p.count = N::Gated, p.gate = .25f, p.rows = 1, p.colour = C::Constant;
		p.argb = grey, p.size = 5.5f, p.gravity = -.015f;
	}
	{
		auto& p = T(23, N_("bąbelki z powierzchni"), D256d, 0x0f, 200, 8);
		p.shape = S::Box, set3(p.box, .4f, .02f, .4f), p.count = N::Gated, p.gate = .25f, p.atlas = A::Fixed, p.fixedV = 2;
		p.colour = C::Constant, p.argb = grey, p.size = 1.2f, p.sizeJitter = .5f, p.buoyancyFrames = 32;
	}
	{
		auto& p = T(24, N_("unoszące się płatki"), D256d, 0x0f, 170, 8);
		p.shape = S::Box, set3(p.box, .1f, .1f, .2f), set3(p.jitter, .05f, .05f, .05f), p.rows = 2, p.size = 2.5f, p.buoyancyFrames = 6;
	}
	{
		auto& p = T(25, N_("promień / lina"), B128d, 0x10, 220, 8);
		p.atlas = A::Fixed, p.fixedU = 6, p.fixedV = 4, p.size = 1.3f, p.sizeCurve = Z::Shrink, p.ageCap = 8;
	}
	{
		auto& p = T(26, N_("wióry (kamieniarz)"), B128d, 0x0f, 255, 8);
		p.motion = M::Ground, p.shape = S::Cone, p.radius = .1f, p.row0 = 5, p.rows = 2, p.size = .5f, p.gravity = -.01f, p.ageCap = 100;
	}
	{
		auto& p = T(27, N_("mała kula ognia (pochodnia)"), A128d, 0x10, 180, 16);
		p.shape = S::Sphere, p.radius = .2f, p.rows = 4, p.col0 = 4, p.cols = 4, p.colour = C::Warm, p.size = 2.f, p.sizeJitter = .2f;
		p.sizeCurve = Z::Grow, p.buoyancyFrames = 10;
	}
	{
		auto& p = T(28, N_("fioletowy dym"), E256, 0x10, 170, 4);
		p.shape = S::Sphere, p.radius = .3f, p.row0 = 1, p.rows = 1, p.colour = C::Constant, p.argb = 0xFF9866C8, p.size = 1.6f;
		p.sizeJitter = .3f, p.sizeCurve = Z::Grow, p.buoyancyFrames = 16;
	}
	{
		auto& p = T(29, N_("fioletowy dym (większy)"), E256, 0x10, 170, 4);
		p.shape = S::Sphere, p.radius = .4f, p.row0 = 1, p.rows = 1, p.colour = C::Constant, p.argb = 0xFF9866C8, p.size = 1.8f;
		p.sizeJitter = .3f, p.sizeCurve = Z::Grow, p.buoyancyFrames = 16;
	}
	{
		auto& p = T(30, N_("unoszący się szary dym"), E256, 0x10, 170, 4);
		p.shape = S::Disc, p.radius = .3f, set3(p.jitter, .05f, .05f, .05f), p.row0 = 1, p.rows = 1, p.colour = C::Grey, p.size = .5f;
		p.sizeCurve = Z::Grow, p.buoyancyFrames = 16, p.wander = true;
	}
	{
		auto& p = T(31, N_("nuta"), B128d, 0x0f, 255, 8);
		p.count = N::Gated, p.gate = .02f, p.atlas = A::Fixed, p.fixedU = 5, p.fixedV = 4, p.size = 1.5f, p.sizeCurve = Z::Grow;
		p.buoyancyFrames = 8, p.sway = true, p.ageCap = 64;
	}
	{
		auto& p = T(32, N_("pulsujący błysk"), C256d, 0x10, 200, 4);
		p.count = N::Gated, p.gate = .012f, p.rows = 1, p.colour = C::Warm, p.argb = gold, p.size = 5.f, p.sizeCurve = Z::Pulse;
		p.ageCap = 16;
	}
	{
		auto& p = T(33, N_("magiczne iskierki (elfy)"), C256d, 0x10, 180, 4);
		p.shape = S::Sphere, p.radius = .5f, p.count = N::Jitter, p.colour = C::Constant, p.argb = 0xFFB0A8FF, p.size = 1.6f;
		p.sizeJitter = .5f, p.sizeCurve = Z::Shrink, p.gravity = -.012f, p.ageCap = 48;
	}
	{
		auto& p = T(34, N_("kula ognia"), E256, 0x10, 170, 4);
		p.shape = S::Sphere, p.radius = .3f, p.rows = 1, p.colour = C::Warm, p.size = 3.f, p.sizeJitter = .2f, p.sizeCurve = Z::Grow;
		p.buoyancyFrames = 12;
	}
	{
		auto& p = T(35, N_("dym"), A128d, 0x0f, 200, 16);
		p.shape = S::Box, set3(p.box, .2f, .1f, .2f), p.count = N::Jitter, p.row0 = 14, p.rows = 2, p.col0 = 4, p.cols = 4;
		p.colour = C::Grey, p.size = 3.f, p.sizeJitter = .3f, p.sizeCurve = Z::Grow, p.drag = .92f, p.ageCap = 40;
	}
	auto poof = [&](int id, const char* name, std::vector<int> frames) {
		auto& p = T(id, name, A256d, 0x10, 100, 8);
		p.count = N::Gated, p.gate = .06f, p.atlas = A::Flipbook, p.frames = std::move(frames), p.size = 6.5f;
		p.sizeCurve = Z::Grow, p.ageCap = int(p.frames.size()) + 1;
	};
	poof(36, N_("obłok „puf” A"), {8, 9, 10, 11, 12, 13, 14, 15, 16});
	poof(37, N_("obłok „puf” B"), {17, 18, 19, 20, 21, 22, 23, 24, 25});
	poof(38, N_("obłok „puf” C"), {26, 27, 28, 29, 30, 31, 32, 33, 0});
	poof(39, N_("turkusowy pierścień energii"), {24, 25, 26, 27, 28, 29, 30, 31});
	poof(40, N_("pomarańczowy wybuch energii"), {16, 17, 18, 19, 20, 21, 22, 23});
	{
		auto& p = T(41, N_("białe błyski"), C256d, 0x10, 200, 4);
		p.shape = S::Sphere, p.radius = .6f, p.count = N::Gated, p.gate = .1f, p.rows = 1, p.colour = C::Constant, p.size = 2.f;
		p.sizeJitter = .3f, p.sizeCurve = Z::Pulse, p.ageCap = 12;
	}
	{
		auto& p = T(42, N_("rosnąca kula pyłu"), E256, 0x0f, 150, 4);
		p.shape = S::Sphere, p.radius = .3f, p.count = N::Jitter, p.row0 = 1, p.rows = 1, p.colour = C::Constant, p.argb = 0xFFA89880;
		p.size = 1.5f, p.sizeJitter = .3f, p.sizeCurve = Z::Grow, p.ageCap = 63;
	}
	return t;
}

void argbToRgba(uint32_t c, float* out)
{
	out[0] = float((c >> 16) & 255) / 255.f;
	out[1] = float((c >> 8) & 255) / 255.f;
	out[2] = float(c & 255) / 255.f;
	out[3] = float((c >> 24) & 255) / 255.f;
}

} // namespace

const std::vector<ParticleType>& particleTypes()
{
	static const std::vector<ParticleType> t = buildTypes();
	return t;
}

const ParticleType* particleType(int id)
{
	const auto& t = particleTypes();
	return id >= 0 && id < int(t.size()) ? &t[size_t(id)] : nullptr;
}

const char* particleTextureFile(int tex)
{
	static const char* kFiles[kParticleTextures] = {"particle_a_128d.tga", "particle_b_128d.tga", "particle_c_256d.tga",
	    "particle_e_256.tga", "particle_d_256d.tga", "particle_a_256d.tga"};
	return tex >= 0 && tex < kParticleTextures ? kFiles[tex] : kFiles[0];
}

// --- emiter ---

uint32_t ParticleEmitter::rnd()
{ // LCG jak w grze (x·0x343FD + 0x269EC3), próbkowany bitami 16+
	seed_ = seed_ * 0x343FDu + 0x269EC3u;
	return seed_;
}

void ParticleEmitter::configure(const ParticleType& type, const ParticleSource& src, uint32_t seed)
{
	type_ = &type;
	src_ = src;
	seed_ = seed ? seed : 0x2545F491u;
	// Układ gry -> symulacji: y w górę, z ze skali skryptu do bloków (·0,5).
	rel_[0] = src.rel[0], rel_[1] = -src.rel[1], rel_[2] = src.rel[2] * 0.5f;
	dir_[0] = src.dir[0], dir_[1] = -src.dir[1], dir_[2] = src.dir[2] * 0.5f;
	pool_.assign(size_t(std::clamp(src.maxParticles > 0 ? src.maxParticles : 256, 8, 4096)), P{});
	drift_[0] = drift_[1] = drift_[2] = 0;
	alive_ = 0;
	setUsableCells({});
}

void ParticleEmitter::setOrigin(float x, float y, float zWorld)
{
	origin_[0] = x + rel_[0];
	origin_[1] = -y + rel_[1];
	origin_[2] = zWorld + rel_[2];
}

void ParticleEmitter::setUsableCells(const std::vector<bool>& usable)
{
	cells_.clear();
	if (!type_) return;
	const ParticleType& t = *type_;
	int g = std::max(1, t.grid);
	int v0 = std::clamp(t.row0, 0, g - 1), v1 = std::min(g, v0 + (t.rows > 0 ? t.rows : g));
	int u0 = std::clamp(t.col0, 0, g - 1), u1 = std::min(g, u0 + (t.cols > 0 ? t.cols : g));
	for (int pass = 0; pass < 2 && cells_.empty(); ++pass)
		for (int v = v0; v < v1; ++v)
			for (int u = u0; u < u1; ++u)
			{
				int c = v * g + u;
				if (pass == 1 || usable.empty() || (size_t(c) < usable.size() && usable[size_t(c)])) cells_.push_back(c);
			}
}

void ParticleEmitter::spawn(P& p)
{
	const ParticleType& t = *type_;
	p.alive = true;
	p.age = 0;
	p.base = t.sizeJitter > 0 ? t.size * (1.f + (unit() * 2.f - 1.f) * t.sizeJitter) : t.size;
	p.size = p.base;
	float off[3] = {0, 0, 0};
	switch (t.shape)
	{
	case ParticleShape::Disc:
	case ParticleShape::Cone:
	{
		float a = float(mod(360)) * kPi / 180.f, r = t.radius * float(mod(100)) / 100.f;
		off[0] = std::cos(a) * r, off[1] = t.shape == ParticleShape::Disc ? float(mod(10)) / 200.f : 0.f, off[2] = std::sin(a) * r;
		break;
	}
	case ParticleShape::Box:
		off[0] = sym(8) * t.box[0], off[1] = sym(8) * t.box[1], off[2] = sym(8) * t.box[2];
		break;
	case ParticleShape::Sphere:
	case ParticleShape::MeshVerts:
	{ // bez siatki modelu: kula 0,3 bloku (🔶)
		float d[3] = {sym(8), sym(8), sym(8)};
		float l = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
		float r = (t.shape == ParticleShape::Sphere ? t.radius * float(mod(100)) / 100.f : 0.3f) / std::max(l, 1e-4f);
		for (int k = 0; k < 3; ++k) off[k] = d[k] * r;
		break;
	}
	default: break;
	}
	for (int k = 0; k < 3; ++k)
	{
		p.pos[k] = origin_[k] + off[k];
		p.vel[k] = dir_[k] + sym(800) * t.jitter[k] * 100.f;
	}
	if (t.motion == ParticleMotion::Ground)
	{ // rozprysk w dół i na boki
		p.vel[0] += sym(200), p.vel[1] -= 0.06f, p.vel[2] += sym(200);
	}
	if (t.atlas == ParticleAtlas::Fixed) p.cell = std::clamp(t.fixedV, 0, t.grid - 1) * t.grid + std::clamp(t.fixedU, 0, t.grid - 1);
	else if (t.atlas == ParticleAtlas::Flipbook && !t.frames.empty()) p.cell = t.frames[0];
	else p.cell = cells_.empty() ? 0 : cells_[size_t(mod(int(cells_.size())))];
	colourAt(p);
}

void ParticleEmitter::colourAt(P& p) const
{
	const ParticleType& t = *type_;
	float a = t.ageCap > 0 ? std::clamp(float(p.age) / float(t.ageCap), 0.f, 1.f) : 0.f;
	float cap = float(t.alpha) / 255.f;
	switch (t.colour)
	{
	case ParticleColour::Warm: // kolor daje tekstura (żółty→czerwony w atlasie); tu tylko wygaszanie
		p.rgba[0] = p.rgba[1] = 1.f, p.rgba[2] = .9f;
		p.rgba[3] = (1.f - a * a) * cap;
		break;
	case ParticleColour::Grey: // narasta przez 20% życia, potem wygasa
		p.rgba[0] = p.rgba[1] = p.rgba[2] = .85f;
		p.rgba[3] = (a < .2f ? a / .2f : (1.f - a) / .8f) * cap;
		break;
	case ParticleColour::Constant:
		argbToRgba(t.argb, p.rgba);
		p.rgba[3] *= (1.f - a) * cap;
		break;
	default:
		p.rgba[0] = p.rgba[1] = p.rgba[2] = 1.f;
		p.rgba[3] = (1.f - a) * cap;
	}
}

void ParticleEmitter::finish(P& p)
{
	const ParticleType& t = *type_;
	float a = t.ageCap > 0 ? std::clamp(float(p.age) / float(t.ageCap), 0.f, 1.f) : 0.f;
	if (t.atlas == ParticleAtlas::Flipbook && !t.frames.empty())
	{
		int n = int(t.frames.size());
		int i = t.flipSquare ? int(a * a * float(n)) : p.age;
		p.cell = t.frames[size_t(std::clamp(i, 0, n - 1))];
	}
	float m = 1.f;
	switch (t.sizeCurve)
	{
	case ParticleSizeCurve::Grow: m = .5f + .8f * a; break;
	case ParticleSizeCurve::Shrink: m = 1.f - .9f * a; break;
	case ParticleSizeCurve::Pulse: m = a < .4f ? .4f + 1.5f * a : 1.f - (a - .4f) / .6f; break;
	default: break;
	}
	p.size = p.base * m;
	colourAt(p);
	if (++p.age >= t.ageCap) p.alive = false;
}

void ParticleEmitter::step()
{
	if (!type_ || type_->motion == ParticleMotion::Inert) return;
	const ParticleType& t = *type_;
	// A) błądzenie emitera
	for (float& d : drift_) d = (d + (t.wander ? sym(800) : 0.f)) * kDriftDamping;
	// B) nowe cząsteczki
	if (active_)
	{
		int n = src_.perFrame;
		switch (t.count)
		{
		case ParticleCount::Jitter: n = std::max(0, n + mod(3) - 1); break;
		case ParticleCount::Single: n = 1; break;
		case ParticleCount::Gated: n = unit() < t.gate ? std::max(1, n) : 0; break;
		default: break;
		}
		for (auto& p : pool_)
		{
			if (n <= 0) break;
			if (p.alive) continue;
			spawn(p);
			--n;
		}
	}
	// C) ruch
	alive_ = 0;
	float floorY = origin_[1] - kGroundDrop;
	for (auto& p : pool_)
	{
		if (!p.alive) continue;
		for (int k = 0; k < 3; ++k) p.pos[k] += p.vel[k];
		p.vel[1] += t.gravity * kGravityScale;
		if (p.age < t.buoyancyFrames) p.vel[1] += kBuoyancy;
		if (t.drag < 1.f)
			for (float& v : p.vel) v *= t.drag;
		if (t.wander)
			for (int k = 0; k < 3; ++k) p.vel[k] += drift_[k] * kDriftCouple;
		if (t.sway) p.pos[0] += std::cos(float(p.age) * .25f) * kSwayAmp;
		if (t.motion == ParticleMotion::Ground && p.pos[1] < floorY && p.vel[1] < 0)
		{ // odbicie od podłoża
			p.pos[1] = floorY;
			p.vel[1] = -p.vel[1] * .3f;
			p.vel[0] *= .5f, p.vel[2] *= .5f;
		}
		finish(p);
		alive_ += p.alive;
	}
}

void ParticleEmitter::collect(std::vector<ParticleSprite>& out, float sizeScale) const
{
	for (const auto& p : pool_)
	{
		if (!p.alive) continue;
		ParticleSprite s;
		s.pos[0] = p.pos[0], s.pos[1] = -p.pos[1], s.pos[2] = p.pos[2];
		s.size = p.size * sizeScale;
		s.cell = p.cell;
		for (int k = 0; k < 4; ++k) s.rgba[k] = p.rgba[k];
		out.push_back(s);
	}
}

// --- skrypty ---

namespace
{

std::string flattenBraces(const std::string& s)
{
	std::string f;
	for (char c : s) f += (c == '{' || c == '}') ? ' ' : c;
	return f;
}

} // namespace

bool parseClassParticle(const std::string& args, ParticleSource& out)
{
	if (args.find('$') != std::string::npos || args.find('[') != std::string::npos) return false;
	std::istringstream in(flattenBraces(args));
	ParticleSource s;
	int fog = 0;
	if (!(in >> s.type >> s.rel[0] >> s.rel[1] >> s.rel[2] >> s.dir[0] >> s.dir[1] >> s.dir[2] >> s.maxParticles >> s.perFrame))
		return false;
	if (in >> fog) s.visibleInFog = fog != 0;
	out = s;
	return true;
}

std::vector<ParticleUsage> scanParticleUsage(const Vfs& vfs)
{
	std::vector<ParticleUsage> out;
	std::vector<std::string> dirs = {"Data/Scripts"};
	static const char* kCommands[] = {"class_particle", "change_particlesource", "create_particlesource"};
	while (!dirs.empty())
	{
		std::string d = dirs.back();
		dirs.pop_back();
		for (const auto& f : vfs.list(d))
		{
			std::string path = d + "/" + f;
			std::string low = Vfs::lower(f);
			if (low.size() < 4 || low.substr(low.size() - 4) != ".tcl")
			{
				if (low.find('.') == std::string::npos) dirs.push_back(path);
				continue;
			}
			auto data = vfs.read(path);
			if (!data) continue;
			std::istringstream in(std::string(data->begin(), data->end()));
			std::string line, owner;
			int n = 0;
			while (std::getline(in, line))
			{
				++n;
				size_t s = line.find_first_not_of(" \t");
				if (s == std::string::npos || line[s] == '#' || line.compare(s, 2, "//") == 0) continue;
				if (line.compare(s, 10, "def_class ") == 0)
				{
					std::istringstream ws(line.substr(s + 10));
					ws >> owner;
				}
				for (const char* cmd : kCommands)
				{
					size_t p = line.find(cmd);
					if (p == std::string::npos) continue;
					size_t e = p + std::strlen(cmd);
					if (e < line.size() && line[e] != ' ' && line[e] != '\t') continue;
					ParticleUsage u;
					u.file = path.substr(5); // bez "Data/"
					u.line = n;
					u.command = cmd;
					u.text = line.substr(s);
					while (!u.text.empty() && (u.text.back() == '\r' || u.text.back() == ' ')) u.text.pop_back();
					// Typ: class_particle <typ>; change_particlesource <obj> <slot> <typ>; create_particlesource <typ>.
					std::istringstream ws(line.substr(e));
					std::string a, b, c;
					ws >> a >> b >> c;
					std::string ty = u.command == "change_particlesource" ? c : a;
					char* endp = nullptr;
					long v = std::strtol(ty.c_str(), &endp, 10);
					if (!ty.empty() && endp && *endp == 0) u.type = int(v);
					if (u.command == "class_particle") u.owner = owner;
					out.push_back(u);
					break;
				}
			}
		}
	}
	std::sort(out.begin(), out.end(), [](const ParticleUsage& a, const ParticleUsage& b) {
		return a.file != b.file ? a.file < b.file : a.line < b.line;
	});
	return out;
}

} // namespace ow
