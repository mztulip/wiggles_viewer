#version 450
#include "common.glsl"
// Płyny (woda, siarka, lawa): siatka pól budowana na CPU w układzie gry (docs/silnik/13_plyny.md).
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
UBO(0) uniform Draw
{
	mat4 uMVP;
	vec4 uUVOffset;
};
layout(location = 0) out vec2 vUV;
layout(location = 1) out vec4 vColor;
void main()
{
	vUV = aUV + uUVOffset.xy;
	vColor = aColor;
	gl_Position = OW_CLIP(uMVP * vec4(aPos, 1.0));
}
