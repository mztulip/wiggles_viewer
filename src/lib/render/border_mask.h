// Ramka ekranu gry (rejestr Performance\BorderMask, tekstura Data/GUI/bordermask.tga): cienka czarna ramka
// z zaokrąglonymi rogami wokół obrazu. Prawo: docs/silnik/09_klatka.md §9.4.
//
// Środek piksela p od krawędzi bierze teksel 3·(p + 0,5 − 2) paska tekstury, próbkowany liniowo
// (64 teksele na 21⅓ piksela), obraz = scena · (1 − α); zgodne z pomiarem w grze co do ±0,02. Kawałki tekstury 128×128: lewa górna ćwiartka to róg,
// prawa górna pasek górnej krawędzi, lewa dolna pasek lewej; pozostałe krawędzie i rogi to ich odbicia.
// 🔶 Zmierzone przy 1024×768 (21⅓ piksela = 1/48 szerokości = 1/36 wysokości); czy gra skaluje ramkę
// z rozdzielczością, nie wiadomo (tu stała w pikselach).
#pragma once

#include "lib/render/rhi/rhi.h"

#include <string>

namespace ow
{

class Vfs;

class BorderMask
{
public:
	~BorderMask();
	// false: brak tekstury albo potoku (komunikat w err); draw nic wtedy nie robi.
	bool init(rhi::Device& dev, const Vfs& vfs, std::string& err);
	// Rysuje ramkę na obszarze obrazu od kolumny x0, szerokości w i wysokości h (po scenie, przed interfejsem).
	void draw(int x0, int w, int h);

private:
	rhi::Device* dev_ = nullptr;
	rhi::Handle pipe_ = rhi::kNull, tex_ = rhi::kNull;
};

} // namespace ow
