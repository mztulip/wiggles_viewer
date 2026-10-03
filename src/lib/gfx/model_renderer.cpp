#include "lib/gfx/model_renderer.h"

#include "lib/gfx/texture_cache.h"

#include <cstring>
#include <glm/gtc/type_ptr.hpp>

namespace ow {

static const char* kVSHead = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in float aByte;
layout(location = 3) in vec3 aPos2;  // pozycja w klatce B
layout(location = 4) in float aByte2; // normalna w klatce B
uniform float uLerp;
uniform mat4 uViewProj;
uniform mat4 uModel;
uniform vec3 uSphere[256];
out vec2 vUV;
out float vByte;
out vec3 vNormal;
out float vGameZ;
out vec3 vGameDiffuse;
out vec3 vGameSpec;
uniform float uHorizonZOffset;
uniform mat4 uGameModel;
uniform float uBase;
)";

static const char* kVSBody = R"(
void main() {
	vUV = aUV;
	vByte = mix(aByte, aByte2, uLerp) / 255.0;
	vec3 n = normalize(mix(uSphere[int(aByte + 0.5)], uSphere[int(aByte2 + 0.5)], uLerp));
	vNormal = mat3(uModel) * n;
	vec3 mp = mix(aPos, aPos2, uLerp);
	if (uGameLight == 1) {
		vec3 gp = (uGameModel * vec4(mp, 1.0)).xyz / 200.0;
		// 🔶 Oświetlenie dwustronne, ustalone porównaniem ze zrzutami gry (OBERWELT): część modeli
		// (np. drzewa Obw_baum) ma normalne skierowane do środka i w grze jest jasna, a namioty
		// z normalnymi na zewnątrz też. Bierzemy jaśniejszy wynik dla N i −N.
		vec3 gn = normalize(mat3(uGameModel) * n), d1, s1, d2, s2;
		gameLighting(gp, gn, uBase, d1, s1);
		gameLighting(gp, -gn, uBase, d2, s2);
		bool first = dot(d1 + s1, vec3(1.0)) >= dot(d2 + s2, vec3(1.0));
		vGameDiffuse = first ? d1 : d2;
		vGameSpec = first ? s1 : s2;
	}
	vec4 wp = uModel * vec4(mp, 1.0);
	vGameZ = wp.z + uHorizonZOffset;
	gl_Position = uViewProj * wp;
}
)";

static const char* kFS = R"(#version 330 core
in vec2 vUV;
in float vByte;
in vec3 vNormal;
in float vGameZ;
in vec3 vGameDiffuse;
in vec3 vGameSpec;
uniform int uGameLight;
uniform vec4 uHorizon;
uniform sampler2D uTex;
uniform int uMode;
uniform int uAlphaTest;
uniform vec3 uLightDir;
uniform float uAmbient;
uniform vec4 uTint;
out vec4 oColor;
void main() {
	vec4 t = texture(uTex, vUV);
	if (uAlphaTest != 0 && t.a < 0.5) discard;
	vec3 c = t.rgb;
	if (uGameLight == 1) c = c * vGameDiffuse + vGameSpec;
	else if (uMode == 1) c *= vByte;
	else if (uMode == 2) {
		vec3 n = normalize(vNormal);
		c *= uAmbient + (1.0 - uAmbient) * max(dot(n, uLightDir), 0.0);
	} else if (uMode == 3) c = normalize(vNormal) * 0.5 + 0.5;
	float zb = vGameZ / 200.0; // bloki
	if (uHorizon.a > 0.0 && zb < 0.0) c = mix(c, uHorizon.rgb, clamp(-zb / uHorizon.a, 0.0, 1.0));
	oColor = vec4(c, t.a) * uTint;
}
)";

namespace {
struct Vertex {
	float pos[3];
	float uv[2];
	float byte; // indeks normalnej (0..255)
};
} // namespace

ModelRenderer::~ModelRenderer() {
	clearCache();
	if (vao_) gl::DeleteVertexArrays(1, &vao_);
}

bool ModelRenderer::init(std::string& err) {
	std::string vs = std::string(kVSHead) + kGameLightingGlsl + kVSBody;
	if (!prog_.build(vs.c_str(), kFS, err)) return false;
	if (!vao_) gl::GenVertexArrays(1, &vao_);
	return true;
}

bool ModelRenderer::canInterpolate(uint32_t meshA, uint32_t meshB) const {
	return model_ && sameTopology(*model_, meshA, meshB);
}

void ModelRenderer::setModel(const Model3db* model, TextureCache* textures) {
	clearCache();
	model_ = model;
	textures_ = textures;
}

void ModelRenderer::setSphere(const std::vector<Vec3f>& sphere) { sphere_ = sphere; }

void ModelRenderer::clearCache() {
	for (auto& [k, links] : cache_)
		for (auto& l : links) {
			gl::DeleteBuffers(1, &l.vbo);
			gl::DeleteBuffers(1, &l.ebo);
		}
	cache_.clear();
}

