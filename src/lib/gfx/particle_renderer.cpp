#include "lib/gfx/particle_renderer.h"

#include "lib/gfx/texture_cache.h"
#include "lib/vfs.h"

#include <algorithm>
#include <cmath>

namespace ow
{

namespace
{
constexpr float kUnitsPerBlock = 200.f;
int gridSlot(int grid)
{
	return grid <= 4 ? 0 : grid <= 8 ? 1
	                                 : 2;
}
} // namespace

ParticleRenderer::~ParticleRenderer()
{
	if (!dev_) return;
	for (auto t : tex_)
		if (t) dev_->destroyTexture(t);
}

bool ParticleRenderer::init(rhi::Device& dev, std::string& err, TextureCache& textures)
{
	dev_ = &dev;
	rhi::PipelineDesc d;
	d.shader = "particle";
	d.strides = {9 * sizeof(float)};
	d.attribs = {{0, 0, 3, 0}, {1, 0, 2, 3 * sizeof(float)}, {2, 0, 4, 5 * sizeof(float)}};
	d.blend = rhi::Blend::Additive;
	d.depth = rhi::DepthTest::LessEqual;
	d.depthWrite = false;
	if (!(pipe_ = dev.createPipeline(d, err))) return false;
	for (int i = 0; i < kParticleTextures; ++i)
	{
		auto data = textures.vfs().read(std::string("Data/Texture/Misc/") + particleTextureFile(i));
		if (!data || !loadTga(*data, img_[i])) continue;
		rhi::TextureDesc td;
		td.repeatU = td.repeatV = false;
		tex_[i] = textures.upload(img_[i], td);
	}
	return true;
}

const std::vector<bool>& ParticleRenderer::usableCells(int tex, int grid)
{
	tex = tex >= 0 && tex < kParticleTextures ? tex : 0;
	auto& u = usable_[tex][gridSlot(grid)];
	const Image& img = img_[tex];
	if (!u.empty() || img.width == 0 || grid <= 0) return u;
	u.assign(size_t(grid * grid), false);
	int cw = img.width / grid, ch = img.height / grid;
	for (int v = 0; v < grid; ++v)
		for (int c = 0; c < grid; ++c)
		{
			int lit = 0;
			for (int y = v * ch; y < (v + 1) * ch; ++y)
				for (int x = c * cw; x < (c + 1) * cw; ++x) lit += img.rgba[(size_t(y) * img.width + x) * 4 + 3] > 24;
			u[size_t(v * grid + c)] = lit * 50 > cw * ch; // co najmniej 2% widocznych pikseli
		}
	return u;
}

void ParticleRenderer::draw(const std::vector<ParticleSprite>& sprites, const ParticleType& type, const glm::mat4& gameToClip)
{
	if (!pipe_ || sprites.empty()) return;
	rhi::Handle tex = texture(type.tex);
	if (!tex) return;
	// Kierunki ekranu w układzie gry: wiersze 0 i 1 macierzy rzutowania (dla rzutu perspektywicznego
	// wiersz 0 = P00·oś x kamery, wiersz 1 = P11·oś y kamery).
	glm::vec3 right = glm::normalize(glm::vec3(gameToClip[0][0], gameToClip[1][0], gameToClip[2][0]));
	glm::vec3 up = glm::normalize(glm::vec3(gameToClip[0][1], gameToClip[1][1], gameToClip[2][1]));
	int g = std::max(1, type.grid);
	verts_.clear();
	verts_.reserve(sprites.size() * 6 * 9);
	for (const auto& s : sprites)
	{
		glm::vec3 c = glm::vec3(s.pos[0], s.pos[1], s.pos[2]) * kUnitsPerBlock;
		float h = s.size * 0.5f * kUnitsPerBlock;
		int u = s.cell % g, v = s.cell / g;
		float u0 = float(u) / g, u1 = float(u + 1) / g, v0 = float(v) / g, v1 = float(v + 1) / g; // v0 = góra komórki
		glm::vec3 p[4] = {c - right * h + up * h, c + right * h + up * h, c + right * h - up * h, c - right * h - up * h};
		float uv[4][2] = {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}};
		for (int k : {0, 1, 2, 0, 2, 3})
		{
			verts_.insert(verts_.end(), {p[k].x, p[k].y, p[k].z, uv[k][0], uv[k][1], s.rgba[0], s.rgba[1], s.rgba[2], s.rgba[3]});
		}
	}
	dev_->bindPipeline(pipe_);
	dev_->setUniforms(rhi::kBindDraw, &gameToClip, sizeof(glm::mat4));
	dev_->bindTexture(0, tex);
	dev_->setVertexData(0, verts_.data(), verts_.size() * sizeof(float));
	dev_->draw(uint32_t(verts_.size() / 9));
}

} // namespace ow
