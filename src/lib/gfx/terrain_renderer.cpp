#include "lib/gfx/terrain_renderer.h"

#include "lib/gfx/texture_cache.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <glm/gtc/type_ptr.hpp>
#include <map>

namespace ow {

static const char* kVS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 3) in vec3 aGround; // wagi gruntów A, B, C
layout(location = 4) in float aAlpha;  // alfa wycięcia (otwory do tła)
uniform mat4 uViewProj;
uniform mat4 uWorld;
out vec3 vNormal;
out vec3 vLocalNormal;
out vec3 vLocalPos;
out float vDepth;
out vec3 vGround;
out float vAlpha;
void main() {
	vGround = aGround;
	vAlpha = aAlpha;
	vNormal = mat3(uWorld) * aNormal;
	vLocalNormal = aNormal;
	vLocalPos = aPos;
	vDepth = (2900.0 - aPos.z) / 1500.0; // 0 = przednia płaszczyzna, 1 = najgłębiej
	gl_Position = uViewProj * uWorld * vec4(aPos, 1.0);
}
)";

static const char* kFS = R"(#version 330 core
in vec3 vNormal;
in vec3 vLocalNormal;
in vec3 vLocalPos;
in float vDepth;
in vec3 vGround;
in float vAlpha;
uniform sampler2D uTex;
uniform sampler2D uTexB;
uniform sampler2D uTexC;
uniform vec3 uLightDir;
uniform float uAmbient;
uniform vec4 uTint;
uniform int uDetail; // 1 = nakładka detalu: UV w obrębie bloku, alfa z tekstury
out vec4 oColor;
void main() {
	// Alpha test jak w grze (scape_solid: GREATER 126): ostre wycięcie otworów do tła.
	if (vAlpha * 255.0 <= 126.0) discard;
	if (uDetail == 1) {
		// Tekstura detalu w trybie modulate x2 (szary 128 = bez zmian, por. detail/50PGray.tga).
		vec4 d = texture(uTex, fract(vLocalPos.xy / 200.0));
		oColor = vec4(d.rgb, 1.0);
		return;
	}
	// Mapowanie trójpłaszczyznowe (tekstura co blok = 200 jednostek), żeby strome ściany się nie rozciągały.
	// Tekstury gruntu mają pustą alfę, więc ją ignorujemy.
	vec3 w = pow(abs(normalize(vLocalNormal)), vec3(4.0));
	w /= (w.x + w.y + w.z);
	vec3 p = vLocalPos / 200.0;
	vec3 tA = texture(uTex, p.xy).rgb * w.z + texture(uTex, p.xz).rgb * w.y + texture(uTex, p.yz).rgb * w.x;
	vec3 tB = texture(uTexB, p.xy).rgb * w.z + texture(uTexB, p.xz).rgb * w.y + texture(uTexB, p.yz).rgb * w.x;
	vec3 tC = texture(uTexC, p.xy).rgb * w.z + texture(uTexC, p.xz).rgb * w.y + texture(uTexC, p.yz).rgb * w.x;
	vec3 g = vGround / max(vGround.x + vGround.y + vGround.z, 1e-4);
	vec3 t = tA * g.x + tB * g.y + tC * g.z;
	float l = uAmbient + (1.0 - uAmbient) * max(dot(normalize(vNormal), uLightDir), 0.0);
	l *= mix(1.0, 0.55, clamp(vDepth, 0.0, 1.0)); // głębiej ciemniej
	oColor = vec4(t * l, 1.0) * uTint;
}
)";

bool TerrainRenderer::init(std::string& err) { return prog_.build(kVS, kFS, err); }

void TerrainRenderer::clear() {
	for (auto& g : groups_) gl::DeleteBuffers(1, &g.ebo);
	for (auto& g : detail_) gl::DeleteBuffers(1, &g.ebo);
	groups_.clear();
	detail_.clear();
	if (vbo_) gl::DeleteBuffers(1, &vbo_);
	if (vao_) gl::DeleteVertexArrays(1, &vao_);
	vbo_ = vao_ = 0;
}