const std::vector<ModelRenderer::GpuLink>& ModelRenderer::frame(uint32_t meshIndex) {
	auto it = cache_.find(meshIndex);
	if (it != cache_.end()) return it->second;
	auto& out = cache_[meshIndex];
	const Mesh3db& mesh = model_->meshes[meshIndex];
	for (const MeshLink& l : mesh.links) {
		const auto& pts = model_->points[l.points];
		const auto& uvs = model_->texcoords[l.texcoords];
		const auto& nrm = model_->normals[l.normals];
		const auto& tris = model_->triangles[l.triangles];
		if (pts.empty() || tris.empty()) continue;
		std::vector<Vertex> verts(pts.size());
		for (size_t i = 0; i < pts.size(); ++i) {
			Vec3f p = Model3db::dequant(pts[i]);
			verts[i] = {{p.x, p.y, p.z},
			            {i < uvs.size() ? uvs[i].x : 0.f, i < uvs.size() ? uvs[i].y : 0.f},
			            float(i < nrm.size() ? nrm[i] : 0)};
		}
		GpuLink g;
		g.material = l.material;
		g.count = GLsizei(tris.size());
		gl::GenBuffers(1, &g.vbo);
		gl::BindBuffer(GL_ARRAY_BUFFER, g.vbo);
		gl::BufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);
		gl::GenBuffers(1, &g.ebo);
		gl::BindVertexArray(vao_); // w core profile EBO wymaga podpiętego VAO; i tak przepinamy go w bindLink
		gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, g.ebo);
		gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, tris.size() * sizeof(uint16_t), tris.data(), GL_STATIC_DRAW);
		out.push_back(g);
	}
	return out;
}

void ModelRenderer::bindLink(const GpuLink& a, const GpuLink& b) {
	gl::BindVertexArray(vao_);
	gl::BindBuffer(GL_ARRAY_BUFFER, a.vbo);
	gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
	gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
	gl::VertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, byte));
	gl::BindBuffer(GL_ARRAY_BUFFER, b.vbo);
	gl::VertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
	gl::VertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, byte));
	for (GLuint i = 0; i < 5; ++i) gl::EnableVertexAttribArray(i);
	gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, a.ebo);
}

void ModelRenderer::drawFrames(uint32_t meshA, uint32_t meshB, float t, const glm::mat4& viewProj,
                               const glm::mat4& model, const ModelDrawOptions& opt) {
	if (!model_ || meshA >= model_->meshes.size()) return;
	if (meshB >= model_->meshes.size() || (meshB != meshA && !canInterpolate(meshA, meshB))) {
		meshB = meshA;
		t = 0.f;
	}
	const auto& linksA = frame(meshA);
	const auto& linksB = frame(meshB);
	// Puste linki są pomijane w frame(), więc przy zgodnej topologii listy mają tę samą długość.
	if (linksA.size() != linksB.size()) t = 0.f;
	auto other = [&](size_t i) -> const GpuLink& { return t > 0.f ? linksB[i] : linksA[i]; };

	prog_.use();
	gl::Uniform1f(prog_.uniform("uLerp"), t);
	gl::Uniform4f(prog_.uniform("uHorizon"), opt.horizon.r, opt.horizon.g, opt.horizon.b, opt.horizon.a);
	gl::Uniform1f(prog_.uniform("uHorizonZOffset"), opt.horizonZOffset);
	uploadGameLights(prog_, opt.gameLights != nullptr, opt.gameLights);
	gl::UniformMatrix4fv(prog_.uniform("uGameModel"), 1, GL_FALSE, glm::value_ptr(opt.gameModel));
	gl::Uniform1f(prog_.uniform("uBase"), opt.base);
	gl::UniformMatrix4fv(prog_.uniform("uViewProj"), 1, GL_FALSE, glm::value_ptr(viewProj));
	gl::UniformMatrix4fv(prog_.uniform("uModel"), 1, GL_FALSE, glm::value_ptr(model));
	if (sphere_.size() == 256)
		gl::Uniform3fv(prog_.uniform("uSphere"), 256, &sphere_[0].x);
	else {
		std::vector<float> up(256 * 3, 0.f);
		for (int i = 0; i < 256; ++i) up[i * 3 + 1] = 1.f;
		gl::Uniform3fv(prog_.uniform("uSphere"), 256, up.data());
	}
	gl::Uniform1i(prog_.uniform("uTex"), 0);
	gl::Uniform1i(prog_.uniform("uMode"), int(opt.lighting));
	gl::Uniform1i(prog_.uniform("uAlphaTest"), opt.alphaTest ? 1 : 0);
	gl::Uniform3f(prog_.uniform("uLightDir"), opt.lightDir.x, opt.lightDir.y, opt.lightDir.z);
	gl::Uniform1f(prog_.uniform("uAmbient"), opt.ambient);
	gl::Uniform4f(prog_.uniform("uTint"), 1, 1, 1, 1);

	gl::Enable(GL_DEPTH_TEST);
	gl::Disable(GL_CULL_FACE);
	gl::Enable(GL_BLEND);
	gl::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	gl::ActiveTexture(GL_TEXTURE0);
	for (size_t i = 0; i < linksA.size(); ++i) {
		const auto& l = linksA[i];
		GLuint tex = textures_ ? textures_->get(model_->materials[l.material].name).id : 0;
		gl::BindTexture(GL_TEXTURE_2D, tex);
		bindLink(l, other(i));
		gl::DrawElements(GL_TRIANGLES, l.count, GL_UNSIGNED_SHORT, nullptr);
	}
	if (opt.wireframe) {
		gl::PolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		gl::Uniform1i(prog_.uniform("uMode"), 0);
		gl::Uniform4f(prog_.uniform("uTint"), 0, 0, 0, 1);
		gl::Uniform1i(prog_.uniform("uAlphaTest"), 0);
		for (size_t i = 0; i < linksA.size(); ++i) {
			bindLink(linksA[i], other(i));
			gl::DrawElements(GL_TRIANGLES, linksA[i].count, GL_UNSIGNED_SHORT, nullptr);
		}
		gl::PolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
	gl::BindVertexArray(0);
}

} // namespace ow
