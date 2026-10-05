#include "lib/sphere.h"

#include <cstring>

namespace ow
{

std::vector<Vec3f> parseSphereDat(const std::vector<uint8_t>& data)
{
	std::vector<Vec3f> v;
	if (data.size() != 256 * sizeof(Vec3f)) return v;
	v.resize(256);
	std::memcpy(v.data(), data.data(), data.size());
	return v;
}

} // namespace ow
