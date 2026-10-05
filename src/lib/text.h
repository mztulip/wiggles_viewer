// Kodowanie tekstu. Pliki gry są w Windows-1252 (teksty polskie w Windows-1250).
#pragma once

#include <string>

namespace ow
{

std::string cp1252ToUtf8(const std::string& s);

} // namespace ow
