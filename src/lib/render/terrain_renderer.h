// Renderer terenu: siatka z bloków BlockDB (TerrainMesh), podkład z gruntów
// Data/Ground/Gnd_<A|B|C><01..15>.tga mieszanych wagami z typu pola .PMP, mapowanie
// trójpłaszczyznowe (tekstura co blok). Druga warstwa (.l2m) dokłada nakładki detali
// Data/Ground/detail/<świat>[2]_<TL TR BL BR>.tga (mech, kryształy…) w trybie modulate×2.
//
// 🔶 Gra mapuje tekstury współrzędnymi u, v z palety BlockDB i robi przejścia maskami
// Data/Ground/Msk_*; tu uproszczenia (triplanar, płynne wagi).
#pragma once

#include "lib/render/game_lighting.h"
#include "lib/render/rhi/rhi.h"
#include "lib/map_template.h"
#include "lib/terrain_mesh.h"

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace ow
{

class TextureCache;

class TerrainRenderer
{
public:
	~TerrainRenderer();
	bool init(rhi::Device& dev, std::string& err);
	// layer2 i world (np. "urw", "kris", "lava", "swf") opcjonalne: nakładki detali.
	void build(const TerrainMesh& mesh, TextureCache& textures, const L2m* layer2 = nullptr,
	    const std::string& world = "urw");
	void clear();
	bool empty() const { return groups_.empty(); }
	bool showDetail = true;
	// world: macierz układu sceny (jak dla obiektów). lightDir w tym samym układzie co normalne po world.
	// gameLighting: oświetlenie jak w grze (game_lighting.h); lampy i mgłę klatki wysyła uploadScene.
	void draw(const glm::mat4& viewProj, const glm::mat4& world, const glm::vec3& lightDir, float ambient,
	    bool wireframe, bool gameLighting = false);

private:
	struct Group
	{
		rhi::Handle texture = rhi::kNull;                     // podkład: Gnd_A; nakładka: tekstura detalu
		rhi::Handle textureB = rhi::kNull, textureC = rhi::kNull; // podkład: Gnd_B, Gnd_C
		rhi::Handle ebo = rhi::kNull;
		uint32_t count = 0;
		bool glow = false; // nakładka z maską alfa (lava*): mieszanie alfa bez oświetlenia zamiast modulate ×2
	};
	rhi::Device* dev_ = nullptr;
	rhi::Handle pipe_ = rhi::kNull, pipeDetail_ = rhi::kNull, pipeGlow_ = rhi::kNull, pipeWire_ = rhi::kNull;
	rhi::Handle vbo_ = rhi::kNull;
	std::vector<Group> groups_;
	std::vector<Group> detail_;
	rhi::Handle maskAtlas_ = rhi::kNull; // atlas masek Msk_* (przejścia gruntów)
};

} // namespace ow
