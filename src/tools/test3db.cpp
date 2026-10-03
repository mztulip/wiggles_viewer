// Test parserów na danych gry: wszystkie modele Data/3db/*.3db i tekstury Data/Texture/**.tga.
// Sprawdza też założenia o danych (zgodność rozmiarów pul, zakresy), żeby wiedzieć,
// czemu renderer może ufać.
//
// Użycie: test3db <katalog gry>      (np. ~/.wine/drive_c/Program Files (x86)/Innonics/Wiggles)

#include "lib/model3db.h"
#include "lib/binreader.h"
#include "lib/tga.h"
#include "lib/vfs.h"

#include <cstdio>
#include <map>

using namespace ow;

static int testModels(const Vfs& vfs) {
	int failed = 0;
	size_t uvMismatch = 0, nrmMismatch = 0, links = 0;
	uint64_t nrmHist[8] = {};
	for (const auto& f : vfs.list("Data/3db")) {
		auto data = vfs.read("Data/3db/" + f);
		try {
			Model3db m = parseModel3db(*data);
			size_t frames = 0;
			for (auto& a : m.animations) frames += a.frames.size();
			for (auto& mesh : m.meshes)
				for (auto& l : mesh.links) {
					++links;
					size_t nv = m.points[l.points].size();
					if (m.texcoords[l.texcoords].size() != nv) ++uvMismatch;
					if (m.normals[l.normals].size() != nv) ++nrmMismatch;
				}
			for (auto& pool : m.normals)
				for (uint8_t b : pool) ++nrmHist[b / 32];
			std::printf("OK   %-24s mat=%3zu mesh=%5zu obj=%4zu anim=%4zu klatek=%6zu cien=%3zu cmap=%2zu\n",
			            f.c_str(), m.materials.size(), m.meshes.size(), m.objects.size(),
			            m.animations.size(), frames, m.shadows.size(), m.cmaps.size());
		} catch (const ParseError& e) {
			std::printf("FAIL %-24s %s\n", f.c_str(), e.what());
			++failed;
		}
	}
	std::printf("\nlinków: %zu, UV != punkty: %zu, normalne != punkty: %zu\n", links, uvMismatch, nrmMismatch);
	std::printf("histogram indeksów normalnych (co 32):");
	for (auto h : nrmHist) std::printf(" %llu", (unsigned long long)h);
	std::printf("\n");
	return failed;
}

static int testTextures(const Vfs& vfs) {
	int failed = 0, ok = 0;
	std::map<std::string, int> kinds;
	for (const auto& dir : vfs.list("Data/Texture")) {
		std::string d = "Data/Texture/" + dir;
		for (const auto& f : vfs.list(d)) {
			if (Vfs::lower(f).size() < 4 || Vfs::lower(f).substr(f.size() - 4) != ".tga") continue;
			auto data = vfs.read(d + "/" + f);
			Image img;
			std::string err;
			if (!data || !loadTga(*data, img, &err)) {
				std::printf("FAIL %s/%s: %s\n", d.c_str(), f.c_str(), err.c_str());
				++failed;
				continue;
			}
			++ok;
			kinds["typ" + std::to_string((*data)[2]) + " " + std::to_string((*data)[16]) + "bit"]++;
		}
	}
	std::printf("tekstury OK: %d, błędy: %d\n", ok, failed);
	for (auto& [k, v] : kinds) std::printf("  %-14s %d\n", k.c_str(), v);
	return failed;
}

int main(int argc, char** argv) {
	if (argc < 2) {
		std::fprintf(stderr, "użycie: %s <katalog gry>\n", argv[0]);
		return 2;
	}
	Vfs vfs;
	vfs.addRoot(argv[1]);
	if (!vfs.exists("Data/3db")) {
		std::fprintf(stderr, "brak Data/3db w %s\n", argv[1]);
		return 2;
	}
	int failed = testModels(vfs);
	std::printf("\n");
	failed += testTextures(vfs);
	std::printf("\n%s\n", failed ? "BŁĘDY" : "WSZYSTKO OK");
	return failed ? 1 : 0;
}
