// Renderer modeli .3db: rysuje pojedynczą klatkę (Mesh3db) z teksturami.
//
// Bufory GPU tworzone są leniwie dla każdej rysowanej klatki i trzymane w cache.
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

	void drawFrame(uint32_t meshIndex, const glm::mat4& viewProj, const glm::mat4& model, const ModelDrawOptions& opt);

	size_t cachedFrames() const { return cache_.size(); }
	void clearCache();

private:
	struct GpuLink {
		GLuint vao = 0, vbo = 0, ebo = 0;
		GLsizei count = 0;
		uint16_t material = 0;
	};
	const std::vector<GpuLink>& frame(uint32_t meshIndex);

	ShaderProgram prog_;
	const Model3db* model_ = nullptr;
	TextureCache* textures_ = nullptr;
	std::vector<Vec3f> sphere_;
	std::unordered_map<uint32_t, std::vector<GpuLink>> cache_;
};

// Wczytuje SPHERE.DAT: 256 × vec3 f32. Pusty wektor przy błędzie.
std::vector<Vec3f> parseSphereDat(const std::vector<uint8_t>& data);

} // namespace ow
