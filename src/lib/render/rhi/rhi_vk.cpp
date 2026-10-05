// Backend Vulkan 1.2 interfejsu rhi.h.
//
// Założenia (proste, wystarczające dla przeglądarki i silnika 2,5D):
//  - wszystkie bufory w pamięci widocznej dla CPU (karty zintegrowane: bez kosztu; dyskretne: wystarczy),
//  - dwie klatki w locie; każda ma własny bufor pierścieniowy uniformów i danych tymczasowych,
//  - jeden układ zestawu deskryptorów dla wszystkich potoków: 0, 1 = dynamiczne bloki uniformów
//    (przesunięcie w pierścieniu), 2 = stały blok, 3..8 = tekstury; zestawy zapamiętywane według
//    kompletu tekstur i bufora stałego,
//  - zasoby usuwane z opóźnieniem, gdy GPU skończy klatki, które mogły ich używać,
//  - oś Y odwrócona ujemną wysokością viewportu (Vulkan 1.1+), głębia przeliczana w shaderach (OW_CLIP).
#ifdef OW_RHI_VK

#include "lib/render/rhi/rhi.h"

#include <SDL.h>
#include <SDL_vulkan.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <functional>
#include <unordered_map>

namespace ow::rhi
{

namespace
{

constexpr int kFrames = 2;
constexpr size_t kUniformRing = 8u << 20, kVertexRing = 8u << 20;
constexpr VkDeviceSize kUboRange = 2048; // zakres dynamicznych bloków uniformów (shadery mają mniejsze)
constexpr uint32_t kSetsPerPool = 4096;

struct VkTex
{
	VkImage image = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
	VkSampler sampler = VK_NULL_HANDLE;
	VkDescriptorSet imgui = VK_NULL_HANDLE;
	int width = 0, height = 0;
};
struct VkBuf
{
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	void* mapped = nullptr;
	BufferKind kind = BufferKind::Vertex;
};
struct VkPipe
{
	VkPipeline pipeline = VK_NULL_HANDLE;
};

struct SetKey
{
	std::array<Handle, kTextureSlots> tex{};
	Handle staticBuf = kNull;
	int frame = 0;
	bool operator==(const SetKey& o) const { return tex == o.tex && staticBuf == o.staticBuf && frame == o.frame; }
};
struct SetKeyHash
{
	size_t operator()(const SetKey& k) const
	{
		size_t h = size_t(k.frame) * 1315423911u ^ k.staticBuf;
		for (Handle t : k.tex) h = h * 16777619u ^ t;
		return h;
	}
};

class VkDevice_ final : public Device
{
public:
	VkDevice_(SDL_Window* w, bool vsync, bool offscreen) : window_(w), vsync_(vsync), offscreen_(offscreen) {}
	~VkDevice_() override { destroyAll(); }

	bool init(std::string& err);

	Backend backend() const override { return Backend::Vulkan; }
	std::string description() const override { return description_; }

	Handle createTexture(const TextureDesc& d, const uint8_t* rgba) override
	{
		Handle h = next_++;
		textures_[h] = makeTexture(d, rgba);
		return h;
	}
	void updateTexture(Handle h, const TextureDesc& d, const uint8_t* rgba) override
	{
		auto it = textures_.find(h);
		if (it == textures_.end()) return;
		retireTexture(it->second);
		it->second = makeTexture(d, rgba);
		setsDirty_ = true; // zestawy deskryptorów wskazują stary obraz
	}
	void destroyTexture(Handle h) override
	{
		auto it = textures_.find(h);
		if (it == textures_.end()) return;
		retireTexture(it->second);
		textures_.erase(it);
		setsDirty_ = true;
	}
	Handle createBuffer(BufferKind kind, const void* data, size_t size) override
	{
		VkBuf b = makeBuffer(kind, std::max<size_t>(size, 16));
		if (data && size) std::memcpy(b.mapped, data, size);
		Handle h = next_++;
		buffers_[h] = b;
		return h;
	}
	void destroyBuffer(Handle h) override
	{
		auto it = buffers_.find(h);
		if (it == buffers_.end()) return;
		VkBuf b = it->second;
		buffers_.erase(it);
		later([this, b] { freeBuffer(b); });
		setsDirty_ = true;
	}
	Handle createPipeline(const PipelineDesc& d, std::string& err) override;

	bool beginFrame(int width, int height, float r, float g, float b) override;
	void setViewport(int x, int y, int w, int h) override
	{
		VkViewport vp{float(x), float(y + h), float(w), -float(h), 0.f, 1.f};
		vkCmdSetViewport(cmd(), 0, 1, &vp);
		VkRect2D sc{{std::max(x, 0), std::max(y, 0)}, {uint32_t(std::max(w, 1)), uint32_t(std::max(h, 1))}};
		vkCmdSetScissor(cmd(), 0, 1, &sc);
	}
	void bindPipeline(Handle h) override
	{
		auto it = pipelines_.find(h);
		if (it == pipelines_.end()) return;
		vkCmdBindPipeline(cmd(), VK_PIPELINE_BIND_POINT_GRAPHICS, it->second.pipeline);
		descDirty_ = true;
	}
	void setUniforms(int binding, const void* data, size_t size) override
	{
		if (binding < 0 || binding > 1 || size > kUboRange) return;
		Frame& f = frames_[frame_];
		size_t off = (f.uniformOff + uboAlign_ - 1) / uboAlign_ * uboAlign_;
		if (off + kUboRange > kUniformRing) off = kUboRange; // przepełnienie (nie powinno się zdarzać): nadpisz od początku
		std::memcpy(static_cast<uint8_t*>(f.uniform.mapped) + off, data, size);
		dynOff_[binding] = uint32_t(off);
		f.uniformOff = off + size;
		descDirty_ = true;
	}
	void bindUniformBuffer(int binding, Handle h) override
	{
		if (binding != kBindStatic) return;
		cur_.staticBuf = h;
		descDirty_ = true;
	}
	void bindTexture(int slot, Handle h) override
	{
		if (slot < 0 || slot >= kTextureSlots) return;
		cur_.tex[size_t(slot)] = h;
		descDirty_ = true;
	}
	void bindVertexBuffer(int binding, Handle h) override
	{
		auto it = buffers_.find(h);
		if (it == buffers_.end()) return;
		VkDeviceSize off = 0;
		vkCmdBindVertexBuffers(cmd(), uint32_t(binding), 1, &it->second.buffer, &off);
	}
	void setVertexData(int binding, const void* data, size_t size) override
	{
		Frame& f = frames_[frame_];
		if (f.vertexOff + size > kVertexRing) return;
		std::memcpy(static_cast<uint8_t*>(f.vertex.mapped) + f.vertexOff, data, size);
		VkDeviceSize off = f.vertexOff;
		vkCmdBindVertexBuffers(cmd(), uint32_t(binding), 1, &f.vertex.buffer, &off);
		f.vertexOff = (f.vertexOff + size + 15) & ~size_t(15);
	}
	void bindIndexBuffer(Handle h) override
	{
		auto it = buffers_.find(h);
		if (it == buffers_.end()) return;
		vkCmdBindIndexBuffer(cmd(), it->second.buffer, 0,
		    it->second.kind == BufferKind::Index32 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);
	}
	void drawIndexed(uint32_t count, uint32_t first) override
	{
		flushDescriptors();
		vkCmdDrawIndexed(cmd(), count, 1, first, 0, 0);
	}
	void draw(uint32_t count) override
	{
		flushDescriptors();
		vkCmdDraw(cmd(), count, 1, 0, 0);
	}
	void endFrame(std::vector<uint8_t>* readback) override;
	void waitIdle() override
	{
		if (device_) vkDeviceWaitIdle(device_);
	}

