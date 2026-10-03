#include "lib/gfx/panorama_renderer.h"

#include "lib/gfx/texture_cache.h"

#include <cmath>
#include <vector>

namespace ow {

namespace {

const char* kVS = R"(#version 330 core
layout(location = 0) in vec2 aNdc;
layout(location = 1) in vec2 aUV;
out vec2 vUV;
void main() {
	vUV = aUV;
	gl_Position = vec4(aNdc, 0.0, 1.0);
}
)";

const char* kFS = R"(#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
out vec4 oColor;
void main() { oColor = texture(uTex, vUV); }
)";

const char* kCarpetVS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
uniform mat4 uMVP;
out vec2 vUV;
void main() {
	vUV = aUV;
	gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

struct Layer {
	float depth, scale, offU, offV;
};
// Stałe z .rdata Wiggles.exe (BACKDROP_PARALLAX_RE §2.1).
const Layer kLayers[6] = {{21100, 0.53f, 0, 0.75f}, {20500, 0.04f, 0, 1.0f},  {11300, 0.11f, 0, 0.5f},
                          {1000, 1.12f, 0, -4.9f},  {200, 6.68f, 0.7f, -33.0f}, {100, 18.0f, 1.0f, -90.25f}};
constexpr float kKu = 0.00075f, kKv = 0.0015f, kUvBias = 5.0f;
constexpr int kGrid = 9;
// Dywan łąki (FUN_00480720).
constexpr float kChunk = 20.f, kCarpetSpan = 140.f, kCarpetDrop = 2.5f;
constexpr float kRowLift[4] = {0.75f, 0.25f, 0.f, 0.f};
constexpr float kRowZ[4] = {-60.f, -37.5f, -15.f, 7.5f};
constexpr float kCarpetUV = 0.33f;
constexpr int kChunks = int(2 * kCarpetSpan / kChunk) + 1;

} // namespace

PanoramaRenderer::~PanoramaRenderer() {
	if (carpetEbo_) gl::DeleteBuffers(1, &carpetEbo_);
	if (carpetVbo_) gl::DeleteBuffers(1, &carpetVbo_);
	if (carpetVao_) gl::DeleteVertexArrays(1, &carpetVao_);
	if (ebo_) gl::DeleteBuffers(1, &ebo_);
	if (vbo_) gl::DeleteBuffers(1, &vbo_);
	if (vao_) gl::DeleteVertexArrays(1, &vao_);
}

bool PanoramaRenderer::init(std::string& err, TextureCache& textures) {
	if (!prog_.build(kVS, kFS, err) || !carpetProg_.build(kCarpetVS, kFS, err)) return false;
	for (int i = 0; i < 7; ++i) {
		tex_[i] = textures.getPath("Data/Texture/Misc/Panorama_layer_" + std::to_string(i) + ".tga").id;
		gl::BindTexture(GL_TEXTURE_2D, tex_[i]);
		gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, i == 0 ? GL_REPEAT : GL_CLAMP_TO_EDGE);
	}
	gl::GenVertexArrays(1, &vao_);
	gl::BindVertexArray(vao_);
	gl::GenBuffers(1, &vbo_);
	gl::BindBuffer(GL_ARRAY_BUFFER, vbo_);
	gl::BufferData(GL_ARRAY_BUFFER, kGrid * kGrid * 4 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
	gl::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
	gl::EnableVertexAttribArray(0);
	gl::EnableVertexAttribArray(1);
	std::vector<uint16_t> idx;
	for (int r = 0; r + 1 < kGrid; ++r)
		for (int c = 0; c + 1 < kGrid; ++c) {
			uint16_t a = uint16_t(r * kGrid + c), b = uint16_t(a + 1), d = uint16_t(a + kGrid), e = uint16_t(d + 1);
			idx.insert(idx.end(), {a, d, b, b, d, e});
		}
	gl::GenBuffers(1, &ebo_);
	gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
	gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * 2, idx.data(), GL_STATIC_DRAW);

	// Dywan: kChunks kawałków po 4×4 wierzchołki (x, y, z, u, v).
	gl::GenVertexArrays(1, &carpetVao_);
	gl::BindVertexArray(carpetVao_);
	gl::GenBuffers(1, &carpetVbo_);
	gl::BindBuffer(GL_ARRAY_BUFFER, carpetVbo_);
	gl::BufferData(GL_ARRAY_BUFFER, kChunks * 16 * 5 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
	gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	gl::EnableVertexAttribArray(0);
	gl::EnableVertexAttribArray(1);
	idx.clear();
	for (int k = 0; k < kChunks; ++k)
		for (int r = 0; r < 3; ++r)
			for (int c = 0; c < 3; ++c) {
				uint16_t a = uint16_t(k * 16 + r * 4 + c), b = uint16_t(a + 1), d = uint16_t(a + 4), e = uint16_t(d + 1);
				idx.insert(idx.end(), {a, d, b, b, d, e});
			}
	gl::GenBuffers(1, &carpetEbo_);
	gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, carpetEbo_);
	gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * 2, idx.data(), GL_STATIC_DRAW);
	gl::BindVertexArray(0);
	return true;
}

