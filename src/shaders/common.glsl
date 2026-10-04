// Wspólne makra shaderów OpenWiggles. Każdy shader to GLSL 450, kompilowany:
//  - dla Vulkana przez glslc do SPIR-V (zdefiniowane OW_VULKAN),
//  - dla OpenGL 4.5 jako tekst (zdefiniowane OW_OPENGL).
// Wiązania (zgodne z src/lib/rhi/rhi.h): 0 = uniformy rysowania, 1 = uniformy klatki (lampy, mgła),
// 2 = stały bufor (normalne SPHERE.DAT), 3..8 = tekstury (sloty 0..5).
#ifdef OW_VULKAN
#define UBO(n) layout(std140, set = 0, binding = n)
#define TEX(n) layout(set = 0, binding = n)
// Vulkan ma głębię w przycięciu 0..1 (OpenGL −1..1). Oś Y odwraca backend ujemną wysokością viewportu.
#define OW_CLIP(p) vec4((p).x, (p).y, ((p).z + (p).w) * 0.5, (p).w)
#else
#define UBO(n) layout(std140, binding = n)
#define TEX(n) layout(binding = n)
#define OW_CLIP(p) (p)
#endif
