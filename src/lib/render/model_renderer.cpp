#include "lib/render/model_renderer.h"

#include "lib/render/texture_cache.h"

#include <cstring>
#include <glm/gtc/type_ptr.hpp>
#include <unordered_map>

namespace ow
{

// Shadery: src/shaders/model.vert, model.frag (blok uniformów Draw: src/shaders/model_ubo.glsl).
namespace
{
// Blok "Draw" (std140), zgodny z src/shaders/model_ubo.glsl.
struct DrawUniforms
{
	glm::mat4 viewProj, model, gameModel;
	glm::vec4 horizon, lightDirAmb, tint, params;
	glm::ivec4 flags;
};
// Potoki wspólne dla wszystkich ModelRenderer na danym urządzeniu.
struct Shared
{
	rhi::Handle pipe = rhi::kNull, wire = rhi::kNull, add = rhi::kNull;
};
std::unordered_map<rhi::Device*, Shared> gShared;
} // namespace

namespace
{
struct Vertex
{
	float pos[3];
	float uv[2];
	float byte; // indeks normalnej (0..255)
};
} // namespace

ModelRenderer::~ModelRenderer()
{
	clearCache();
	if (dev_ && sphereBuf_) dev_->destroyBuffer(sphereBuf_);
}

bool ModelRenderer::init(rhi::Device& dev, std::string& err)
{
	dev_ = &dev;
	Shared& sh = gShared[&dev];
	if (!sh.pipe)
	{
		rhi::PipelineDesc d;
		d.shader = "model";
		d.strides = {sizeof(Vertex), sizeof(Vertex)}; // wiązanie 0 = klatka A, 1 = klatka B
		d.attribs = {{0, 0, 3, offsetof(Vertex, pos)}, {1, 0, 2, offsetof(Vertex, uv)}, {2, 0, 1, offsetof(Vertex, byte)},
		    {3, 1, 3, offsetof(Vertex, pos)}, {4, 1, 1, offsetof(Vertex, byte)}};
		d.blend = rhi::Blend::Alpha;
		d.depth = rhi::DepthTest::Less;
		sh.pipe = dev.createPipeline(d, err);
		if (!sh.pipe) return false;
		// Tryb 2 (addytywny): blask, bez zapisu głębi, rysowany po pozostałych linkach modelu.
		d.blend = rhi::Blend::Additive;
		d.depthWrite = false;
		sh.add = dev.createPipeline(d, err);
		if (!sh.add) return false;
		d.blend = rhi::Blend::Alpha;
		d.depthWrite = true;
		d.wireframe = true;
		sh.wire = dev.createPipeline(d, err);
		if (!sh.wire) return false;
	}
	pipe_ = sh.pipe;
	pipeWire_ = sh.wire;
	pipeAdd_ = sh.add;
	setSphere(sphere_);
	return true;
}

bool ModelRenderer::canInterpolate(uint32_t meshA, uint32_t meshB) const
{
	return model_ && sameTopology(*model_, meshA, meshB);
}

void ModelRenderer::setModel(const Model3db* model, TextureCache* textures)
{
	clearCache();
	model_ = model;
	textures_ = textures;
}

void ModelRenderer::setSphere(const std::vector<Vec3f>& sphere)
{
	sphere_ = sphere;
	if (!dev_) return;
	std::vector<glm::vec4> v(256, glm::vec4(0.f, 1.f, 0.f, 0.f));
	if (sphere_.size() == 256)
		for (int i = 0; i < 256; ++i) v[size_t(i)] = glm::vec4(sphere_[size_t(i)].x, sphere_[size_t(i)].y, sphere_[size_t(i)].z, 0.f);
	if (sphereBuf_) dev_->destroyBuffer(sphereBuf_);
	sphereBuf_ = dev_->createBuffer(rhi::BufferKind::Uniform, v.data(), v.size() * sizeof(glm::vec4));
}

void ModelRenderer::clearCache()
{
	if (dev_)
		for (auto& [k, links] : cache_)
			for (auto& l : links)
			{
				dev_->destroyBuffer(l.vbo);
				dev_->destroyBuffer(l.ebo);
			}
	cache_.clear();
}

const std::vector<ModelRenderer::GpuLink>& ModelRenderer::frame(uint32_t meshIndex)
{
	auto it = cache_.find(meshIndex);
	if (it != cache_.end()) return it->second;
	auto& out = cache_[meshIndex];
	const Mesh3db& mesh = model_->meshes[meshIndex];
	for (const MeshLink& l : mesh.links)
	{
		const auto& pts = model_->points[l.points];
		const auto& uvs = model_->texcoords[l.texcoords];
		const auto& nrm = model_->normals[l.normals];
		const auto& tris = model_->triangles[l.triangles];
		if (pts.empty() || tris.empty()) continue;
		std::vector<Vertex> verts(pts.size());
		for (size_t i = 0; i < pts.size(); ++i)
		{
			Vec3f p = Model3db::dequant(pts[i]);
			verts[i] = {{p.x, p.y, p.z},
			    {i < uvs.size() ? uvs[i].x : 0.f, i < uvs.size() ? uvs[i].y : 0.f},
			    float(i < nrm.size() ? nrm[i] : 0)};
		}
		GpuLink g;
		g.material = l.material;
		g.flags = l.flags;
		g.count = uint32_t(tris.size());
		g.vbo = dev_->createBuffer(rhi::BufferKind::Vertex, verts.data(), verts.size() * sizeof(Vertex));
		g.ebo = dev_->createBuffer(rhi::BufferKind::Index16, tris.data(), tris.size() * sizeof(uint16_t));
		out.push_back(g);
	}
	return out;
}

void ModelRenderer::drawFrames(uint32_t meshA, uint32_t meshB, float t, const glm::mat4& viewProj,
    const glm::mat4& model, const ModelDrawOptions& opt)
{
	if (!model_ || meshA >= model_->meshes.size()) return;
	if (meshB >= model_->meshes.size() || (meshB != meshA && !canInterpolate(meshA, meshB)))
	{
		meshB = meshA;
		t = 0.f;
	}
	const auto& linksA = frame(meshA);
	const auto& linksB = frame(meshB);
	// Puste linki są pomijane w frame(), więc przy zgodnej topologii listy mają tę samą długość.
	if (linksA.size() != linksB.size()) t = 0.f;
	auto other = [&](size_t i) -> const GpuLink& { return t > 0.f ? linksB[i] : linksA[i]; };

	if (!dev_ || !pipe_) return;
	DrawUniforms u{};
	u.viewProj = viewProj;
	u.model = model;
	u.gameModel = opt.gameModel;
	u.horizon = opt.horizon;
	u.lightDirAmb = glm::vec4(opt.lightDir, opt.ambient);
	u.tint = glm::vec4(1.f);
	u.params = glm::vec4(t, opt.horizonZOffset, opt.base, 0.f);
	u.flags = glm::ivec4(int(opt.lighting), opt.alphaTest ? 1 : 0, opt.gameLights ? 1 : 0, 0);
	// Dwa przebiegi: najpierw linki zwykłe (tryby 1, 3–11), potem addytywne (2); tryb 0 nie jest rysowany.
	for (int pass = 0; pass < 2; ++pass)
	{
		bool pipeBound = false;
		for (size_t i = 0; i < linksA.size(); ++i)
		{
			const auto& l = linksA[i];
			int mode = opt.drawModes ? l.flags : 1;
			if (mode == 0 || (mode == 2) != (pass == 1)) continue;
			if (!pipeBound)
			{
				dev_->bindPipeline(pass == 0 ? pipe_ : pipeAdd_);
				dev_->bindUniformBuffer(rhi::kBindStatic, sphereBuf_);
				pipeBound = true;
			}
			u.flags.w = mode;
			dev_->setUniforms(rhi::kBindDraw, &u, sizeof u);
			dev_->bindTexture(0, textures_ ? textures_->get(model_->materials[l.material].name).id : rhi::kNull);
			dev_->bindVertexBuffer(0, l.vbo);
			dev_->bindVertexBuffer(1, other(i).vbo);
			dev_->bindIndexBuffer(l.ebo);
			dev_->drawIndexed(l.count);
		}
	}
	if (opt.wireframe)
	{
		u.flags = glm::ivec4(0, 0, 0, 0);
		u.tint = glm::vec4(0.f, 0.f, 0.f, 1.f);
		dev_->bindPipeline(pipeWire_);
		dev_->setUniforms(rhi::kBindDraw, &u, sizeof u);
		dev_->bindUniformBuffer(rhi::kBindStatic, sphereBuf_);
		for (size_t i = 0; i < linksA.size(); ++i)
		{
			dev_->bindVertexBuffer(0, linksA[i].vbo);
			dev_->bindVertexBuffer(1, other(i).vbo);
			dev_->bindIndexBuffer(linksA[i].ebo);
			dev_->drawIndexed(linksA[i].count);
		}
	}
}

} // namespace ow
