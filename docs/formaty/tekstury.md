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
| `Texture/texlevels.txt` | rozdzielczość tekstury dla 5 poziomów jakości (§ Poziomy jakości) |
| `Texture/TEXMAPS.BIN` | paczka 550 tekstur w 16-bitowym ARGB4444 z mipmapami; ✅ gra rysuje z niej, gdy plik istnieje (§ TEXMAPS.BIN) |
| `Ground/`, `Ground/detail` | teren: grunty A/B/C, maski przejść, detale per świat (opis w [rozdziale 4 przewodnika](../silnik/04_teren.md)) |
| `GUI/...` | interfejs |

Wyszukiwanie tekstury materiału: [3db.md](3db.md) §6.

## Poziomy jakości: `texlevels.txt` ✅

Tekst (CRLF), komentarze od `#`. Każdy wiersz: nazwa tekstury i pięć rozdzielczości dla poziomów *minimal, low, medium, high, maximal*, zapisanych jako `0..4` albo `16, 32, 64, 128, 256` (katalogi `m016`…`m256`), np. `interface01.tga 32 32 256 256 256`. Gdy wskazanego rozmiaru nie ma, gra szuka w kolejności z nagłówka pliku:

| poziom | kolejność |
|---|---|
| minimal | 16 → 32 → 64 → 128 → 256 |
| low | 32 → 64 → 16 → 128 → 256 |
| medium | 64 → 128 → 32 → 256 → 16 |
| high | 128 → 256 → 64 → 32 → 16 |
| maximum | 256 → 128 → 64 → 32 → 16 |

Poziom wybiera rejestr `Performance\TextureQuality` (w laboratorium 2). ❓ Przypisanie wartości rejestru do poziomów: zrzuty menu przy 0, 2 i 4 nie różnią się (różnice to tylko animacja).

## `TEXMAPS.BIN` ✅

| Przesunięcie | Pole |
|---|---|
| 0 | `"TexC"` |
| 4 | `u32` liczba tekstur *n* (550) |
| 8 | 4 × `u32` maski pikseli: `0x0F00` (R), `0x00F0` (G), `0x000F` (B), `0xF000` (A), czyli **ARGB4444** |
| 24 | *n* × 50 B nazw (`aufzugseil.tga`, dopełnione zerami), w kolejności alfabetycznej |
| 24 + 50*n* | *n* bloków po 176 816 B, w kolejności nazw |

Blok tekstury (16-bitowe piksele little-endian, wiersze od góry): 256×256 i dalej mipmapy 128, 64, 32, 16, 8, 4, 2, 1 (razem z dopełnieniem 174 768 B), a na końcu szara wersja 32×32 (2048 B, odpowiednik `Texture/Gray`). Poziomy, których tekstura nie ma w TGA, są wypełnione zerami (np. `AufzugSeil` istnieje tylko jako 32×32, więc pierwsze 172 032 B bloku to zera). Rozmiar pliku zgadza się co do bajta: 24 + 550 · (50 + 176 816) = 97 276 324 (`test3db`), a piksele są dokładnie konwersją TGA do 4 bitów na kanał (`brainposter` 256 i 128, `AufzugSeil` 32: wszystkie piksele zgodne).

**Gra rysuje z tej paczki** (eksperyment w laboratorium, [BAZA_WIEDZY §8.8](../BAZA_WIEDZY.md)): menu z paczką ma wyraźniejsze tekstury (1794 kolory w wycinku), a po jej usunięciu gra bierze z TGA mniejsze rozmiary według `texlevels.txt` i obraz jest rozmyty (923 kolory). Dlatego menedżer modów [DigglesModManager](https://github.com/DigglesMods/DigglesModManager) przemianowuje `texmaps.bin`, gdy mod zmienia tekstury. Wniosek dla wierności: oryginał używa tekstur 256 px z **4 bitami na kanał**; nasz silnik czyta 32-bitowe TGA (TODO: opcjonalna kwantyzacja do ARGB4444).

## Nakładki: mody i tekstury HD

Mody w układzie [DigglesModManager](https://github.com/DigglesMods/DigglesModManager) i [DigglesHDmod](https://github.com/ZarfaZ/DigglesHDmod) mają katalog `Data/` (pliki przykrywające grę) i opcjonalnie warianty językowe `Data_<język>/` (np. `Data_de/Texture/m256/brainposter.tga` z niemieckim napisem). Sama gra ich nie zna: menedżer modów kopiuje pliki do katalogu gry. Nasze programy nakładają je bez kopiowania: `--overlay <katalog moda>` (wigview, `openwiggles`), w kodzie `Vfs::addOverlay` ([`lib/vfs.h`](../../src/lib/vfs.h)); `Data_<język>` (język interfejsu) przykrywa `Data/` moda, a ten pliki gry. Tekstury HD (np. 1024×1024 w `m256`) działają bez zmian w rendererze. Łatki skryptów `change_*.tcl` nie są jeszcze stosowane (faza 7).

Źródła: układ katalogów i opis tekstur terenu z [ZarfaZ/DigglesHDmod](https://github.com/ZarfaZ/DigglesHDmod) (`Documentation.txt`). Statystyki formatu pochodzą z naszego `test3db`.

---
**Diggles-Unity** (ich opracowanie, 🔶 do sprawdzenia w laboratorium): [RENDERING_PIPELINE_COMPLETE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/RENDERING_PIPELINE_COMPLETE.md). Porównanie i stan: [mapa Diggles-Unity](../DIGGLES_UNITY.md).
