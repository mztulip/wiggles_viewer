#include "lib/gfx/fluid_renderer.h"

#include "lib/gfx/texture_cache.h"
#include "lib/tga.h"
#include "lib/vfs.h"

#include <algorithm>

namespace ow {

namespace {
constexpr float kUnitsPerBlock = 200.f;
constexpr float kFrontZ = 14.4f; // przednia ściana płynu (bloki głębokości)
int medium(uint8_t type) { return type == 2 ? 1 : (type == 3 || type == 4) ? 2 : 0; }
} // namespace

FluidRenderer::~FluidRenderer() {
	if (!dev_) return;
	for (auto t : tex_)
		if (t) dev_->destroyTexture(t);
}

bool FluidRenderer::init(rhi::Device& dev, std::string& err, TextureCache& textures) {
	dev_ = &dev;
	rhi::PipelineDesc d;
	d.shader = "fluid";
	d.strides = {9 * sizeof(float)};
	d.attribs = {{0, 0, 3, 0}, {1, 0, 2, 3 * sizeof(float)}, {2, 0, 4, 5 * sizeof(float)}};
	d.blend = rhi::Blend::Alpha;
	d.depth = rhi::DepthTest::LessEqual;
	d.depthWrite = false;
	if (!(pipe_ = dev.createPipeline(d, err))) return false;
	const char* files[3] = {"wasser_env.tga", "schwef_nahtlos.tga", "lava_nahtlos.tga"};
	for (int i = 0; i < 3; ++i) {
		Image img;
		auto data = textures.vfs().read(std::string("Data/Texture/m064/") + files[i]);
		if (!data || !loadTga(*data, img)) continue;
		rhi::TextureDesc td;
		tex_[i] = textures.upload(img, td);
	}
	return true;
}

int FluidRenderer::build(int W, int H, const std::vector<uint8_t>& type, const std::vector<float>& fill,
                         const std::vector<uint32_t>& blocks, const BlockDb& db, const std::vector<bool>* visible) {
	clear();
	if (type.size() != size_t(W) * H || fill.size() != type.size()) return 0;
	int cells = 0;
	auto has = [&](int x, int y) {
		if (x < 0 || y < 0 || x >= W || y >= H) return false;
		size_t i = size_t(y) * W + x;
		return type[i] != 0 && fill[i] > 0.001f;
	};
	for (int y = 0; y < H; ++y)
		for (int x = 0; x < W; ++x) {
			if (!has(x, y)) continue;
			size_t i = size_t(y) * W + x;
			if (visible && !(*visible)[i]) continue;
			FluidShape s = db.fluid(blocks[i]);
			if (!s.valid()) s.upper = 8, s.lower = 0;
			float f = fill[i];
			// Powierzchnia: granica stała y + upper/8, płyn sięga w górę o (upper − lower)·min(f, 1)/8.
			float bottom = float(y) + float(s.upper) / 8.f;
			float top = bottom + float(s.lower - s.upper) * std::min(f, 1.f) / 8.f;
			if (bottom - top < 0.002f) continue;
			int m = medium(type[i]);
			// Przezroczystość: woda i lawa min(255, max(f − 1, 1)·64), siarka stałe 64.
			float a = m == 1 ? 64.f / 255.f : std::min(255.f, std::max(f - 1.f, 1.f) * 64.f) / 255.f;
			float c = 0.75f; // 🔶 jasność bez oświetlenia gry
			auto& v = verts_[m];
			auto quad = [&](glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec2 t0, glm::vec2 t1, glm::vec2 t2, glm::vec2 t3) {
				glm::vec3 p[4] = {p0, p1, p2, p3};
				glm::vec2 t[4] = {t0, t1, t2, t3};
				for (int k : {0, 1, 2, 0, 2, 3}) {
					glm::vec3 q = p[k] * kUnitsPerBlock;
					v.insert(v.end(), {q.x, q.y, q.z, t[k].x, t[k].y, c, c, c, a});
				}
			};
			float x0 = float(x), x1 = float(x + 1);
			// Przednia ściana.
			quad({x0, top, kFrontZ}, {x1, top, kFrontZ}, {x1, bottom, kFrontZ}, {x0, bottom, kFrontZ},
			     {x0 * .5f, top * .5f}, {x1 * .5f, top * .5f}, {x1 * .5f, bottom * .5f}, {x0 * .5f, bottom * .5f});
			// Odsłonięta powierzchnia: nad polem nie ma płynu albo pole nie jest pełne.
			if (!has(x, y - 1) || f < 1.f) {
				int strips = (16 - (db.variant(blocks[i]) & 15)) / 2;
				float zb = kFrontZ - float(strips);
				quad({x0, top, zb}, {x1, top, zb}, {x1, top, kFrontZ}, {x0, top, kFrontZ},
				     {x0 * .5f, zb * .5f}, {x1 * .5f, zb * .5f}, {x1 * .5f, kFrontZ * .5f}, {x0 * .5f, kFrontZ * .5f});
			}
			++cells;
		}
	return cells;
}

void FluidRenderer::draw(const glm::mat4& gameToClip, float time) {
	if (!pipe_) return;
	struct {
		glm::mat4 mvp;
		glm::vec4 uv;
	} u{gameToClip, glm::vec4(time * 0.02f, time * 0.01f, 0, 0)};
	for (int m = 0; m < 3; ++m) {
		if (verts_[m].empty() || !tex_[m]) continue;
		dev_->bindPipeline(pipe_);
		dev_->setUniforms(rhi::kBindDraw, &u, sizeof u);
		dev_->bindTexture(0, tex_[m]);
		dev_->setVertexData(0, verts_[m].data(), verts_[m].size() * sizeof(float));
		dev_->draw(uint32_t(verts_[m].size() / 9));
	}
}

} // namespace ow
