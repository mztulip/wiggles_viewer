#version 450
#include "common.glsl"
// Mieszanie addytywne (ONE, ONE): kolor tekstury × kolor cząsteczki, wygaszany przez alfę.
layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;
TEX(3) uniform sampler2D uAtlas;
layout(location = 0) out vec4 oColor;
void main()
{
	vec4 t = texture(uAtlas, vUV);
	oColor = vec4(t.rgb * t.a * vColor.rgb * vColor.a, 0.0);
}
