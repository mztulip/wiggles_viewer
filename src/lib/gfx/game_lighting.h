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
// Nieodkryty teren (docs/silnik/10_nieodkryty_teren.md, prawo za Diggles-Unity Docs/FOG_OF_WAR_RE.md §4):
// tekstura W×H, texel (bx, by) = narożnik lewy górny pola, kanały: R = szarość g, G = czerń k,
// B = skala lamp S/255; próbkowana dwuliniowo. Po oświetleniu:
//   f = clamp((1 − (z − 7)·0,125·1,02)·g),  rozproszone ·= (1 − f),  dodatek += f·(170,170,155)/255,
//   oba · k.
#pragma once

#include "lib/rhi/rhi.h"

#include <glm/glm.hpp>
#include <vector>

namespace ow {

constexpr int kMaxGameLights = 32;

struct GameLight {
	glm::vec4 posRadius; // x, y, z (bloki), promień R (bloki)
	glm::vec4 color;     // współczynniki c (już × 31 × 0,5)
};

// Kod GLSL tego prawa: src/shaders/game_lighting.glsl.

// Tekstura nieodkrytego terenu (kNull = brak mgły) i rozmiar w polach.
struct GameFog {
	rhi::Handle texture = rhi::kNull;
	int width = 0, height = 0;
};
constexpr int kFogTextureSlot = 5; // sampler uFow (wiązanie 8)

// Blok uniformów "Scene" (wiązanie 1, std140), wspólny dla terenu i modeli w jednej klatce.
struct SceneUniforms {
	glm::vec4 lightPos[kMaxGameLights];
	glm::vec4 lightCol[kMaxGameLights];
	glm::ivec4 counts{0};   // x = liczba lamp
	glm::vec4 fowInfo{0.f}; // szerokość, wysokość, 1 = mgła włączona
};

// Ustawia lampy i mgłę klatki (wiązanie 1 i tekstura mgły). Wołać przed rysowaniem terenu i modeli.
void uploadScene(rhi::Device& dev, const std::vector<GameLight>* lights, const GameFog* fog);

} // namespace ow
