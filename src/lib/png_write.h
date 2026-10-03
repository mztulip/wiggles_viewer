// Zapis PNG RGBA8 (kompresja zlib). Do zrzutów ekranu i osadzania tekstur w glTF.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ow {

// rgba: width*height*4, wiersze od góry. Zwraca gotowy plik PNG w pamięci.
std::vector<uint8_t> encodePng(int width, int height, const std::vector<uint8_t>& rgba);

// Zwraca false przy błędzie zapisu.
bool writePng(const std::string& path, int width, int height, const std::vector<uint8_t>& rgba);

} // namespace ow
