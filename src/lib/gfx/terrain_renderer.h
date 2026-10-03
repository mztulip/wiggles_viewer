// Renderer terenu z mapy wysokości (Heightfield): jedna siatka, wierzchołek na próbkę,
// normalne z gradientu, tekstury gruntu Data/Ground/Gnd_<A|B|C><01..15>.tga powtarzane co blok
// (mapowanie trójpłaszczyznowe).
//
// 🔶 Wybór rodzaju gruntu jest tymczasowy: A (skała) dla przedniej płaszczyzny, B (ziemia)
// dla wydrążeń. Prawdziwe przypisanie (pola BlockDB / warstwy PMP) nie jest jeszcze znane.
#pragma once

#include "lib/gfx/shader.h"
#include "lib/heightfield.h"

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace ow {

class TextureCache;

class TerrainRenderer {
public:
	~TerrainRenderer() { clear(); }
	bool init(std::string& err);
	void build(const Heightfield& h, TextureCache& textures);
	void clear();
	bool empty() const { return groups_.empty(); }
	// world: macierz układu sceny (jak dla obiektów). lightDir w tym samym układzie co normalne po world.
	void draw(const glm::mat4& viewProj, const glm::mat4& world, const glm::vec3& lightDir, float ambient,
	          bool wireframe);

private:
	struct Group {
		GLuint texture = 0;
		GLuint ebo = 0;
		GLsizei count = 0;
	};
	ShaderProgram prog_;
	GLuint vao_ = 0, vbo_ = 0;
	std::vector<Group> groups_;
};

} // namespace ow
