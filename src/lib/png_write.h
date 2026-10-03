// Zapis PNG RGBA8 bez kompresji (deflate "stored"). Wystarcza do zrzutów ekranu i eksportu tekstur.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ow {

// rgba: width*height*4, wiersze od góry. Zwraca false przy błędzie zapisu.
bool writePng(const std::string& path, int width, int height, const std::vector<uint8_t>& rgba);

} // namespace ow
