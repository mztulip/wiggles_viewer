// Rysowanie płynów (docs/silnik/13_plyny.md): każde pole z płynem to przednia ściana na głębokości
// 14,4 i, gdy powierzchnia jest odsłonięta, pasy powierzchni w głąb. Mieszanie alfa, bez zapisu głębi.
// Prawo z Zimennik/Diggles-Unity (MIT, Docs/WATER_PHASE1_RE.md §2).
#pragma once

#include "lib/blockdb.h"
#include "lib/rhi/rhi.h"

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace ow
{

class TextureCache;

class FluidRenderer
{
public:
	~FluidRenderer();
	bool init(rhi::Device& dev, std::string& err, TextureCache& textures);
	// type/fill: pole po polu (width·height, wiersz po wierszu), blocks: ID bloków z BlockDB.
	// visible(x, y) == false pomija pole (nieodkryty teren). Zwraca liczbę pól z płynem.
	int build(int width, int height, const std::vector<uint8_t>& type, const std::vector<float>& fill,
	    const std::vector<uint32_t>& blocks, const BlockDb& db, const std::vector<bool>* visible = nullptr);
	void clear() { verts_[0].clear(), verts_[1].clear(), verts_[2].clear(); }
	// gameToClip: z jednostek modelu (blok·200, układ gry) do przycięcia; time: przesuw tekstury.
	void draw(const glm::mat4& gameToClip, float time);

private:
	rhi::Device* dev_ = nullptr;
	rhi::Handle pipe_ = rhi::kNull;
	rhi::Handle tex_[3] = {}; // woda, siarka, lawa
	std::vector<float> verts_[3];
};

} // namespace ow
