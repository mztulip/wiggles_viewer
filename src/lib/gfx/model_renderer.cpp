#include "lib/gfx/model_renderer.h"

#include "lib/gfx/texture_cache.h"

#include <cstring>
#include <glm/gtc/type_ptr.hpp>

namespace ow {

static const char* kVS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in float aByte;
uniform mat4 uViewProj;
uniform mat4 uModel;
uniform vec3 uSphere[256];
out vec2 vUV;
out float vByte;
out vec3 vNormal;
void main() {
	vUV = aUV;
	vByte = aByte / 255.0;
	vNormal = mat3(uModel) * uSphere[int(aByte + 0.5)];
	gl_Position = uViewProj * uModel * vec4(aPos, 1.0);
}
)";

static const char* kFS = R"(#version 330 core
in vec2 vUV;
in float vByte;
in vec3 vNormal;
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
	if (uMode == 1) c *= vByte;
	else if (uMode == 2) {
		vec3 n = normalize(vNormal);
		c *= uAmbient + (1.0 - uAmbient) * max(dot(n, uLightDir), 0.0);
	} else if (uMode == 3) c = normalize(vNormal) * 0.5 + 0.5;
	oColor = vec4(c, t.a) * uTint;
}
)";

std::vector<Vec3f> parseSphereDat(const std::vector<uint8_t>& data) {
	std::vector<Vec3f> v;
	if (data.size() != 256 * 12) return v;
	v.resize(256);
	std::memcpy(v.data(), data.data(), data.size());
	return v;
}

ModelRenderer::~ModelRenderer() { clearCache(); }

bool ModelRenderer::init(std::string& err) { return prog_.build(kVS, kFS, err); }

void ModelRenderer::setModel(const Model3db* model, TextureCache* textures) {
	clearCache();
	model_ = model;
	textures_ = textures;
}

void ModelRenderer::setSphere(const std::vector<Vec3f>& sphere) { sphere_ = sphere; }

void ModelRenderer::clearCache() {
	for (auto& [k, links] : cache_)
		for (auto& l : links) {
			gl::DeleteVertexArrays(1, &l.vao);
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
	struct Vertex {
		float pos[3];
		float uv[2];
		float byte;
	};
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
		gl::GenVertexArrays(1, &g.vao);
		gl::BindVertexArray(g.vao);
		gl::GenBuffers(1, &g.vbo);
		gl::BindBuffer(GL_ARRAY_BUFFER, g.vbo);
		gl::BufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);
		gl::GenBuffers(1, &g.ebo);
		gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, g.ebo);
		gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, tris.size() * sizeof(uint16_t), tris.data(), GL_STATIC_DRAW);
		gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
		gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
		gl::VertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, byte));
		gl::EnableVertexAttribArray(0);
		gl::EnableVertexAttribArray(1);
		gl::EnableVertexAttribArray(2);
		gl::BindVertexArray(0);
		out.push_back(g);
	}
	return out;
}

void ModelRenderer::drawFrame(uint32_t meshIndex, const glm::mat4& viewProj, const glm::mat4& model,
                              const ModelDrawOptions& opt) {
	if (!model_ || meshIndex >= model_->meshes.size()) return;
	const auto& links = frame(meshIndex);

	prog_.use();
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
	for (const auto& l : links) {
		GLuint tex = textures_ ? textures_->get(model_->materials[l.material].name).id : 0;
		gl::BindTexture(GL_TEXTURE_2D, tex);
		gl::BindVertexArray(l.vao);
		gl::DrawElements(GL_TRIANGLES, l.count, GL_UNSIGNED_SHORT, nullptr);
	}
	if (opt.wireframe) {
		gl::PolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		gl::Uniform1i(prog_.uniform("uMode"), 0);
		gl::Uniform4f(prog_.uniform("uTint"), 0, 0, 0, 1);
		gl::Uniform1i(prog_.uniform("uAlphaTest"), 0);
		for (const auto& l : links) {
			gl::BindVertexArray(l.vao);
			gl::DrawElements(GL_TRIANGLES, l.count, GL_UNSIGNED_SHORT, nullptr);
		}
		gl::PolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
	gl::BindVertexArray(0);
}

} // namespace ow
