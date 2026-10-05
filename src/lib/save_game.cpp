#include "lib/save_game.h"
#include "lib/i18n.h"

#include "lib/binreader.h"

#include <zlib.h>

#include <cmath>
#include <cstring>

namespace ow
{

std::optional<std::vector<uint8_t>> gunzip(const std::vector<uint8_t>& data, std::string& err)
{
	z_stream zs{};
	if (inflateInit2(&zs, 16 + MAX_WBITS) != Z_OK)
	{
		err = "inflateInit2"; // nazwa funkcji zlib
		return std::nullopt;
	}
	std::vector<uint8_t> out;
	out.resize(std::max<size_t>(data.size() * 8, 1 << 16));
	zs.next_in = const_cast<Bytef*>(data.data());
	zs.avail_in = uInt(data.size());
	int r = Z_OK;
	while (r != Z_STREAM_END)
	{
		if (zs.total_out >= out.size()) out.resize(out.size() * 2);
		zs.next_out = out.data() + zs.total_out;
		zs.avail_out = uInt(out.size() - zs.total_out);
		r = inflate(&zs, Z_NO_FLUSH);
		if (r != Z_OK && r != Z_STREAM_END)
		{
			err = std::string("inflate: ") + (zs.msg ? zs.msg : std::to_string(r));
			inflateEnd(&zs);
			return std::nullopt;
		}
	}
	out.resize(zs.total_out);
	inflateEnd(&zs);
	return out;
}

namespace
{

// Czytnik archiwum zapisu: liczby little-endian bez wyrównania, napisy z długością (z zerem na końcu).
class SaveReader
{
public:
	explicit SaveReader(const std::vector<uint8_t>& d) : r_(d) {}
	size_t pos() const { return r_.pos(); }
	void skip(size_t n) { r_.skip(n); }
	void skipI32(size_t n) { r_.skip(n * 4); }
	uint8_t u8() { return r_.u8(); }
	int32_t i32() { return int32_t(r_.u32()); }
	float f32() { return r_.f32(); }
	double f64()
	{
		double v;
		r_.readInto(&v, 8);
		return v;
	}
	int32_t count(const char* what)
	{
		int32_t n = i32();
		if (n < 0 || n > (4 << 20)) throw ParseError(strf(tr("nieprawidłowa liczba: %s = %d"), tr(what), int(n)));
		return n;
	}
	bool flag(const char* what)
	{
		uint8_t b = u8();
		if (b > 1) throw ParseError(strf(tr("nieprawidłowy bajt obecności: %s"), tr(what)));
		return b == 1;
	}
	void marker(int32_t expected)
	{
		size_t at = pos();
		int32_t v = i32();
		if (v != expected)
			throw ParseError(strf(tr("oczekiwano znacznika %d, jest %d (bajt %zu)"), int(expected), int(v), at));
	}
	std::string str()
	{
		int32_t n = i32();
		if (n <= 0 || n > (1 << 20)) throw ParseError(strf(tr("nieprawidłowa długość napisu %d"), int(n)));
		auto b = r_.bytes(size_t(n));
		if (b.back() != 0) throw ParseError(tr("napis bez zera na końcu"));
		return std::string(b.begin(), b.end() - 1);
	}
	void skipIntVector() { skipI32(size_t(count(N_("wektor liczb")))); }
	void skipStringVector()
	{
		int32_t n = count(N_("wektor napisów"));
		for (int32_t i = 0; i < n; ++i) str();
	}

private:
	BinReader r_;
};

void skipSupportPayload(SaveReader& r)
{
	r.skip(32);     // nazwa klasy/zasobu
	r.skipI32(4);   // cztery odwołania do obiektów
	r.skipI32(9);
	r.str();
	r.str();
	r.skipI32(3);
	r.skip(8);
}

void skipRouteVector(SaveReader& r)
{
	r.skipIntVector();
	r.skipI32(2);
}

void skipActionVariant(SaveReader& r)
{
	int32_t v = r.i32();
	if (v == 0)
	{
		r.skipI32(8);
		r.skipI32(size_t(r.count(N_("wpisy akcji"))) * 10);
		r.skipI32(3);
		r.skipI32(16 * 3);
		r.skipI32(4 + 1 + 1 + 1 + 8);
		r.flag(N_("akcja 0"));
	}
	else if (v == 1)
	{
		r.skipI32(3 + 2 + 1 + 1);
		r.flag(N_("akcja 1"));
		r.skipI32(2);
	}
	else if (v == 2)
	{
		r.skipI32(1 + 1 + 1 + 3 + 1);
		r.flag(N_("akcja 2"));
		r.skipI32(1);
		r.skipIntVector();
		r.skipStringVector();
		r.skipI32(1);
	}
	else
		throw ParseError(strf(tr("nieznany wariant akcji %d"), int(v)));
}

// Ciało obiektu (wersja 0xA5). Pozycja i obrót są na początku; resztę pomijamy zgodnie z kolejnością pól.
SaveObject parseObjectBody(SaveReader& r, int id, const std::string& declared)
{
	SaveObject o;
	o.id = id;
	o.className = r.str();
	if (o.className != declared) throw ParseError(strf(tr("klasa ciała obiektu %d różni się od deklaracji"), int(id)));
	o.type = r.i32();
	if (o.type < -1 || o.type > 12) throw ParseError(strf(tr("nieprawidłowy typ obiektu %d"), int(o.type)));
	o.posOffset = r.pos();
	for (float& v : o.pos) v = r.f32();
	r.skipI32(3);           // pozycja przejściowa
	r.skipI32(3);           // czas utworzenia, początek i długość pracy
	int32_t core[13];
	o.rotOffset = r.pos() + 7 * 4; // rdzeń[7..9] = obrót
	for (int32_t& v : core) v = r.i32();
	for (int i = 0; i < 3; ++i) std::memcpy(&o.rot[i], &core[7 + i], 4); // obrót: rdzeń[7..9]
	r.skipIntVector();      // odwołania do obiektów potomnych
	r.skipStringVector();
	r.skipI32(16);
	int32_t tcl = r.i32();  // zmienne Tcl obiektu
	if (tcl == 1)
	{
		int32_t n = r.count(N_("zmienne Tcl"));
		for (int32_t i = 0; i < n; ++i)
		{
			std::string k = r.str();
			o.tclVars.emplace_back(k, r.str());
		}
	}
	else if (tcl != 0)
		throw ParseError(tr("nieprawidłowa obecność zmiennych Tcl"));
	r.skipI32(1);           // ID obiektu stanu
	o.instanceName = r.str(); // nazwa instancji (krasnale: imię)
	r.skipI32(8);
	skipRouteVector(r);
	if (r.i32() != 0) r.skipI32(512); // siatka 512 wartości
	r.skipI32(2 + 4);
	int32_t s26c[10];       // +0x26C..: [3] = rodzic przypięcia (+0x278), [4] = punkt (+0x27C)
	for (int32_t& v : s26c) v = r.i32();
	o.linkParent = s26c[3] > 0 ? s26c[3] : -1;
	o.linkPoint = s26c[4];
	r.skipI32(3);
	if (r.flag(N_("cząsteczki"))) r.skipI32(160);
	r.skip(3);
	r.str();                // napis wyświetlania
	r.skipI32(2);
	o.owner = r.i32();      // właściciel (+0x36C)
	r.skipI32(1 + 8 + 5);
	r.skip(1);
	r.skipI32(4 + 6 + 1);
	if (o.type == 1 || o.type == 11)
	{ // powiązania aktora
		r.skipI32(9);
		r.skipStringVector();
		r.flag(N_("powiązania aktora"));
	}
	if (o.type == 1)
	{ // indeks nazwany
		r.skipI32(1);
		for (int32_t s = r.i32(); s != -1; s = r.i32())
		{
			if (s != 1) throw ParseError(tr("nieprawidłowy wpis indeksu nazwanego"));
			r.str();
			r.skipI32(1);
		}
	}
	if (o.type == 5 || o.type == 6 || o.type == 7 || o.type == 10)
	{ // stan pracy budynku lub pojazdu
		int32_t n = r.count(N_("stan ruchomy"));
		for (int32_t i = 0; i < n; ++i)
		{
			r.skipI32(2);
			r.str();
			r.str();
			r.skipI32(2);
			r.skip(2);
		}
		r.skipI32(2);
		r.skip(7);
		r.skipI32(3);
		r.skip(8);
		r.skipI32(4);
		r.skip(1);
		r.skipI32(5 + 16 * 3);
		r.skip(2);
	}
	if (o.type == 1) r.skipI32(4);
	if (r.flag(N_("historia powiązań")))
	{
		int32_t n = r.count(N_("historia powiązań"));
		for (int32_t i = 0; i < n; ++i) skipRouteVector(r);
	}
	r.skipI32(16);
	r.skip(16);
	r.skipI32(16);
	r.skip(1);
	r.skipI32(1);
	r.skip(1);
	if (r.flag(N_("wariant akcji"))) skipActionVariant(r);
	r.skipI32(2); // tabela rozszerzona
	for (int i = 0; i < 64; ++i)
		if (r.flag(N_("wpis rozszerzony"))) skipSupportPayload(r);
	int32_t n = r.count(N_("rekordy rozszerzone"));
	for (int32_t i = 0; i < n; ++i) skipSupportPayload(r);
	r.skipI32(1);
	r.skip(1);
	return o;
}

// Tablica obiektów menedżera (3 grupy po 2): klasa, 7 × i32, 3 B, napis Tcl, i32.
void skipManagerArray(SaveReader& r)
{
	int32_t n = r.count(N_("tablica menedżera"));
	for (int32_t i = 0; i < n; ++i)
	{
		r.str();
		r.skipI32(7);
		r.skip(3);
		r.str();
		r.skipI32(1);
	}
}

// Sekcje po ciałach obiektów (kolejność i rozmiary za Diggles-Unity,
// Tools/SaveFormatResearch/parse_a5_object_bodies.py; B3-support-class-spatial.md §2–3).
void parseDecorations(SaveReader& r, SaveGame& s)
{
	r.skipI32(18 * 512); // tablice atrybutów graczy
	r.skipI32(2);
	for (int g = 0; g < 3; ++g)
	{
		skipManagerArray(r);
		skipManagerArray(r);
		r.skipIntVector();
		r.skip(2);
	}
	if (r.count(N_("rekordy akcji menedżera")) != 0) throw ParseError(tr("nieobsługiwane rekordy akcji menedżera"));
	// Tabela klas: lekkie obiekty zastępcze.
	r.skipI32(1);
	r.skipI32(0x3000);
	r.skipI32(1);
	r.skip(1);
	r.skipIntVector();
	r.skipIntVector();
	for (int32_t idx = r.i32(); idx != -1; idx = r.i32())
	{
		if (idx < 0 || idx >= 0x3000) throw ParseError(tr("nieprawidłowy indeks tabeli klas"));
		SaveDeco d;
		d.proxy = true;
		float m[16];
		for (float& v : m) v = r.f32(); // macierz przekształcenia 4×4 (🔶 obrót z niej)
		for (float& v : d.pos) v = r.f32();
		r.skipI32(1);
		r.skipIntVector();
		r.skipI32(1);          // własny indeks
		d.link = r.i32();      // obiekt, którego jest kopią
		d.className = r.str();
		r.skip(1);
		r.skipI32(4);
		r.skip(2);
		r.skipI32(10);
		r.str();               // nazwa instancji
		r.skip(3);
		r.skipI32(1 + 6);
		r.skip(1);
		d.rot[1] = std::atan2(m[8], m[0]); // 🔶 obrót wokół osi pionowej z macierzy
		s.decos.push_back(d);
	}
	// Kubełki przestrzenne: ceil(W/8)·ceil(H/8), w każdym liczba i rekordy.
	int buckets = ((s.width + 7) / 8) * ((s.height + 7) / 8);
	for (int b = 0; b < buckets; ++b)
	{
		int32_t n = r.count(N_("kubełek"));
		for (int32_t i = 0; i < n; ++i)
		{
			SaveDeco d;
			for (float& v : d.pos) v = r.f32();
			for (float& v : d.rot) v = r.f32();
			d.className = r.str();
			r.skipI32(4);
			r.skip(2);
			s.decos.push_back(d);
		}
	}
	r.marker(7);
}

} // namespace

SaveGame parseSaveGame(const std::vector<uint8_t>& fileData)
{
	std::string err;
	auto data = gunzip(fileData, err);
	if (!data) throw ParseError(strf(tr("zapis nie jest strumieniem gzip: %s"), err.c_str()));
	SaveReader r(*data);
	SaveGame s;
	s.raw = *data;
	s.version = r.i32();
	if (s.version != 0xA5) throw ParseError(strf(tr("obsługujemy tylko wersję zapisu 0xA5, jest 0x%X"), unsigned(s.version)));
	s.header = r.str();
	s.timeOffset = r.pos();
	s.gameTime = r.f64();
	r.marker(1);
	// Sekcja 1 (205 B): kamera na przesunięciach 45..61 (ognisko X, Y, zoom, kąty dx, dy).
	size_t sec1 = r.pos();
	s.viewOffset = sec1 + 45;
	r.skip(45);
	s.viewX = r.f32();
	s.viewY = r.f32();
	s.viewZoom = r.f32();
	s.viewDx = r.f32();
	s.viewDy = r.f32();
	r.skip(205 - (r.pos() - sec1));
	r.marker(2);
	r.marker(3);

	// Świat: wymiary, rekordy pól po 53 B, stan świata, woda, mgła.
	s.width = r.i32();
	s.height = r.i32();
	if (s.width <= 0 || s.height <= 0 || size_t(s.width) * size_t(s.height) > (4u << 20))
		throw ParseError(tr("nieprawidłowe wymiary świata"));
	r.skipI32(4);
	size_t n = size_t(s.width) * size_t(s.height);
	s.tiles.resize(n);
	s.tilesOffset = r.pos();
	for (SaveTile& t : s.tiles)
	{
		size_t start = r.pos();
		t.block = uint32_t(r.i32());
		r.skip(1 + 4 + 4 + 1 + 1); // koszt nawigacji, kopanie (8 graczy), stan podłogi, flagi szablonu
		uint8_t lo = r.u8(), hi = r.u8();
		t.cell = uint16_t(lo | (hi << 8));
		uint8_t st0 = r.u8(), st1 = r.u8();
		t.material = st0 & 3;
		t.flags = st0;
		t.variant = st1;
		// Płyn: typ w dwóch buforach (bajty 19, 20), wypełnienie (f32 21, 25), przepływ poziomy (29, 33)
		// i pionowy (37, 41). Ustalone na zapisie z wodą z laboratorium (docs/formaty/sav.md).
		t.fluidType = r.u8();
		r.skip(1);
		t.fluidFill = r.f32();
		r.skip(4 + 16);
		for (uint8_t& f : t.fog) f = r.u8();
		if (r.pos() - start != 53) throw ParseError(tr("rekord pola ma nie 53 B"));
	}
	for (int& v : s.fowBegin) v = r.i32();
	s.lightBegin = r.i32();
	s.groundBegin = r.i32();
	r.skip(size_t(r.count(N_("źródła wody"))) * 37);
	r.skip(size_t(r.count(N_("tamy wody"))) * 41);
	r.skip(8);
	for (int i = 0; i < 4; ++i) r.skip(size_t(r.count(N_("lista wody"))) * 8);
	r.skip(1);
	for (int i = 0; i < 8; ++i) r.skip(size_t(r.count(N_("lista mgły"))) * 8);
	r.marker(4);
	r.marker(5);
	r.marker(6);

	// Obiekty: deklaracje (ID, klasa), rekordy pomocnicze, ciała w tej samej kolejności ID.
	std::vector<std::pair<int, std::string>> decl;
	for (int32_t id = r.i32(); id != -1; id = r.i32()) decl.emplace_back(id, r.str());
	if (!r.str().empty()) throw ParseError(tr("brak pustego separatora po deklaracjach obiektów"));
	int32_t support = r.count(N_("rekordy pomocnicze"));
	for (int32_t i = 0; i < support; ++i)
	{
		r.skip(4 + 8 + 8 + 4 + 4);
		skipSupportPayload(r);
	}
	size_t k = 0;
	for (int32_t id = r.i32(); id != -1; id = r.i32())
	{
		while (k < decl.size() && decl[k].first < id) ++k;
		if (k >= decl.size() || decl[k].first != id) throw ParseError(strf(tr("ciało obiektu %d bez deklaracji"), int(id)));
		s.objects.push_back(parseObjectBody(r, id, decl[k].second));
	}
	try
	{
		parseDecorations(r, s);
	}
	catch (const std::exception&)
	{
		s.decoError = true; // dekoracje są dodatkiem: zapis bez nich nadal się wyświetla
	}
	return s;
}

bool writeSavePatch(const SaveGame& base, const SavePatch& p, const std::string& path, std::string& err)
{
	if (base.raw.empty())
	{
		err = tr("brak bazowego zapisu");
		return false;
	}
	std::vector<uint8_t> d = base.raw;
	auto put = [&](size_t off, const void* v, size_t n) {
		if (off + n <= d.size()) std::memcpy(d.data() + off, v, n);
	};
	put(base.timeOffset, &p.gameTime, 8);
	put(base.viewOffset, p.view, 20);
	if (p.tiles.size() == base.tiles.size())
		for (size_t i = 0; i < p.tiles.size(); ++i)
		{
			const auto& t = p.tiles[i];
			size_t o = base.tilesOffset + i * 53;
			put(o, &t.block, 4);
			put(o + 15, &t.cell, 2);
			// Typ .pmp -> rodzaj gruntu zapisu (4→0 A, 3→1 B, 2 i 0→2 C, 5→3 otwór), bit 0x80 bez zmian.
			uint8_t ty = t.pmpType & 0x7F, mat = ty == 4 ? 0 : ty == 3 ? 1
			    : ty == 5                                              ? 3
			                                                           : 2;
			uint8_t st0 = d[o + 17];
			st0 = uint8_t((st0 & ~0x83) | mat | (t.pmpType & 0x80));
			put(o + 17, &st0, 1);
			uint8_t ft[2] = {t.fluidType, t.fluidType};
			put(o + 19, ft, 2);
			put(o + 21, &t.fluidFill, 4);
			put(o + 25, &t.fluidFill, 4);
			put(o + 45, &t.fog, 1);
		}
	for (const auto& o : base.objects)
	{
		auto it = p.objects.find(o.id);
		if (it == p.objects.end()) continue;
		put(o.posOffset, it->second.pos, 12);
		put(o.rotOffset, it->second.rot, 12);
	}
	gzFile f = gzopen(path.c_str(), "wb7"); // GameSaveCompression = 7 jak w rejestrze gry
	if (!f)
	{
		err = strf(tr("nie można utworzyć %s"), path.c_str());
		return false;
	}
	bool ok = gzwrite(f, d.data(), unsigned(d.size())) == int(d.size());
	ok = gzclose(f) == Z_OK && ok;
	if (!ok) err = strf(tr("błąd zapisu %s"), path.c_str());
	return ok;
}

} // namespace ow
