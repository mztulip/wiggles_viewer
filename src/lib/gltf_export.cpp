#include "lib/gltf_export.h"

#include "lib/texture_find.h"
#include "lib/png_write.h"
#include "lib/text.h"
#include "lib/tga.h"
#include "lib/vfs.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>

namespace ow
{

namespace
{

constexpr int kArrayBuffer = 34962, kElementArrayBuffer = 34963;
constexpr int kFloat = 5126, kUShort = 5123;

std::string jstr(const std::string& s)
{
	std::string out = "\"";
	for (unsigned char c : s)
	{
		if (c == '"' || c == '\\') out += '\\', out += char(c);
		else if (c < 0x20)
		{
			char buf[8];
			std::snprintf(buf, sizeof buf, "\\u%04x", c);
			out += buf;
		}
		else out += char(c);
	}
	return out + "\"";
}

std::string jnum(float f)
{
	if (!std::isfinite(f)) f = 0.f;
	char buf[32];
	std::snprintf(buf, sizeof buf, "%.9g", double(f));
	return buf;
}

// Buduje JSON i bufor binarny glTF.
class GltfBuilder
{
public:
	std::vector<uint8_t> bin;
	std::vector<std::string> bufferViews, accessors, images, textures, materials, meshes, nodes, animations;

	int addView(const void* data, size_t bytes, int target = 0)
	{
		while (bin.size() % 4) bin.push_back(0);
		size_t off = bin.size();
		const auto* p = static_cast<const uint8_t*>(data);
		bin.insert(bin.end(), p, p + bytes);
		std::ostringstream o;
		o << "{\"buffer\":0,\"byteOffset\":" << off << ",\"byteLength\":" << bytes;
		if (target) o << ",\"target\":" << target;
		o << "}";
		bufferViews.push_back(o.str());
		return int(bufferViews.size()) - 1;
	}

	int addVec3(const std::vector<float>& v, bool minmax, int target = kArrayBuffer)
	{
		int view = addView(v.data(), v.size() * 4, target);
		std::ostringstream o;
		o << "{\"bufferView\":" << view << ",\"componentType\":" << kFloat << ",\"count\":" << v.size() / 3
		  << ",\"type\":\"VEC3\"";
		if (minmax)
		{
			float mn[3] = {1e30f, 1e30f, 1e30f}, mx[3] = {-1e30f, -1e30f, -1e30f};
			for (size_t i = 0; i < v.size(); ++i)
			{
				mn[i % 3] = std::min(mn[i % 3], v[i]);
				mx[i % 3] = std::max(mx[i % 3], v[i]);
			}
			o << ",\"min\":[" << jnum(mn[0]) << "," << jnum(mn[1]) << "," << jnum(mn[2]) << "],\"max\":["
			  << jnum(mx[0]) << "," << jnum(mx[1]) << "," << jnum(mx[2]) << "]";
		}
		o << "}";
		accessors.push_back(o.str());
		return int(accessors.size()) - 1;
	}

	int addVec2(const std::vector<float>& v)
	{
		int view = addView(v.data(), v.size() * 4, kArrayBuffer);
		accessors.push_back("{\"bufferView\":" + std::to_string(view) + ",\"componentType\":" +
		    std::to_string(kFloat) + ",\"count\":" + std::to_string(v.size() / 2) +
		    ",\"type\":\"VEC2\"}");
		return int(accessors.size()) - 1;
	}

	int addIndices(const std::vector<uint16_t>& idx)
	{
		int view = addView(idx.data(), idx.size() * 2, kElementArrayBuffer);
		accessors.push_back("{\"bufferView\":" + std::to_string(view) + ",\"componentType\":" +
		    std::to_string(kUShort) + ",\"count\":" + std::to_string(idx.size()) +
		    ",\"type\":\"SCALAR\"}");
		return int(accessors.size()) - 1;
	}

	// Dane animacji (bez targetu bufferView). type: SCALAR, VEC3.
	int addAnimData(const std::vector<float>& v, const char* type, int comps, bool minmax)
	{
		int view = addView(v.data(), v.size() * 4);
		std::ostringstream o;
		o << "{\"bufferView\":" << view << ",\"componentType\":" << kFloat << ",\"count\":" << v.size() / comps
		  << ",\"type\":\"" << type << "\"";
		if (minmax && !v.empty())
		{
			auto [mn, mx] = std::minmax_element(v.begin(), v.end());
			o << ",\"min\":[" << jnum(*mn) << "],\"max\":[" << jnum(*mx) << "]";
		}
		o << "}";
		accessors.push_back(o.str());
		return int(accessors.size()) - 1;
	}

