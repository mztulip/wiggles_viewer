// Oświetlenie per wierzchołek jak w Wiggles.exe (docs/silnik/07_oswietlenie.md) i nieodkryty teren
// (docs/silnik/10_nieodkryty_teren.md §10.5). Prawo i stałe za Zimennik/Diggles-Unity (MIT),
// Docs/RENDERING_LIGHTING_RESEARCH.md §2a i Docs/FOG_OF_WAR_RE.md.
UBO(1) uniform Scene
{
	vec4 uLightPos[32]; // x, y, z (bloki), promień
	vec4 uLightCol[32]; // c = int(kolor·31)·0,5
	ivec4 uSceneI;      // x = liczba lamp
	vec4 uFowInfo;      // szerokość, wysokość (pola), 1 = mgła włączona
};
TEX(8) uniform sampler2D uFow; // R = szarość g, G = czerń k, B = skala lamp S/255 (narożniki pól)

vec3 fowAt(vec2 xy)
{
	if (uFowInfo.z < 0.5) return vec3(0.0, 1.0, 1.0);
	return textureLod(uFow, (xy + 0.5) / uFowInfo.xy, 0.0).rgb;
}

// p: pozycja w blokach (układ gry), n: normalna (układ gry), B: jasność bazowa.
void gameLighting(vec3 p, vec3 n, float B, out vec3 diffuse, out vec3 spec)
{
	vec3 fw = fowAt(p.xy);
	float sun = 3.6 - 2.546 * n.x - 2.546 * n.y;
	vec3 acc = vec3(0.0);
	for (int i = 0; i < uSceneI.x; ++i)
	{
		vec3 d = uLightPos[i].xyz - p;
		float d2 = dot(d, d);
		float r2 = uLightPos[i].w * uLightPos[i].w;
		if (d2 >= r2) continue;
		float att = exp(-2.0 * d2 / r2);
		float nl = max(dot(n, d * inversesqrt(max(d2, 1e-6))), 0.1);
		acc += uLightCol[i].rgb * att * nl;
	}
	vec3 I = (vec3(B * sun) + 255.0 * fw.z * acc) * 0.1;
	diffuse = min(I, vec3(255.0)) / 255.0;
	spec = clamp(I - 255.0, 0.0, 64.0) / 255.0;
	float f = clamp((1.0 - (p.z - 7.0) * 0.125 * 1.02) * fw.x, 0.0, 1.0);
	diffuse *= (1.0 - f) * fw.y;
	spec = (spec + f * vec3(170.0, 170.0, 155.0) / 255.0) * fw.y;
}
