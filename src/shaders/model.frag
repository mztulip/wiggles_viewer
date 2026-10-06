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
	int draw = uFlags.w % 100; // tryb rysowania linku
	bool fringe = uFlags.w >= 100; // faza przezroczysta: miękkie brzegi trybu 3, całe linki 6 i 7
	// 🔶 5–7: mapowanie otoczenia, współrzędne tekstury z normalnej (kamera gry patrzy prawie wzdłuż −z).
	vec2 uv = (draw >= 5 && draw <= 7) ? normalize(vNormal).xy * 0.5 + 0.5 : vUV;
	vec4 t = texture(uTex, uv);
	// 2 (addytywny) bez progu alfa. 3: rdzeń (alfa ≥ 0,5) jak 1, a miękkie brzegi w osobnej fazie bez zapisu
	// głębi, po wszystkich nieprzezroczystych obiektach sceny (inaczej brzeg zasłania obiekty rysowane później).
	// 🔶 6, 7: w całości w tej fazie, mieszane alfą tekstury. ✅ 8, 9, 11 nieprzezroczyste: alfa nie wycina
	// (BAZA_WIEDZY §8.10); 1 i 4 wycinają progiem 0,5.
	bool opaque = draw == 8 || draw == 9 || draw == 11;
	if (draw == 2)
	{
		if (t.a < 1.0 / 255.0) discard;
	}
	else if (fringe)
	{
		if ((draw == 3 && t.a >= 0.5) || t.a < 1.0 / 255.0) discard;
	}
	else if (uFlags.y != 0 && !opaque && t.a < 0.5) discard;
	vec3 c = t.rgb;
	int mode = uFlags.x;
	bool lit = draw != 2 && draw != 11; // 🔶 2 (blask) bez oświetlenia; ✅ 11 bez oświetlenia i bez maski
	vec3 unlit = c;
	if (mode == 3) c = normalize(vNormal) * 0.5 + 0.5;
	else if (lit && uFlags.z == 1) c = c * vGameDiffuse + vGameSpec;
	else if (lit && mode == 1) c *= vByte;
	else if (lit && mode == 2)
	{
		vec3 n = normalize(vNormal);
		c *= uLightDirAmb.w + (1.0 - uLightDirAmb.w) * max(dot(n, uLightDirAmb.xyz), 0.0);
	}
	// 🔶 8: alfa tekstury to maska samoświecenia z progiem 0,5 (te same piksele, które tryb 1 zostawia po teście
	// alfy, świecą własnym kolorem; reszta oświetlona jak 1). BAZA_WIEDZY §8.10.
	if (draw == 8 && mode != 3 && t.a >= 0.5) c = unlit;
	if (opaque) t.a = 1.0;
	// Podświetlenie w przeglądarce (obiekt pod kursorem / zaznaczony): rozjaśnienie w stronę jasnego błękitu.
	if (uParams.w > 0.0) c = mix(c, vec3(0.55, 0.8, 1.0), uParams.w);
	// Strefa horyzontu (docs/silnik/08_tlo.md §8.1): modele za terenem przechodzą w kolor strefy.
	float zb = vGameZ / 200.0;
	if (uHorizon.a > 0.0 && zb < 0.0) c = mix(c, uHorizon.rgb, clamp(-zb / uHorizon.a, 0.0, 1.0));
	oColor = draw == 2 ? vec4(c * t.a, 1.0) * uTint : vec4(c, t.a) * uTint;
}
