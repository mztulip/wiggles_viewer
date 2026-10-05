// Loader obrazów TGA (Truevision Targa): typy 2 (RGB), 3 (szarość), 10 i 11 (RLE),
// 8/16/24/32 bit. Wynik to zawsze RGBA8, wiersze od góry do dołu.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ow
{

struct Image
{
	int width = 0, height = 0;
	std::vector<uint8_t> rgba; // width*height*4
	bool hasAlpha = false;     // czy plik zawierał kanał alfa
};

// Zwraca false i opis błędu w err przy nieobsługiwanym lub uszkodzonym pliku.
bool loadTga(const std::vector<uint8_t>& data, Image& out, std::string* err = nullptr);

} // namespace ow
