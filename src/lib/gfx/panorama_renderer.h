// Panorama powierzchni: 6 warstw nieba/krajobrazu z paralaksą (Data/Texture/Misc/Panorama_layer_0..5).
//
// Prawo z Wiggles.exe (FUN_00480ee0, FUN_004802c0, FUN_0047f440) za Zimennik/Diggles-Unity (MIT),
// Docs/BACKDROP_PARALLAX_RE.md §2. Siatka 9×9 punktów ekranu; dla każdego promień kamery w układzie
// gry (bloki: x w prawo, y w dół, z do widza): punkt bliski (x_n, y_n) i nachylenia Δx/Δz, Δy/Δz.
// Dla warstwy o głębokości D, skali S i przesunięciach (offU, offV):
//   u = S·(Ku·(Δx/Δz·(D−1) − x_n) + 5) + offU,   v = S·(−Kv·(Δy/Δz·(D−1) − y_n) + 5) + offV
//   Ku = 0,00075, Kv = 0,0015
// Warstwy 0→5 rysowane w przestrzeni ekranu z mieszaniem alfa, bez bufora głębi, przed terenem.
// Adresowanie: U zawijane; V zawijane tylko w warstwie 0, w pozostałych przycięte.
//
// Po warstwach dywan łąki (FUN_00480720, §2.4, Panorama_layer_6): w układzie świata, kawałki po
// 20 pól w x od snap(ogniskoX) − 140 do + 140, każdy siatka 4×4 wierzchołków:
//   x = kawałekX + kol·20/3, y = wiersz_gruntu − 2,5 (wiersz 0 podniesiony o 0,75, wiersz 1 o 0,25),
//   z = {−60, −37,5, −15, +7,5} (bloki głębokości, jak teren 7..14,5), uv = (kol·0,33, wiersz·0,33).
// Też bez bufora głębi: teren i tak go zamalowuje.
// Bramka (FUN_00480ee0 wg Diggles-Unity): całość tylko gdy wiersz ogniska kamery − 20 ≤ wiersz gruntu.
// Wiersz gruntu ustalony obserwacją gry (docs/silnik/08_tlo.md §8.4): set_ground_begin
// przesuwa dywan; bez tej komendy (dane gry jej nie używają) obraz odpowiada set_light_begin,
// a bez set_light_begin gra nie rysuje panoramy wcale.
#pragma once

#include "lib/rhi/rhi.h"

#include <glm/glm.hpp>
#include <string>

namespace ow {

class TextureCache;

class PanoramaRenderer {
public:
	~PanoramaRenderer();
	bool init(rhi::Device& dev, std::string& err, TextureCache& textures);
	// ndcToGame: odwrotność pełnej macierzy (projekcja·widok·świat) przeskalowana do bloków gry.
	void draw(const glm::mat4& ndcToGame);
	// gameToClip: z bloków gry do przestrzeni przycięcia; focusX i groundRow w blokach.
	void drawCarpet(const glm::mat4& gameToClip, float focusX, float groundRow);
	static bool gateOpen(float focusRow, float groundRow) { return focusRow - 20.f <= groundRow; }

private:
	rhi::Device* dev_ = nullptr;
	rhi::Handle pipe_ = rhi::kNull, pipeCarpet_ = rhi::kNull;
	rhi::Handle ebo_ = rhi::kNull, carpetEbo_ = rhi::kNull;
	rhi::Handle tex_[7] = {};
};

} // namespace ow
