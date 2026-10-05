#include "lib/vfs.h"

#include <algorithm>
#include <fstream>
#include <set>

namespace fs = std::filesystem;

namespace ow
{

std::string Vfs::lower(std::string s)
{
	for (auto& c : s)
		if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
	return s;
}

void Vfs::addRoot(const fs::path& dir)
{
	roots_.insert(roots_.begin(), dir);
}

const std::unordered_map<std::string, std::string>& Vfs::dirIndex(const fs::path& dir) const
{
	auto key = dir.string();
	auto it = cache_.find(key);
	if (it != cache_.end()) return it->second;
	auto& idx = cache_[key];
	std::error_code ec;
	for (auto& e : fs::directory_iterator(dir, ec))
	{
		auto name = e.path().filename().string();
		idx.emplace(lower(name), name);
	}
	return idx;
}

std::optional<fs::path> Vfs::resolveIn(const fs::path& root, const std::string& gamePath) const
{
	fs::path cur = root;
	size_t i = 0;
	while (i <= gamePath.size())
	{
		size_t j = gamePath.find_first_of("/\\", i);
		if (j == std::string::npos) j = gamePath.size();
		std::string part = gamePath.substr(i, j - i);
		i = j + 1;
		if (part.empty() || part == ".") continue;
		const auto& idx = dirIndex(cur);
		auto it = idx.find(lower(part));
		if (it == idx.end()) return std::nullopt;
		cur /= it->second;
	}
	return cur;
}

std::optional<fs::path> Vfs::resolve(const std::string& gamePath) const
{
	for (const auto& r : roots_)
		if (auto p = resolveIn(r, gamePath)) return p;
	return std::nullopt;
}

std::optional<std::vector<uint8_t>> Vfs::read(const std::string& gamePath) const
{
	auto p = resolve(gamePath);
	if (!p) return std::nullopt;
	std::error_code ec;
	if (!fs::is_regular_file(*p, ec)) return std::nullopt;
	std::ifstream f(*p, std::ios::binary | std::ios::ate);
	if (!f) return std::nullopt;
	auto size = f.tellg();
	if (size < 0) return std::nullopt;
	std::vector<uint8_t> data(static_cast<size_t>(size));
	f.seekg(0);
	if (!data.empty() && !f.read(reinterpret_cast<char*>(data.data()), std::streamsize(data.size())))
		return std::nullopt;
	return data;
}

std::vector<std::string> Vfs::list(const std::string& gameDir) const
{
	std::set<std::string> seen;
	std::vector<std::string> out;
	for (const auto& r : roots_)
	{
		auto d = resolveIn(r, gameDir);
		if (!d) continue;
		for (const auto& [low, real] : dirIndex(*d))
			if (seen.insert(low).second) out.push_back(real);
	}
	std::sort(out.begin(), out.end(), [](const std::string& a, const std::string& b) { return lower(a) < lower(b); });
	return out;
}

} // namespace ow
