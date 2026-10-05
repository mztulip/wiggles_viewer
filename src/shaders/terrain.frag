#version 450
#include "common.glsl"
#include "terrain_ubo.glsl"
layout(location = 0) in vec3 vNormal;
layout(location = 1) in float vDepth;
layout(location = 2) flat in vec4 vCorner;
layout(location = 3) in float vAlpha;
layout(location = 4) in vec3 vGameDiffuse;
layout(location = 5) in vec3 vGameSpec;
layout(location = 6) in vec2 vUV;
TEX(3) uniform sampler2D uTex; // grunt A (w przebiegu detali: tekstura detalu)
TEX(4) uniform sampler2D uTexB; // grunt B
TEX(5) uniform sampler2D uTexC; // grunt C
TEX(6) uniform sampler2D uMasks; // atlas 4×4 masek Msk_<TL TR BL BR>01: komórka = bity (B = 1; TL 8, TR 4, BL 2, BR 1)
layout(location = 0) out vec4 oColor;
void main()
{
	// Alpha test jak w grze (> 126/255): ostre wycięcie otworów do tła (§4.5).
	if (vAlpha * 255.0 <= 126.0) discard;
	if (uFlags.y == 1)
	{
		// Detal .l2m w trybie modulate ×2 (szary 128 = bez zmian), UV z palety jak grunt (§4.4).
		oColor = vec4(texture(uTex, vUV).rgb, 1.0);
		return;
	}
	vec3 tex[3];
	tex[0] = texture(uTex, vUV).rgb;
	tex[1] = texture(uTexB, vUV).rgb;
	tex[2] = texture(uTexC, vUV).rgb;
	// Przejścia gruntów (§4.2): przy dwóch gruntach w narożnikach maska Msk_* (biel = A, czerń = B).
	// 🔶 A = grunt o niższym indeksie; trzy grunty w bloku: dwa wyższe łączymy jako B.
	ivec4 c = ivec4(vCorner + 0.5);
	int gA = min(min(c.x, c.y), min(c.z, c.w));
	int gB = max(max(c.x, c.y), max(c.z, c.w));
	vec3 t = tex[gA];
	if (gB != gA)
	{
		int bits = (c.x != gA ? 8 : 0) + (c.y != gA ? 4 : 0) + (c.z != gA ? 2 : 0) + (c.w != gA ? 1 : 0);
		vec2 cell = vec2(float(bits % 4), float(bits / 4));
		vec2 f = clamp(fract(vUV), vec2(0.5 / 64.0), vec2(1.0 - 0.5 / 64.0));
		float m = texture(uMasks, (cell + f) / 4.0).r;
		t = mix(tex[gB], tex[gA], m);
	}
	if (uFlags.x == 1)
	{
		oColor = vec4(t * vGameDiffuse + vGameSpec, 1.0) * uTint;
		return;
	}
	float l = uLightDirAmb.w + (1.0 - uLightDirAmb.w) * max(dot(normalize(vNormal), uLightDirAmb.xyz), 0.0);
	l *= mix(1.0, 0.55, clamp(vDepth, 0.0, 1.0)); // głębiej ciemniej
	oColor = vec4(t * l, 1.0) * uTint;
}
