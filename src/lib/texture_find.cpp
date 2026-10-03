#include "lib/texture_find.h"

#include "lib/vfs.h"

namespace ow {

static const char* kTextureDirs[] = {"m256", "m128", "m064", "m032", "Gray", "Misc", "ClassIcons"};

std::optional<std::string> findTexture(const Vfs& vfs, const std::string& name) {
	for (const char* d : kTextureDirs) {
		std::string p = std::string("Data/Texture/") + d + "/" + name + ".tga";
		if (vfs.exists(p)) return p;
	}
	return std::nullopt;
}

} // namespace ow
