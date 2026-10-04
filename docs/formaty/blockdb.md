# `Data/BlockDB0/1/2.BDB`: kształty bloków terenu

Baza 118 688 gotowych kształtów 3D wnętrza bloku. Mapa (`.pmp`) wskazuje dla każdego bloku ID kształtu. Jak z tego zbudować teren: [rozdział 4 przewodnika](../silnik/04_teren.md).

Układ pliku za [Zimennik/Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) (MIT, `Docs/RENDERING_PIPELINE_COMPLETE.md` §2.1). ✅ Zweryfikowany na plikach gry: wszystkie sekcje sumują się dokładnie do rozmiaru pliku dla `BlockDB0` i `BlockDB2`, a wszystkie odwołania mieszczą się w zakresach.

Trzy pliki zawierają te same kształty (rekordy 89 B są identyczne), w różnej szczegółowości siatek. Gra wybiera plik ustawieniem `LandscapeQuality` 0–2. Rozmiary: 20, 26 i 37 MB.

## 1. Układ

Wszystko little-endian, bez wyrównań, sekcje kolejno:

```
nagłówek, 7 × u32
  version      = 11
  zero         = 0
  nBlocks      = 118688
  zero         = 0
  nIndex       liczba indeksów u16 w puli trójkątów      (BDB0: 561168, BDB2: 4195746)
  nDescriptors liczba deskryptorów                        (BDB0: 17564,  BDB2: 104056)
  nPalette     liczba punktów palety                      (BDB0: 1411,   BDB2: 3323)

komórki: nBlocks × 3 × { u16 vertCount, u32 descriptor }   6 B każda; 3 podsiatki na blok (góra, środek, dół)
rekordy: nBlocks × 89 B                                     opis w §2
indeksy: nIndex × u16                                       trójkąty, indeksy wierzchołków w obrębie podsiatki
deskryptory: nDescriptors × { u32 offset, u32 count }       zakres w puli indeksów
paleta: nPalette × { f32 x, y, z, u, v }                    20 B każdy punkt
dane podsiatek: dla każdej komórki z vertCount > 0, w kolejności komórek:
    vertCount × u16   indeks punktu palety (bajty mogą być niewyrównane)
    vertCount × u8    indeks normalnej w SPHERE.DAT
```

Po ostatniej sekcji wskaźnik ma stać na końcu pliku.

**Komórka `(blok b, podsiatka s)`** to element `b · 3 + s`. Jeśli `vertCount = 0`, podsiatka jest pusta. W przeciwnym razie:
- wierzchołek `i` to punkt `paleta[indeks_palety[i]]` z normalną `sphere[indeks_normalnej[i]]`;
- trójkąty to `indeksy[descriptor.offset … descriptor.offset + descriptor.count)`, po trzy, każdy < `vertCount`.

Dane podsiatek leżą jedna za drugą: początek podsiatki to suma `3 · vertCount` wszystkich wcześniejszych komórek.

## 2. Rekord bloku (89 B)

| Offset | Typ | Pole | Opis |
|---|---|---|---|
| 0 | `u8[8][8]` | wysokość | wiersze od góry, kolumny od lewej; v ∈ 0…15; głębokość `z = 7 + 0,5·v` bloku, 15 = lita skała na przedniej płaszczyźnie ✅ |
| 64 | `u32[4]` | sąsiedzi | kształty pasujące od strony NW, NE, SW, SE (do zszywania przy kopaniu); `0xFFFFFFFF` = brak 🔶 |
| 80 | `u8[4]` | narożniki | wysokości narożników NW, NE, SW, SE 🔶 |
| 84 | `u8` | wariant | młodszy półbajt = wariant shadera 🔶 |
| 85 | `u32` | deskryptor płynu | bity 0–9 pojemność, 10–13 granica górna, 14–17 granica dolna (ósme części pola), 18–24 przepuszczalność pionowa, 25–31 pozioma ✅ (Diggles-Unity `FLUID_SYSTEM_RE.md`; sprawdzone: blok 0 = 15/15 nieprawidłowy, blok 59 = pojemność 960, 8/0; 110 686 bloków ma 8/0) |

✅ Mapa wysokości zgadza się z eksportem `save_map` z gry (253 wierzchołki OBJ dokładnie, 20 na krawędziach pionowych ścian, 12 to punkty pośrednie ścian).

## 3. Paleta

| Pole | Znaczenie |
|---|---|
| x, y | położenie w obrębie bloku, zwykle 0…1 (y w dół) |
| z | głębokość w blokach: 14,5 = przednia płaszczyzna, 7 = najgłębsze dno ✅ (zakres sprawdzony na wszystkich punktach) |
| u, v | ✅ współrzędne tekstury w blokach (0…7,5): (x, y), (x, z − 7) albo (z − 7, y) zależnie od kierunku powierzchni; każdy punkt pasuje dokładnie do jednego rzutu ([rozdział 4.2](../silnik/04_teren.md)) |

Pozycja wierzchołka bloku (bx, by) w świecie to `(bx + x, by + y, z)` bloków.

## 4. Przydatne ID

| ID | Kształt |
|---|---|
| 0 | lita skała: wszystkie wysokości 15 ✅; wartość domyślna `map create` |
| 59 | pusty blok: wszystkie wysokości 0, czyli wykopany do samego dna (z = 7) ✅; na powierzchni wypełnia powietrze nad gruntem |

Implementacja wzorcowa: `src/lib/blockdb.cpp`.