	void imguiInit() override;
	void imguiNewFrame() override { ImGui_ImplVulkan_NewFrame(); }
	void imguiRender(ImDrawData* data) override { ImGui_ImplVulkan_RenderDrawData(data, cmd()); }
	void imguiShutdown() override
	{
		if (!imguiReady_) return;
		vkDeviceWaitIdle(device_);
		for (auto& [h, t] : textures_)
			if (t.imgui)
			{
				ImGui_ImplVulkan_RemoveTexture(t.imgui);
				t.imgui = VK_NULL_HANDLE;
			}
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		imguiReady_ = false;
	}
	uint64_t imguiTexture(Handle h) override
	{
		auto it = textures_.find(h);
		if (it == textures_.end() || !imguiReady_) return 0;
		if (!it->second.imgui)
			it->second.imgui = ImGui_ImplVulkan_AddTexture(it->second.sampler, it->second.view,
			    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		return uint64_t(uintptr_t(it->second.imgui));
	}

private:
	struct Frame
	{
		VkCommandPool pool = VK_NULL_HANDLE;
		VkCommandBuffer cmd = VK_NULL_HANDLE;
		VkFence fence = VK_NULL_HANDLE;
		VkSemaphore acquired = VK_NULL_HANDLE;
		VkBuf uniform, vertex;
		size_t uniformOff = 0, vertexOff = 0;
		std::vector<std::function<void()>> deletions; // do wykonania po zakończeniu tej klatki na GPU
	};
	struct Swapchain
	{
		VkSwapchainKHR swapchain = VK_NULL_HANDLE;
		VkFormat format = VK_FORMAT_UNDEFINED;
		VkExtent2D extent{0, 0};
		std::vector<VkImage> images;
		std::vector<VkImageView> views;
		std::vector<VkFramebuffer> framebuffers;
		std::vector<VkSemaphore> rendered; // na obraz swapchaina
		VkImage depth = VK_NULL_HANDLE;
		VkDeviceMemory depthMem = VK_NULL_HANDLE;
		VkImageView depthView = VK_NULL_HANDLE;
	};

	VkCommandBuffer cmd() const { return frames_[frame_].cmd; }
	void later(std::function<void()> fn)
	{
		if (inFrame_) frames_[frame_].deletions.push_back(std::move(fn));
		else frames_[(frame_ + kFrames - 1) % kFrames].deletions.push_back(std::move(fn));
	}

	uint32_t memoryType(uint32_t bits, VkMemoryPropertyFlags flags) const
	{
		for (uint32_t i = 0; i < memProps_.memoryTypeCount; ++i)
			if ((bits & (1u << i)) && (memProps_.memoryTypes[i].propertyFlags & flags) == flags) return i;
		return UINT32_MAX;
	}
	VkBuf makeBuffer(BufferKind kind, size_t size)
	{
		VkBuf b;
		b.kind = kind;
		VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
		bi.size = size;
		bi.usage = kind == BufferKind::Uniform ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
		    : kind == BufferKind::Vertex       ? VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
		                                       : VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		bi.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		vkCreateBuffer(device_, &bi, nullptr, &b.buffer);
		VkMemoryRequirements req;
		vkGetBufferMemoryRequirements(device_, b.buffer, &req);
		VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
		ai.allocationSize = req.size;
		ai.memoryTypeIndex = memoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		vkAllocateMemory(device_, &ai, nullptr, &b.memory);
		vkBindBufferMemory(device_, b.buffer, b.memory, 0);
		vkMapMemory(device_, b.memory, 0, VK_WHOLE_SIZE, 0, &b.mapped);
		return b;
	}
	void freeBuffer(const VkBuf& b)
	{
		if (b.mapped) vkUnmapMemory(device_, b.memory);
		if (b.buffer) vkDestroyBuffer(device_, b.buffer, nullptr);
		if (b.memory) vkFreeMemory(device_, b.memory, nullptr);
	}

	VkSampler sampler(const TextureDesc& d, uint32_t mips)
	{
		uint32_t key = (d.repeatU ? 1u : 0u) | (d.repeatV ? 2u : 0u) | (mips << 2);
		auto it = samplers_.find(key);
		if (it != samplers_.end()) return it->second;
		VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
		si.magFilter = si.minFilter = VK_FILTER_LINEAR;
		si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		si.addressModeU = d.repeatU ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		si.addressModeV = d.repeatV ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		si.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		si.maxLod = float(mips);
		VkSampler s = VK_NULL_HANDLE;
		vkCreateSampler(device_, &si, nullptr, &s);
		samplers_[key] = s;
		return s;
	}

	// Polecenia jednorazowe (wysyłka tekstur): osobny bufor poleceń, czekamy na wykonanie.
	void oneShot(const std::function<void(VkCommandBuffer)>& fn)
	{
		VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
		ai.commandPool = uploadPool_;
		ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		ai.commandBufferCount = 1;
		VkCommandBuffer c;
		vkAllocateCommandBuffers(device_, &ai, &c);
		VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
		bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(c, &bi);
		fn(c);
		vkEndCommandBuffer(c);
		VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
		si.commandBufferCount = 1;
		si.pCommandBuffers = &c;
		vkQueueSubmit(queue_, 1, &si, VK_NULL_HANDLE);
		vkQueueWaitIdle(queue_);
		vkFreeCommandBuffers(device_, uploadPool_, 1, &c);
	}

	static void barrier(VkCommandBuffer c, VkImage img, VkImageLayout from, VkImageLayout to, uint32_t baseMip, uint32_t mips,
	    VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkPipelineStageFlags srcStage,
	    VkPipelineStageFlags dstStage, VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT)
	{
		VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
		b.oldLayout = from;
		b.newLayout = to;
		b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.image = img;
		b.subresourceRange = {aspect, baseMip, mips, 0, 1};
		b.srcAccessMask = srcAccess;
		b.dstAccessMask = dstAccess;
		vkCmdPipelineBarrier(c, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &b);
	}

	VkTex makeTexture(const TextureDesc& d, const uint8_t* rgba);
	void retireTexture(const VkTex& t)
	{
		VkTex c = t;
		if (c.imgui && imguiReady_) ImGui_ImplVulkan_RemoveTexture(c.imgui);
		later([this, c] {
			vkDestroyImageView(device_, c.view, nullptr);
			vkDestroyImage(device_, c.image, nullptr);
			vkFreeMemory(device_, c.memory, nullptr);
		});
	}

	bool createSwapchain(int width, int height, std::string& err);
	bool finishTargets(VkExtent2D ext, uint32_t count, std::string& err);
	void destroySwapchain();
	void flushDescriptors();
	VkDescriptorSet descriptorSet(const SetKey& key);
	void resetDescriptorCache()
	{
		vkDeviceWaitIdle(device_);
		for (VkDescriptorPool p : descPools_) vkResetDescriptorPool(device_, p, 0);
		poolUsed_ = 0;
		poolIndex_ = 0;
		setCache_.clear();
		setsDirty_ = false;
	}
	void destroyAll();

	SDL_Window* window_;
	bool vsync_, offscreen_;
	VkImageLayout finalLayout_ = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	VkDeviceMemory offscreenMem_ = VK_NULL_HANDLE;
	std::string description_;
	VkInstance instance_ = VK_NULL_HANDLE;
	VkSurfaceKHR surface_ = VK_NULL_HANDLE;
	VkPhysicalDevice phys_ = VK_NULL_HANDLE;
	VkPhysicalDeviceMemoryProperties memProps_{};
	VkDevice device_ = VK_NULL_HANDLE;
	VkQueue queue_ = VK_NULL_HANDLE;
	uint32_t queueFamily_ = 0;
	bool fillModeNonSolid_ = false;
	size_t uboAlign_ = 256;
	VkRenderPass renderPass_ = VK_NULL_HANDLE;
	VkDescriptorSetLayout setLayout_ = VK_NULL_HANDLE;
	VkPipelineLayout pipeLayout_ = VK_NULL_HANDLE;
	std::vector<VkDescriptorPool> descPools_;
	uint32_t poolUsed_ = 0;
	size_t poolIndex_ = 0;
	VkCommandPool uploadPool_ = VK_NULL_HANDLE;
	VkFormat depthFormat_ = VK_FORMAT_D32_SFLOAT;
	Swapchain sc_;
	std::array<Frame, kFrames> frames_;
	int frame_ = 0;
	uint32_t image_ = 0;
	bool inFrame_ = false, imguiReady_ = false, setsDirty_ = false, descDirty_ = true;
	uint32_t dynOff_[2] = {0, 0};
	SetKey cur_;
	Handle next_ = 1, white_ = kNull, zeroStatic_ = kNull;
	std::unordered_map<Handle, VkTex> textures_;
	std::unordered_map<Handle, VkBuf> buffers_;
	std::unordered_map<Handle, VkPipe> pipelines_;
	std::unordered_map<uint32_t, VkSampler> samplers_;
	std::unordered_map<SetKey, VkDescriptorSet, SetKeyHash> setCache_;
	VkBuf readbackBuf_;
	size_t readbackSize_ = 0;
};

bool VkDevice_::init(std::string& err)
{
	unsigned extCount = 0;
	std::vector<const char*> exts;
	if (!offscreen_)
	{
		if (!SDL_Vulkan_GetInstanceExtensions(window_, &extCount, nullptr))
		{
			err = std::string("SDL_Vulkan_GetInstanceExtensions: ") + SDL_GetError();
			return false;
		}
		exts.resize(extCount);
		SDL_Vulkan_GetInstanceExtensions(window_, &extCount, exts.data());
	}
	else
		finalLayout_ = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

	VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
	app.pApplicationName = "OpenWiggles";
	app.pEngineName = "OpenWiggles";
	app.apiVersion = VK_API_VERSION_1_2;
	VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
	ici.pApplicationInfo = &app;
	ici.enabledExtensionCount = uint32_t(exts.size());
	ici.ppEnabledExtensionNames = exts.data();
	// Warstwa walidacji na żądanie: OW_VK_VALIDATION=1.
	const char* validation = "VK_LAYER_KHRONOS_validation";
	if (const char* v = std::getenv("OW_VK_VALIDATION"); v && *v == '1')
	{
		ici.enabledLayerCount = 1;
		ici.ppEnabledLayerNames = &validation;
	}
	if (VkResult r = vkCreateInstance(&ici, nullptr, &instance_); r != VK_SUCCESS)
	{
		err = "vkCreateInstance: " + std::to_string(int(r));
		return false;
	}
	if (!offscreen_ && !SDL_Vulkan_CreateSurface(window_, instance_, &surface_))
	{
		err = std::string("SDL_Vulkan_CreateSurface: ") + SDL_GetError();
		return false;
	}

	uint32_t n = 0;
	vkEnumeratePhysicalDevices(instance_, &n, nullptr);
	std::vector<VkPhysicalDevice> devs(n);
	vkEnumeratePhysicalDevices(instance_, &n, devs.data());
	int bestScore = -1;
	for (VkPhysicalDevice pd : devs)
	{
		uint32_t qn = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(pd, &qn, nullptr);
		std::vector<VkQueueFamilyProperties> qs(qn);
		vkGetPhysicalDeviceQueueFamilyProperties(pd, &qn, qs.data());
		for (uint32_t i = 0; i < qn; ++i)
		{
			VkBool32 present = offscreen_ ? VK_TRUE : VK_FALSE;
			if (!offscreen_) vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, surface_, &present);
			if (!(qs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) || !present) continue;
			VkPhysicalDeviceProperties p;
			vkGetPhysicalDeviceProperties(pd, &p);
			int score = p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 3
			    : p.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU     ? 2
			                                                                 : 1;
			if (score > bestScore)
			{
				bestScore = score;
				phys_ = pd;
				queueFamily_ = i;
			}
			break;
		}
	}
	if (!phys_)
	{
		err = "brak karty z obsługą Vulkana i wyświetlania w tym oknie";
		return false;
	}
	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(phys_, &props);
	vkGetPhysicalDeviceMemoryProperties(phys_, &memProps_);
	uboAlign_ = size_t(std::max<VkDeviceSize>(props.limits.minUniformBufferOffsetAlignment, 16));
	description_ = std::string("Vulkan ") + std::to_string(VK_API_VERSION_MAJOR(props.apiVersion)) + "." +
	    std::to_string(VK_API_VERSION_MINOR(props.apiVersion)) + " / " + props.deviceName;
	VkPhysicalDeviceFeatures have;
	vkGetPhysicalDeviceFeatures(phys_, &have);
	fillModeNonSolid_ = have.fillModeNonSolid == VK_TRUE;

