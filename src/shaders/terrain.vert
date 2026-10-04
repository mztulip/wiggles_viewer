#version 450
#include "common.glsl"
#include "terrain_ubo.glsl"
#include "game_lighting.glsl"
// Teren z BlockDB (docs/silnik/04_teren.md).
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 3) in vec4 aCorner; // grunty narożników TL, TR, BL, BR (0 A, 1 B, 2 C)
layout(location = 4) in float aAlpha; // alfa wycięcia (otwory do tła)
layout(location = 5) in float aBase;  // jasność bazowa (oświetlenie gry)
layout(location = 6) in vec2 aUV;     // UV gruntu z palety BlockDB
layout(location = 0) out vec3 vNormal;
layout(location = 1) out float vDepth;
layout(location = 2) flat out vec4 vCorner;
layout(location = 3) out float vAlpha;
layout(location = 4) out vec3 vGameDiffuse;
layout(location = 5) out vec3 vGameSpec;
layout(location = 6) out vec2 vUV;
void main() {
	vGameDiffuse = vec3(1.0);
	vGameSpec = vec3(0.0);
	if (uFlags.x == 1) gameLighting(aPos / 200.0, aNormal, aBase, vGameDiffuse, vGameSpec);
	vCorner = aCorner;
	vAlpha = aAlpha;
	vUV = aUV;
	vNormal = mat3(uWorld) * aNormal;
	vDepth = (2900.0 - aPos.z) / 1500.0; // 0 = przednia płaszczyzna, 1 = najgłębiej
	gl_Position = OW_CLIP(uViewProj * uWorld * vec4(aPos, 1.0));
}
