// Format modeli .3db ("3DDB 1.0") z Wiggles/Diggles.
//
// Opis formatu: docs/BAZA_WIEDZY.md §1. Wiedza pochodzi z projektu
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

namespace ow {

struct Vec2f { float x, y; };
struct Vec3f { float x, y, z; };

struct Material3db {
	std::string name; // nazwa tekstury bez rozszerzenia
	std::string path; // oryginalna ścieżka z serwera twórców (nieużywana)
	uint32_t unknown = 0;
};

// Fragment klatki z jednym materiałem. Pola to indeksy do pul modelu.
struct MeshLink {
	uint16_t material = 0;
	uint16_t flags = 0; // 🔶 zwykle 1; także 2, 3, 5, 6
	uint16_t triangles = 0;
	uint16_t texcoords = 0;
	uint16_t points = 0;
	uint16_t normals = 0; // pula indeksów normalnych
};

struct Mesh3db {
	std::vector<MeshLink> links;
	Vec3f bboxMin{}, bboxMax{};
	std::array<uint8_t, 0x80> unknown80{}; // 🔶 32 × u32
	uint16_t shadowIndex = 0xFFFF; // ✅ indeks w Model3db::shadows albo 0xFFFF
	std::array<uint8_t, 0x30> unknown30{}; // 🔶 12 × f32
	uint16_t cmapIndex = 0xFFFF; // ✅ indeks w Model3db::cmaps albo 0xFFFF
};

struct Animation3db {
	std::string name;
	std::vector<uint32_t> frames; // indeksy do Model3db::meshes
	uint16_t unknownFlags = 0;
	float unknownF = 0;
	std::string sourceFile; // ścieżka do pliku .sex eksportera
	Vec3f move{};   // 🔶 ruch korzenia na cykl
	Vec3f rotate{}; // 🔶 obrót na cykl (radiany)
};

struct Object3db {
	std::string name;
	std::vector<uint32_t> animations; // indeksy do Model3db::animations
};

struct CMap3db {
	uint16_t width = 0, height = 0, unknown1 = 0, unknown2 = 0;
	std::vector<uint8_t> pixels;
};

struct Model3db {
	std::string version; // "3DDB 1.0"
	std::string name;
	std::vector<Material3db> materials;
	std::vector<Mesh3db> meshes;
	std::vector<Object3db> objects;
	std::vector<Animation3db> animations;
	std::vector<std::array<uint8_t, 32 * 32>> shadows;
	std::vector<CMap3db> cmaps;
	std::vector<std::array<uint8_t, 20>> unknown20;

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
};

// Czy dwie klatki (meshe) mają tę samą topologię: te same materiały i pule trójkątów
// w linkach oraz tę samą liczbę wierzchołków. Tylko takie klatki można interpolować.
bool sameTopology(const Model3db& m, uint32_t meshA, uint32_t meshB);

// Rzuca ParseError przy uszkodzonych danych lub nadmiarowych bajtach na końcu.
Model3db parseModel3db(const std::vector<uint8_t>& data);

} // namespace ow
