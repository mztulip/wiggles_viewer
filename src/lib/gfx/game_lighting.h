// Oświetlenie per wierzchołek jak w Wiggles.exe (teren FUN_004cc040, modele FUN_004856e0).
// Prawo i stałe za Zimennik/Diggles-Unity (MIT), Docs/RENDERING_LIGHTING_RESEARCH.md §2a:
//
//   I_c = (B · słońce(N) + S · Σ_i c_i,c · exp(−2·d_i²/R_i²) · max(N·L_i, 0,1)) · 0,1
//   słońce(N) = 3,6 − 2,546·nx − 2,546·ny       (stałe światło z lewej góry, nz pomijane)
//   B = 16 · warstwa2(dwuliniowo z narożników) + 64,  S = 255 (pole odkryte)
//   c = int(kolor·31) · 0,5  (class_light {przesunięcie} promień {r g b})
//   kolor rozproszony = min(I, 255)/255,  nadwyżka = clamp(I − 255, 0, 64)/255 (dodawana)
//
// Współrzędne: układ gry w blokach (x w prawo, y w dół, z do widza; teren z ∈ [7, 14,5]).
#pragma once

#include <glm/glm.hpp>
#include <vector>

namespace ow {

constexpr int kMaxGameLights = 32;

struct GameLight {
	glm::vec4 posRadius; // x, y, z (bloki), promień R (bloki)
	glm::vec4 color;     // współczynniki c (już × 31 × 0,5)
};

// Wspólny kod GLSL (wstawiany do shaderów wierzchołków).
extern const char* kGameLightingGlsl;

class ShaderProgram;
// Ustawia uniformy uGameLight, uLightCount, uLightPos[], uLightCol[].
void uploadGameLights(const ShaderProgram& prog, bool enabled, const std::vector<GameLight>* lights);

} // namespace ow
