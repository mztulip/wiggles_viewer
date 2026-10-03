// Czytnik danych binarnych little-endian z kontrolą zakresu.
#pragma once

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace ow {

struct ParseError : std::runtime_error {
	using std::runtime_error::runtime_error;
};

class BinReader {
public:
	BinReader(const uint8_t* data, size_t size) : data_(data), size_(size) {}
	explicit BinReader(const std::vector<uint8_t>& v) : BinReader(v.data(), v.size()) {}

	size_t pos() const { return pos_; }
	size_t size() const { return size_; }
	size_t remaining() const { return size_ - pos_; }
	bool atEnd() const { return pos_ == size_; }

	void need(size_t n) const {
		if (n > remaining())
			throw ParseError("koniec danych: potrzeba " + std::to_string(n) + " B na pozycji " +
			                 std::to_string(pos_) + " (rozmiar " + std::to_string(size_) + ")");
	}
	void skip(size_t n) { need(n); pos_ += n; }

	template <typename T> T read() {
		need(sizeof(T));
		T v;
		std::memcpy(&v, data_ + pos_, sizeof(T)); // zakładamy hosta little-endian (x86, ARM)
		pos_ += sizeof(T);
		return v;
	}
	uint8_t u8() { return read<uint8_t>(); }
	uint16_t u16() { return read<uint16_t>(); }
	uint32_t u32() { return read<uint32_t>(); }
	float f32() { return read<float>(); }

	// Ciąg poprzedzony długością u32, bez terminatora.
	std::string str32() {
		uint32_t n = u32();
		need(n);
		std::string s(reinterpret_cast<const char*>(data_ + pos_), n);
		pos_ += n;
		return s;
	}
	// Kopiuje n bajtów prosto do dst (szybkie czytanie całych tablic).
	void readInto(void* dst, size_t n) {
		need(n);
		std::memcpy(dst, data_ + pos_, n);
		pos_ += n;
	}
	// Tablica elementów typu T (POD, little-endian) zapisanych jeden po drugim.
	template <typename T> void readArray(std::vector<T>& out, size_t count) {
		if (count > remaining() / sizeof(T)) need(count * sizeof(T)); // błąd przed alokacją
		out.resize(count);
		if (count) readInto(out.data(), count * sizeof(T));
	}
	std::vector<uint8_t> bytes(size_t n) {
		need(n);
		std::vector<uint8_t> v(data_ + pos_, data_ + pos_ + n);
		pos_ += n;
		return v;
	}

private:
	const uint8_t* data_;
	size_t size_;
	size_t pos_ = 0;
};

} // namespace ow
