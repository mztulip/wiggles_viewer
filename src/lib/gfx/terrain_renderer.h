// Renderer terenu: siatka z bloków BlockDB (TerrainMesh), podkład z gruntów
// Data/Ground/Gnd_<A|B|C><01..15>.tga mieszanych wagami z typu pola .PMP, mapowanie
// trójpłaszczyznowe (tekstura co blok). Druga warstwa (.l2m) dokłada nakładki detali
// Data/Ground/detail/<świat>[2]_<TL TR BL BR>.tga (mech, kryształy…) w trybie modulate×2.
//
// 🔶 Gra mapuje tekstury współrzędnymi u, v z palety BlockDB i robi przejścia maskami
// Data/Ground/Msk_*; tu uproszczenia (triplanar, płynne wagi).
#pragma once

#include "lib/gfx/shader.h"
#include "lib/map_template.h"
#include "lib/terrain_mesh.h"

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace ow {

class TextureCache;

class TerrainRenderer {
public:
	~TerrainRenderer() { clear(); }
	bool init(std::string& err);
	// layer2 i world (np. "urw", "kris", "lava", "swf") opcjonalne: nakładki detali.
	void build(const TerrainMesh& mesh, TextureCache& textures, const L2m* layer2 = nullptr,
	           const std::string& world = "urw");
	void clear();
	bool empty() const { return groups_.empty(); }
	bool showDetail = true;
	// world: macierz układu sceny (jak dla obiektów). lightDir w tym samym układzie co normalne po world.
	void draw(const glm::mat4& viewProj, const glm::mat4& world, const glm::vec3& lightDir, float ambient,
	          bool wireframe);

private:
	struct Group {
		GLuint texture = 0;                // podkład: Gnd_A; nakładka: tekstura detalu
		GLuint textureB = 0, textureC = 0; // podkład: Gnd_B, Gnd_C
		GLuint ebo = 0;
		GLsizei count = 0;
	};
	ShaderProgram prog_;
	GLuint vao_ = 0, vbo_ = 0;
	std::vector<Group> groups_;
	std::vector<Group> detail_;
};

} // namespace ow
