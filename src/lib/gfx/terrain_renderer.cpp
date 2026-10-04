#include "lib/gfx/terrain_renderer.h"

#include "lib/gfx/game_lighting.h"
#include "lib/gfx/texture_cache.h"
#include "lib/vfs.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <glm/gtc/type_ptr.hpp>
#include <map>

namespace ow {

static const char* kVSHead = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 3) in vec4 aCorner; // grunty narożników TL, TR, BL, BR (0 A, 1 B, 2 C)
layout(location = 4) in float aAlpha;  // alfa wycięcia (otwory do tła)
layout(location = 5) in float aBase;   // jasność bazowa (oświetlenie gry)
layout(location = 6) in vec2 aUV;      // UV gruntu z palety BlockDB
uniform mat4 uViewProj;
uniform mat4 uWorld;
out vec3 vNormal;
out vec3 vLocalNormal;
out vec3 vLocalPos;
out float vDepth;
flat out vec4 vCorner;
out float vAlpha;
out vec3 vGameDiffuse;
out vec3 vGameSpec;
out vec2 vUV;
)";

static const char* kVSBody = R"(
void main() {
	gameLighting(aPos / 200.0, aNormal, aBase, vGameDiffuse, vGameSpec);
	vCorner = aCorner;
	vAlpha = aAlpha;
	vUV = aUV;
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
flat in vec4 vCorner;
in float vAlpha;
in vec3 vGameDiffuse;
in vec3 vGameSpec;
in vec2 vUV;
uniform int uGameLight;
uniform sampler2D uTex;
uniform sampler2D uTexB;
uniform sampler2D uTexC;
uniform sampler2D uMasks; // atlas 4×4 masek Msk_<TL TR BL BR>01: komórka = bity (B = 1; TL 8, TR 4, BL 2, BR 1)
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
		vec4 d = texture(uTex, vUV); // UV z palety jak grunt (na podłogach (x, z - 7)), zawijanie
		oColor = vec4(d.rgb, 1.0);
		return;
	}
	// UV z palety BlockDB, jak w grze (1 tekstura na blok). Tekstury gruntu mają pustą alfę.
	vec3 tex[3];
	tex[0] = texture(uTex, vUV).rgb;
	tex[1] = texture(uTexB, vUV).rgb;
	tex[2] = texture(uTexC, vUV).rgb;
	// Przejścia gruntów (docs/silnik/04_teren.md §4.2): narożniki bloku mają grunty; przy dwóch gruntach
	// maska Msk_<TL TR BL BR>01 (biel = A, czerń = B) w tych samych UV co grunt. 🔶 A = grunt o niższym
	// indeksie; trzy grunty w jednym bloku: dwa wyższe łączymy jako B.
	ivec4 c = ivec4(vCorner + 0.5);
	int gA = min(min(c.x, c.y), min(c.z, c.w));
	int gB = max(max(c.x, c.y), max(c.z, c.w));
	vec3 t = tex[gA];
	if (gB != gA) {
		int bits = (c.x != gA ? 8 : 0) + (c.y != gA ? 4 : 0) + (c.z != gA ? 2 : 0) + (c.w != gA ? 1 : 0);
		vec2 cell = vec2(float(bits % 4), float(bits / 4));
		vec2 f = clamp(fract(vUV), vec2(0.5 / 64.0), vec2(1.0 - 0.5 / 64.0));
		float m = texture(uMasks, (cell + f) / 4.0).r;
		t = mix(tex[gB], tex[gA], m);
	}
	if (uGameLight == 1) {
		oColor = vec4(t * vGameDiffuse + vGameSpec, 1.0) * uTint;
		return;
	}
	float l = uAmbient + (1.0 - uAmbient) * max(dot(normalize(vNormal), uLightDir), 0.0);
	l *= mix(1.0, 0.55, clamp(vDepth, 0.0, 1.0)); // głębiej ciemniej
	oColor = vec4(t * l, 1.0) * uTint;
}
)";

bool TerrainRenderer::init(std::string& err) {
	std::string vs = std::string(kVSHead) + kGameLightingGlsl + kVSBody;
	return prog_.build(vs.c_str(), kFS, err);
}

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
	gl::VertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, corner));
	gl::EnableVertexAttribArray(0);
	gl::EnableVertexAttribArray(1);
	gl::VertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, alpha));
	gl::VertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, base));
	gl::VertexAttribPointer(6, 2, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, uv));
	gl::EnableVertexAttribArray(3);
	gl::EnableVertexAttribArray(4);
	gl::EnableVertexAttribArray(5);
	gl::EnableVertexAttribArray(6);

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

	// Atlas masek przejść gruntów: 4×4 komórki po 64 px, komórka = bity narożników z literą B.
	if (!maskAtlas_) {
		std::vector<uint8_t> atlas(256 * 256 * 4, 255);
		for (int bits = 1; bits < 15; ++bits) {
			std::string name = "Data/Ground/Msk_";
			for (int k = 3; k >= 0; --k) name += (bits >> k) & 1 ? 'B' : 'A';
			name += "01.tga";
			Image img;
			auto data = textures.vfs().read(name);
			if (!data || !loadTga(*data, img) || img.width != 64 || img.height != 64) continue;
			int cx = (bits % 4) * 64, cy = (bits / 4) * 64;
			for (int y = 0; y < 64; ++y)
				for (int x = 0; x < 64; ++x)
					for (int ch = 0; ch < 4; ++ch)
						atlas[(size_t(cy + y) * 256 + size_t(cx + x)) * 4 + ch] = img.rgba[(size_t(y) * 64 + x) * 4 + ch];
		}
		Image a;
		a.width = a.height = 256;
		a.rgba = std::move(atlas);
		maskAtlas_ = uploadTexture(a, false);
		gl::BindTexture(GL_TEXTURE_2D, maskAtlas_);
		gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}

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
                           bool wireframe, const std::vector<GameLight>* gameLights, const GameFog* fog) {
	if (groups_.empty()) return;
	prog_.use();
	uploadGameLights(prog_, gameLights != nullptr, gameLights, fog);
	gl::UniformMatrix4fv(prog_.uniform("uViewProj"), 1, GL_FALSE, glm::value_ptr(viewProj));
	gl::UniformMatrix4fv(prog_.uniform("uWorld"), 1, GL_FALSE, glm::value_ptr(world));
	gl::Uniform3f(prog_.uniform("uLightDir"), lightDir.x, lightDir.y, lightDir.z);
	gl::Uniform1f(prog_.uniform("uAmbient"), ambient);
	gl::Uniform1i(prog_.uniform("uTex"), 0);
	gl::Uniform1i(prog_.uniform("uTexB"), 1);
	gl::Uniform1i(prog_.uniform("uTexC"), 2);
	gl::Uniform1i(prog_.uniform("uMasks"), 3);
	gl::ActiveTexture(GL_TEXTURE3);
	gl::BindTexture(GL_TEXTURE_2D, maskAtlas_);
	gl::ActiveTexture(GL_TEXTURE0);
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
