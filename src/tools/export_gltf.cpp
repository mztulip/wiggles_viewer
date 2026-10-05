// Eksport modeli .3db do glTF (.glb) z linii poleceń.
//
// Użycie:
//   export_gltf <katalog gry> <model> [obiekt] [animacja] [-o plik.glb] [--fps N] [--scale S] [--step]
//   export_gltf <katalog gry> <model> --all-objects [-o katalog]
// Bez obiektu: pierwszy obiekt. Bez animacji: wszystkie animacje obiektu.
// --all-objects: każdy obiekt do osobnego pliku <model>_<obiekt>.glb (np. zestawy dekoracji).

#include "lib/gltf_export.h"
#include "lib/sphere.h"
#include "lib/vfs.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>

using namespace ow;

static std::string safeName(std::string s)
{
	for (auto& c : s)
		if (!(isalnum((unsigned char)c) || c == '_' || c == '-')) c = '_';
	return s;
}

int main(int argc, char** argv)
{
	std::vector<std::string> pos;
	std::string out;
	GltfExportOptions opt;
	bool allObjects = false;
	for (int i = 1; i < argc; ++i)
	{
		std::string a = argv[i];
		if (a == "-o" && i + 1 < argc) out = argv[++i];
		else if (a == "--fps" && i + 1 < argc) opt.fps = float(std::atof(argv[++i]));
		else if (a == "--scale" && i + 1 < argc) opt.scale = float(std::atof(argv[++i]));
		else if (a == "--step") opt.interpolate = false;
		else if (a == "--all-objects") allObjects = true;
		else pos.push_back(a);
	}
	if (pos.size() < 2)
	{
		std::fprintf(stderr, "użycie: %s <katalog gry> <model> [obiekt] [animacja] [-o plik.glb] [--fps N] "
		                     "[--scale S] [--step] [--all-objects]\n",
		    argv[0]);
		return 2;
	}
	Vfs vfs;
	vfs.addRoot(pos[0]);
	std::string file = pos[1];
	if (!vfs.exists("Data/3db/" + file)) file += ".3db";
	auto data = vfs.read("Data/3db/" + file);
	if (!data)
	{
		std::fprintf(stderr, "brak modelu %s\n", pos[1].c_str());
		return 1;
	}
	Model3db m;
	try
	{
		m = parseModel3db(*data);
	}
	catch (const std::exception& e)
	{
		std::fprintf(stderr, "błąd modelu: %s\n", e.what());
		return 1;
	}
	std::vector<Vec3f> sphere;
	if (auto sd = vfs.read("Data/SPHERE.DAT")) sphere = parseSphereDat(*sd);

	auto run = [&](int objIdx, const std::string& path) {
		std::string err;
		GltfExportStats st;
		if (!exportGltf(m, vfs, sphere, objIdx, opt, path, err, &st))
		{
			std::fprintf(stderr, "BŁĄD %s: %s\n", path.c_str(), err.c_str());
			return false;
		}
		std::printf("%s: animacje %zu, klatki %zu, prymitywy %zu, tekstury %zu, %.1f MB%s\n", path.c_str(),
		    st.animations, st.frames, st.primitives, st.textures, st.bytes / 1048576.0,
		    st.incompatibleFrames ? (", klatki o innej topologii: " + std::to_string(st.incompatibleFrames)).c_str() : "");
		return true;
	};

	if (allObjects)
	{
		std::string dir = out.empty() ? "." : out;
		std::filesystem::create_directories(dir);
		int fails = 0;
		for (size_t i = 0; i < m.objects.size(); ++i)
			fails += !run(int(i), dir + "/" + safeName(m.name) + "_" + safeName(m.objects[i].name) + ".glb");
		return fails ? 1 : 0;
	}

	int obj = 0;
	if (pos.size() > 2)
	{
		obj = -1;
		for (size_t i = 0; i < m.objects.size(); ++i)
			if (m.objects[i].name == pos[2]) obj = int(i);
		if (obj < 0)
		{
			std::fprintf(stderr, "brak obiektu %s\n", pos[2].c_str());
			return 1;
		}
	}
	if (pos.size() > 3)
	{
		const auto& o = m.objects[size_t(obj)];
		for (size_t i = 0; i < o.animations.size(); ++i)
			if (m.animations[o.animations[i]].name == pos[3]) opt.animations.push_back(int(i));
		if (opt.animations.empty())
		{
			std::fprintf(stderr, "brak animacji %s\n", pos[3].c_str());
			return 1;
		}
	}
	if (out.empty())
	{
		out = safeName(m.name) + "_" + safeName(m.objects[size_t(obj)].name);
		if (pos.size() > 3) out += "_" + safeName(pos[3]);
		out += ".glb";
	}
	return run(obj, out) ? 0 : 1;
}
