#version 450
#include "common.glsl"
layout(location = 0) in vec2 vUV;
TEX(3) uniform sampler2D uTex;
layout(location = 0) out vec4 oColor;
void main()
{
	oColor = texture(uTex, vUV);
}