	float prio = 1.f;
	VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
	qci.queueFamilyIndex = queueFamily_;
	qci.queueCount = 1;
	qci.pQueuePriorities = &prio;
	VkPhysicalDeviceFeatures feats{};
	feats.fillModeNonSolid = have.fillModeNonSolid;
	const char* devExt = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
	VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
	dci.queueCreateInfoCount = 1;
	dci.pQueueCreateInfos = &qci;
	dci.enabledExtensionCount = offscreen_ ? 0 : 1;
	dci.ppEnabledExtensionNames = &devExt;
	dci.pEnabledFeatures = &feats;
	if (VkResult r = vkCreateDevice(phys_, &dci, nullptr, &device_); r != VK_SUCCESS)
	{
		err = "vkCreateDevice: " + std::to_string(int(r));
		return false;
	}
	vkGetDeviceQueue(device_, queueFamily_, 0, &queue_);

	// Format głębi.
	for (VkFormat f : {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM})
	{
		VkFormatProperties fp;
		vkGetPhysicalDeviceFormatProperties(phys_, f, &fp);
		if (fp.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			depthFormat_ = f;
			break;
		}
	}
	// Format obrazu okna (bez sRGB, jak w OpenGL).
	uint32_t fn = 0;
	std::vector<VkSurfaceFormatKHR> fmts;
	if (!offscreen_)
	{
		vkGetPhysicalDeviceSurfaceFormatsKHR(phys_, surface_, &fn, nullptr);
		fmts.resize(fn);
		vkGetPhysicalDeviceSurfaceFormatsKHR(phys_, surface_, &fn, fmts.data());
	}
	else
		fmts.push_back({VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR});
	sc_.format = fmts.empty() ? VK_FORMAT_B8G8R8A8_UNORM : fmts[0].format;
	for (auto& f : fmts)
		if (f.format == VK_FORMAT_B8G8R8A8_UNORM || f.format == VK_FORMAT_R8G8B8A8_UNORM)
		{
			sc_.format = f.format;
			break;
		}

