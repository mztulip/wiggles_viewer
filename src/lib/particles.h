// Efekty cząsteczkowe: typy (zestawy 0..42), źródła z komend Tcl i symulacja emitera.
//
// Opis: docs/silnik/12_czasteczki.md. Wiedza o typach (tekstura atlasu, siatka komórek, jasność,
// kształt rozrzutu, czas życia, ruch, kolor) pochodzi z Zimennik/Diggles-Unity (MIT,
// https://github.com/Zimennik/Diggles-Unity: Docs/PARTICLE_SYSTEM_REWRITE.md §4,
// Runtime/Engine/Particles/ParticleTypeTable.cs, CpuParticleEmitter.cs). Kod jest nasz.
// 🔶 Status: wspólny model ruchu („szablon” z Diggles-Unity) zamiast 42 osobnych funkcji gry; część
// wartości dobrali tamci autorzy na oko. Do sprawdzenia zrzutami w laboratorium.
//
// Układ symulacji: bloki, x w prawo, y W GÓRĘ (odwrotnie niż w grze), z = głębokość świata
// (7..14,5 = teren, rośnie ku widzowi). Wejście z Tcl (relpos, dir) jest w układzie gry: y w dół,
// z w skali skryptu (·0,5), więc przeliczamy je przy konfiguracji (toSim).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ow
{

class Vfs;

enum class ParticleShape
{
	Point,
	Box,
	Disc,
	Cone,
	Sphere,
	MeshVerts
};
enum class ParticleAtlas
{
	Fixed,
	Random,
	Flipbook
};
enum class ParticleColour
{
	White,
	Constant,
	Warm,
	Grey
};
enum class ParticleSizeCurve
{
	Constant,
	Grow,
	Shrink,
	Pulse
};
enum class ParticleCount
{
	Fixed,
	Jitter,
	Single,
	Gated
};
enum class ParticleMotion
{
	Standard,
	Ground,
	Swirl,
	Spiral,
	Flock,
	Inert
};

struct ParticleType
{
	int id = 0;
	const char* name = "";  // opis po polsku (tłumaczony tr() w UI)
	int tex = 0;            // atlas 0..5 (particleTextureFile)
	int renderMode = 0x0f;  // 0x0f / 0x10 (❓ różnica; oba addytywne)
	int alpha = 255;        // jasność materiału 70..255
	int grid = 4;           // atlas dzielony na grid × grid komórek
	ParticleMotion motion = ParticleMotion::Standard;
	ParticleShape shape = ParticleShape::Point;
	float radius = 0;            // Disc, Sphere, Cone (bloki)
	float box[3] = {0, 0, 0};    // Box: półwymiary (bloki)
	ParticleCount count = ParticleCount::Fixed;
	float gate = 1;              // Gated: szansa na klatkę
	float jitter[3] = {0, 0, 0}; // rozrzut prędkości (bloki/klatkę)
	ParticleAtlas atlas = ParticleAtlas::Random;
	int fixedU = 0, fixedV = 0;
	int row0 = 0, rows = -1, col0 = 0, cols = -1; // pasmo komórek dla Random (−1 = całość)
	std::vector<int> frames;     // Flipbook: indeksy komórek (v·grid + u) kolejnych klatek wieku
	bool flipSquare = false;     // Flipbook rozłożony na całe życie wg (wiek/życie)² (ogień: długo żółty, czerwony pod koniec)
	ParticleColour colour = ParticleColour::White;
	uint32_t argb = 0xFFFFFFFF;  // Constant
	float size = 1, sizeJitter = 0; // rozmiar startowy (× kParticleSizeScale = bok w blokach, 🔶)
	ParticleSizeCurve sizeCurve = ParticleSizeCurve::Constant;
	bool sway = false;           // kołysanie w poziomie (Z śpiącego, nuta)
	float gravity = 0;           // zmiana prędkości w górę na klatkę (ujemna = opada)
	float drag = 1;              // mnożnik prędkości na klatkę
	int buoyancyFrames = 0;      // przez tyle klatek cząsteczka unosi się
	bool wander = false;         // błądzenie emitera przenoszone na cząsteczki
	int ageCap = 32;             // czas życia w klatkach
};

const std::vector<ParticleType>& particleTypes(); // indeks = id (0..42)
const ParticleType* particleType(int id);
const char* particleTextureFile(int tex);         // np. "particle_a_128d.tga" (Data/Texture/Misc)
constexpr int kParticleTextures = 6;
// Rozmiar typu × skala = bok cząsteczki w blokach. Diggles-Unity przyjmuje 0,25; zrzut gry (kielichy
// ognia w kris_unq_060, docs/silnik/12) daje płomień ok. 0,27 bloku szerokości, czyli skalę ok. 0,12 (🔶).
constexpr float kParticleSizeScale = 0.12f;

// Źródło cząsteczek z Tcl: class_particle / change_particlesource / create_particlesource.
// Wartości w układzie gry (relpos: przesunięcie od obiektu w blokach, y w dół, z w skali skryptu).
struct ParticleSource
{
	int type = 0;
	float rel[3] = {0, 0, 0};
	float dir[3] = {0, 0, 0};    // prędkość początkowa (bloki na klatkę)
	int maxParticles = 64;
	int perFrame = 1;
	bool visibleInFog = false;
};

// Jedna cząsteczka gotowa do narysowania (układ gry, bloki: x, y w dół, z świata).
struct ParticleSprite
{
	float pos[3];
	float size;       // bok kwadratu w blokach
	int cell;         // komórka atlasu v·grid + u
	float rgba[4];    // kolor × jasność; alfa wygasza
};

class ParticleEmitter
{
public:
	void configure(const ParticleType& type, const ParticleSource& src, uint32_t seed = 1);
	// Punkt zaczepienia (obiekt) w układzie gry: x, y (bloki, y w dół), z świata (bloki).
	void setOrigin(float x, float y, float zWorld);
	void setActive(bool a) { active_ = a; }
	// cellUsable: komórki atlasu, które nie są puste (z kanału alfa tekstury); puste = wszystkie.
	void setUsableCells(const std::vector<bool>& cellUsable);
	void step();                  // jedna klatka symulacji
	void collect(std::vector<ParticleSprite>& out, float sizeScale) const;
	int alive() const { return alive_; }
	const ParticleType* type() const { return type_; }
	const ParticleSource& source() const { return src_; }

private:
	struct P
	{
		float pos[3], vel[3], base, size;
		int age, cell;
		float rgba[4];
		bool alive;
	};
	uint32_t rnd();
	float sym(float norm) { return float(int((rnd() >> 16) & 0xF) - 8) / norm; }
	float unit() { return float((rnd() >> 16) & 0x7FFF) / 32767.f; }
	int mod(int m) { return m <= 0 ? 0 : int((rnd() >> 16) & 0x7FFF) % m; }
	void spawn(P& p);
	void finish(P& p);
	void colourAt(P& p) const;
	const ParticleType* type_ = nullptr;
	ParticleSource src_;
	float origin_[3] = {0, 0, 0}; // układ symulacji (y w górę)
	float rel_[3] = {0, 0, 0}, dir_[3] = {0, 0, 0}, drift_[3] = {0, 0, 0};
	std::vector<P> pool_;
	std::vector<int> cells_;
	uint32_t seed_ = 1;
	bool active_ = true;
	int alive_ = 0;
};

// Użycie typu w skryptach gry (do listy w przeglądarce).
struct ParticleUsage
{
	int type = -1;          // −1: typ podany zmienną
	std::string file;       // ścieżka skryptu względem Data/
	int line = 0;
	std::string command;    // class_particle / change_particlesource / create_particlesource
	std::string owner;      // klasa (def_class) dla class_particle
	std::string text;       // linia skryptu
};
std::vector<ParticleUsage> scanParticleUsage(const Vfs& vfs);
// Pola class_particle: typ {relpos} {dir} max naKlatkę widocznyWMgle … (liczby; false = zmienne).
bool parseClassParticle(const std::string& args, ParticleSource& out);

} // namespace ow
