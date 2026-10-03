# Format `.3db`: modele i animacje

Specyfikacja wystarczająca do napisania własnego loadera i przeglądarki. Dotyczy wszystkich 43 plików `Data/3db/*.3db` z gry Wiggles / Diggles (wersja 1.0 build 844). Każde pole oznaczone ✅ jest zweryfikowane na wszystkich plikach; 🔶 oznacza pole, którego znaczenia nie znamy (trzeba je tylko poprawnie przeczytać lub pominąć).

Źródła: struktura pliku pochodzi z [djunkles/Diggles3db](https://github.com/djunkles/Diggles3db) (narzędzia w Pythonie i C# z [forum Wiggles](https://wiggles.ruka.at/forum/)). Dekwantyzację pozycji, znaczenie bajtu normalnej, orientację osi, interpolację i walidację ustaliliśmy sami. Implementacja wzorcowa: `src/lib/model3db.cpp`, renderer: `src/lib/gfx/model_renderer.cpp`.

---

## 1. Model danych w skrócie

```
Model
 ├─ materiały[]        nazwa tekstury
 ├─ meshe[]            JEDEN mesh = JEDNA klatka animacji (lub statyczny obiekt)
 │    └─ linki[]       fragment klatki z jednym materiałem -> indeksy do pul
 ├─ obiekty[]          nazwa obiektu -> lista indeksów animacji
 ├─ animacje[]         nazwa animacji -> lista indeksów meshy (klatek)
 ├─ cienie[], cmapy[]  dodatki (mapy 8-bit)
 └─ pule               trójkąty[][], UV[][], pozycje[][], normalne[][]
```

- **Animacja jest wierzchołkowa** (bez szkieletu). Każda klatka to osobny mesh z własnymi pozycjami wierzchołków.
- Mesh nie zawiera geometrii bezpośrednio. Jego **linki** wskazują numery pul. Pule trójkątów i UV są zwykle **wspólne** dla wszystkich klatek animacji, a zmieniają się pule pozycji i normalnych.
- Plik może zawierać jeden obiekt z setkami animacji (postacie: `MANN.3DB` ma 1023 animacje i 11 573 klatki) albo setki obiektów z jedną animacją `standard` (zestawy dekoracji: `KRISTALL.3DB`, `OBERWELT.3DB`).
- Skrypty gry odwołują się do animacji jako `obiekt.animacja`, np. `urw_sumpf.standard`, a do pliku po nazwie bez rozszerzenia: `db_load mann`.

## 2. Typy

Wszystko jest **little-endian**.

| Typ | Rozmiar | Opis |
|---|---|---|
| `u8`, `u16`, `u32` | 1, 2, 4 | liczby bez znaku |
| `f32` | 4 | IEEE-754 float |
| `vec3` | 12 | 3 × `f32` (x, y, z) |
| `str` | 4 + n | `u32 n`, potem `n` bajtów tekstu, **bez** zera na końcu. Kodowanie Windows-1252 |

## 3. Układ pliku

Plik czyta się sekwencyjnie, bez przesunięć i wyrównań. Po przeczytaniu ostatniej sekcji wskaźnik musi stać **dokładnie na końcu pliku** (✅ dla wszystkich 43 plików). To najlepszy test poprawności loadera.

### 3.1 Nagłówek
| Pole | Typ | Opis |
|---|---|---|
| version | `str` | zawsze `"3DDB 1.0"` |
| name | `str` | nazwa bazy, np. `"baby"` |

### 3.2 Materiały
| Pole | Typ | Opis |
|---|---|---|
| count | `u16` | |
| *powtórz count razy:* | | |
| name | `str` | **nazwa tekstury bez rozszerzenia**, np. `Character_ZBaby_a` (patrz §6) |
| path | `str` | ścieżka z serwera twórców, np. `\\Fileserver\Projects\Heinz\Grafik\...tga`. Ignorować |
| unknown | `u32` | 🔶 ✅ zawsze 1 (967 materiałów w grze) |

### 3.3 Meshe (klatki)
| Pole | Typ | Opis |
|---|---|---|
| count | **`u32`** | uwaga: tu `u32`, a liczniki pozostałych sekcji są `u16` (poza `nUnk20`) |
| *powtórz count razy:* | | |
| linkCount | `u16` | zwykle tyle, ile materiałów używa klatka |
| links | 12 B × linkCount | patrz niżej |
| bboxMin | `vec3` | ✅ AABB klatki, w tych samych jednostkach co pozycje po dekwantyzacji |
| bboxMax | `vec3` | |
| unknown80 | 128 B | 🔶 wygląda na 32 × `u32` (małe, rosnące liczby) |
| shadowIndex | `u16` | ✅ indeks cienia (§3.6) albo `0xFFFF`, gdy klatka nie ma cienia |
| unknown30 | 48 B | 🔶 wygląda na 12 × `f32` |
| cmapIndex | `u16` | ✅ indeks cmapy (§3.7) albo `0xFFFF`. Każda cmapa należy do dokładnie jednego mesha |

**Link** (6 × `u16` = 12 B):
| Pole | Opis |
|---|---|
| material | indeks w tablicy materiałów |
| flags | 🔶 1 w 90% linków, rzadziej 2, 3, 5, 6. Prawdopodobnie flagi renderowania (przezroczystość? dwustronność?) |
| triangles | indeks puli trójkątów |
| texcoords | indeks puli UV |
| points | indeks puli pozycji |
| normals | indeks puli normalnych (w starszych narzędziach mylnie „brightness”) |

### 3.4 Obiekty
| Pole | Typ | Opis |
|---|---|---|
| count | `u16` | |
| *powtórz:* name | `str` | np. `"baby"`, `"urw_sumpf"` |
| animCount | `u16` | |
| animations | `u32` × animCount | indeksy w tablicy animacji |

### 3.5 Animacje
| Pole | Typ | Opis |
|---|---|---|
| count | `u16` | |
| *powtórz:* name | `str` | np. `"gehen_loop"`, `"standard"` |
| frameCount | `u16` | |
| frames | `u32` × frameCount | indeksy meshy, kolejne klatki |
| flags | `u16` | 🔶 (0, 3, …) |
| unknownF | `f32` | 🔶 |
| source | `str` | ścieżka do pliku eksportera, np. `...\baby_aufwachen.sex`. Ignorować |
| move | `vec3` | 🔶 prawdopodobnie przesunięcie postaci na cykl animacji |
| rotate | `vec3` | 🔶 prawdopodobnie obrót na cykl w radianach (`drehen_links`: z = +0,1309 = 7,5°) |

### 3.6 Cienie
| Pole | Typ | Opis |
|---|---|---|
| count | `u16` | 0–2 |
| *powtórz:* data | 1024 B | mapa 32 × 32, 8 bit, skala szarości (cień rzucany pod obiektem). Klatki wskazują cień przez `shadowIndex` |

### 3.7 Cmapy
| Pole | Typ | Opis |
|---|---|---|
| count | `u16` | (MANN: 6390) |
| *powtórz:* width, height | `u16`, `u16` | |
| unknown1, unknown2 | `u16`, `u16` | 🔶 |
| pixels | `u8` × width × height | 🔶 8-bit mapa przypisana do jednej klatki (`cmapIndex` mesha). Gra ma komendę `has_cmap obiekt.animacja` |

### 3.8 Nagłówek pul
| Pole | Typ | Opis |
|---|---|---|
| nTri | `u16` | liczba pul trójkątów |
| nUV | `u16` | liczba pul UV |
| nPts | `u16` | liczba pul pozycji |
| nNrm | `u16` | liczba pul normalnych |
| nUnk20 | `u32` | liczba rekordów 20-bajtowych |
| triCount | `u16` × nTri | liczba **indeksów** w każdej puli trójkątów (✅ zawsze wielokrotność 3) |
| uvCount | `u16` × nUV | liczba par UV w każdej puli |
| ptsCount | `u16` × nPts | liczba punktów w każdej puli |
| nrmCount | `u16` × nNrm | liczba bajtów w każdej puli normalnych |
| unk20 | 20 B × nUnk20 | 🔶 |

### 3.9 Dane pul (w tej kolejności, pula po puli)
| Pula | Element | Opis |
|---|---|---|
| trójkąty | `u16` | indeks wierzchołka w obrębie linku. Co trzy indeksy tworzą trójkąt |
| UV | `f32 u`, `f32 v` | konwencja Direct3D: v = 0 to **górna** krawędź tekstury |
| pozycje | 3 × `u16` | skwantyzowane x, y, z (§4) |
| normalne | `u8` | indeks w tablicy `SPHERE.DAT` (§5) |

## 4. Pozycje: dekwantyzacja ✅

```
x = (u16_x - 32768) / 4.0      // tak samo y i z
```
Wynik to stały przecinek z krokiem 0,25 jednostki. Wszystkie klatki używają tego samego układu, więc animacja nie „pływa”. Weryfikacja: rozpiętość zdekwantyzowanych punktów zgadza się z `bboxMin/bboxMax` (stosunek 3,99 przed podziałem przez 4).

**Orientacja ✅:** oś **Y jest skierowana w dół**. Głowa postaci ma ujemne y; `BABY` mieści się w y ∈ (−97, 5). Aby postawić model w układzie z Y w górę, obróć go o 180° wokół osi X (y → −y, z → −z). Zwykłe odwrócenie samego y dałoby lustrzane odbicie.

Skala: postać krasnala ma około 200 jednostek wysokości. 🔶 Relacja do jednostek świata gry nie jest jeszcze znana.

## 5. Normalne ✅

Bajt na wierzchołek to **indeks w tablicy 256 wektorów jednostkowych** z pliku `Data/SPHERE.DAT` (§ [sphere_dat.md](sphere_dat.md)). Dowody: wartości rozkładają się równomiernie na 0–255, a narysowanie `sphere[bajt]` jako koloru daje gładkie gradienty zgodne z kształtem modelu. Normalne są w układzie modelu, więc trzeba je obracać razem z modelem.

## 6. Tekstury materiałów ✅

Nazwa materiału to nazwa pliku `.tga` bez rozszerzenia. Szukaj **bez rozróżniania wielkości liter** (`Character_ZBaby_a` vs `character_zbaby_a.tga`), w tej kolejności katalogów:

```
Data/Texture/m256, m128, m064, m032, Gray, Misc, ClassIcons
```
Katalogi `mNNN` to te same tekstury w różnych rozdzielczościach (jak LOD), więc bierzemy największą. Format plików: [tekstury.md](tekstury.md).

## 7. Wierzchołki linku ✅

Dla linku `L` mesh-a:
```
pos  = pozycje [L.points]       // n elementów
uv   = UV      [L.texcoords]    // też n elementów (✅ zawsze)
nrm  = normalne[L.normals]      // też n elementów (✅ zawsze)
idx  = trójkąty[L.triangles]    // indeksy 0..n-1 (✅ zawsze w zakresie)
```
Wierzchołek `i` = (`pos[i]`, `uv[i]`, `nrm[i]`). Indeks wspólny dla wszystkich trzech pul, tak jak w D3D. Link może być pusty (0 punktów), wtedy pomijamy go.

Walidacja, którą warto zrobić po wczytaniu (wszystkie pliki gry ją przechodzą): numery pul w linkach i materiały w zakresie, indeksy trójkątów < n, klatki animacji < liczba meshy, animacje obiektów < liczba animacji.

## 8. Odtwarzanie animacji

```
klatki = animacja.frames            // indeksy meshy
czas  += dt * fps
i      = floor(czas) mod N
j      = (i + 1) mod N              // dla animacji zapętlonych; inaczej min(i+1, N-1)
t      = czas - floor(czas)
pozycja   = lerp(pos_i, pos_j, t)
normalna  = normalize(lerp(sphere[nrm_i], sphere[nrm_j], t))
```

**Interpolacja ✅:** oryginalny silnik interpoluje między klatkami. W exe jest opcja `AnimIpol [true|false] (true)`, w rejestrze `Performance\AnimInterpolation=1`, a dla obiektów komendy `set_forceipol`/`get_forceipol`. Dane są do tego przygotowane: w ok. 99,5% par kolejnych klatek linki mają ten sam materiał, tę samą pulę trójkątów i tę samą liczbę wierzchołków. Gdy topologia się różni, rysuj klatkę `i` bez interpolacji.

🔶 Prędkość odtwarzania nie jest znana. 20 kl./s wygląda naturalnie; gra pozwala skryptom sprawdzić długość animacji w klatkach (`db_animlength obiekt.animacja`).

Nazewnictwo animacji postaci: `*_start`, `*_loop`, `*_end` (np. `gehen_start` → `gehen_loop` → `gehen_end`). Statyczne obiekty mają jedną animację `standard`, często z jedną klatką.

## 9. Minimalny renderer: przepis

1. Wczytaj `SPHERE.DAT` (256 × vec3).
2. Wczytaj model wg §3; sprawdź koniec pliku i odwołania.
3. Dla wybranego obiektu weź animację, a z niej klatkę → mesh.
4. Dla każdego linku zbuduj bufor wierzchołków (pos po dekwantyzacji, uv, indeks normalnej) i bufor indeksów `u16`.
5. Teksturę wyszukaj wg §6 i wczytaj TGA. Filtrowanie liniowe, zawijanie REPEAT.
6. Macierz modelu: obrót 180° wokół X (§4), przesunięcie środka bbox do zera.
7. Shader: `kolor = tekstura(uv) * (ambient + (1-ambient) * max(dot(N, L), 0))`, gdzie `N = sphere[indeks]` obrócone macierzą modelu. Odrzuć piksele z alfą < 0,5 (liście, włosy, kraty).
8. Bez cullingu (modele mają cienkie, dwustronne elementy), z buforem głębi.

## 10. Przykład: `BABY.3DB`

```
version  "3DDB 1.0", name "baby"
materiały 2: Character_ZBaby_a, helme_huete_a
meshe 479 (każdy z 2 linkami, po jednym na materiał)
  mesh 0: linki [mat 0, tri 0, uv 0, pts 0, nrm 0], [mat 1, tri 1, uv 1, pts 1, nrm 1]
          bbox (-48.1, -97.3, -112.6) .. (48.0, 5.0, 67.6)
  mesh 1: linki [mat 0, tri 0, uv 0, pts 2, nrm 2], [mat 1, tri 1, uv 1, pts 3, nrm 3]
          -> te same trójkąty i UV, nowe pozycje i normalne
obiekty 1: "baby" -> animacje 0..47
animacje 48, np. "auf_po_fallen" 21 klatek (meshe 0..20), "aufwachen" 11 klatek
cienie 2, cmapy 0
```

## 11. Otwarte pytania
- `unknown80`, `unknown30` w meshu; flagi linku; rekordy 20-bajtowe; znaczenie `cmap` (kolizje? maski zaznaczania?).
- Pola animacji: `flags`, `unknownF`, `move`, `rotate` (ruch korzenia?).
- Prędkość animacji i skala względem świata.
