#include "lib/gfx/panorama_renderer.h"

#include "lib/gfx/texture_cache.h"
#include "lib/tga.h"
#include "lib/vfs.h"

#include <cmath>
#include <vector>

namespace ow {

namespace {

// Shadery: src/shaders/panorama.vert/.frag i carpet.vert/.frag.

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
	if (!dev_) return;
	for (auto t : tex_)
		if (t) dev_->destroyTexture(t);
	if (ebo_) dev_->destroyBuffer(ebo_);
	if (carpetEbo_) dev_->destroyBuffer(carpetEbo_);
}

bool PanoramaRenderer::init(rhi::Device& dev, std::string& err, TextureCache& textures) {
	dev_ = &dev;
	rhi::PipelineDesc d;
	d.shader = "panorama";
	d.strides = {4 * sizeof(float)};
	d.attribs = {{0, 0, 2, 0}, {1, 0, 2, 2 * sizeof(float)}};
	d.blend = rhi::Blend::Alpha;
	d.depth = rhi::DepthTest::Off;
	d.depthWrite = false;
	if (!(pipe_ = dev.createPipeline(d, err))) return false;
	rhi::PipelineDesc c = d;
	c.shader = "carpet";
	c.strides = {5 * sizeof(float)};
	c.attribs = {{0, 0, 3, 0}, {1, 0, 2, 3 * sizeof(float)}};
	if (!(pipeCarpet_ = dev.createPipeline(c, err))) return false;
	// Warstwy 0..5 i dywan (6): U zawijane; V zawijane tylko w warstwie 0 (BACKDROP_PARALLAX_RE §5).
	for (int i = 0; i < 7; ++i) {
		std::string path = "Data/Texture/Misc/Panorama_layer_" + std::to_string(i) + ".tga";
		Image img;
		auto data = textures.vfs().read(path);
		if (!data || !loadTga(*data, img)) continue;
		rhi::TextureDesc td;
		td.repeatU = true;
		td.repeatV = i == 0;
		tex_[i] = textures.upload(img, td);
	}
	std::vector<uint16_t> idx;
	for (int r = 0; r + 1 < kGrid; ++r)
		for (int c2 = 0; c2 + 1 < kGrid; ++c2) {
			uint16_t a = uint16_t(r * kGrid + c2), b = uint16_t(a + 1), e = uint16_t(a + kGrid), f = uint16_t(e + 1);
			idx.insert(idx.end(), {a, e, b, b, e, f});
		}
	ebo_ = dev.createBuffer(rhi::BufferKind::Index16, idx.data(), idx.size() * 2);
	idx.clear();
	for (int k = 0; k < kChunks; ++k)
		for (int r = 0; r < 3; ++r)
			for (int c2 = 0; c2 < 3; ++c2) {
				uint16_t a = uint16_t(k * 16 + r * 4 + c2), b = uint16_t(a + 1), e = uint16_t(a + 4), f = uint16_t(e + 1);
				idx.insert(idx.end(), {a, e, b, b, e, f});
			}
	carpetEbo_ = dev.createBuffer(rhi::BufferKind::Index16, idx.data(), idx.size() * 2);
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
	if (!dev_ || !pipeCarpet_) return;
	dev_->bindPipeline(pipeCarpet_);
	dev_->setUniforms(rhi::kBindDraw, &gameToClip, sizeof gameToClip);
	dev_->bindTexture(0, tex_[6]);
	dev_->setVertexData(0, v.data(), v.size() * sizeof(float));
	dev_->bindIndexBuffer(carpetEbo_);
	dev_->drawIndexed(uint32_t(kChunks * 9 * 6));
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

	if (!dev_ || !pipe_) return;
	dev_->bindPipeline(pipe_);
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
		dev_->bindTexture(0, tex_[l]);
		dev_->setVertexData(0, verts, sizeof verts);
		dev_->bindIndexBuffer(ebo_);
		dev_->drawIndexed(uint32_t((kGrid - 1) * (kGrid - 1) * 6));
	}
}

} // namespace ow
