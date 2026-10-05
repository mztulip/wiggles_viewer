# Tekstury: `Data/Texture`, `Data/Ground`, `Data/GUI`

## Format plików ✅
Wszystkie 2052 tekstury w `Data/Texture` to **TGA (Truevision Targa) typu 2**: nieskompresowane true-color, **32 bit BGRA**, bez palety. Boki są potęgą dwójki, a obrazy zwykle kwadratowe (32–256 px w oryginale).

Nagłówek TGA (18 B):
| Offset | Typ | Pole | Wartość w grze |
|---|---|---|---|
| 0 | `u8` | długość pola ID | 0 (pomiń tyle bajtów po nagłówku) |
| 1 | `u8` | typ palety | 0 |
| 2 | `u8` | typ obrazu | 2 |
| 3 | 5 B | specyfikacja palety | ignorować |
| 8 | `u16` × 2 | początek x, y | ignorować |
| 12 | `u16` | szerokość | |
| 14 | `u16` | wysokość | |
| 16 | `u8` | bity na piksel | 32 |
| 17 | `u8` | deskryptor | bity 0–3 = bity alfy (8), **bit 5 = wiersze od góry** |

Dalej `szerokość × wysokość × 4` bajty w kolejności **B, G, R, A**. Jeśli bit 5 deskryptora jest wyzerowany (standard TGA), pierwszy wiersz w pliku to **dół** obrazu, więc przy wczytywaniu odwracamy kolejność wierszy, żeby wiersz 0 był górą.

Kanał alfa jest używany (np. liście, włosy, kraty: wartości 0–255). Do renderu wystarcza alpha test z progiem 0,5.

UV w modelach ma konwencję Direct3D (v = 0 to góra obrazu). Jeżeli trzymasz obraz wierszami od góry i wysyłasz go do OpenGL bez odwracania, UV działają bez zmian.

Nasz loader ([`src/lib/tga.cpp`](../../src/lib/tga.cpp)) obsługuje też typy 3, 10 i 11 (szarość, RLE), na wypadek modów.

## Katalogi
| Katalog | Zawartość |
|---|---|
| `Texture/m256`, `m128`, `m064`, `m032` | tekstury modeli według rozdzielczości. Ta sama nazwa może być w kilku (poziomy jakości) |
| `Texture/Gray` | 528 małych wersji tekstur (🔶 dla najniższej jakości) |
| `Texture/Misc` | efekty, interfejs |
| `Texture/ClassIcons` | ikony klas obiektów |
| `Texture/texlevels.txt` | wybór rozdzielczości dla poziomów jakości (Minimal, Low, High, Maximal) |
| `Texture/TEXMAPS.BIN` | cache (magic `TexC`). Gra odbudowuje go sama, więc można pominąć |
| `Ground/`, `Ground/detail` | teren: grunty A/B/C, maski przejść, detale per świat (opis w [rozdziale 4 przewodnika](../silnik/04_teren.md)) |
| `GUI/...` | interfejs |

Wyszukiwanie tekstury materiału: [3db.md](3db.md) §6.

Źródła: układ katalogów i opis tekstur terenu z [ZarfaZ/DigglesHDmod](https://github.com/ZarfaZ/DigglesHDmod) (`Documentation.txt`). Statystyki formatu pochodzą z naszego `test3db`.

---
**Diggles-Unity** (ich opracowanie, 🔶 do sprawdzenia w laboratorium): [RENDERING_PIPELINE_COMPLETE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/RENDERING_PIPELINE_COMPLETE.md). Porównanie i stan: [mapa Diggles-Unity](../DIGGLES_UNITY.md).
