#include "lib/gfx/game_lighting.h"

#include "lib/gfx/shader.h"

#include <algorithm>

namespace ow {

const char* kGameLightingGlsl = R"(
uniform int uGameLight;
uniform int uLightCount;
uniform vec4 uLightPos[32];
uniform vec4 uLightCol[32];
uniform sampler2D uFow;
uniform vec3 uFowInfo; // szerokość, wysokość (pola), 1 = mgła włączona
// Mgła wojny w punkcie (x, y) w blokach: (szarość g, czerń k, skala lamp S/255).
vec3 fowAt(vec2 xy) {
	if (uFowInfo.z < 0.5) return vec3(0.0, 1.0, 1.0);
	return textureLod(uFow, (xy + 0.5) / uFowInfo.xy, 0.0).rgb;
}
// p: pozycja w blokach (układ gry), n: normalna (układ gry), B: jasność bazowa.
void gameLighting(vec3 p, vec3 n, float B, out vec3 diffuse, out vec3 spec) {
	vec3 fw = fowAt(p.xy);
	float sun = 3.6 - 2.546 * n.x - 2.546 * n.y;
	vec3 acc = vec3(0.0);
	for (int i = 0; i < uLightCount; ++i) {
		vec3 d = uLightPos[i].xyz - p;
		float d2 = dot(d, d);
		float r2 = uLightPos[i].w * uLightPos[i].w;
		if (d2 >= r2) continue;
		float att = exp(-2.0 * d2 / r2);
		float nl = max(dot(n, d * inversesqrt(max(d2, 1e-6))), 0.1);
		acc += uLightCol[i].rgb * att * nl;
	}
	vec3 I = (vec3(B * sun) + 255.0 * fw.z * acc) * 0.1;
	diffuse = min(I, vec3(255.0)) / 255.0;
	spec = clamp(I - 255.0, 0.0, 64.0) / 255.0;
	float f = clamp((1.0 - (p.z - 7.0) * 0.125 * 1.02) * fw.x, 0.0, 1.0);
	diffuse *= (1.0 - f) * fw.y;
	spec = (spec + f * vec3(170.0, 170.0, 155.0) / 255.0) * fw.y;
}
)";

void uploadGameLights(const ShaderProgram& prog, bool enabled, const std::vector<GameLight>* lights,
                      const GameFog* fog) {
	gl::Uniform1i(prog.uniform("uGameLight"), enabled ? 1 : 0);
	bool fogOn = enabled && fog && fog->texture && fog->width > 0;
	gl::Uniform1i(prog.uniform("uFow"), kFogTextureUnit);
	gl::Uniform3f(prog.uniform("uFowInfo"), fogOn ? float(fog->width) : 1.f, fogOn ? float(fog->height) : 1.f,
	              fogOn ? 1.f : 0.f);
	if (fogOn) {
		gl::ActiveTexture(GL_TEXTURE0 + kFogTextureUnit);
		gl::BindTexture(GL_TEXTURE_2D, fog->texture);
		gl::ActiveTexture(GL_TEXTURE0);
	}
	int n = (enabled && lights) ? std::min<int>(int(lights->size()), kMaxGameLights) : 0;
	gl::Uniform1i(prog.uniform("uLightCount"), n);
	if (n == 0) return;
	std::vector<float> pos(size_t(n) * 4), col(size_t(n) * 4);
	for (int i = 0; i < n; ++i)
		for (int k = 0; k < 4; ++k) {
			pos[size_t(i) * 4 + k] = (*lights)[size_t(i)].posRadius[k];
			col[size_t(i) * 4 + k] = (*lights)[size_t(i)].color[k];
		}
	gl::Uniform4fv(prog.uniform("uLightPos"), n, pos.data());
	gl::Uniform4fv(prog.uniform("uLightCol"), n, col.data());
}

} // namespace ow
