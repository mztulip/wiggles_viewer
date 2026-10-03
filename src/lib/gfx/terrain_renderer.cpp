#include "lib/gfx/terrain_renderer.h"

#include "lib/gfx/texture_cache.h"

#include <cmath>
#include <cstdio>
#include <glm/gtc/type_ptr.hpp>
#include <map>

namespace ow {

static const char* kVS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
uniform mat4 uViewProj;
uniform mat4 uWorld;
out vec3 vNormal;
out vec3 vLocalNormal;
out vec3 vLocalPos;
out float vDepth;
void main() {
	vNormal = mat3(uWorld) * aNormal;
	vLocalNormal = aNormal;
	vLocalPos = aPos;
	vDepth = (3000.0 - aPos.z) / 1500.0; // 0 = przednia płaszczyzna, 1 = najgłębiej
	gl_Position = uViewProj * uWorld * vec4(aPos, 1.0);
}
)";

static const char* kFS = R"(#version 330 core
in vec3 vNormal;
in vec3 vLocalNormal;
in vec3 vLocalPos;
in float vDepth;
uniform sampler2D uTex;
uniform vec3 uLightDir;
uniform float uAmbient;
uniform vec4 uTint;
out vec4 oColor;
void main() {
	// Mapowanie trójpłaszczyznowe (tekstura co blok = 200 jednostek), żeby strome ściany się nie rozciągały.
	// Tekstury gruntu mają pustą alfę, więc ją ignorujemy.
	vec3 w = pow(abs(normalize(vLocalNormal)), vec3(4.0));
	w /= (w.x + w.y + w.z);
	vec3 p = vLocalPos / 200.0;
	vec3 t = texture(uTex, p.xy).rgb * w.z + texture(uTex, p.xz).rgb * w.y + texture(uTex, p.yz).rgb * w.x;
	float l = uAmbient + (1.0 - uAmbient) * max(dot(normalize(vNormal), uLightDir), 0.0);
	l *= mix(1.0, 0.55, clamp(vDepth, 0.0, 1.0)); // głębiej ciemniej
	oColor = vec4(t * l, 1.0) * uTint;
}
)";

bool TerrainRenderer::init(std::string& err) { return prog_.build(kVS, kFS, err); }

void TerrainRenderer::clear() {
	for (auto& g : groups_) gl::DeleteBuffers(1, &g.ebo);
	groups_.clear();
	if (vbo_) gl::DeleteBuffers(1, &vbo_);
	if (vao_) gl::DeleteVertexArrays(1, &vao_);
	vbo_ = vao_ = 0;
}

void TerrainRenderer::build(const Heightfield& h, TextureCache& textures) {
	clear();
	if (h.width < 2 || h.height < 2) return;
	const float S = Heightfield::kSampleUnits;
	struct V {
		float pos[3], nrm[3], uv[2];
	};
	std::vector<V> verts(size_t(h.width) * h.height);
	for (int y = 0; y < h.height; ++y)
		for (int x = 0; x < h.width; ++x) {
			float z = Heightfield::depthZ(h.at(x, y));
			float dzdx = (Heightfield::depthZ(h.at(x + 1, y)) - Heightfield::depthZ(h.at(x - 1, y))) / (2 * S);
			float dzdy = (Heightfield::depthZ(h.at(x, y + 1)) - Heightfield::depthZ(h.at(x, y - 1))) / (2 * S);
			glm::vec3 n = glm::normalize(glm::vec3(-dzdx, -dzdy, 1.f));
			float px = (float(x) + 0.5f) * S, py = (float(y) + 0.5f) * S;
			verts[size_t(y) * h.width + x] = {{px, py, z}, {n.x, n.y, n.z}, {px / 200.f, py / 200.f}};
		}

	// Trójkąty pogrupowane według tekstury (rodzaj gruntu + wariant zależny od bloku).
	std::map<std::string, std::vector<uint32_t>> byTex;
	for (int y = 0; y + 1 < h.height; ++y)
		for (int x = 0; x + 1 < h.width; ++x) {
			uint8_t v = h.at(x, y);
			uint32_t id = h.blockId[size_t(y) * h.width + x];
			char name[64];
			std::snprintf(name, sizeof name, "Data/Ground/Gnd_%c%02u.tga", v == 15 ? 'A' : 'B', unsigned(id * 2654435761u % 15 + 1));
			auto& idx = byTex[name];
			uint32_t a = uint32_t(y * h.width + x), b = a + 1, c = a + uint32_t(h.width), d = c + 1;
			idx.insert(idx.end(), {a, c, b, b, c, d});
		}

	gl::GenVertexArrays(1, &vao_);
	gl::BindVertexArray(vao_);
	gl::GenBuffers(1, &vbo_);
	gl::BindBuffer(GL_ARRAY_BUFFER, vbo_);
	gl::BufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(V), verts.data(), GL_STATIC_DRAW);
	gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, pos));
	gl::VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, nrm));
	gl::VertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, uv));
	for (GLuint i = 0; i < 3; ++i) gl::EnableVertexAttribArray(i);
	for (auto& [name, idx] : byTex) {
		Group g;
		g.texture = textures.getPath(name).id;
		g.count = GLsizei(idx.size());
		gl::GenBuffers(1, &g.ebo);
		gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, g.ebo);
		gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * 4, idx.data(), GL_STATIC_DRAW);
		groups_.push_back(g);
	}
	gl::BindVertexArray(0);
}

void TerrainRenderer::draw(const glm::mat4& viewProj, const glm::mat4& world, const glm::vec3& lightDir, float ambient,
                           bool wireframe) {
	if (groups_.empty()) return;
	prog_.use();
	gl::UniformMatrix4fv(prog_.uniform("uViewProj"), 1, GL_FALSE, glm::value_ptr(viewProj));
	gl::UniformMatrix4fv(prog_.uniform("uWorld"), 1, GL_FALSE, glm::value_ptr(world));
	gl::Uniform3f(prog_.uniform("uLightDir"), lightDir.x, lightDir.y, lightDir.z);
	gl::Uniform1f(prog_.uniform("uAmbient"), ambient);
	gl::Uniform1i(prog_.uniform("uTex"), 0);
	gl::Uniform4f(prog_.uniform("uTint"), 1, 1, 1, 1);
	gl::Enable(GL_DEPTH_TEST);
	gl::Disable(GL_CULL_FACE);
	gl::Disable(GL_BLEND);
	gl::ActiveTexture(GL_TEXTURE0);
	gl::BindVertexArray(vao_);
	for (const auto& g : groups_) {
		gl::BindTexture(GL_TEXTURE_2D, g.texture);
		gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, g.ebo);
		gl::DrawElements(GL_TRIANGLES, g.count, GL_UNSIGNED_INT, nullptr);
	}
	if (wireframe) {
		gl::PolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		gl::Uniform4f(prog_.uniform("uTint"), 0, 0, 0, 1);
		for (const auto& g : groups_) {
			gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, g.ebo);
			gl::DrawElements(GL_TRIANGLES, g.count, GL_UNSIGNED_INT, nullptr);
		}
		gl::PolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
	gl::BindVertexArray(0);
}

} // namespace ow
