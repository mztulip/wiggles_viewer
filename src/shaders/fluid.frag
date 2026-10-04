#version 450
#include "common.glsl"
layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;
TEX(3) uniform sampler2D uTex;
layout(location = 0) out vec4 oColor;
void main() {
	vec4 t = texture(uTex, vUV);
	oColor = vec4(t.rgb * vColor.rgb, vColor.a);
}
