// Backend OpenGL 4.5 core interfejsu rhi.h.
#ifdef OW_RHI_GL

#include "lib/render/gl.h"
#include "lib/render/rhi/rhi.h"

#include <SDL.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>

#include <algorithm>
#include <cstring>
#include <unordered_map>

namespace ow::rhi
{

namespace
{

constexpr size_t kUniformRing = 8u << 20, kVertexRing = 8u << 20, kZeroBlock = 8192;

struct GlTexture
{
	GLuint id = 0;
};
struct GlBuffer
{
	GLuint id = 0;
	BufferKind kind = BufferKind::Vertex;
};
struct GlPipeline
{
	GLuint program = 0;
	PipelineDesc desc;
};

GLuint compileStage(GLenum type, const char* src, const std::string& what, std::string& err)
{
	GLuint s = gl::CreateShader(type);
	gl::ShaderSource(s, 1, &src, nullptr);
	gl::CompileShader(s);
	GLint ok = 0;
	gl::GetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok)
	{
		GLint len = 0;
		gl::GetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
		std::vector<char> log(size_t(len) + 1);
		gl::GetShaderInfoLog(s, len, nullptr, log.data());
		err = what + ": " + log.data();
		gl::DeleteShader(s);
		return 0;
	}
	return s;
}

class GlDevice final : public Device
{
public:
	GlDevice(SDL_Window* w, SDL_GLContext ctx) : window_(w), ctx_(ctx) {}
	~GlDevice() override
	{
		for (auto& [h, t] : textures_) gl::DeleteTextures(1, &t.id);
		for (auto& [h, b] : buffers_) gl::DeleteBuffers(1, &b.id);
		for (auto& [h, p] : pipelines_) gl::DeleteProgram(p.program);
		GLuint bufs[2] = {uniformRing_, vertexRing_};
		gl::DeleteBuffers(2, bufs);
		gl::DeleteVertexArrays(1, &vao_);
		SDL_GL_DeleteContext(ctx_);
	}

	bool init(std::string& err)
	{
		if (const char* missing = gl::load(SDL_GL_GetProcAddress))
		{
			err = std::string("brak funkcji OpenGL: ") + missing;
			return false;
		}
		GLint align = 256;
		gl::GetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &align);
		uboAlign_ = size_t(std::max(align, 16));
		gl::GenVertexArrays(1, &vao_);
		gl::BindVertexArray(vao_);
		gl::GenBuffers(1, &uniformRing_);
		gl::BindBuffer(GL_UNIFORM_BUFFER, uniformRing_);
		gl::BufferData(GL_UNIFORM_BUFFER, GLsizeiptr(kUniformRing), nullptr, GL_STREAM_DRAW);
		gl::GenBuffers(1, &vertexRing_);
		gl::BindBuffer(GL_ARRAY_BUFFER, vertexRing_);
		gl::BufferData(GL_ARRAY_BUFFER, GLsizeiptr(kVertexRing), nullptr, GL_STREAM_DRAW);
		const uint8_t white[4] = {255, 255, 255, 255};
		TextureDesc d;
		d.width = d.height = 1;
		d.mipmaps = false;
		white_ = createTexture(d, white);
		return true;
	}

	Backend backend() const override { return Backend::OpenGL; }
	std::string description() const override
	{
		return std::string("OpenGL ") + (const char*)gl::GetString(GL_VERSION) + " / " +
		    (const char*)gl::GetString(GL_RENDERER);
	}