	std::vector<uint8_t> glb(const std::string& sceneJson) const
	{
		std::ostringstream j;
		auto arr = [&](const char* name, const std::vector<std::string>& v) {
			if (v.empty()) return;
			j << ",\"" << name << "\":[";
			for (size_t i = 0; i < v.size(); ++i) j << (i ? "," : "") << v[i];
			j << "]";
		};
		j << "{\"asset\":{\"version\":\"2.0\",\"generator\":\"OpenWiggles\"}," << sceneJson;
		arr("nodes", nodes);
		arr("meshes", meshes);
		arr("materials", materials);
		arr("textures", textures);
		arr("images", images);
		if (!textures.empty())
			j << ",\"samplers\":[{\"magFilter\":9729,\"minFilter\":9987,\"wrapS\":10497,\"wrapT\":10497}]";
		arr("accessors", accessors);
		arr("bufferViews", bufferViews);
		arr("animations", animations);
		j << ",\"buffers\":[{\"byteLength\":" << bin.size() << "}]}";

		std::string json = j.str();
		while (json.size() % 4) json += ' ';
		std::vector<uint8_t> b = bin;
		while (b.size() % 4) b.push_back(0);

		std::vector<uint8_t> out;
		auto u32 = [&](uint32_t x) {
			for (int i = 0; i < 4; ++i) out.push_back(uint8_t(x >> (8 * i)));
		};
		u32(0x46546C67); // "glTF"
		u32(2);
		u32(uint32_t(12 + 8 + json.size() + 8 + b.size()));
		u32(uint32_t(json.size()));
		u32(0x4E4F534A); // "JSON"
		out.insert(out.end(), json.begin(), json.end());
		u32(uint32_t(b.size()));
		u32(0x004E4942); // "BIN"
		out.insert(out.end(), b.begin(), b.end());
		return out;
	}
};

} // namespace

bool exportGltf(const Model3db& m, const Vfs& vfs, const std::vector<Vec3f>& sphere, int objectIndex,
    const GltfExportOptions& opt, const std::string& outPath, std::string& err, GltfExportStats* stats)
{
	GltfExportStats st;
	if (objectIndex < 0 || size_t(objectIndex) >= m.objects.size())
	{
		err = "brak obiektu";
		return false;
	}
	const Object3db& obj = m.objects[size_t(objectIndex)];
	std::vector<int> anims = opt.animations;
	if (anims.empty())
		for (size_t i = 0; i < obj.animations.size(); ++i) anims.push_back(int(i));
	if (anims.empty())
	{
		err = "obiekt nie ma animacji";
		return false;
	}
	for (int a : anims)
		if (a < 0 || size_t(a) >= obj.animations.size())
		{
			err = "indeks animacji poza zakresem";
			return false;
		}

	const float s = opt.scale;
	auto pos = [&](const std::array<uint16_t, 3>& q) {
		Vec3f p = Model3db::dequant(q);
		return Vec3f{p.x * s, -p.y * s, p.z * s}; // układ gry (lewoskrętny, Y w dół) -> glTF (Y w górę)
	};
	auto nrm = [&](uint8_t i) {
		Vec3f n = sphere.size() == 256 ? sphere[i] : Vec3f{0, -1, 0};
		return Vec3f{n.x, -n.y, n.z};
	};

	GltfBuilder g;

	// Materiały tworzone na żądanie (tekstura wczytana raz).
	std::map<int, int> materialIndex;
	auto material = [&](int matId) -> int {
		auto it = materialIndex.find(matId);
		if (it != materialIndex.end()) return it->second;
		const auto& mat = m.materials[size_t(matId)];
		std::string name = cp1252ToUtf8(mat.name);
		std::string json;
		Image img;
		bool ok = false;
		if (auto p = findTexture(vfs, mat.name))
			if (auto data = vfs.read(*p)) ok = loadTga(*data, img);
		if (ok)
		{
			bool alpha = false;
			for (size_t i = 3; i < img.rgba.size(); i += 4)
				if (img.rgba[i] < 255)
				{
					alpha = true;
					break;
				}
			auto png = encodePng(img.width, img.height, img.rgba);
			int view = g.addView(png.data(), png.size());
			g.images.push_back("{\"name\":" + jstr(name) + ",\"bufferView\":" + std::to_string(view) +
			    ",\"mimeType\":\"image/png\"}");
			g.textures.push_back("{\"sampler\":0,\"source\":" + std::to_string(g.images.size() - 1) + "}");
			json = "{\"name\":" + jstr(name) + ",\"doubleSided\":true" +
			    (alpha ? ",\"alphaMode\":\"MASK\",\"alphaCutoff\":0.5" : "") +
			    ",\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":" +
			    std::to_string(g.textures.size() - 1) + "},\"metallicFactor\":0,\"roughnessFactor\":1}}";
			++st.textures;
		}
		else
		{
			json = "{\"name\":" + jstr(name) +
			    ",\"doubleSided\":true,\"pbrMetallicRoughness\":{\"baseColorFactor\":[1,0,1,1],"
			    "\"metallicFactor\":0,\"roughnessFactor\":1}}";
		}
		g.materials.push_back(json);
		return materialIndex[matId] = int(g.materials.size()) - 1;
	};

	std::vector<int> animNodes;
	bool multi = anims.size() > 1;

	for (size_t ai = 0; ai < anims.size(); ++ai)
	{
		const Animation3db& an = m.animations[obj.animations[size_t(anims[ai])]];
		if (an.frames.empty()) continue;
		const uint32_t base = an.frames[0];
		const size_t nTargets = an.frames.size() - 1;
		const Mesh3db& baseMesh = m.meshes[base];

		// Dla każdej klatki: którą klatkę faktycznie użyć (inna topologia -> poprzednia zgodna).
		std::vector<uint32_t> useFrame(an.frames.size(), base);
		for (size_t f = 1; f < an.frames.size(); ++f)
		{
			if (sameTopology(m, base, an.frames[f])) useFrame[f] = an.frames[f];
			else
			{
				useFrame[f] = useFrame[f - 1];
				++st.incompatibleFrames;
			}
		}

		std::ostringstream prims;
		size_t primCount = 0;
		for (size_t li = 0; li < baseMesh.links.size(); ++li)
		{
			const MeshLink& L = baseMesh.links[li];
			const auto& pts = m.points[L.points];
			const auto& tris = m.triangles[L.triangles];
			if (pts.empty() || tris.empty()) continue;
			const auto& uvs = m.texcoords[L.texcoords];
			const auto& nb = m.normals[L.normals];
			size_t n = pts.size();

			std::vector<float> P(n * 3), N(n * 3), UV(n * 2);
			for (size_t i = 0; i < n; ++i)
			{
				Vec3f p = pos(pts[i]), q = nrm(i < nb.size() ? nb[i] : 0);
				P[i * 3] = p.x, P[i * 3 + 1] = p.y, P[i * 3 + 2] = p.z;
				N[i * 3] = q.x, N[i * 3 + 1] = q.y, N[i * 3 + 2] = q.z;
				UV[i * 2] = i < uvs.size() ? uvs[i].x : 0.f;
				UV[i * 2 + 1] = i < uvs.size() ? uvs[i].y : 0.f;
			}
			int aPos = g.addVec3(P, true), aNrm = g.addVec3(N, false), aUV = g.addVec2(UV);
			int aIdx = g.addIndices(tris);

			std::ostringstream targets;
			for (size_t t = 0; t < nTargets; ++t)
			{
				const MeshLink& Lt = m.meshes[useFrame[t + 1]].links[li];
				const auto& pt = m.points[Lt.points];
				const auto& nt = m.normals[Lt.normals];
				std::vector<float> dP(n * 3), dN(n * 3);
				for (size_t i = 0; i < n; ++i)
				{
					Vec3f p = pos(pt[i]), q = nrm(i < nt.size() ? nt[i] : 0);
					dP[i * 3] = p.x - P[i * 3], dP[i * 3 + 1] = p.y - P[i * 3 + 1], dP[i * 3 + 2] = p.z - P[i * 3 + 2];
					dN[i * 3] = q.x - N[i * 3], dN[i * 3 + 1] = q.y - N[i * 3 + 1], dN[i * 3 + 2] = q.z - N[i * 3 + 2];
				}
				int tp = g.addVec3(dP, true), tn = g.addVec3(dN, false);
				targets << (t ? "," : "") << "{\"POSITION\":" << tp << ",\"NORMAL\":" << tn << "}";
			}

			prims << (primCount ? "," : "") << "{\"attributes\":{\"POSITION\":" << aPos << ",\"NORMAL\":" << aNrm
			      << ",\"TEXCOORD_0\":" << aUV << "},\"indices\":" << aIdx << ",\"material\":" << material(L.material);
			if (nTargets) prims << ",\"targets\":[" << targets.str() << "]";
			prims << "}";
			++primCount;
		}
		if (!primCount) continue;
		st.primitives += primCount;
		st.frames += an.frames.size();

		std::string animName = cp1252ToUtf8(an.name);
		std::ostringstream mesh;
		mesh << "{\"name\":" << jstr(animName) << ",\"primitives\":[" << prims.str() << "]";
		if (nTargets)
		{
			mesh << ",\"weights\":[";
			for (size_t t = 0; t < nTargets; ++t) mesh << (t ? "," : "") << "0";
			mesh << "]";
		}
		mesh << "}";
		g.meshes.push_back(mesh.str());
		int meshIdx = int(g.meshes.size()) - 1;

		bool visible = !multi || animNodes.empty();
		g.nodes.push_back("{\"name\":" + jstr(animName) + ",\"mesh\":" + std::to_string(meshIdx) +
		    (visible ? "" : ",\"scale\":[0,0,0]") + "}");
		int node = int(g.nodes.size()) - 1;
		animNodes.push_back(node);

		// Klip animacji.
		std::ostringstream samplers, channels;
		int ns = 0;
		const char* interp = opt.interpolate ? "LINEAR" : "STEP";
		if (nTargets)
		{
			std::vector<float> times(an.frames.size()), w(an.frames.size() * nTargets, 0.f);
			for (size_t f = 0; f < an.frames.size(); ++f)
			{
				times[f] = float(f) / opt.fps;
				if (f) w[f * nTargets + (f - 1)] = 1.f;
			}
			int in = g.addAnimData(times, "SCALAR", 1, true);
			int out = g.addAnimData(w, "SCALAR", 1, false);
			samplers << "{\"input\":" << in << ",\"output\":" << out << ",\"interpolation\":\"" << interp << "\"}";
			channels << "{\"sampler\":" << ns++ << ",\"target\":{\"node\":" << node << ",\"path\":\"weights\"}}";
		}
		if (multi)
		{
			// Widoczność: pokaż własny węzeł, ukryj domyślnie widoczny węzeł pierwszej animacji.
			std::vector<float> t0 = {0.f}, one = {1, 1, 1}, zero = {0, 0, 0};
			int in = g.addAnimData(t0, "SCALAR", 1, true);
			auto addScale = [&](int targetNode, const std::vector<float>& value) {
				int out = g.addAnimData(value, "VEC3", 3, false);
				samplers << (ns ? "," : "") << "{\"input\":" << in << ",\"output\":" << out
				         << ",\"interpolation\":\"STEP\"}";
				channels << (ns ? "," : "") << "{\"sampler\":" << ns << ",\"target\":{\"node\":" << targetNode
				         << ",\"path\":\"scale\"}}";
				++ns;
			};
			addScale(node, one);
			if (node != animNodes.front()) addScale(animNodes.front(), zero);
		}
		if (ns)
			g.animations.push_back("{\"name\":" + jstr(animName) + ",\"samplers\":[" + samplers.str() +
			    "],\"channels\":[" + channels.str() + "]}");
		++st.animations;
	}

	if (animNodes.empty())
	{
		err = "brak geometrii do eksportu";
		return false;
	}

	std::ostringstream root;
	root << "{\"name\":" << jstr(cp1252ToUtf8(obj.name)) << ",\"children\":[";
	for (size_t i = 0; i < animNodes.size(); ++i) root << (i ? "," : "") << animNodes[i];
	root << "]}";
	g.nodes.push_back(root.str());
	std::string scene = "\"scene\":0,\"scenes\":[{\"name\":" + jstr(cp1252ToUtf8(m.name)) + ",\"nodes\":[" +
	    std::to_string(g.nodes.size() - 1) + "]}]";

	auto glb = g.glb(scene);
	std::ofstream f(outPath, std::ios::binary);
	if (!f || !f.write(reinterpret_cast<const char*>(glb.data()), std::streamsize(glb.size())))
	{
		err = "nie można zapisać " + outPath;
		return false;
	}
	st.bytes = glb.size();
	if (stats) *stats = st;
	return true;
}

} // namespace ow
