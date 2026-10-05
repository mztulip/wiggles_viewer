#include "lib/render/rhi/rhi.h"

#include <SDL.h>

#include <cstring>

#ifdef OW_RHI_VK
#include <vulkan/vulkan.h>
#endif

namespace ow::rhi
{

#ifdef OW_RHI_GL
std::unique_ptr<Device> createDeviceGL(SDL_Window* window, bool vsync, std::string& err);
#endif
#ifdef OW_RHI_VK
std::unique_ptr<Device> createDeviceVK(SDL_Window* window, bool vsync, bool offscreen, std::string& err);
#endif

const char* backendName(Backend b)
{
	return b == Backend::Vulkan ? "Vulkan" : "OpenGL";
}

bool backendCompiled(Backend b)
{
#ifdef OW_RHI_VK
	if (b == Backend::Vulkan) return true;
#endif
#ifdef OW_RHI_GL
	if (b == Backend::OpenGL) return true;
#endif
	(void)b;
	return false;
}

bool parseBackend(const std::string& name, Backend& out)
{
	if (name == "vulkan" || name == "vk") out = Backend::Vulkan;
	else if (name == "opengl" || name == "gl") out = Backend::OpenGL;
	else return false;
	return true;
}

uint32_t windowFlags(Backend b)
{
	return b == Backend::Vulkan ? SDL_WINDOW_VULKAN : SDL_WINDOW_OPENGL;
}

void prepareWindow(Backend b)
{
	if (b != Backend::OpenGL) return;
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
}

std::unique_ptr<Device> createDevice(Backend b, SDL_Window* window, bool vsync, std::string& err, bool offscreen)
{
#ifdef OW_RHI_VK
	if (b == Backend::Vulkan) return createDeviceVK(window, vsync, offscreen, err);
#endif
#ifdef OW_RHI_GL
	if (b == Backend::OpenGL) return createDeviceGL(window, vsync, err);
#endif
	(void)window;
	(void)vsync;
	(void)offscreen;
	err = std::string("backend ") + backendName(b) + " nie jest wkompilowany (./build.sh vulkan|opengl|both)";
	return nullptr;
}

namespace
{
struct GlShaderEntry
{
	const char* name;
	const char* stage;
	const char* text;
};
const GlShaderEntry kGlShaders[] = {
#include "ow_shaders_gl.inc"
};
} // namespace

const char* shaderGlsl(const std::string& name, const char* stage)
{
	for (const auto& e : kGlShaders)
		if (name == e.name && std::strcmp(stage, e.stage) == 0) return e.text;
	return nullptr;
}

#ifdef OW_RHI_VK
namespace
{
struct VkShaderEntry
{
	const char* name;
	const char* stage;
	const uint32_t* words;
	size_t count;
};
#include "ow_shaders_vk.inc"
} // namespace

const uint32_t* shaderSpirv(const std::string& name, const char* stage, size_t& words)
{
	for (const auto& e : kVkShaders)
		if (name == e.name && std::strcmp(stage, e.stage) == 0)
		{
			words = e.count;
			return e.words;
		}
	words = 0;
	return nullptr;
}
#else
const uint32_t* shaderSpirv(const std::string&, const char*, size_t& words)
{
	words = 0;
	return nullptr;
}
#endif

} // namespace ow::rhi
