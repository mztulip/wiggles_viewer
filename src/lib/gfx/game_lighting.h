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
//
// Mgła wojny (docs/silnik/10_mgla_wojny.md, prawo za Diggles-Unity Docs/FOG_OF_WAR_RE.md §4):
// tekstura W×H, texel (bx, by) = narożnik lewy górny pola, kanały: R = szarość g, G = czerń k,
// B = skala lamp S/255; próbkowana dwuliniowo. Po oświetleniu:
//   f = clamp((1 − (z − 7)·0,125·1,02)·g),  rozproszone ·= (1 − f),  dodatek += f·(170,170,155)/255,
//   oba · k.
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

// Tekstura mgły wojny (0 = brak mgły) i rozmiar w polach.
struct GameFog {
	unsigned texture = 0;
	int width = 0, height = 0;
};
constexpr int kFogTextureUnit = 5;

class ShaderProgram;
// Ustawia uniformy uGameLight, uLightCount, uLightPos[], uLightCol[] i mgłę wojny (uFow, uFowInfo).
void uploadGameLights(const ShaderProgram& prog, bool enabled, const std::vector<GameLight>* lights,
                      const GameFog* fog = nullptr);

} // namespace ow
