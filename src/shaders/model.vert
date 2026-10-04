#version 450
#include "common.glsl"
#include "model_ubo.glsl"
#include "game_lighting.glsl"
// Model .3db: interpolacja dwóch klatek (docs/formaty/3db.md §8), normalne z SPHERE.DAT.
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in float aByte;  // indeks normalnej
layout(location = 3) in vec3 aPos2;   // pozycja w klatce B
layout(location = 4) in float aByte2; // normalna w klatce B
UBO(2) uniform Sphere { vec4 uSphere[256]; };
layout(location = 0) out vec2 vUV;
layout(location = 1) out float vByte;
layout(location = 2) out vec3 vNormal;
layout(location = 3) out float vGameZ;
layout(location = 4) out vec3 vGameDiffuse;
layout(location = 5) out vec3 vGameSpec;
void main() {
	float lerpT = uParams.x;
	vUV = aUV;
	vByte = mix(aByte, aByte2, lerpT) / 255.0;
	vec3 n = normalize(mix(uSphere[int(aByte + 0.5)].xyz, uSphere[int(aByte2 + 0.5)].xyz, lerpT));
	vNormal = mat3(uModel) * n;
	vec3 mp = mix(aPos, aPos2, lerpT);
	vGameDiffuse = vec3(1.0);
	vGameSpec = vec3(0.0);
	if (uFlags.z == 1) {
		// 🔶 Oświetlenie dwustronne (docs/silnik/07_oswietlenie.md §7.5): jaśniejszy wynik z N i −N.
		vec3 gp = (uGameModel * vec4(mp, 1.0)).xyz / 200.0;
		vec3 gn = normalize(mat3(uGameModel) * n), d1, s1, d2, s2;
		gameLighting(gp, gn, uParams.z, d1, s1);
		gameLighting(gp, -gn, uParams.z, d2, s2);
		bool first = dot(d1 + s1, vec3(1.0)) >= dot(d2 + s2, vec3(1.0));
		vGameDiffuse = first ? d1 : d2;
		vGameSpec = first ? s1 : s2;
	}
	vec4 wp = uModel * vec4(mp, 1.0);
	vGameZ = wp.z + uParams.y;
	gl_Position = OW_CLIP(uViewProj * wp);
}
