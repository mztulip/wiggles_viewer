#include "lib/model3db.h"

#include "lib/binreader.h"

namespace ow {

namespace {

Vec3f readVec3(BinReader& r) {
	Vec3f v;
	v.x = r.f32();
	v.y = r.f32();
	v.z = r.f32();
	return v;
}

template <size_t N> void readArray(BinReader& r, std::array<uint8_t, N>& out) {
	auto b = r.bytes(N);
	std::copy(b.begin(), b.end(), out.begin());
}

} // namespace

const Object3db* Model3db::findObject(const std::string& n) const {
	for (const auto& o : objects)
		if (o.name == n) return &o;
	return nullptr;
}

Model3db parseModel3db(const std::vector<uint8_t>& data) {
	BinReader r(data);
	Model3db m;

	m.version = r.str32();
	if (m.version != "3DDB 1.0") throw ParseError("nieznana wersja: '" + m.version + "'");
	m.name = r.str32();

	m.materials.resize(r.u16());
	for (auto& mat : m.materials) {
		mat.name = r.str32();
		mat.path = r.str32();
		mat.unknown = r.u32();
	}

	m.meshes.resize(r.u32());
	for (auto& mesh : m.meshes) {
		mesh.links.resize(r.u16());
		for (auto& l : mesh.links) {
			l.material = r.u16();
			l.flags = r.u16();
			l.triangles = r.u16();
			l.texcoords = r.u16();
			l.points = r.u16();
			l.normals = r.u16();
		}
		mesh.bboxMin = readVec3(r);
		mesh.bboxMax = readVec3(r);
		readArray(r, mesh.unknown80);
		mesh.shadowIndex = r.u16();
		readArray(r, mesh.unknown30);
		mesh.cmapIndex = r.u16();
	}

	m.objects.resize(r.u16());
	for (auto& o : m.objects) {
		o.name = r.str32();
		o.animations.resize(r.u16());
		for (auto& a : o.animations) a = r.u32();
	}

	m.animations.resize(r.u16());
	for (auto& a : m.animations) {
		a.name = r.str32();
		a.frames.resize(r.u16());
		for (auto& f : a.frames) f = r.u32();
		a.unknownFlags = r.u16();
		a.unknownF = r.f32();
		a.sourceFile = r.str32();
		a.move = readVec3(r);
		a.rotate = readVec3(r);
	}

	m.shadows.resize(r.u16());
	for (auto& s : m.shadows) readArray(r, s);

	m.cmaps.resize(r.u16());
	for (auto& c : m.cmaps) {
		c.width = r.u16();
		c.height = r.u16();
		c.unknown1 = r.u16();
		c.unknown2 = r.u16();
		c.pixels = r.bytes(size_t(c.width) * c.height);
	}

	m.triangles.resize(r.u16());
	m.texcoords.resize(r.u16());
	m.points.resize(r.u16());
	m.normals.resize(r.u16());
	m.unknown20.resize(r.u32());

	std::vector<uint16_t> triCounts(m.triangles.size()), uvCounts(m.texcoords.size()),
	    ptCounts(m.points.size()), brCounts(m.normals.size());
	for (auto& c : triCounts) c = r.u16();
	for (auto& c : uvCounts) c = r.u16();
	for (auto& c : ptCounts) c = r.u16();
	for (auto& c : brCounts) c = r.u16();
	for (auto& u : m.unknown20) readArray(r, u);

	for (size_t i = 0; i < m.triangles.size(); ++i) {
		m.triangles[i].resize(triCounts[i]);
		for (auto& t : m.triangles[i]) t = r.u16();
	}
	for (size_t i = 0; i < m.texcoords.size(); ++i) {
		m.texcoords[i].resize(uvCounts[i]);
		for (auto& t : m.texcoords[i]) {
			t.x = r.f32();
			t.y = r.f32();
		}
	}
	for (size_t i = 0; i < m.points.size(); ++i) {
		m.points[i].resize(ptCounts[i]);
		for (auto& p : m.points[i]) p = {r.u16(), r.u16(), r.u16()};
	}
	for (size_t i = 0; i < m.normals.size(); ++i) {
		m.normals[i].resize(brCounts[i]);
		for (auto& b : m.normals[i]) b = r.u8();
	}

	if (!r.atEnd())
		throw ParseError("nadmiarowe dane na końcu pliku: " + std::to_string(r.remaining()) + " B");

	// Walidacja odwołań, żeby renderer mógł im ufać.
	for (const auto& mesh : m.meshes)
		for (const auto& l : mesh.links) {
			if (l.material >= m.materials.size() || l.triangles >= m.triangles.size() ||
			    l.texcoords >= m.texcoords.size() || l.points >= m.points.size() ||
			    l.normals >= m.normals.size())
				throw ParseError("MeshLink poza zakresem pul");
			size_t nv = m.points[l.points].size();
			for (uint16_t idx : m.triangles[l.triangles])
				if (idx >= nv) throw ParseError("indeks trójkąta poza zakresem punktów");
		}
	for (const auto& mesh : m.meshes) {
		if (mesh.shadowIndex != 0xFFFF && mesh.shadowIndex >= m.shadows.size()) throw ParseError("indeks cienia poza zakresem");
		if (mesh.cmapIndex != 0xFFFF && mesh.cmapIndex >= m.cmaps.size()) throw ParseError("indeks cmapy poza zakresem");
	}
	for (const auto& a : m.animations)
		for (uint32_t f : a.frames)
			if (f >= m.meshes.size()) throw ParseError("klatka animacji poza zakresem meshy");
	for (const auto& o : m.objects)
		for (uint32_t a : o.animations)
			if (a >= m.animations.size()) throw ParseError("animacja obiektu poza zakresem");

	return m;
}

} // namespace ow
