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

#include "lib/gfx/game_lighting.h"
#include "lib/rhi/rhi.h"
#include "lib/model3db.h"
#include "lib/sphere.h"

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
	// Strefa horyzontu: model za skałą (z gry < 0 bloków) przechodzi w kolor rgb ∝ |z|/głębokość.
	// horizon.a = głębokość w blokach (0 = wyłączone); horizonZOffset dodaje się do z po uModel,
	// żeby otrzymać z gry w jednostkach modelu (przesunięcie kamery w macierzy świata).
	glm::vec4 horizon{0.f};
	float horizonZOffset = 0.f;
	// Oświetlenie jak w grze (game_lighting.h): gameLights != nullptr włącza (same lampy i mgłę klatki
	// wysyła uploadScene); gameModel = macierz obiektu w układzie gry (jednostki modelu),
	// base = jasność bazowa B w miejscu obiektu.
	const std::vector<GameLight>* gameLights = nullptr;
	glm::mat4 gameModel{1.f};
	float base = 128.f;
};

class ModelRenderer {
public:
	~ModelRenderer();
	bool init(rhi::Device& dev, std::string& err);

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
		rhi::Handle vbo = rhi::kNull, ebo = rhi::kNull;
		uint32_t count = 0;
		uint16_t material = 0;
	};
	const std::vector<GpuLink>& frame(uint32_t meshIndex);

	rhi::Device* dev_ = nullptr;
	rhi::Handle pipe_ = rhi::kNull, pipeWire_ = rhi::kNull; // wspólne dla wszystkich modeli na urządzeniu
	rhi::Handle sphereBuf_ = rhi::kNull;                     // stały bufor 256 × vec4 (wiązanie 2)
	const Model3db* model_ = nullptr;
	TextureCache* textures_ = nullptr;
	std::vector<Vec3f> sphere_;
	std::unordered_map<uint32_t, std::vector<GpuLink>> cache_;
};

} // namespace ow
