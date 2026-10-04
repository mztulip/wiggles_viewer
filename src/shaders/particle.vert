#version 450
#include "common.glsl"
// Cząsteczki: billboardy budowane na CPU w układzie gry (docs/silnik/12_czasteczki.md).
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
UBO(0) uniform Draw { mat4 uMVP; };
layout(location = 0) out vec2 vUV;
layout(location = 1) out vec4 vColor;
void main() {
	vUV = aUV;
	vColor = aColor;
	gl_Position = OW_CLIP(uMVP * vec4(aPos, 1.0));
}
