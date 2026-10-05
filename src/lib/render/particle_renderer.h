// Rysowanie cząsteczek (docs/silnik/12_czasteczki.md): kwadraty zwrócone do kamery z komórki atlasu
// (Data/Texture/Misc/particle_*.tga), mieszanie addytywne, test głębi bez zapisu.
#pragma once

#include "lib/particles.h"
#include "lib/render/rhi/rhi.h"
#include "lib/tga.h"

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace ow
{

class TextureCache;

class ParticleRenderer
{
public:
	~ParticleRenderer();
	bool init(rhi::Device& dev, std::string& err, TextureCache& textures);
	bool ready() const { return pipe_ != rhi::kNull; }
	rhi::Handle texture(int tex) const { return tex >= 0 && tex < kParticleTextures ? tex_[tex] : rhi::kNull; }
	const Image& image(int tex) const { return img_[tex >= 0 && tex < kParticleTextures ? tex : 0]; }
	// Komórki atlasu tex przy podziale grid × grid, które nie są puste (kanał alfa).
	const std::vector<bool>& usableCells(int tex, int grid);
	// sprites: układ gry w blokach (y w dół); gameToClip: z jednostek modelu (blok·200) do przycięcia.
	void draw(const std::vector<ParticleSprite>& sprites, const ParticleType& type, const glm::mat4& gameToClip);

private:
	rhi::Device* dev_ = nullptr;
	rhi::Handle pipe_ = rhi::kNull;
	rhi::Handle tex_[kParticleTextures] = {};
	Image img_[kParticleTextures];
	std::vector<bool> usable_[kParticleTextures][3]; // grid 4, 8, 16
	std::vector<float> verts_;
};

} // namespace ow