void TerrainRenderer::build(const TerrainMesh& mesh, TextureCache& textures, const L2m* layer2,
                            const std::string& world) {
	clear();
	if (mesh.indices.empty()) return;

	gl::GenVertexArrays(1, &vao_);
	gl::BindVertexArray(vao_);
	gl::GenBuffers(1, &vbo_);
	gl::BindBuffer(GL_ARRAY_BUFFER, vbo_);
	gl::BufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(TerrainVertex), mesh.vertices.data(), GL_STATIC_DRAW);
	gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, pos));
	gl::VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, normal));
	gl::VertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, ground));
	gl::EnableVertexAttribArray(0);
	gl::EnableVertexAttribArray(1);
	gl::VertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, alpha));
	gl::EnableVertexAttribArray(3);
	gl::EnableVertexAttribArray(4);

	auto blockIndices = [&](int bx, int by, std::vector<uint32_t>& out) {
		auto [first, count] = mesh.blockRanges[size_t(by) * mesh.width + bx];
		out.insert(out.end(), mesh.indices.begin() + first, mesh.indices.begin() + first + count);
	};
	auto upload = [&](const std::vector<uint32_t>& idx, Group g, std::vector<Group>& out) {
		if (idx.empty()) return;
		g.count = GLsizei(idx.size());
		gl::GenBuffers(1, &g.ebo);
		gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, g.ebo);
		gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * 4, idx.data(), GL_STATIC_DRAW);
		out.push_back(g);
	};

	// Podkład: grupy według wariantu tekstury (1..15, zależny od pozycji bloku).
	std::map<int, std::vector<uint32_t>> byVariant;
	for (int by = 0; by < mesh.height; ++by)
		for (int bx = 0; bx < mesh.width; ++bx) {
			unsigned h = unsigned(bx) * 73856093u ^ unsigned(by) * 19349663u;
			blockIndices(bx, by, byVariant[int((h ^ (h >> 13)) % 15u) + 1]);
		}
	for (auto& [variant, idx] : byVariant) {
		char n[8];
		std::snprintf(n, sizeof n, "%02d", variant);
		Group g;
		g.texture = textures.getPath(std::string("Data/Ground/Gnd_A") + n + ".tga").id;
		g.textureB = textures.getPath(std::string("Data/Ground/Gnd_B") + n + ".tga").id;
		g.textureC = textures.getPath(std::string("Data/Ground/Gnd_C") + n + ".tga").id;
		upload(idx, g, groups_);
	}

	// Nakładki detali z .l2m: starszy bajt = 4 narożniki po 2 bity (TL, TR, BL, BR; 0 brak, 1 zestaw
	// <świat>, 2 zestaw <świat>2). Plik detail/<świat>[2]_<kod>.tga, pełne pokrycie 1111 z wariantami _a.._f.
	if (layer2 && int(layer2->width) == mesh.width && int(layer2->height) == mesh.height) {
		std::map<std::string, std::vector<uint32_t>> byDetail;
		for (int by = 0; by < mesh.height; ++by)
			for (int bx = 0; bx < mesh.width; ++bx) {
				uint16_t v = layer2->values[size_t(by) * layer2->width + bx];
				int corner[4] = {(v >> 14) & 3, (v >> 12) & 3, (v >> 10) & 3, (v >> 8) & 3};
				for (int set = 1; set <= 2; ++set) {
					std::string bits;
					for (int c : corner) bits += c == set ? '1' : '0';
					if (bits == "0000") continue;
					std::string name = "Data/Ground/detail/" + world + (set == 2 ? "2" : "") + "_" + bits;
					if (bits == "1111") name += std::string("_") + char('a' + (bx * 7 + by * 13) % 6);
					blockIndices(bx, by, byDetail[name + ".tga"]);
				}
			}
		for (auto& [name, idx] : byDetail) {
			Group g;
			g.texture = textures.getPath(name).id;
			upload(idx, g, detail_);
		}
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
	gl::Uniform1i(prog_.uniform("uTexB"), 1);
	gl::Uniform1i(prog_.uniform("uTexC"), 2);
	gl::Uniform4f(prog_.uniform("uTint"), 1, 1, 1, 1);
	gl::Uniform1i(prog_.uniform("uDetail"), 0);
	gl::Enable(GL_DEPTH_TEST);
	gl::Disable(GL_CULL_FACE);
	gl::Disable(GL_BLEND);
	gl::ActiveTexture(GL_TEXTURE0);
	gl::BindVertexArray(vao_);
	for (const auto& g : groups_) {
		gl::ActiveTexture(GL_TEXTURE1);
		gl::BindTexture(GL_TEXTURE_2D, g.textureB);
		gl::ActiveTexture(GL_TEXTURE2);
		gl::BindTexture(GL_TEXTURE_2D, g.textureC);
		gl::ActiveTexture(GL_TEXTURE0);
		gl::BindTexture(GL_TEXTURE_2D, g.texture);
		gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, g.ebo);
		gl::DrawElements(GL_TRIANGLES, g.count, GL_UNSIGNED_INT, nullptr);
	}
	if (showDetail && !detail_.empty()) {
		gl::Uniform1i(prog_.uniform("uDetail"), 1);
		gl::Enable(GL_BLEND);
		gl::BlendFunc(GL_DST_COLOR, GL_SRC_COLOR); // wynik = cel * źródło * 2
		gl::DepthMask(GL_FALSE);
		gl::DepthFunc(GL_LEQUAL);
		for (const auto& g : detail_) {
			gl::BindTexture(GL_TEXTURE_2D, g.texture);
			gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, g.ebo);
			gl::DrawElements(GL_TRIANGLES, g.count, GL_UNSIGNED_INT, nullptr);
		}
		gl::DepthMask(GL_TRUE);
		gl::DepthFunc(GL_LESS);
		gl::Disable(GL_BLEND);
		gl::Uniform1i(prog_.uniform("uDetail"), 0);
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