	// Przebieg renderowania: kolor (czyszczony, do wyświetlenia) i głębia.
	VkAttachmentDescription att[2]{};
	att[0].format = sc_.format;
	att[0].samples = VK_SAMPLE_COUNT_1_BIT;
	att[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	att[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	att[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	att[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	att[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	att[0].finalLayout = finalLayout_;
	att[1] = att[0];
	att[1].format = depthFormat_;
	att[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	att[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
	VkSubpassDescription sub{};
	sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	sub.colorAttachmentCount = 1;
	sub.pColorAttachments = &colorRef;
	sub.pDepthStencilAttachment = &depthRef;
	VkSubpassDependency dep{};
	dep.srcSubpass = VK_SUBPASS_EXTERNAL;
	dep.dstSubpass = 0;
	dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	VkRenderPassCreateInfo rpi{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
	rpi.attachmentCount = 2;
	rpi.pAttachments = att;
	rpi.subpassCount = 1;
	rpi.pSubpasses = &sub;
	rpi.dependencyCount = 1;
	rpi.pDependencies = &dep;
	vkCreateRenderPass(device_, &rpi, nullptr, &renderPass_);

	// Układ deskryptorów wspólny dla wszystkich potoków.
	VkDescriptorSetLayoutBinding b[3 + kTextureSlots]{};
	for (int i = 0; i < 3 + kTextureSlots; ++i)
	{
		b[i].binding = uint32_t(i);
		b[i].descriptorCount = 1;
		b[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		b[i].descriptorType = i < 2 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
		    : i == 2                ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
		                            : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	}
	VkDescriptorSetLayoutCreateInfo dli{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
	dli.bindingCount = 3 + kTextureSlots;
	dli.pBindings = b;
	vkCreateDescriptorSetLayout(device_, &dli, nullptr, &setLayout_);
	VkPipelineLayoutCreateInfo pli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	pli.setLayoutCount = 1;
	pli.pSetLayouts = &setLayout_;
	vkCreatePipelineLayout(device_, &pli, nullptr, &pipeLayout_);

	VkCommandPoolCreateInfo cpi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
	cpi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	cpi.queueFamilyIndex = queueFamily_;
	vkCreateCommandPool(device_, &cpi, nullptr, &uploadPool_);
	for (Frame& f : frames_)
	{
		vkCreateCommandPool(device_, &cpi, nullptr, &f.pool);
		VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
		ai.commandPool = f.pool;
		ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		ai.commandBufferCount = 1;
		vkAllocateCommandBuffers(device_, &ai, &f.cmd);
		VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
		fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		vkCreateFence(device_, &fi, nullptr, &f.fence);
		VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
		vkCreateSemaphore(device_, &si, nullptr, &f.acquired);
		f.uniform = makeBuffer(BufferKind::Uniform, kUniformRing);
		std::memset(f.uniform.mapped, 0, kUboRange); // blok zerowy na początku: domyślne uniformy
		f.vertex = makeBuffer(BufferKind::Vertex, kVertexRing);
	}

	const uint8_t white[4] = {255, 255, 255, 255};
	TextureDesc wd;
	wd.width = wd.height = 1;
	wd.mipmaps = false;
	white_ = createTexture(wd, white);
	std::vector<uint8_t> zero(4096, 0);
	zeroStatic_ = createBuffer(BufferKind::Uniform, zero.data(), zero.size());
	int w = 0, h = 0;
	SDL_Vulkan_GetDrawableSize(window_, &w, &h);
	return createSwapchain(w, h, err);
}

VkTex VkDevice_::makeTexture(const TextureDesc& d, const uint8_t* rgba)
{
	VkTex t;
	t.width = std::max(d.width, 1);
	t.height = std::max(d.height, 1);
	uint32_t mips = 1;
	if (d.mipmaps)
		for (int s = std::max(t.width, t.height); s > 1; s /= 2) ++mips;
	VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
	ii.imageType = VK_IMAGE_TYPE_2D;
	ii.format = VK_FORMAT_R8G8B8A8_UNORM;
	ii.extent = {uint32_t(t.width), uint32_t(t.height), 1};
	ii.mipLevels = mips;
	ii.arrayLayers = 1;
	ii.samples = VK_SAMPLE_COUNT_1_BIT;
	ii.tiling = VK_IMAGE_TILING_OPTIMAL;
	ii.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	vkCreateImage(device_, &ii, nullptr, &t.image);
	VkMemoryRequirements req;
	vkGetImageMemoryRequirements(device_, t.image, &req);
	VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	ai.allocationSize = req.size;
	ai.memoryTypeIndex = memoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (ai.memoryTypeIndex == UINT32_MAX) ai.memoryTypeIndex = memoryType(req.memoryTypeBits, 0);
	vkAllocateMemory(device_, &ai, nullptr, &t.memory);
	vkBindImageMemory(device_, t.image, t.memory, 0);

	size_t bytes = size_t(t.width) * t.height * 4;
	VkBuf staging = makeBuffer(BufferKind::Vertex, bytes);
	if (rgba) std::memcpy(staging.mapped, rgba, bytes);
	else std::memset(staging.mapped, 255, bytes);
	oneShot([&](VkCommandBuffer c) {
		barrier(c, t.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, mips, 0,
		    VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
		VkBufferImageCopy copy{};
		copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
		copy.imageExtent = {uint32_t(t.width), uint32_t(t.height), 1};
		vkCmdCopyBufferToImage(c, staging.buffer, t.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
		// Mipmapy: kolejne poziomy pomniejszane z poprzedniego (filtr liniowy).
		int mw = t.width, mh = t.height;
		for (uint32_t m = 1; m < mips; ++m)
		{
			barrier(c, t.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m - 1, 1,
			    VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			    VK_PIPELINE_STAGE_TRANSFER_BIT);
			VkImageBlit blit{};
			blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, m - 1, 0, 1};
			blit.srcOffsets[1] = {mw, mh, 1};
			mw = std::max(mw / 2, 1);
			mh = std::max(mh / 2, 1);
			blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, m, 0, 1};
			blit.dstOffsets[1] = {mw, mh, 1};
			vkCmdBlitImage(c, t.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, t.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			    1, &blit, VK_FILTER_LINEAR);
			barrier(c, t.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m - 1, 1,
			    VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);
		}
		barrier(c, t.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mips - 1, 1,
		    VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
		    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);
	});
	freeBuffer(staging);

	VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
	vi.image = t.image;
	vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
	vi.format = VK_FORMAT_R8G8B8A8_UNORM;
	vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mips, 0, 1};
	vkCreateImageView(device_, &vi, nullptr, &t.view);
	t.sampler = sampler(d, mips);
	return t;
}

Handle VkDevice_::createPipeline(const PipelineDesc& d, std::string& err)
{
	size_t vn = 0, fn = 0;
	const uint32_t* vs = shaderSpirv(d.shader, "vert", vn);
	const uint32_t* fs = shaderSpirv(d.shader, "frag", fn);
	if (!vs || !fs)
	{
		err = "brak SPIR-V shadera " + d.shader;
		return kNull;
	}
	auto module = [&](const uint32_t* code, size_t words) {
		VkShaderModuleCreateInfo mi{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
		mi.codeSize = words * 4;
		mi.pCode = code;
		VkShaderModule m = VK_NULL_HANDLE;
		vkCreateShaderModule(device_, &mi, nullptr, &m);
		return m;
	};
	VkShaderModule vm = module(vs, vn), fm = module(fs, fn);
	VkPipelineShaderStageCreateInfo stages[2]{};
	stages[0].sType = stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vm;
	stages[0].pName = "main";
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = fm;
	stages[1].pName = "main";

	std::vector<VkVertexInputBindingDescription> binds;
	for (uint32_t i = 0; i < d.strides.size(); ++i) binds.push_back({i, d.strides[i], VK_VERTEX_INPUT_RATE_VERTEX});
	std::vector<VkVertexInputAttributeDescription> attrs;
	for (const VertexAttrib& a : d.attribs)
	{
		static const VkFormat fmt[5] = {VK_FORMAT_UNDEFINED, VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT,
		    VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT};
		attrs.push_back({a.location, a.binding, fmt[std::min<uint32_t>(a.components, 4)], a.offset});
	}
	VkPipelineVertexInputStateCreateInfo vin{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
	vin.vertexBindingDescriptionCount = uint32_t(binds.size());
	vin.pVertexBindingDescriptions = binds.data();
	vin.vertexAttributeDescriptionCount = uint32_t(attrs.size());
	vin.pVertexAttributeDescriptions = attrs.data();
	VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
	vp.viewportCount = vp.scissorCount = 1;
	VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
	rs.polygonMode = d.wireframe && fillModeNonSolid_ ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_NONE;
	rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rs.lineWidth = 1.f;
	VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
	ds.depthTestEnable = d.depth != DepthTest::Off;
	ds.depthWriteEnable = d.depthWrite && d.depth != DepthTest::Off;
	ds.depthCompareOp = d.depth == DepthTest::LessEqual ? VK_COMPARE_OP_LESS_OR_EQUAL : VK_COMPARE_OP_LESS;
	VkPipelineColorBlendAttachmentState cb{};
	cb.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	if (d.blend == Blend::Alpha)
	{
		cb.blendEnable = VK_TRUE;
		cb.srcColorBlendFactor = cb.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		cb.dstColorBlendFactor = cb.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	}
	else if (d.blend == Blend::Additive)
	{
		cb.blendEnable = VK_TRUE;
		cb.srcColorBlendFactor = cb.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		cb.dstColorBlendFactor = cb.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	}
	else if (d.blend == Blend::Modulate2x)
	{
		cb.blendEnable = VK_TRUE;
		cb.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
		cb.dstColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
		cb.srcAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
		cb.dstAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	}
	cb.colorBlendOp = cb.alphaBlendOp = VK_BLEND_OP_ADD;
	VkPipelineColorBlendStateCreateInfo cbs{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
	cbs.attachmentCount = 1;
	cbs.pAttachments = &cb;
	VkDynamicState dyn[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo dys{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
	dys.dynamicStateCount = 2;
	dys.pDynamicStates = dyn;

	VkGraphicsPipelineCreateInfo pi{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
	pi.stageCount = 2;
	pi.pStages = stages;
	pi.pVertexInputState = &vin;
	pi.pInputAssemblyState = &ia;
	pi.pViewportState = &vp;
	pi.pRasterizationState = &rs;
	pi.pMultisampleState = &ms;
	pi.pDepthStencilState = &ds;
	pi.pColorBlendState = &cbs;
	pi.pDynamicState = &dys;
	pi.layout = pipeLayout_;
	pi.renderPass = renderPass_;
	VkPipe p;
	VkResult r = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pi, nullptr, &p.pipeline);
	vkDestroyShaderModule(device_, vm, nullptr);
	vkDestroyShaderModule(device_, fm, nullptr);
	if (r != VK_SUCCESS)
	{
		err = d.shader + ": vkCreateGraphicsPipelines " + std::to_string(int(r));
		return kNull;
	}
	Handle h = next_++;
	pipelines_[h] = p;
	return h;
}

bool VkDevice_::createSwapchain(int width, int height, std::string& err)
{
	if (offscreen_)
	{
		// Jeden obraz w pamięci karty zamiast swapchaina.
		destroySwapchain();
		VkExtent2D ext{uint32_t(std::max(width, 1)), uint32_t(std::max(height, 1))};
		sc_.extent = ext;
		VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
		ii.imageType = VK_IMAGE_TYPE_2D;
		ii.format = sc_.format;
		ii.extent = {ext.width, ext.height, 1};
		ii.mipLevels = ii.arrayLayers = 1;
		ii.samples = VK_SAMPLE_COUNT_1_BIT;
		ii.tiling = VK_IMAGE_TILING_OPTIMAL;
		ii.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		VkImage img;
		vkCreateImage(device_, &ii, nullptr, &img);
		VkMemoryRequirements req;
		vkGetImageMemoryRequirements(device_, img, &req);
		VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
		ai.allocationSize = req.size;
		ai.memoryTypeIndex = memoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkAllocateMemory(device_, &ai, nullptr, &offscreenMem_);
		vkBindImageMemory(device_, img, offscreenMem_, 0);
		sc_.images = {img};
		return finishTargets(ext, 1, err);
	}
	VkSurfaceCapabilitiesKHR caps;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys_, surface_, &caps);
	VkExtent2D ext = caps.currentExtent;
	if (ext.width == UINT32_MAX) ext = {uint32_t(std::max(width, 1)), uint32_t(std::max(height, 1))};
	ext.width = std::clamp(ext.width, caps.minImageExtent.width, caps.maxImageExtent.width);
	ext.height = std::clamp(ext.height, caps.minImageExtent.height, caps.maxImageExtent.height);
	if (ext.width == 0 || ext.height == 0) return false;
	uint32_t pmn = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(phys_, surface_, &pmn, nullptr);
	std::vector<VkPresentModeKHR> pms(pmn);
	vkGetPhysicalDeviceSurfacePresentModesKHR(phys_, surface_, &pmn, pms.data());
	VkPresentModeKHR mode = VK_PRESENT_MODE_FIFO_KHR; // zawsze dostępny (synchronizacja z odświeżaniem)
	if (!vsync_)
		for (auto m : pms)
			if (m == VK_PRESENT_MODE_MAILBOX_KHR || m == VK_PRESENT_MODE_IMMEDIATE_KHR) mode = m;
	uint32_t count = std::max(caps.minImageCount + 1, 2u);
	if (caps.maxImageCount) count = std::min(count, caps.maxImageCount);

	VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
	ci.surface = surface_;
	ci.minImageCount = count;
	ci.imageFormat = sc_.format;
	ci.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	ci.imageExtent = ext;
	ci.imageArrayLayers = 1;
	ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
	    (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT); // TRANSFER_SRC: zrzuty ekranu
	ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ci.preTransform = caps.currentTransform;
	ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	if (!(caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR))
		ci.compositeAlpha = VkCompositeAlphaFlagBitsKHR(caps.supportedCompositeAlpha & -caps.supportedCompositeAlpha);
	ci.presentMode = mode;
	ci.clipped = VK_TRUE;
	ci.oldSwapchain = sc_.swapchain;
	VkSwapchainKHR sw = VK_NULL_HANDLE;
	if (VkResult r = vkCreateSwapchainKHR(device_, &ci, nullptr, &sw); r != VK_SUCCESS)
	{
		err = "vkCreateSwapchainKHR: " + std::to_string(int(r));
		return false;
	}
	destroySwapchain();
	sc_.swapchain = sw;
	sc_.extent = ext;
	uint32_t n = 0;
	vkGetSwapchainImagesKHR(device_, sw, &n, nullptr);
	sc_.images.resize(n);
	vkGetSwapchainImagesKHR(device_, sw, &n, sc_.images.data());
	if (!finishTargets(ext, count, err)) return false;
	return true;
}

// Głębia, widoki i bufory ramek dla obrazów sc_.images (swapchain albo obraz pozaekranowy).
bool VkDevice_::finishTargets(VkExtent2D ext, uint32_t count, std::string& err)
{
	(void)err;
	VkImageCreateInfo di{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
	di.imageType = VK_IMAGE_TYPE_2D;
	di.format = depthFormat_;
	di.extent = {ext.width, ext.height, 1};
	di.mipLevels = di.arrayLayers = 1;
	di.samples = VK_SAMPLE_COUNT_1_BIT;
	di.tiling = VK_IMAGE_TILING_OPTIMAL;
	di.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	vkCreateImage(device_, &di, nullptr, &sc_.depth);
	VkMemoryRequirements req;
	vkGetImageMemoryRequirements(device_, sc_.depth, &req);
	VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	ai.allocationSize = req.size;
	ai.memoryTypeIndex = memoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	vkAllocateMemory(device_, &ai, nullptr, &sc_.depthMem);
	vkBindImageMemory(device_, sc_.depth, sc_.depthMem, 0);
	VkImageViewCreateInfo dvi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
	dvi.image = sc_.depth;
	dvi.viewType = VK_IMAGE_VIEW_TYPE_2D;
	dvi.format = depthFormat_;
	dvi.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
	vkCreateImageView(device_, &dvi, nullptr, &sc_.depthView);

	for (VkImage img : sc_.images)
	{
		VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
		vi.image = img;
		vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
		vi.format = sc_.format;
		vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
		VkImageView view;
		vkCreateImageView(device_, &vi, nullptr, &view);
		sc_.views.push_back(view);
		VkImageView atts[2] = {view, sc_.depthView};
		VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
		fi.renderPass = renderPass_;
		fi.attachmentCount = 2;
		fi.pAttachments = atts;
		fi.width = ext.width;
		fi.height = ext.height;
		fi.layers = 1;
		VkFramebuffer fb;
		vkCreateFramebuffer(device_, &fi, nullptr, &fb);
		sc_.framebuffers.push_back(fb);
		VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
		VkSemaphore s;
		vkCreateSemaphore(device_, &si, nullptr, &s);
		sc_.rendered.push_back(s);
	}
	if (imguiReady_) ImGui_ImplVulkan_SetMinImageCount(std::max(count, 2u));
	return true;
}

void VkDevice_::destroySwapchain()
{
	if (!device_) return;
	vkDeviceWaitIdle(device_);
	for (auto fb : sc_.framebuffers) vkDestroyFramebuffer(device_, fb, nullptr);
	for (auto v : sc_.views) vkDestroyImageView(device_, v, nullptr);
	for (auto s : sc_.rendered) vkDestroySemaphore(device_, s, nullptr);
	sc_.framebuffers.clear();
	sc_.views.clear();
	sc_.rendered.clear();
	if (sc_.depthView) vkDestroyImageView(device_, sc_.depthView, nullptr);
	if (sc_.depth) vkDestroyImage(device_, sc_.depth, nullptr);
	if (sc_.depthMem) vkFreeMemory(device_, sc_.depthMem, nullptr);
	sc_.depthView = VK_NULL_HANDLE;
	sc_.depth = VK_NULL_HANDLE;
	sc_.depthMem = VK_NULL_HANDLE;
	if (offscreen_)
	{
		for (VkImage img : sc_.images) vkDestroyImage(device_, img, nullptr);
		if (offscreenMem_) vkFreeMemory(device_, offscreenMem_, nullptr);
		offscreenMem_ = VK_NULL_HANDLE;
		sc_.images.clear();
		return;
	}
	sc_.images.clear();
	VkSwapchainKHR old = sc_.swapchain;
	sc_.swapchain = VK_NULL_HANDLE;
	if (old) vkDestroySwapchainKHR(device_, old, nullptr);
}

bool VkDevice_::beginFrame(int width, int height, float r, float g, float b)
{
	if (width <= 0 || height <= 0) return false;
	Frame& f = frames_[frame_];
	vkWaitForFences(device_, 1, &f.fence, VK_TRUE, UINT64_MAX);
	for (auto& fn : f.deletions) fn();
	f.deletions.clear();
	if (setsDirty_) resetDescriptorCache();
	std::string err;
	if (sc_.extent.width != uint32_t(width) || sc_.extent.height != uint32_t(height))
		if (!createSwapchain(width, height, err)) return false;
	VkResult res = VK_SUCCESS;
	image_ = 0;
	if (!offscreen_) res = vkAcquireNextImageKHR(device_, sc_.swapchain, UINT64_MAX, f.acquired, VK_NULL_HANDLE, &image_);
	if (res == VK_ERROR_OUT_OF_DATE_KHR)
	{
		createSwapchain(width, height, err);
		return false;
	}
	vkResetFences(device_, 1, &f.fence);
	f.uniformOff = kUboRange; // początek pierścienia zajmuje blok zerowy
	f.vertexOff = 0;
	vkResetCommandBuffer(f.cmd, 0);
	VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(f.cmd, &bi);
	VkClearValue clear[2];
	clear[0].color = {{r, g, b, 1.f}};
	clear[1].depthStencil = {1.f, 0};
	VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
	rp.renderPass = renderPass_;
	rp.framebuffer = sc_.framebuffers[image_];
	rp.renderArea.extent = sc_.extent;
	rp.clearValueCount = 2;
	rp.pClearValues = clear;
	vkCmdBeginRenderPass(f.cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
	inFrame_ = true;
	cur_ = SetKey{};
	cur_.frame = frame_;
	dynOff_[0] = dynOff_[1] = 0;
	descDirty_ = true;
	setViewport(0, 0, int(sc_.extent.width), int(sc_.extent.height));
	return true;
}

VkDescriptorSet VkDevice_::descriptorSet(const SetKey& key)
{
	auto it = setCache_.find(key);
	if (it != setCache_.end()) return it->second;
	if (poolIndex_ >= descPools_.size() || poolUsed_ >= kSetsPerPool)
	{
		if (poolUsed_ >= kSetsPerPool)
		{
			++poolIndex_;
			poolUsed_ = 0;
		}
		if (poolIndex_ >= descPools_.size())
		{
			VkDescriptorPoolSize sizes[3] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, kSetsPerPool * 2},
			    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, kSetsPerPool},
			    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kSetsPerPool * kTextureSlots}};
			VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
			pi.maxSets = kSetsPerPool;
			pi.poolSizeCount = 3;
			pi.pPoolSizes = sizes;
			VkDescriptorPool p;
			vkCreateDescriptorPool(device_, &pi, nullptr, &p);
			descPools_.push_back(p);
		}
	}
	VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
	ai.descriptorPool = descPools_[poolIndex_];
	ai.descriptorSetCount = 1;
	ai.pSetLayouts = &setLayout_;
	VkDescriptorSet set = VK_NULL_HANDLE;
	vkAllocateDescriptorSets(device_, &ai, &set);
	++poolUsed_;

	const Frame& f = frames_[size_t(key.frame)];
	VkDescriptorBufferInfo dyn = {f.uniform.buffer, 0, kUboRange};
	auto sb = buffers_.find(key.staticBuf);
	if (sb == buffers_.end()) sb = buffers_.find(zeroStatic_);
	VkDescriptorBufferInfo stat = {sb->second.buffer, 0, VK_WHOLE_SIZE};
	VkDescriptorImageInfo img[kTextureSlots];
	for (int i = 0; i < kTextureSlots; ++i)
	{
		auto t = textures_.find(key.tex[size_t(i)]);
		if (t == textures_.end()) t = textures_.find(white_);
		img[i] = {t->second.sampler, t->second.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
	}
	VkWriteDescriptorSet w[3 + kTextureSlots]{};
	for (int i = 0; i < 3 + kTextureSlots; ++i)
	{
		w[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[i].dstSet = set;
		w[i].dstBinding = uint32_t(i);
		w[i].descriptorCount = 1;
		if (i < 2)
		{
			w[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
			w[i].pBufferInfo = &dyn;
		}
		else if (i == 2)
		{
			w[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			w[i].pBufferInfo = &stat;
		}
		else
		{
			w[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			w[i].pImageInfo = &img[i - 3];
		}
	}
	vkUpdateDescriptorSets(device_, 3 + kTextureSlots, w, 0, nullptr);
	setCache_[key] = set;
	return set;
}

void VkDevice_::flushDescriptors()
{
	if (!descDirty_) return;
	VkDescriptorSet set = descriptorSet(cur_);
	vkCmdBindDescriptorSets(cmd(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeLayout_, 0, 1, &set, 2, dynOff_);
	descDirty_ = false;
}

void VkDevice_::endFrame(std::vector<uint8_t>* readback)
{
	Frame& f = frames_[frame_];
	VkCommandBuffer c = f.cmd;
	vkCmdEndRenderPass(c);
	bool copy = readback != nullptr;
	size_t bytes = size_t(sc_.extent.width) * sc_.extent.height * 4;
	if (copy)
	{
		if (readbackSize_ < bytes)
		{
			if (readbackBuf_.buffer) freeBuffer(readbackBuf_);
			readbackBuf_ = makeBuffer(BufferKind::Vertex, bytes);
			readbackSize_ = bytes;
		}
		VkImage img = sc_.images[image_];
		barrier(c, img, finalLayout_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 1,
		    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		    VK_PIPELINE_STAGE_TRANSFER_BIT);
		VkBufferImageCopy region{};
		region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
		region.imageExtent = {sc_.extent.width, sc_.extent.height, 1};
		vkCmdCopyImageToBuffer(c, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readbackBuf_.buffer, 1, &region);
		barrier(c, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, finalLayout_, 0, 1, VK_ACCESS_TRANSFER_READ_BIT,
		    0, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
	}
	vkEndCommandBuffer(c);
	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
	si.waitSemaphoreCount = offscreen_ ? 0 : 1;
	si.pWaitSemaphores = &f.acquired;
	si.pWaitDstStageMask = &waitStage;
	si.commandBufferCount = 1;
	si.pCommandBuffers = &c;
	si.signalSemaphoreCount = offscreen_ ? 0 : 1;
	si.pSignalSemaphores = &sc_.rendered[image_];
	vkQueueSubmit(queue_, 1, &si, f.fence);
	if (!offscreen_)
	{
		VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
		pi.waitSemaphoreCount = 1;
		pi.pWaitSemaphores = &sc_.rendered[image_];
		pi.swapchainCount = 1;
		pi.pSwapchains = &sc_.swapchain;
		pi.pImageIndices = &image_;
		vkQueuePresentKHR(queue_, &pi);
	}
	if (copy)
	{
		vkWaitForFences(device_, 1, &f.fence, VK_TRUE, UINT64_MAX);
		readback->resize(bytes);
		const uint8_t* src = static_cast<const uint8_t*>(readbackBuf_.mapped);
		bool bgra = sc_.format == VK_FORMAT_B8G8R8A8_UNORM || sc_.format == VK_FORMAT_B8G8R8A8_SRGB;
		for (size_t i = 0; i < bytes; i += 4)
		{
			(*readback)[i + 0] = src[i + (bgra ? 2 : 0)];
			(*readback)[i + 1] = src[i + 1];
			(*readback)[i + 2] = src[i + (bgra ? 0 : 2)];
			(*readback)[i + 3] = 255;
		}
	}
	inFrame_ = false;
	frame_ = (frame_ + 1) % kFrames;
}

void VkDevice_::imguiInit()
{
	ImGui_ImplSDL2_InitForVulkan(window_);
	ImGui_ImplVulkan_InitInfo ii{};
	ii.ApiVersion = VK_API_VERSION_1_2;
	ii.Instance = instance_;
	ii.PhysicalDevice = phys_;
	ii.Device = device_;
	ii.QueueFamily = queueFamily_;
	ii.Queue = queue_;
	ii.DescriptorPoolSize = 256; // pula wewnętrzna backendu (czcionki, ImGui::Image)
	ii.MinImageCount = 2;
	ii.ImageCount = uint32_t(std::max<size_t>(sc_.images.size(), 2));
	ii.PipelineInfoMain.RenderPass = renderPass_;
	ii.PipelineInfoMain.Subpass = 0;
	ii.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	ImGui_ImplVulkan_Init(&ii);
	imguiReady_ = true;
}

void VkDevice_::destroyAll()
{
	if (!device_)
	{
		if (surface_) vkDestroySurfaceKHR(instance_, surface_, nullptr);
		surface_ = VK_NULL_HANDLE;
		if (instance_) vkDestroyInstance(instance_, nullptr);
		return;
	}
	vkDeviceWaitIdle(device_);
	for (Frame& f : frames_)
	{
		for (auto& fn : f.deletions) fn();
		f.deletions.clear();
	}
	for (auto& [h, t] : textures_)
	{
		vkDestroyImageView(device_, t.view, nullptr);
		vkDestroyImage(device_, t.image, nullptr);
		vkFreeMemory(device_, t.memory, nullptr);
	}
	for (auto& [h, b] : buffers_) freeBuffer(b);
	for (auto& [h, p] : pipelines_) vkDestroyPipeline(device_, p.pipeline, nullptr);
	for (auto& [k, s] : samplers_) vkDestroySampler(device_, s, nullptr);
	for (auto p : descPools_) vkDestroyDescriptorPool(device_, p, nullptr);
	if (readbackBuf_.buffer) freeBuffer(readbackBuf_);
	for (Frame& f : frames_)
	{
		freeBuffer(f.uniform);
		freeBuffer(f.vertex);
		vkDestroyFence(device_, f.fence, nullptr);
		vkDestroySemaphore(device_, f.acquired, nullptr);
		vkDestroyCommandPool(device_, f.pool, nullptr);
	}
	destroySwapchain();
	vkDestroyCommandPool(device_, uploadPool_, nullptr);
	vkDestroyPipelineLayout(device_, pipeLayout_, nullptr);
	vkDestroyDescriptorSetLayout(device_, setLayout_, nullptr);
	vkDestroyRenderPass(device_, renderPass_, nullptr);
	vkDestroyDevice(device_, nullptr);
	if (surface_) vkDestroySurfaceKHR(instance_, surface_, nullptr);
	vkDestroyInstance(instance_, nullptr);
}

} // namespace

std::unique_ptr<Device> createDeviceVK(SDL_Window* window, bool vsync, bool offscreen, std::string& err)
{
	auto d = std::make_unique<VkDevice_>(window, vsync, offscreen);
	if (!d->init(err)) return nullptr;
	return d;
}

} // namespace ow::rhi

#endif // OW_RHI_VK
