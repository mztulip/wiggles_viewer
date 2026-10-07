#version 450
#include "common.glsl"
// Mieszanie cel · źródło · 2: podłoga · (1 − 0,4 · obraz cienia), więc zwracamy połowę mnożnika.
layout(location = 0) in vec2 vUV;
TEX(3) uniform sampler2D uShadow;
layout(location = 0) out vec4 oColor;
void main()
{
	float m = 1.0 - 0.4 * texture(uShadow, vUV).r;
	oColor = vec4(vec3(0.5 * m), 1.0);
}
