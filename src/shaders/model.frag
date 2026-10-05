#version 450
#include "common.glsl"
#include "model_ubo.glsl"
layout(location = 0) in vec2 vUV;
layout(location = 1) in float vByte;
layout(location = 2) in vec3 vNormal;
layout(location = 3) in float vGameZ;
layout(location = 4) in vec3 vGameDiffuse;
layout(location = 5) in vec3 vGameSpec;
TEX(3) uniform sampler2D uTex;
layout(location = 0) out vec4 oColor;
void main()
{
	int draw = uFlags.w; // tryb rysowania linku
	// 🔶 5–7: mapowanie otoczenia, współrzędne tekstury z normalnej (kamera gry patrzy prawie wzdłuż −z).
	vec2 uv = (draw >= 5 && draw <= 7) ? normalize(vNormal).xy * 0.5 + 0.5 : vUV;
	vec4 t = texture(uTex, uv);
	// 2 (addytywny) bez progu alfa. 🔶 3 (miękka alfa) na razie jak 1: miękki brzeg zapisujący głębię zasłania
	// obiekty rysowane później (widać przez niego niebo); wymaga osobnego przebiegu przezroczystych w scenie.
	if (draw == 2)
	{
		if (t.a < 1.0 / 255.0) discard;
	}
	else if (uFlags.y != 0 && t.a < 0.5) discard;
	vec3 c = t.rgb;
	int mode = uFlags.x;
	bool lit = draw != 8 && draw != 2; // 8 bez oświetlenia; 🔶 2 (blask) też
	if (mode == 3) c = normalize(vNormal) * 0.5 + 0.5;
	else if (lit && uFlags.z == 1) c = c * vGameDiffuse + vGameSpec;
	else if (lit && mode == 1) c *= vByte;
	else if (lit && mode == 2)
	{
		vec3 n = normalize(vNormal);
		c *= uLightDirAmb.w + (1.0 - uLightDirAmb.w) * max(dot(n, uLightDirAmb.xyz), 0.0);
	}
	// Strefa horyzontu (docs/silnik/08_tlo.md §8.1): modele za terenem przechodzą w kolor strefy.
	float zb = vGameZ / 200.0;
	if (uHorizon.a > 0.0 && zb < 0.0) c = mix(c, uHorizon.rgb, clamp(-zb / uHorizon.a, 0.0, 1.0));
	oColor = draw == 2 ? vec4(c * t.a, 1.0) * uTint : vec4(c, t.a) * uTint;
}