	Handle createTexture(const TextureDesc& d, const uint8_t* rgba) override
	{
		GlTexture t;
		gl::GenTextures(1, &t.id);
		Handle h = next_++;
		textures_[h] = t;
		updateTexture(h, d, rgba);
		return h;
	}
	void updateTexture(Handle h, const TextureDesc& d, const uint8_t* rgba) override
	{
		auto it = textures_.find(h);
		if (it == textures_.end()) return;
		gl::ActiveTexture(GL_TEXTURE0);
		gl::BindTexture(GL_TEXTURE_2D, it->second.id);
		gl::PixelStorei(GL_UNPACK_ALIGNMENT, 1);
		gl::TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, d.width, d.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
		gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, d.repeatU ? GL_REPEAT : GL_CLAMP_TO_EDGE);
		gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, d.repeatV ? GL_REPEAT : GL_CLAMP_TO_EDGE);
		gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, d.mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
		if (d.mipmaps) gl::GenerateMipmap(GL_TEXTURE_2D);
	}
	void destroyTexture(Handle h) override
	{
		auto it = textures_.find(h);
		if (it == textures_.end()) return;
		gl::DeleteTextures(1, &it->second.id);
		textures_.erase(it);
	}

	Handle createBuffer(BufferKind kind, const void* data, size_t size) override
	{
		GlBuffer b;
		b.kind = kind;
		gl::GenBuffers(1, &b.id);
		GLenum target = kind == BufferKind::Uniform ? GL_UNIFORM_BUFFER : GL_ARRAY_BUFFER;
		gl::BindBuffer(target, b.id);
		gl::BufferData(target, GLsizeiptr(size), data, GL_STATIC_DRAW);
		Handle h = next_++;
		buffers_[h] = b;
		return h;
	}
	void destroyBuffer(Handle h) override
	{
		auto it = buffers_.find(h);
		if (it == buffers_.end()) return;
		gl::DeleteBuffers(1, &it->second.id);
		buffers_.erase(it);
	}

	Handle createPipeline(const PipelineDesc& d, std::string& err) override
	{
		const char* vs = shaderGlsl(d.shader, "vert");
		const char* fs = shaderGlsl(d.shader, "frag");
		if (!vs || !fs)
		{
			err = "brak shadera " + d.shader;
			return kNull;
		}
		GLuint v = compileStage(GL_VERTEX_SHADER, vs, d.shader + ".vert", err);
		if (!v) return kNull;
		GLuint f = compileStage(GL_FRAGMENT_SHADER, fs, d.shader + ".frag", err);
		if (!f)
		{
			gl::DeleteShader(v);
			return kNull;
		}
		GLuint p = gl::CreateProgram();
		gl::AttachShader(p, v);
		gl::AttachShader(p, f);
		gl::LinkProgram(p);
		gl::DeleteShader(v);
		gl::DeleteShader(f);
		GLint ok = 0;
		gl::GetProgramiv(p, GL_LINK_STATUS, &ok);
		if (!ok)
		{
			GLint len = 0;
			gl::GetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
			std::vector<char> log(size_t(len) + 1);
			gl::GetProgramInfoLog(p, len, nullptr, log.data());
			err = d.shader + " link: " + log.data();
			gl::DeleteProgram(p);
			return kNull;
		}
		Handle h = next_++;
		pipelines_[h] = {p, d};
		return h;
	}

	bool beginFrame(int width, int height, float r, float g, float b) override
	{
		if (width <= 0 || height <= 0) return false;
		fbW_ = width;
		fbH_ = height;
		// Bufory pierścieniowe: porzucenie starej zawartości (sterownik nie czeka na GPU).
		gl::BindBuffer(GL_UNIFORM_BUFFER, uniformRing_);
		gl::BufferData(GL_UNIFORM_BUFFER, GLsizeiptr(kUniformRing), nullptr, GL_STREAM_DRAW);
		std::vector<uint8_t> zero(kZeroBlock, 0);
		gl::BufferSubData(GL_UNIFORM_BUFFER, 0, GLsizeiptr(kZeroBlock), zero.data());
		uniformOff_ = kZeroBlock;
		gl::BindBuffer(GL_ARRAY_BUFFER, vertexRing_);
		gl::BufferData(GL_ARRAY_BUFFER, GLsizeiptr(kVertexRing), nullptr, GL_STREAM_DRAW);
		vertexOff_ = 0;
		for (int i = 0; i < 3; ++i) gl::BindBufferRange(GL_UNIFORM_BUFFER, GLuint(i), uniformRing_, 0, GLsizeiptr(kZeroBlock));
		for (int s = 0; s < kTextureSlots; ++s) bindTexture(s, kNull);
		gl::BindVertexArray(vao_);
		gl::Viewport(0, 0, width, height);
		gl::DepthMask(GL_TRUE);
		gl::ClearColor(r, g, b, 1.f);
		gl::Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		return true;
	}
	void setViewport(int x, int y, int w, int h) override { gl::Viewport(x, fbH_ - y - h, w, h); }

	void bindPipeline(Handle h) override
	{
		auto it = pipelines_.find(h);
		if (it == pipelines_.end())
		{
			pipe_ = nullptr;
			return;
		}
		pipe_ = &it->second;
		const PipelineDesc& d = pipe_->desc;
		gl::UseProgram(pipe_->program);
		gl::BindVertexArray(vao_);
		gl::Disable(GL_CULL_FACE);
		if (d.depth == DepthTest::Off) gl::Disable(GL_DEPTH_TEST);
		else
		{
			gl::Enable(GL_DEPTH_TEST);
			gl::DepthFunc(d.depth == DepthTest::LessEqual ? GL_LEQUAL : GL_LESS);
		}
		gl::DepthMask(d.depthWrite ? GL_TRUE : GL_FALSE);
		switch (d.blend)
		{
		case Blend::None: gl::Disable(GL_BLEND); break;
		case Blend::Alpha:
			gl::Enable(GL_BLEND);
			gl::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			break;
		case Blend::Modulate2x:
			gl::Enable(GL_BLEND);
			gl::BlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
			break;
		case Blend::Additive:
			gl::Enable(GL_BLEND);
			gl::BlendFunc(GL_ONE, GL_ONE);
			break;
		}
		gl::PolygonMode(GL_FRONT_AND_BACK, d.wireframe ? GL_LINE : GL_FILL);
	}

	void setUniforms(int binding, const void* data, size_t size) override
	{
		size_t off = (uniformOff_ + uboAlign_ - 1) / uboAlign_ * uboAlign_;
		gl::BindBuffer(GL_UNIFORM_BUFFER, uniformRing_);
		if (off + size > kUniformRing)
		{ // przepełnienie: nowy bufor (stary zostaje dla GPU)
			gl::BufferData(GL_UNIFORM_BUFFER, GLsizeiptr(kUniformRing), nullptr, GL_STREAM_DRAW);
			off = 0;
		}
		gl::BufferSubData(GL_UNIFORM_BUFFER, GLintptr(off), GLsizeiptr(size), data);
		gl::BindBufferRange(GL_UNIFORM_BUFFER, GLuint(binding), uniformRing_, GLintptr(off), GLsizeiptr(size));
		uniformOff_ = off + size;
	}
	void bindUniformBuffer(int binding, Handle h) override
	{
		auto it = buffers_.find(h);
		if (it != buffers_.end()) gl::BindBufferBase(GL_UNIFORM_BUFFER, GLuint(binding), it->second.id);
	}
	void bindTexture(int slot, Handle h) override
	{
		auto it = textures_.find(h == kNull ? white_ : h);
		if (it == textures_.end()) it = textures_.find(white_);
		gl::ActiveTexture(GLenum(GL_TEXTURE0 + kBindFirstTexture + slot));
		gl::BindTexture(GL_TEXTURE_2D, it->second.id);
		gl::ActiveTexture(GL_TEXTURE0);
	}
	void bindVertexBuffer(int binding, Handle h) override
	{
		auto it = buffers_.find(h);
		vbuf_[binding] = {it == buffers_.end() ? 0u : it->second.id, 0};
	}
	void setVertexData(int binding, const void* data, size_t size) override
	{
		gl::BindBuffer(GL_ARRAY_BUFFER, vertexRing_);
		if (vertexOff_ + size > kVertexRing)
		{
			gl::BufferData(GL_ARRAY_BUFFER, GLsizeiptr(kVertexRing), nullptr, GL_STREAM_DRAW);
			vertexOff_ = 0;
		}
		gl::BufferSubData(GL_ARRAY_BUFFER, GLintptr(vertexOff_), GLsizeiptr(size), data);
		vbuf_[binding] = {vertexRing_, vertexOff_};
		vertexOff_ = (vertexOff_ + size + 15) & ~size_t(15);
	}
	void bindIndexBuffer(Handle h) override
	{
		auto it = buffers_.find(h);
		if (it == buffers_.end()) return;
		gl::BindVertexArray(vao_);
		gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, it->second.id);
		index32_ = it->second.kind == BufferKind::Index32;
	}
	void drawIndexed(uint32_t count, uint32_t first) override
	{
		if (!applyAttribs()) return;
		gl::DrawElements(GL_TRIANGLES, GLsizei(count), index32_ ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT,
		    (const void*)(uintptr_t(first) * (index32_ ? 4 : 2)));
	}
	void draw(uint32_t count) override
	{
		if (!applyAttribs()) return;
		gl::DrawArrays(GL_TRIANGLES, 0, GLsizei(count));
	}

	void endFrame(std::vector<uint8_t>* readback) override
	{
		gl::PolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		if (readback)
		{
			std::vector<uint8_t> px(size_t(fbW_) * fbH_ * 4);
			gl::PixelStorei(GL_PACK_ALIGNMENT, 1);
			gl::ReadPixels(0, 0, fbW_, fbH_, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
			readback->resize(px.size());
			size_t row = size_t(fbW_) * 4;
			for (int y = 0; y < fbH_; ++y) std::memcpy(readback->data() + size_t(y) * row, px.data() + size_t(fbH_ - 1 - y) * row, row);
		}
		SDL_GL_SwapWindow(window_);
	}
	void waitIdle() override { gl::Finish(); }

	void imguiInit() override
	{
		ImGui_ImplSDL2_InitForOpenGL(window_, ctx_);
		ImGui_ImplOpenGL3_Init("#version 330 core");
	}
	void imguiNewFrame() override { ImGui_ImplOpenGL3_NewFrame(); }
	void imguiRender(ImDrawData* data) override
	{
		gl::PolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		gl::Viewport(0, 0, fbW_, fbH_);
		ImGui_ImplOpenGL3_RenderDrawData(data);
	}
	void imguiShutdown() override
	{
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplSDL2_Shutdown();
	}
	uint64_t imguiTexture(Handle h) override
	{
		auto it = textures_.find(h);
		return it == textures_.end() ? 0 : it->second.id;
	}

private:
	bool applyAttribs()
	{
		if (!pipe_) return false;
		gl::BindVertexArray(vao_);
		for (GLuint i = 0; i < 16; ++i) gl::DisableVertexAttribArray(i);
		const PipelineDesc& d = pipe_->desc;
		for (const VertexAttrib& a : d.attribs)
		{
			auto vb = vbuf_[a.binding];
			if (!vb.buffer) return false;
			gl::BindBuffer(GL_ARRAY_BUFFER, vb.buffer);
			gl::VertexAttribPointer(a.location, GLint(a.components), GL_FLOAT, GL_FALSE, GLsizei(d.strides[a.binding]),
			    (const void*)(uintptr_t(vb.offset + a.offset)));
			gl::EnableVertexAttribArray(a.location);
		}
		return true;
	}

	struct VBind
	{
		GLuint buffer = 0;
		size_t offset = 0;
	};
	SDL_Window* window_;
	SDL_GLContext ctx_;
	GLuint vao_ = 0, uniformRing_ = 0, vertexRing_ = 0;
	size_t uboAlign_ = 256, uniformOff_ = 0, vertexOff_ = 0;
	int fbW_ = 0, fbH_ = 0;
	Handle next_ = 1, white_ = kNull;
	std::unordered_map<Handle, GlTexture> textures_;
	std::unordered_map<Handle, GlBuffer> buffers_;
	std::unordered_map<Handle, GlPipeline> pipelines_;
	const GlPipeline* pipe_ = nullptr;
	VBind vbuf_[4];
	bool index32_ = false;
};

} // namespace

std::unique_ptr<Device> createDeviceGL(SDL_Window* window, bool vsync, std::string& err)
{
	SDL_GLContext ctx = SDL_GL_CreateContext(window);
	if (!ctx)
	{
		err = std::string("SDL_GL_CreateContext: ") + SDL_GetError();
		return nullptr;
	}
	SDL_GL_MakeCurrent(window, ctx);
	SDL_GL_SetSwapInterval(vsync ? 1 : 0);
	auto d = std::make_unique<GlDevice>(window, ctx);
	if (!d->init(err)) return nullptr;
	return d;
}

} // namespace ow::rhi

#endif // OW_RHI_GL
