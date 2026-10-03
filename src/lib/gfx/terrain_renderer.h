// Renderer terenu z mapy wysokości (Heightfield): jedna siatka, wierzchołek na próbkę,
// normalne z gradientu, tekstury gruntu Data/Ground/Gnd_<A|B|C><01..15>.tga powtarzane co blok
// (mapowanie trójpłaszczyznowe).
//
// Druga warstwa (.l2m) dokłada nakładki detali Data/Ground/detail/<świat>[2]_<TL TR BL BR>.tga
// (mech, kryształy…) mieszane w trybie modulate×2 (szary 128 = bez zmian), drugim przebiegiem.
//
// Podkład: grunt A/B/C z typu pola .PMP (Heightfield::ground, ✅ ze zrzutów gry), mieszany płynnie
// na granicach bloków. 🔶 Gra robi przejścia maskami Data/Ground/Msk_*; tu proste uśrednienie.
#pragma once

#include "lib/gfx/shader.h"
#include "lib/heightfield.h"
#include "lib/map_template.h"

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
	void build(const Heightfield& h, TextureCache& textures, const L2m* layer2 = nullptr, const std::string& world = "urw");
	void clear();
	bool empty() const { return groups_.empty(); }
	bool showDetail = true;
	// world: macierz układu sceny (jak dla obiektów). lightDir w tym samym układzie co normalne po world.
	void draw(const glm::mat4& viewProj, const glm::mat4& world, const glm::vec3& lightDir, float ambient,
	          bool wireframe);

private:
	struct Group {
		GLuint texture = 0;          // dla nakładek detali: tekstura detalu; dla podkładu: Gnd_A
		GLuint textureB = 0, textureC = 0;
		GLuint ebo = 0;
		GLsizei count = 0;
	};
	ShaderProgram prog_;
	GLuint vao_ = 0, vbo_ = 0;
	std::vector<Group> groups_;
	std::vector<Group> detail_; // nakładki z .l2m
};

} // namespace ow
