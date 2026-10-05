#version 450
#include "common.glsl"
// Ramka ekranu (BorderMask): prostokąt na cały ekran, docs/silnik/09_klatka.md §9.4.
layout(location = 0) in vec2 aNdc;
void main()
{
	gl_Position = OW_CLIP(vec4(aNdc, 0.0, 1.0));
}
