#version 450
#include "common.glsl"
// Ramka ekranu (BorderMask), docs/silnik/09_klatka.md §9.4: środek piksela p od krawędzi -> teksel
// 3·(p + 0,5 − 2) paska tekstury 128×128, próbkowany liniowo (lewa górna ćwiartka róg, prawa górna pasek
// górny, lewa dolna pasek lewy); pozostałe krawędzie i rogi to odbicia.
UBO(0) uniform Draw
{
	vec4 uSize; // xy = rozmiar obszaru w pikselach, z = jego lewa kolumna w obrazie
};
TEX(3) uniform sampler2D uMask;
layout(location = 0) out vec4 oColor;
float maskAt(vec2 texel) // współrzędne w tekselach (środek teksela i = i + 0,5)
{
	return textureLod(uMask, texel / 128.0, 0.0).a;
}
void main()
{
	vec2 p = floor(gl_FragCoord.xy) - vec2(uSize.z, 0.0);
	vec2 d = min(p, uSize.xy - 1.0 - p); // odległość od najbliższej krawędzi (ramka symetryczna)
	vec2 t = clamp(3.0 * (d + 0.5 - 2.0), 0.0, 63.0) + 0.5;
	bool nx = d.x < 23.0, ny = d.y < 23.0; // dalej pasek ma alfę 0 (teksel ≥ 63)
	float a = 0.0;
	if (nx && ny) a = maskAt(t);
	else if (ny) a = maskAt(vec2(96.0, t.y));
	else if (nx) a = maskAt(vec2(t.x, 96.0));
	oColor = vec4(0.0, 0.0, 0.0, a);
}
