// Format modeli .3db ("3DDB 1.0") z Wiggles/Diggles.
//
// Opis formatu: docs/formaty/3db.md. Wiedza pochodzi z projektu
// djunkles/Diggles3db (https://github.com/djunkles/Diggles3db) oraz z naszej
// weryfikacji na plikach gry (dekwantyzacja pozycji, bbox).
//
// Animacja jest wierzchołkowa: każda klatka to osobny Mesh, a Animation::frames
// zawiera indeksy meshy. Dane geometrii leżą we wspólnych pulach (pools),
// do których odwołują się MeshLink-i.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace ow
{

struct Vec2f
{
	float x, y;
};
struct Vec3f
{
	float x, y, z;
};

struct Material3db
{
	std::string name; // nazwa tekstury bez rozszerzenia
	std::string path; // oryginalna ścieżka z serwera twórców (nieużywana)
	uint32_t unknown = 0;
};

// Fragment klatki z jednym materiałem. Pola to indeksy do pul modelu.
struct MeshLink
{
	uint16_t material = 0;
	uint16_t flags = 0; // ✅ tryb rysowania: 0 niewidoczny, 1 zwykły, 2 addytywnie, 3 alfa, 5 otoczenie, 8 bez światła (3db.md)
	uint16_t triangles = 0;
	uint16_t texcoords = 0;
	uint16_t points = 0;
	uint16_t normals = 0; // pula indeksów normalnych
};

struct Mesh3db
{
	std::vector<MeshLink> links;
	Vec3f bboxMin{}, bboxMax{};
	// ✅ Punkty zaczepienia: 32 × u32, indeks rekordu w Model3db::linkRecords dla punktu 0..31 (głowa = 4,
	// prawa ręka = 0, lewa = 1, usta = 10); indeks poza zakresem = brak punktu (Diggles-Unity HAT_LINK_SYSTEM.md §4).
	std::array<uint8_t, 0x80> linkTable{};
	uint16_t shadowIndex = 0xFFFF; // ✅ indeks w Model3db::shadows albo 0xFFFF
	// ✅ Prostokąt cienia: 4 narożniki × vec3 (f32) na ziemi (y = 0); same zera, gdy klatka nie ma cienia.
	std::array<uint8_t, 0x30> shadowQuad{};
	uint16_t cmapIndex = 0xFFFF; // ✅ indeks w Model3db::cmaps albo 0xFFFF
};

struct Animation3db
{
	std::string name;
	std::vector<uint32_t> frames; // indeksy do Model3db::meshes
	uint16_t unknownFlags = 0; // ❓ najpewniej niezainicjowana pamięć eksportera
	float unknownF = 0;         // ❓ zwykle 0
	std::string sourceFile; // ścieżka do pliku .sex eksportera
	Vec3f move{};   // ✅ przesunięcie na klatkę (jednostki modelu, −y = do przodu)
	Vec3f rotate{}; // ✅ obrót na krok między klatkami (radiany)
};

struct Object3db
{
	std::string name;
	std::vector<uint32_t> animations; // indeksy do Model3db::animations
};

struct CMap3db
{
	uint16_t width = 0, height = 0;
	int16_t originX = 0, originY = 0; // ✅ lewy górny róg maski względem punktu obiektu, w kratkach 1/8 bloku
	// ✅ Maska zajętości 0/1 na ziemi: kolumna ↔ x, wiersz ↔ z (głębokość), kratka 25 jednostek.
	// 🔶 Rzut bryły klatki poszerzony o kratkę; kolizje (has_cmap).
	std::vector<uint8_t> pixels;
};

struct Model3db
{
	std::string version; // "3DDB 1.0"
	std::string name;
	std::vector<Material3db> materials;
	std::vector<Mesh3db> meshes;
	std::vector<Object3db> objects;
	std::vector<Animation3db> animations;
	// 🔶 Cień 32×32 na prostokącie shadowQuad (kolumna ↔ x, wiersz ↔ z): rzut bryły światłem pod 45° w +z.
	std::vector<std::array<uint8_t, 32 * 32>> shadows;
	std::vector<CMap3db> cmaps;
	// Rekordy punktów zaczepienia: 3 × u16 skwantyzowane xyz, 2 B dopełnienia, 3 × f32 kąty Eulera (radiany).
	std::vector<std::array<uint8_t, 20>> linkRecords;

	// Pule danych
	std::vector<std::vector<uint16_t>> triangles;            // indeksy, po 3 na trójkąt
	std::vector<std::vector<Vec2f>> texcoords;
	std::vector<std::vector<std::array<uint16_t, 3>>> points; // skwantyzowane pozycje
	// Indeks normalnej per wierzchołek do tablicy SPHERE.DAT (256 wektorów). ✅ Zweryfikowane wizualnie;
	// we wcześniejszych narzędziach społeczności pula nazywała się "brightness".
	std::vector<std::vector<uint8_t>> normals;

	// Dekwantyzacja pozycji (✅ zweryfikowana z bbox na plikach gry).
	static float dequant(uint16_t v) { return (float(v) - 32768.0f) * 0.25f; }
	static Vec3f dequant(const std::array<uint16_t, 3>& p) { return {dequant(p[0]), dequant(p[1]), dequant(p[2])}; }

	const Object3db* findObject(const std::string& name) const;
	// Punkt zaczepienia `link` (0..31) w klatce (meshu) `mesh`, w jednostkach modelu (jak wierzchołki).
	// x, y = dequant(u16); z = 2·dequant(u16) (gra skaluje z punktów podwójnie). false = brak punktu.
	bool linkOffset(uint32_t mesh, int link, Vec3f& out) const
	{
		if (mesh >= meshes.size() || link < 0 || link >= 32) return false;
		const auto& t = meshes[mesh].linkTable;
		uint32_t slot = uint32_t(t[size_t(link) * 4]) | uint32_t(t[size_t(link) * 4 + 1]) << 8 |
		    uint32_t(t[size_t(link) * 4 + 2]) << 16 | uint32_t(t[size_t(link) * 4 + 3]) << 24;
		if (slot >= linkRecords.size()) return false;
		const auto& r = linkRecords[slot];
		auto u = [&](int o) { return uint16_t(r[size_t(o)] | (r[size_t(o) + 1] << 8)); };
		out = {dequant(u(0)), dequant(u(2)), 2.f * dequant(u(4))};
		return true;
	}
};

// Czy dwie klatki (meshe) mają tę samą topologię: te same materiały i pule trójkątów
// w linkach oraz tę samą liczbę wierzchołków. Tylko takie klatki można interpolować.
bool sameTopology(const Model3db& m, uint32_t meshA, uint32_t meshB);

// Rzuca ParseError przy uszkodzonych danych lub nadmiarowych bajtach na końcu.
Model3db parseModel3db(const std::vector<uint8_t>& data);

} // namespace ow
