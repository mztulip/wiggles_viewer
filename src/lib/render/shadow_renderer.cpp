#include "lib/render/shadow_renderer.h"

#include "lib/render/texture_cache.h"
#include "lib/tga.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ow
{

namespace
{
constexpr float kLift = 2.f; // nad podłogą (jednostki modelu, y w dół), żeby nie migotał z terenem
} // namespace

ShadowRenderer::~ShadowRenderer()
{
	if (!dev_) return;
	for (auto& t : tex_)
		if (t.second) dev_->destroyTexture(t.second);
}

bool ShadowRenderer::init(rhi::Device& dev, std::string& err, TextureCache& textures)
{
	dev_ = &dev;
	textures_ = &textures;
	rhi::PipelineDesc d;
	d.shader = "shadow";
	d.strides = {5 * sizeof(float)};
	d.attribs = {{0, 0, 3, 0}, {1, 0, 2, 3 * sizeof(float)}};
	d.blend = rhi::Blend::Modulate2x; // cel · źródło · 2, shader zwraca połowę mnożnika
	d.depth = rhi::DepthTest::LessEqual;
	d.depthWrite = false;
	pipe_ = dev.createPipeline(d, err);
	return pipe_ != rhi::kNull;
}

rhi::Handle ShadowRenderer::shadowTexture(const Model3db& m, uint16_t index)
{
	auto key = std::make_pair(&m, index);
	if (auto it = tex_.find(key); it != tex_.end()) return it->second;
	Image img;
	img.width = img.height = 32;
	img.rgba.resize(32 * 32 * 4);
	const auto& s = m.shadows[index];
	for (size_t i = 0; i < s.size(); ++i)
	{
		std::memset(&img.rgba[i * 4], s[i], 3);
		img.rgba[i * 4 + 3] = 255; // nieprzezroczyste: upload rozlewa barwę na piksele z alfą 0
	}
	rhi::TextureDesc td;
	td.mipmaps = false;
	td.repeatU = td.repeatV = false;
	return tex_[key] = textures_->upload(img, td);
}

void ShadowRenderer::draw(const std::vector<Caster>& casters, const std::vector<GameLight>& lights, const glm::mat4& gameToClip)
{
	if (!pipe_ || casters.empty() || lights.empty()) return;
	dev_->bindPipeline(pipe_);
	dev_->setUniforms(rhi::kBindDraw, &gameToClip, sizeof(glm::mat4));
	std::vector<std::pair<float, float>> near; // (odległość, dx) lamp w zasięgu
	for (const auto& c : casters)
	{
		if (c.mesh >= c.model->meshes.size()) continue;
		const Mesh3db& mesh = c.model->meshes[c.mesh];
		if (mesh.shadowIndex >= c.model->shadows.size()) continue;
		near.clear();
		for (const auto& l : lights)
		{
			float dx = c.x - l.posRadius.x, d = std::hypot(dx, c.y - l.posRadius.y);
			if (d <= kShadowLightDist) near.push_back({d, dx});
		}
		if (near.empty()) continue;
		std::sort(near.begin(), near.end());
		near.resize(std::min<size_t>(near.size(), kShadowLightsPerObject));
		// shadowQuad: narożniki (x0, z0), (x1, z0), (x0, z1), (x1, z1); obraz: kolumna ↔ x, wiersz ↔ z.
		float q[12];
		std::memcpy(q, mesh.shadowQuad.data(), sizeof q);
		glm::vec3 w[4];
		float zMin = 1e30f;
		for (int k = 0; k < 4; ++k)
		{
			w[k] = glm::vec3(c.gameModel * glm::vec4(q[k * 3], -kLift, q[k * 3 + 2], 1.f));
			zMin = std::min(zMin, w[k].z);
		}
		const float uv[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
		verts_.clear();
		for (const auto& n : near)
		{
			// Pochylenie od lampy: wiersz w głębi z przesuwa się w x o σ·(z − z_tył); lampa nad obiektem
			// (dx ≈ 0) nie ma kierunku: cień zostaje pod obiektem (🔶, w grze smuga w obie strony).
			float dir = std::fabs(n.second) < 0.05f ? 0.f : (n.second > 0 ? 1.f : -1.f);
			float sh = dir * shadowShear(n.first);
			glm::vec3 p[4];
			for (int k = 0; k < 4; ++k) p[k] = w[k] + glm::vec3(sh * (w[k].z - zMin), 0.f, 0.f);
			for (int k : {0, 1, 3, 0, 3, 2})
				verts_.insert(verts_.end(), {p[k].x, p[k].y, p[k].z, uv[k][0], uv[k][1]});
		}
		dev_->bindTexture(0, shadowTexture(*c.model, mesh.shadowIndex));
		dev_->setVertexData(0, verts_.data(), verts_.size() * sizeof(float));
		dev_->draw(uint32_t(verts_.size() / 5));
	}
}

} // namespace ow
