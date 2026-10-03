// Renderer modeli .3db: rysuje pojedynczą klatkę (Mesh3db) z teksturami.
//
// Bufory GPU tworzone są leniwie dla każdej rysowanej klatki i trzymane w cache.
// Interpolacja: pozycje i normalne dwóch klatek są mieszane w vertex shaderze.
// Działa tylko dla par klatek o tej samej topologii (te same pule trójkątów,
// ta sama liczba wierzchołków), co dotyczy ~99,5% par w danych gry. Dla
// pozostałych rysujemy klatkę A bez interpolacji. Oryginalny silnik też
// interpoluje (opcja AnimIpol / rejestr Performance\AnimInterpolation, domyślnie włączona).
// Bajt per wierzchołek to indeks normalnej do tablicy SPHERE.DAT (256 wektorów
// jednostkowych). Tryby LightingMode pozwalają porównać różne interpretacje.
#pragma once

#include "lib/gfx/shader.h"
#include "lib/model3db.h"

#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>

namespace ow {

class TextureCache;

enum class LightingMode : int {
	Unlit = 0,        // sama tekstura
	Brightness = 1,   // tekstura × bajt/255 (stara, błędna interpretacja; do porównań)
	SphereNormal = 2, // tekstura × oświetlenie z normalnej SPHERE.DAT[bajt]
	ShowNormal = 3,   // podgląd normalnej jako kolor
};

struct ModelDrawOptions {
	LightingMode lighting = LightingMode::SphereNormal;
	bool wireframe = false;
	bool alphaTest = true;
	glm::vec3 lightDir = glm::normalize(glm::vec3(-0.4f, 0.8f, 0.5f)); // w przestrzeni modelu po transformacji
	float ambient = 0.45f;
};

class ModelRenderer {
public:
	~ModelRenderer();
	bool init(std::string& err);

	// Ustawia model (nie przejmuje własności) i czyści cache GPU.
	void setModel(const Model3db* model, TextureCache* textures);
	// 256 znormalizowanych wektorów z Data/SPHERE.DAT (może być pusty: wtedy normalna = 0,1,0).
	void setSphere(const std::vector<Vec3f>& sphere);

	void drawFrame(uint32_t meshIndex, const glm::mat4& viewProj, const glm::mat4& model, const ModelDrawOptions& opt) {
		drawFrames(meshIndex, meshIndex, 0.f, viewProj, model, opt);
	}
	// Rysuje klatkę pośrednią między meshA i meshB (t w [0,1]).
	void drawFrames(uint32_t meshA, uint32_t meshB, float t, const glm::mat4& viewProj, const glm::mat4& model,
	                const ModelDrawOptions& opt);
	// Czy dwie klatki mają zgodną topologię (można je interpolować)?
	bool canInterpolate(uint32_t meshA, uint32_t meshB) const;

	size_t cachedFrames() const { return cache_.size(); }
	void clearCache();

private:
	struct GpuLink {
		GLuint vbo = 0, ebo = 0;
		GLsizei count = 0;
		uint16_t material = 0;
	};
	const std::vector<GpuLink>& frame(uint32_t meshIndex);

	void bindLink(const GpuLink& a, const GpuLink& b);

	ShaderProgram prog_;
	GLuint vao_ = 0; // jeden VAO, atrybuty przepinane przy każdym rysowaniu
	const Model3db* model_ = nullptr;
	TextureCache* textures_ = nullptr;
	std::vector<Vec3f> sphere_;
	std::unordered_map<uint32_t, std::vector<GpuLink>> cache_;
};

// Wczytuje SPHERE.DAT: 256 × vec3 f32. Pusty wektor przy błędzie.
std::vector<Vec3f> parseSphereDat(const std::vector<uint8_t>& data);

} // namespace ow
