// Zakładka Cząsteczki: lista typów efektów (0..42), ich użycie w skryptach gry i podgląd emitera.
// Opis typów i symulacji: src/lib/particles.h, docs/silnik/12_czasteczki.md.
#pragma once

#include "lib/gfx/particle_renderer.h"
#include "lib/particles.h"
#include "lib/rhi/rhi.h"

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace ow {
class Vfs;
class TextureCache;
} // namespace ow

class ParticleView {
public:
	static constexpr float kBlock = 200.f; // jednostek modelu na blok

	void init(ow::rhi::Device& dev, const ow::Vfs* vfs, ow::TextureCache* textures);
	bool ui();                     // true: wycentruj kamerę
	void update(float dt);
	void step(int d);              // poprzedni/następny typ (strzałki)
	// gameToClip: z jednostek modelu w układzie gry (y w dół) do przycięcia.
	void render(const glm::mat4& gameToClip);
	glm::vec3 center() const { return glm::vec3(0.f, -1.5f * kBlock, 0.f); }
	float radius() const { return 2.5f * kBlock; }
	void select(int type);
	void prewarm(int frames) {
		for (int i = 0; i < frames; ++i) emitter_.step();
	}
	int selected() const { return sel_; }

private:
	void restart();
	void uiAtlas(const ow::ParticleType& t);

	ow::rhi::Device* dev_ = nullptr;
	ow::ParticleRenderer renderer_;
	std::string error_;
	std::vector<ow::ParticleUsage> usage_;
	std::vector<int> usageCount_;
	int sel_ = 3;
	int usageSel_ = -1;
	ow::ParticleSource src_;
	ow::ParticleEmitter emitter_;
	std::vector<ow::ParticleSprite> sprites_;
	float fps_ = 12.f, sizeScale_ = ow::kParticleSizeScale, acc_ = 0.f;
	bool playing_ = true, scrollToSel_ = false;
	char filter_[64] = "";
};
