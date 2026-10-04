// Warstwa pośrednia renderowania (RHI): wspólny interfejs dla backendów Vulkan i OpenGL 4.5.
//
// Renderery (teren, modele, panorama) opisują, CO narysować: potoki z nazwanych shaderów
// (src/shaders/<nazwa>.vert/.frag), bufory, tekstury, bloki uniformów. Backend odpowiada za to, JAK.
//
// Konwencje wspólne dla obu backendów:
//  - Macierze liczymy jak w OpenGL (glm, przycięcie z −1..1, oś Y w górę). Backend Vulkan odwraca Y
//    ujemną wysokością viewportu, a głębię przelicza makro OW_CLIP w shaderach (src/shaders/common.glsl).
//  - Wiązania: 0 = uniformy rysowania (setUniforms), 1 = uniformy klatki (setUniforms, trwają do zmiany),
//    2 = stały bufor uniformów (bindUniformBuffer), 3..8 = tekstury (bindTexture, sloty 0..5).
//  - Bloki uniformów mają układ std140 (wektory 3D uzupełniamy do vec4).
//  - Dane zmieniane co klatkę (setUniforms, setVertexData) są kopiowane do bufora pierścieniowego
//    backendu: wolno je zmieniać między rysowaniami w jednej klatce.
//  - Viewport w pikselach od lewego górnego rogu okna.
// Budowanie: ./build.sh [vulkan|opengl|both]; wybór przy starcie: createDevice(Backend…).
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct SDL_Window;
struct ImDrawData;

namespace ow::rhi {

enum class Backend { Vulkan, OpenGL };
const char* backendName(Backend b);
bool backendCompiled(Backend b);
// "vulkan" / "opengl" -> Backend; false dla nieznanej nazwy.
bool parseBackend(const std::string& name, Backend& out);

using Handle = uint32_t;
constexpr Handle kNull = 0;

constexpr int kBindDraw = 0, kBindFrame = 1, kBindStatic = 2, kBindFirstTexture = 3;
constexpr int kTextureSlots = 6;

struct TextureDesc {
	int width = 0, height = 0;
	bool mipmaps = true;
	bool repeatU = true, repeatV = true; // inaczej przycięcie do krawędzi
};

enum class BufferKind { Vertex, Index16, Index32, Uniform };
enum class Blend { None, Alpha, Modulate2x }; // Modulate2x: wynik = cel · źródło · 2
enum class DepthTest { Off, Less, LessEqual };

struct VertexAttrib {
	uint32_t location, binding, components, offset; // składowe float
};

struct PipelineDesc {
	std::string shader;            // src/shaders/<shader>.vert + .frag
	std::vector<uint32_t> strides; // krok wierzchołka dla każdego wiązania bufora
	std::vector<VertexAttrib> attribs;
	Blend blend = Blend::None;
	DepthTest depth = DepthTest::Less;
	bool depthWrite = true;
	bool wireframe = false;
};

class Device {
public:
	virtual ~Device() = default;
	virtual Backend backend() const = 0;
	virtual std::string description() const = 0; // karta i sterownik

	// Zasoby. Tekstury RGBA8, wiersz 0 = góra obrazu.
	virtual Handle createTexture(const TextureDesc& d, const uint8_t* rgba) = 0;
	virtual void updateTexture(Handle t, const TextureDesc& d, const uint8_t* rgba) = 0; // nowa treść (i rozmiar)
	virtual void destroyTexture(Handle t) = 0;
	virtual Handle createBuffer(BufferKind kind, const void* data, size_t size) = 0; // niezmienny
	virtual void destroyBuffer(Handle b) = 0;
	virtual Handle createPipeline(const PipelineDesc& d, std::string& err) = 0;

	// Klatka: beginFrame czyści ekran (false = pomiń klatkę, np. okno zminimalizowane).
	virtual bool beginFrame(int width, int height, float r, float g, float b) = 0;
	virtual void setViewport(int x, int y, int w, int h) = 0;
	virtual void bindPipeline(Handle p) = 0;
	virtual void setUniforms(int binding, const void* data, size_t size) = 0;
	virtual void bindUniformBuffer(int binding, Handle b) = 0;
	virtual void bindTexture(int slot, Handle t) = 0; // kNull = biała tekstura 1×1
	virtual void bindVertexBuffer(int binding, Handle b) = 0;
	virtual void setVertexData(int binding, const void* data, size_t size) = 0; // dane tymczasowe
	virtual void bindIndexBuffer(Handle b) = 0;
	virtual void drawIndexed(uint32_t count, uint32_t first = 0) = 0;
	virtual void draw(uint32_t count) = 0;
	// Kończy klatkę i pokazuje ją. readback != nullptr: kopia obrazu RGBA, wiersz 0 = góra.
	virtual void endFrame(std::vector<uint8_t>* readback = nullptr) = 0;
	virtual void waitIdle() = 0;

	// Dear ImGui: imguiInit po ImGui::CreateContext (inicjuje też platformę SDL2), render między beginFrame
	// a endFrame, imguiShutdown przed ImGui::DestroyContext.
	virtual void imguiInit() = 0;
	virtual void imguiNewFrame() = 0;
	virtual void imguiRender(ImDrawData* data) = 0;
	virtual void imguiShutdown() = 0;
	virtual uint64_t imguiTexture(Handle t) = 0; // ImTextureID dla ImGui::Image
};

// Flagi SDL_CreateWindow dla backendu (SDL_WINDOW_VULKAN albo SDL_WINDOW_OPENGL).
uint32_t windowFlags(Backend b);
// Przygotowanie przed utworzeniem okna (atrybuty kontekstu OpenGL).
void prepareWindow(Backend b);
std::unique_ptr<Device> createDevice(Backend b, SDL_Window* window, bool vsync, std::string& err);

// Shadery osadzone przez build.sh (build/gen/ow_shaders_*.inc).
const char* shaderGlsl(const std::string& name, const char* stage);                           // OpenGL
const uint32_t* shaderSpirv(const std::string& name, const char* stage, size_t& words);       // Vulkan

} // namespace ow::rhi
