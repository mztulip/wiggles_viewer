#include "lib/model3db.h"
#include "lib/i18n.h"

#include "lib/binreader.h"

#include <algorithm>

namespace ow
{

namespace
{

Vec3f readVec3(BinReader& r)
{
	Vec3f v;
	v.x = r.f32();
	v.y = r.f32();
	v.z = r.f32();
	return v;
}

template <size_t N>
void readArray(BinReader& r, std::array<uint8_t, N>& out)
{
	r.readInto(out.data(), N);
}

static_assert(sizeof(Vec2f) == 8 && sizeof(MeshLink) == 12 && sizeof(std::array<uint16_t, 3>) == 6,
    "struktury czytane bezpośrednio z pliku muszą mieć rozmiar zgodny z formatem");

} // namespace

bool sameTopology(const Model3db& m, uint32_t meshA, uint32_t meshB)
{
	if (meshA >= m.meshes.size() || meshB >= m.meshes.size()) return false;
	const auto& la = m.meshes[meshA].links;
	const auto& lb = m.meshes[meshB].links;
	if (la.size() != lb.size()) return false;
	for (size_t i = 0; i < la.size(); ++i)
		if (la[i].material != lb[i].material || la[i].triangles != lb[i].triangles ||
		    m.points[la[i].points].size() != m.points[lb[i].points].size())
			return false;
	return true;
}

const Object3db* Model3db::findObject(const std::string& n) const
{
	for (const auto& o : objects)
		if (o.name == n) return &o;
	return nullptr;
}

Model3db parseModel3db(const std::vector<uint8_t>& data)
{
	BinReader r(data);
	Model3db m;

	m.version = r.str32();
	if (m.version != "3DDB 1.0") throw ParseError(strf(tr("nieznana wersja: '%s'"), m.version.c_str()));
	m.name = r.str32();

	m.materials.resize(r.u16());
	for (auto& mat : m.materials)
	{
		mat.name = r.str32();
		mat.path = r.str32();
		mat.unknown = r.u32();
	}

	m.meshes.resize(r.u32());
	for (auto& mesh : m.meshes)
	{
		r.readArray(mesh.links, r.u16()); // 6 × u16 w kolejności pól MeshLink
		mesh.bboxMin = readVec3(r);
		mesh.bboxMax = readVec3(r);
		readArray(r, mesh.linkTable);
		mesh.shadowIndex = r.u16();
		readArray(r, mesh.shadowQuad);
		mesh.cmapIndex = r.u16();
	}

	m.objects.resize(r.u16());
	for (auto& o : m.objects)
	{
		o.name = r.str32();
		r.readArray(o.animations, r.u16());
	}

	m.animations.resize(r.u16());
	for (auto& a : m.animations)
	{
		a.name = r.str32();
		r.readArray(a.frames, r.u16());
		a.unknownFlags = r.u16();
		a.unknownF = r.f32();
		a.sourceFile = r.str32();
		a.move = readVec3(r);
		a.rotate = readVec3(r);
	}

	m.shadows.resize(r.u16());
	for (auto& s : m.shadows) readArray(r, s);

	m.cmaps.resize(r.u16());
	for (auto& c : m.cmaps)
	{
		c.width = r.u16();
		c.height = r.u16();
		c.originX = int16_t(r.u16());
		c.originY = int16_t(r.u16());
		r.readArray(c.pixels, size_t(c.width) * c.height);
	}

	m.triangles.resize(r.u16());
	m.texcoords.resize(r.u16());
	m.points.resize(r.u16());
	m.normals.resize(r.u16());
	m.linkRecords.resize(r.u32());

	std::vector<uint16_t> triCounts, uvCounts, ptCounts, nrmCounts;
	r.readArray(triCounts, m.triangles.size());
	r.readArray(uvCounts, m.texcoords.size());
	r.readArray(ptCounts, m.points.size());
	r.readArray(nrmCounts, m.normals.size());
	for (auto& u : m.linkRecords) readArray(r, u);

	for (size_t i = 0; i < m.triangles.size(); ++i) r.readArray(m.triangles[i], triCounts[i]);
	for (size_t i = 0; i < m.texcoords.size(); ++i) r.readArray(m.texcoords[i], uvCounts[i]);
	for (size_t i = 0; i < m.points.size(); ++i) r.readArray(m.points[i], ptCounts[i]);
	for (size_t i = 0; i < m.normals.size(); ++i) r.readArray(m.normals[i], nrmCounts[i]);

	if (!r.atEnd())
		throw ParseError(strf(tr("nadmiarowe dane na końcu pliku: %zu B"), r.remaining()));

	// Walidacja odwołań, żeby renderer mógł im ufać.
	// Pule trójkątów są współdzielone przez wiele klatek, więc maksymalny indeks liczymy raz na pulę.
	std::vector<int> maxIndex(m.triangles.size(), -1);
	for (size_t i = 0; i < m.triangles.size(); ++i)
		for (uint16_t idx : m.triangles[i]) maxIndex[i] = std::max(maxIndex[i], int(idx));
	for (const auto& mesh : m.meshes)
		for (const auto& l : mesh.links)
		{
			if (l.material >= m.materials.size() || l.triangles >= m.triangles.size() ||
			    l.texcoords >= m.texcoords.size() || l.points >= m.points.size() ||
			    l.normals >= m.normals.size())
				throw ParseError(tr("MeshLink poza zakresem pul"));
			if (maxIndex[l.triangles] >= int(m.points[l.points].size()))
				throw ParseError(tr("indeks trójkąta poza zakresem punktów"));
		}
	for (const auto& mesh : m.meshes)
	{
		if (mesh.shadowIndex != 0xFFFF && mesh.shadowIndex >= m.shadows.size()) throw ParseError(tr("indeks cienia poza zakresem"));
		if (mesh.cmapIndex != 0xFFFF && mesh.cmapIndex >= m.cmaps.size()) throw ParseError(tr("indeks cmapy poza zakresem"));
	}
	for (const auto& a : m.animations)
		for (uint32_t f : a.frames)
			if (f >= m.meshes.size()) throw ParseError(tr("klatka animacji poza zakresem meshy"));
	for (const auto& o : m.objects)
		for (uint32_t a : o.animations)
			if (a >= m.animations.size()) throw ParseError(tr("animacja obiektu poza zakresem"));

	return m;
}

} // namespace ow
