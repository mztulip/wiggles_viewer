// Cienie obiektów od lamp jak w grze (docs/silnik/18_cienie.md): obraz 32×32 z .3db na prostokącie shadowQuad
// klatki, leżący na ziemi, pochylony w stronę od lampy i przyciemniający podłogę mnożeniem.
#pragma once

#include "lib/model3db.h"
#include "lib/render/game_lighting.h"
#include "lib/render/rhi/rhi.h"

#include <algorithm>
#include <glm/glm.hpp>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace ow
{

class TextureCache;

// Stałe prawa (🔶 zmierzone w działającej grze, BAZA_WIEDZY §9.11).
constexpr float kShadowLightDist = 8.f;   // Performance\MaxShadowLightDist (rejestr, domyślnie 8), w płaszczyźnie x/y
constexpr int kShadowLightsPerObject = 2; // 🔶 najwyżej dwa cienie na obiekt
constexpr float kShadowStrength = 0.4f;   // podłoga · (1 − 0,4 · obraz/255)

// Pochylenie cienia od lampy w odległości d (bloki, płaszczyzna x/y): przesunięcie w x na jednostkę głębokości z.
inline float shadowShear(float d)
{
	return std::max(0.7f, 0.285f * d);
}

class ShadowRenderer
{
public:
	~ShadowRenderer();
	bool init(rhi::Device& dev, std::string& err, TextureCache& textures);
	bool ready() const { return pipe_ != rhi::kNull; }

	// Obiekt do cieni klatki: model, mesh klatki, macierz obiektu w układzie gry (jednostki modelu),
	// pozycja (x, y) w blokach.
	struct Caster
	{
		const Model3db* model;
		uint32_t mesh;
		glm::mat4 gameModel;
		float x, y;
	};
	// Lampy w blokach jak w GameLight; gameToClip z jednostek modelu do przycięcia.
	void draw(const std::vector<Caster>& casters, const std::vector<GameLight>& lights, const glm::mat4& gameToClip);

private:
	rhi::Handle shadowTexture(const Model3db& m, uint16_t index);

	rhi::Device* dev_ = nullptr;
	TextureCache* textures_ = nullptr;
	rhi::Handle pipe_ = rhi::kNull;
	std::map<std::pair<const Model3db*, uint16_t>, rhi::Handle> tex_;
	std::vector<float> verts_;
};

} // namespace ow
