#version 450
#include "common.glsl"
// Panorama: siatka punktów ekranu z UV liczonymi na CPU (docs/silnik/08_tlo.md §8.2).
layout(location = 0) in vec2 aNdc;
layout(location = 1) in vec2 aUV;
layout(location = 0) out vec2 vUV;
void main() {
	vUV = aUV;
	gl_Position = OW_CLIP(vec4(aNdc, 0.0, 1.0));
}
