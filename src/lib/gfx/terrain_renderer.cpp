#include "lib/gfx/terrain_renderer.h"

#include "lib/gfx/game_lighting.h"
#include "lib/gfx/texture_cache.h"
#include "lib/vfs.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

namespace ow {

// Shadery: src/shaders/terrain.vert, terrain.frag (blok Draw: src/shaders/terrain_ubo.glsl).
namespace {
struct DrawUniforms { // std140, zgodny z terrain_ubo.glsl
	glm::mat4 viewProj, world;
	glm::vec4 lightDirAmb, tint;
	glm::ivec4 flags; // x = oświetlenie gry, y = przebieg detali
};
} // namespace

TerrainRenderer::~TerrainRenderer() {
	clear();
	if (dev_ && maskAtlas_) dev_->destroyTexture(maskAtlas_);
}

bool TerrainRenderer::init(rhi::Device& dev, std::string& err) {
	dev_ = &dev;
	rhi::PipelineDesc d;
	d.shader = "terrain";
	d.strides = {sizeof(TerrainVertex)};
	d.attribs = {{0, 0, 3, offsetof(TerrainVertex, pos)},   {1, 0, 3, offsetof(TerrainVertex, normal)},
	             {3, 0, 4, offsetof(TerrainVertex, corner)}, {4, 0, 1, offsetof(TerrainVertex, alpha)},
	             {5, 0, 1, offsetof(TerrainVertex, base)},   {6, 0, 2, offsetof(TerrainVertex, uv)}};
	d.blend = rhi::Blend::None;
	d.depth = rhi::DepthTest::Less;
	if (!(pipe_ = dev.createPipeline(d, err))) return false;
	// Detale .l2m: te same trójkąty, modulate ×2, głębia LEQUAL bez zapisu (docs/silnik/04_teren.md §4.4).
	rhi::PipelineDesc dd = d;
	dd.blend = rhi::Blend::Modulate2x;
	dd.depth = rhi::DepthTest::LessEqual;
	dd.depthWrite = false;
	if (!(pipeDetail_ = dev.createPipeline(dd, err))) return false;
	rhi::PipelineDesc dw = d;
	dw.wireframe = true;
	dw.depth = rhi::DepthTest::LessEqual;
	if (!(pipeWire_ = dev.createPipeline(dw, err))) return false;
	return true;
}

void TerrainRenderer::clear() {
	if (dev_) {
		for (auto& g : groups_) dev_->destroyBuffer(g.ebo);
		for (auto& g : detail_) dev_->destroyBuffer(g.ebo);
		if (vbo_) dev_->destroyBuffer(vbo_);
	}
	groups_.clear();
	detail_.clear();
	vbo_ = rhi::kNull;
}

void TerrainRenderer::build(const TerrainMesh& mesh, TextureCache& textures, const L2m* layer2,
                            const std::string& world) {
	clear();
	if (mesh.indices.empty() || !dev_) return;
	vbo_ = dev_->createBuffer(rhi::BufferKind::Vertex, mesh.vertices.data(), mesh.vertices.size() * sizeof(TerrainVertex));

	auto blockIndices = [&](int bx, int by, std::vector<uint32_t>& out) {
		auto [first, count] = mesh.blockRanges[size_t(by) * mesh.width + bx];
		out.insert(out.end(), mesh.indices.begin() + first, mesh.indices.begin() + first + count);
	};
	auto upload = [&](const std::vector<uint32_t>& idx, Group g, std::vector<Group>& out) {
		if (idx.empty()) return;
		g.count = uint32_t(idx.size());
		g.ebo = dev_->createBuffer(rhi::BufferKind::Index32, idx.data(), idx.size() * 4);
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
		rhi::TextureDesc d;
		d.width = d.height = 256;
		d.mipmaps = false;
		d.repeatU = d.repeatV = false;
		maskAtlas_ = dev_->createTexture(d, atlas.data());
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
}

void TerrainRenderer::draw(const glm::mat4& viewProj, const glm::mat4& world, const glm::vec3& lightDir, float ambient,
                           bool wireframe, bool gameLighting) {
	if (groups_.empty() || !dev_) return;
	DrawUniforms u{};
	u.viewProj = viewProj;
	u.world = world;
	u.lightDirAmb = glm::vec4(lightDir, ambient);
	u.tint = glm::vec4(1.f);
	u.flags = glm::ivec4(gameLighting ? 1 : 0, 0, 0, 0);
	dev_->bindPipeline(pipe_);
	dev_->setUniforms(rhi::kBindDraw, &u, sizeof u);
	dev_->bindTexture(3, maskAtlas_);
	dev_->bindVertexBuffer(0, vbo_);
	for (const auto& g : groups_) {
		dev_->bindTexture(0, g.texture);
		dev_->bindTexture(1, g.textureB);
		dev_->bindTexture(2, g.textureC);
		dev_->bindIndexBuffer(g.ebo);
		dev_->drawIndexed(g.count);
	}
	if (showDetail && !detail_.empty()) {
		u.flags.y = 1;
		dev_->bindPipeline(pipeDetail_);
		dev_->setUniforms(rhi::kBindDraw, &u, sizeof u);
		dev_->bindVertexBuffer(0, vbo_);
		for (const auto& g : detail_) {
			dev_->bindTexture(0, g.texture);
			dev_->bindIndexBuffer(g.ebo);
			dev_->drawIndexed(g.count);
		}
	}
	if (wireframe) {
		u.flags = glm::ivec4(0);
		u.tint = glm::vec4(0.f, 0.f, 0.f, 1.f);
		u.lightDirAmb.w = 1.f;
		dev_->bindPipeline(pipeWire_);
		dev_->setUniforms(rhi::kBindDraw, &u, sizeof u);
		dev_->bindVertexBuffer(0, vbo_);
		for (const auto& g : groups_) {
			dev_->bindIndexBuffer(g.ebo);
			dev_->drawIndexed(g.count);
		}
	}
}

} // namespace ow
