// Wyszukiwanie tekstury materiału po nazwie (bez GL).
//
// Kolejność katalogów (od największej rozdzielczości) pochodzi ze skryptu
// przeglądarki w djunkles/Diggles3db (https://github.com/djunkles/Diggles3db):
// m256, m128, m064, m032, Gray, Misc, ClassIcons.
#pragma once

#include <optional>
#include <string>

namespace ow
{

class Vfs;

// Ścieżka w grze do tekstury o danej nazwie (bez rozszerzenia) albo nullopt.
std::optional<std::string> findTexture(const Vfs& vfs, const std::string& name);

} // namespace ow