void PanoramaRenderer::drawCarpet(const glm::mat4& gameToClip, float focusX, float groundRow) {
	std::vector<float> v;
	v.reserve(kChunks * 16 * 5);
	float x0 = std::floor(focusX / kChunk) * kChunk - kCarpetSpan;
	for (int k = 0; k < kChunks; ++k)
		for (int r = 0; r < 4; ++r)
			for (int c = 0; c < 4; ++c) {
				v.push_back(x0 + float(k) * kChunk + float(c) * kChunk / 3.f);
				v.push_back(groundRow - kCarpetDrop - kRowLift[r]);
				v.push_back(kRowZ[r]);
				v.push_back(float(c) * kCarpetUV);
				v.push_back(float(r) * kCarpetUV);
			}
	carpetProg_.use();
	gl::Uniform1i(carpetProg_.uniform("uTex"), 0);
	gl::UniformMatrix4fv(carpetProg_.uniform("uMVP"), 1, GL_FALSE, &gameToClip[0][0]);
	gl::Disable(GL_DEPTH_TEST);
	gl::DepthMask(GL_FALSE);
	gl::Disable(GL_CULL_FACE);
	gl::Enable(GL_BLEND);
	gl::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	gl::ActiveTexture(GL_TEXTURE0);
	gl::BindTexture(GL_TEXTURE_2D, tex_[6]);
	gl::BindVertexArray(carpetVao_);
	gl::BindBuffer(GL_ARRAY_BUFFER, carpetVbo_);
	gl::BufferData(GL_ARRAY_BUFFER, GLsizeiptr(v.size() * sizeof(float)), v.data(), GL_DYNAMIC_DRAW);
	gl::DrawElements(GL_TRIANGLES, kChunks * 9 * 6, GL_UNSIGNED_SHORT, nullptr);
	gl::BindVertexArray(0);
	gl::DepthMask(GL_TRUE);
	gl::Enable(GL_DEPTH_TEST);
	gl::Disable(GL_BLEND);
}

void PanoramaRenderer::draw(const glm::mat4& ndcToGame) {
	// Promienie siatki 9×9 w układzie gry.
	struct Ray {
		float ndcX, ndcY, xNear, yNear, slopeX, slopeY;
	};
	Ray rays[kGrid * kGrid];
	for (int r = 0; r < kGrid; ++r)
		for (int c = 0; c < kGrid; ++c) {
			float x = -1.f + 2.f * float(c) / (kGrid - 1), y = 1.f - 2.f * float(r) / (kGrid - 1);
			glm::vec4 n = ndcToGame * glm::vec4(x, y, -1.f, 1.f), f = ndcToGame * glm::vec4(x, y, 1.f, 1.f);
			glm::vec3 near = glm::vec3(n) / n.w, far = glm::vec3(f) / f.w, d = far - near;
			float dz = std::abs(d.z) < 1e-6f ? -1e-6f : d.z;
			rays[r * kGrid + c] = {x, y, near.x, near.y, d.x / dz, d.y / dz};
		}

	prog_.use();
	gl::Uniform1i(prog_.uniform("uTex"), 0);
	gl::Disable(GL_DEPTH_TEST);
	gl::DepthMask(GL_FALSE);
	gl::Disable(GL_CULL_FACE);
	gl::Enable(GL_BLEND);
	gl::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	gl::ActiveTexture(GL_TEXTURE0);
	gl::BindVertexArray(vao_);
	float verts[kGrid * kGrid * 4];
	for (int l = 0; l < 6; ++l) {
		const Layer& L = kLayers[l];
		float dp = L.depth - 1.f;
		for (int i = 0; i < kGrid * kGrid; ++i) {
			const Ray& ry = rays[i];
			float uCore = kKu * (ry.slopeX * dp - ry.xNear) + kUvBias;
			float vCore = -kKv * (ry.slopeY * dp - ry.yNear) + kUvBias;
			verts[i * 4 + 0] = ry.ndcX;
			verts[i * 4 + 1] = ry.ndcY;
			verts[i * 4 + 2] = L.scale * uCore + L.offU;
			verts[i * 4 + 3] = L.scale * vCore + L.offV;
		}
		gl::BindBuffer(GL_ARRAY_BUFFER, vbo_);
		gl::BufferData(GL_ARRAY_BUFFER, sizeof verts, verts, GL_DYNAMIC_DRAW);
		gl::BindTexture(GL_TEXTURE_2D, tex_[l]);
		gl::DrawElements(GL_TRIANGLES, (kGrid - 1) * (kGrid - 1) * 6, GL_UNSIGNED_SHORT, nullptr);
	}
	gl::BindVertexArray(0);
	gl::DepthMask(GL_TRUE);
	gl::Enable(GL_DEPTH_TEST);
	gl::Disable(GL_BLEND);
}

} // namespace ow
