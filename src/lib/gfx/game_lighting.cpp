#include "lib/gfx/game_lighting.h"

#include <algorithm>

namespace ow
{

void uploadScene(rhi::Device& dev, const std::vector<GameLight>* lights, const GameFog* fog)
{
	SceneUniforms u{};
	int n = lights ? std::min<int>(int(lights->size()), kMaxGameLights) : 0;
	for (int i = 0; i < n; ++i)
	{
		u.lightPos[i] = (*lights)[size_t(i)].posRadius;
		u.lightCol[i] = (*lights)[size_t(i)].color;
	}
	u.counts.x = n;
	bool fogOn = fog && fog->texture && fog->width > 0;
	u.fowInfo = glm::vec4(fogOn ? float(fog->width) : 1.f, fogOn ? float(fog->height) : 1.f, fogOn ? 1.f : 0.f, 0.f);
	dev.setUniforms(rhi::kBindFrame, &u, sizeof u);
	dev.bindTexture(kFogTextureSlot, fogOn ? fog->texture : rhi::kNull);
}

} // namespace ow
