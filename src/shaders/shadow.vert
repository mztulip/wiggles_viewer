#version 450
#include "common.glsl"
// Cienie obiektów: prostokąty na ziemi budowane na CPU w układzie gry (docs/silnik/18_cienie.md).
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
UBO(0) uniform Draw
{
	mat4 uMVP;
};
layout(location = 0) out vec2 vUV;
void main()
{
	vUV = aUV;
	gl_Position = OW_CLIP(uMVP * vec4(aPos, 1.0));
}
