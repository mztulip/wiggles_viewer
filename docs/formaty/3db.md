# Format `.3db`: modele i animacje

Specyfikacja wystarczająca do napisania własnego loadera i przeglądarki. Dotyczy wszystkich 43 plików `Data/3db/*.3db` z gry Wiggles / Diggles (wersja 1.0 build 844). Każde pole oznaczone ✅ jest zweryfikowane na wszystkich plikach; 🔶 oznacza pole, którego znaczenia nie znamy (trzeba je tylko poprawnie przeczytać lub pominąć).

Źródła: struktura pliku pochodzi z [djunkles/Diggles3db](https://github.com/djunkles/Diggles3db) (narzędzia w Pythonie i C# z [forum Wiggles](https://wiggles.ruka.at/forum/)). Dekwantyzację pozycji, znaczenie bajtu normalnej, orientację osi, interpolację i walidację ustaliliśmy sami. Implementacja wzorcowa: [`src/lib/model3db.cpp`](../../src/lib/model3db.cpp), renderer: [`src/lib/render/model_renderer.cpp`](../../src/lib/render/model_renderer.cpp).

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
- Skrypty gry odwołują się do animacji jako `obiekt.animacja`, np. `urw_sumpf.standard`, a do pliku po nazwie bez rozszerzenia: `db_load mann`. ✅ Liczba klatek każdej animacji z naszego parsera zgadza się z `db_animlength` w działającej grze (6444 z 6445; jedyna różnica to nazwa obiektu powtórzona w dwóch plikach, [rozdział 5.1](../silnik/05_obiekty.md)).

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
| linkTable | 128 B | ✅ 32 × `u32`: punkty zaczepienia 0..31 tej klatki, wartość = indeks rekordu w puli 20-bajtowej (§9); indeks poza pulą = brak punktu |
| shadowIndex | `u16` | ✅ indeks cienia (§3.6) albo `0xFFFF`, gdy klatka nie ma cienia |
| shadowQuad | 48 B | ✅ prostokąt cienia: 4 narożniki × `vec3` na ziemi (y = 0): (x0, 0, z0), (x1, 0, z0), (x0, 0, z1), (x1, 0, z1). Niezerowy dokładnie wtedy, gdy klatka ma cień (37 431 meshy), same zera bez cienia (12 744); o ok. 42 jednostki szerszy niż obrys modelu i wydłużony w +z o mniej więcej wysokość modelu (BRUT: z do 269 przy bbox do 111 i wysokości 188). 🔶 Miejsce, na które gra kładzie plamę cienia 32×32 (§3.6) |
| cmapIndex | `u16` | ✅ indeks cmapy (§3.7) albo `0xFFFF`. Każda cmapa należy do dokładnie jednego mesha |

**Link** (6 × `u16` = 12 B; tryby rysowania niżej):
| Pole | Opis |
|---|---|
| material | indeks w tablicy materiałów |
| flags | ✅ **tryb rysowania fragmentu** (tabela niżej; wartości 1–11, nie są niezależnymi bitami). Nie wynika z kanału alfa tekstury: ten sam materiał ma różne flagi w różnych modelach (159 z 331 materiałów) |
| triangles | indeks puli trójkątów |
| texcoords | indeks puli UV |
| points | indeks puli pozycji |
| normals | indeks puli normalnych (w starszych narzędziach mylnie „brightness”) |

**Tryb rysowania linku (`flags`)** ✅ obserwacja w grze: w kopii laboratoryjnej zmieniona flaga łąki `wiese_rund_a` (`flusslandschaft.3db`, tekstura mchu z alfą), zrzuty tej samej sceny ([BAZA_WIEDZY §8.9](../BAZA_WIEDZY.md)):

| flags | Fragmentów w danych | W grze | Przykładowe materiały |
|---|---|---|---|
| 0 | 0 | **niewidoczny** (nie jest rysowany) | – |
| 1 | 168 801 | zwykły: tekstura × oświetlenie, alfa jak tekstura | prawie wszystko |
| 2 | 6 120 | **addytywnie** (świeci, tło przebija; jasne jak blask) | `FX_Licht*`, `Interface01`, `Elfen_Koerper`, `Character_Odin_a` |
| 3 | 10 613 | jak 1, ale miękko przejrzysty: **zwykłe mieszanie alfa** | postacie (smok, chomik, troll), liany, mech |
| 4 | 933 | jak 1 (różnicy nie widać; ❓) | `Urwald_stein_256`, `lava_nahtlos` |
| 5 | 2 695 | **mapowanie otoczenia**: wzór tekstury nie według UV (sprawdzone wzorzystą teksturą) | `kr_enviroment_*`, `enviro_*` (kryształy, chrom, złoto, szkło) |
| 6, 7 | 956, 333 | ❓ (mieszanie + otoczenie) | `enviro_glas`, `enviro_wasser`, `enviro_rot` |
| 8 | 726 | **bez oświetlenia** (świeci własnym kolorem: w ciemności tak samo jasny jak przy lampach), nieprzejrzysty | `Checker` (znacznik światła), `Zwerge_Boden_c`, `brainposter` |
| 9 | 81 | ciemniejszy niż 1 (❓) | `lava_boden_a`, `lava_rinne_a` |
| 11 | 190 | ❓ | `lava_nahtlos`, `Trollh_Folterkammer_a_256` |

Wartości zachowują się jak tryby (prawdopodobnie zestawy stanów etapów tekstur Direct3D), a nie niezależne bity: 1 i 2 osobno dają zwykły i addytywny obraz, a 3 zwykłe mieszanie alfa; 4 osobno nic nie zmienia, a 5 włącza mapowanie otoczenia. Mapowanie otoczenia zależy też od rejestru `Performance\UseEnvMapping`.

**Nasz renderer** ([`render/model_renderer`](../../src/lib/render/model_renderer.cpp), [model.frag](../../src/shaders/model.frag)): 0 pomija link; 2 rysuje w drugim przebiegu addytywnie (`ONE, ONE`, kolor × alfa), bez zapisu głębi i bez oświetlenia (🔶 blask); 5–7 biorą współrzędne tekstury z normalnej (`n.xy · 0,5 + 0,5`, 🔶 kamera gry patrzy prawie wzdłuż osi z); 8 pomija oświetlenie; 3 rysuje rdzeń (alfa ≥ 0,5) jak 1, a miękkie brzegi (alfa < 0,5) w fazie przezroczystej: scena rysuje najpierw wszystkie obiekty bez brzegów i blasku, potem brzegi trybu 3 i blask trybu 2 bez zapisu głębi, od obiektu najdalszego (najmniejsze z). Bez tego podziału miękki brzeg z zapisem głębi zasłaniał obiekty rysowane później i w menu prześwitywało przez niego niebo; 1, 4, 9, 11 jak 1. Porównanie: przełącznik „Tryby rysowania” w wigview albo `--no-draw-modes`.

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
| flags | `u16` | ❓ 648 różnych wartości, 1414 animacji ma 0; wygląda na niezainicjowaną pamięć eksportera (np. 26981 = `0x6965` = tekst „ei”). Ignorować |
| unknownF | `f32` | ❓ 867 różnych wartości, zwykle 0. Ignorować |
| source | `str` | ścieżka do pliku eksportera, np. `...\baby_aufwachen.sex`. Ignorować |
| move | `vec3` | ✅ przesunięcie postaci **na klatkę** (jednostki modelu; 200 = blok), w układzie modelu (−y = do przodu, z = w górę przy wspinaniu). Przy 10 kl./s: `gehen_loop` 16,22 → 0,81 bloku/s, `laufen_schnell_loop` 34,29 → 1,71 bloku/s; w grze zmierzone 0,85 i 1,59 bloku/s (rozbieg i bieg po rozkazie, [BAZA_WIEDZY §8.8](../BAZA_WIEDZY.md)). Niezerowe w 366 z 6445 animacji |
| rotate | `vec3` | ✅ obrót **na krok między klatkami**, w radianach: `baby.drehen_links` 13 klatek × 0,1309 → 12 kroków = π/2 (90°), `mann.drehen_links` 4 klatki × 0,5265 → 3 kroki ≈ 90°. Niezerowe w 83 animacjach |

### 3.6 Cienie
| Pole | Typ | Opis |
|---|---|---|
| count | `u16` | 0–2 |
| *powtórz:* data | 1024 B | mapa 32 × 32, 8 bit, skala szarości (cień rzucany pod obiektem). Klatki wskazują cień przez `shadowIndex` |

🔶 **Co przedstawia obraz cienia.** Obraz leży na prostokącie `shadowQuad` klatki: kolumna i rośnie z x (od x0 do x1), wiersz j rośnie z z (od z0 do z1), bez odbić. Jest to rozmyty rzut bryły na ziemię światłem padającym pod 45° od tyłu: punkt (x, y, z) trafia w (x, z − y), gdzie −y to wysokość nad ziemią (oś y w dół), więc cień pada w +z, ku widzowi. Sprawdzenie: piksele > 40 porównane z rzutem trójkątów klatki dają średnio IoU 0,70 na 39 cieniach z 8 modeli; przesunięcie k·wysokość z k = 0,75 daje 0,65, a z k = 1,25 daje 0,58, odbicie osi x daje 0,62. Model ma najwyżej 2 obrazy cienia, które dzielą wszystkie klatki z cieniem, więc cień nie nadąża za animacją ([BAZA_WIEDZY §9.7](../BAZA_WIEDZY.md)). 🔶 Gra rysuje cienie od lamp w zasięgu `MaxShadowLightDist` (rejestr, domyślnie 8): przy 0 znikają długie smugi od stworzeń skierowane od lampy; na powierzchni ich nie widać ([BAZA_WIEDZY §9.8](../BAZA_WIEDZY.md)). ❓ Jak obraz jest rozciągany w stronę lampy. Podgląd: `wigview --model BRUT.3DB --shadow-cmap`.

### 3.7 Cmapy
| Pole | Typ | Opis |
|---|---|---|
| count | `u16` | (MANN: 6390) |
| *powtórz:* width, height | `u16`, `u16` | |
| originX, originY | `i16`, `i16` | ✅ położenie lewego górnego rogu maski względem punktu obiektu, w kratkach (`BRUT`: 65531 = −5, 65533 = −3) |
| pixels | `u8` × width × height | ✅ maska zajętości 0/1 (wszystkie piksele w danych to 0 albo 1) w kratkach **1/8 bloku** (25 jednostek modelu; `BRUT`: 8 wypełnionych kolumn na obrys 204 jednostek), **na ziemi, w płaszczyźnie x/z**: kolumna i ↔ x = (originX + i)·25, wiersz j ↔ z = (originY + j)·25 (z to głębokość, + ku widzowi). ✅ Płaszczyzna: leżący krasnal (`mann.schlafen_boden_loop`, bbox z −116..105) ma maskę 9×12, a stojący (bbox z −33..92) 8×8. 🔶 Kształt: rzut trójkątów klatki na x/z poszerzony o kratkę (krzyżem) daje IoU 0,70 z maską na 1799 klatkach 6 modeli; bez poszerzenia 0,54 (88% rzutu leży w masce, ale maska jest większa), płaszczyzna x/y najwyżej 0,34 ([BAZA_WIEDZY §9.7](../BAZA_WIEDZY.md)). Przypisana do jednej klatki (`cmapIndex` mesha). 🔶 Mapa kolizji: `animinit.tcl` ustawia klasie `class_collision 1`, gdy `has_cmap obiekt.standard` (Diggles-Unity `DigglesModelLibrary.HasCMap`). Mają ją postacie (`MANN` 1020, `FRAU` 1016 klatek), budynki (`produktionsstaetten` 384) i część dekoracji |

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
| linkRecords | 20 B × n | ✅ rekordy punktów zaczepienia (§9): 3 × `u16` skwantyzowane x, y, z (0–5), 2 B dopełnienia (6–7, 🔶 śmieci z pamięci eksportera: wyrównanie `f32` do 4 bajtów; wartości nie zależą od numeru punktu, tylko od pliku), 3 × `f32` kąty Eulera w radianach (8–19, wszystkie 197 623 w [0, 2π]) |

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

**Orientacja ✅:** układ gry (Direct3D) jest **lewoskrętny**: x w prawo, **y w dół**, z w stronę widza (przód modelu patrzy w +z). Głowa postaci ma ujemne y; `BABY` mieści się w y ∈ (−97, 5). Do prawoskrętnego układu z Y w górę (OpenGL, glTF) **odbij samą oś Y**: (x, y, z) → (x, −y, z), normalne tak samo. Sprawdzone na teksturze z napisem (`brainposter` w `KRISTALL.3DB`): napis czyta się poprawnie z kamery w +z. Wcześniejsza wersja tej dokumentacji zalecała obrót o 180° wokół X i to było **błędne**, bo dawało lustrzany obraz.

Skala ✅: 1 blok świata = 200 jednostek modelu, a krasnal ma około 200 jednostek wysokości. Modele stawia się w świecie bez skalowania ([rozdział 5.3 przewodnika](../silnik/05_obiekty.md)).

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

**Interpolacja:** oryginalny silnik najpewniej interpoluje między klatkami (Diggles-Unity twierdzi, że nie). Nie rozstrzygamy tego: nasz renderer interpoluje zawsze (decyzja projektu, [rozdział 11.6](../silnik/11_obiekty_tcl_i_petla.md)). W exe jest opcja `AnimIpol [true|false] (true)`, w rejestrze `Performance\AnimInterpolation=1`, a dla obiektów komendy `set_forceipol`/`get_forceipol`. Dane są do tego przygotowane: w ok. 99,5% par kolejnych klatek linki mają ten sam materiał, tę samą pulę trójkątów i tę samą liczbę wierzchołków. Gdy topologia się różni, rysuj klatkę `i` bez interpolacji.

🔶 Prędkość odtwarzania: **10 kl./s** według stałej gry podanej przez [Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) (`Docs/ANIMATION_CONTROLLER.md` §1; [rozdział 11.6 przewodnika](../silnik/11_obiekty_tcl_i_petla.md)). Skrypty sprawdzają długość animacji w klatkach komendą `db_animlength obiekt.animacja`.

Nazewnictwo animacji postaci: `*_start`, `*_loop`, `*_end` (np. `gehen_start` → `gehen_loop` → `gehen_end`). Statyczne obiekty mają jedną animację `standard`, często z jedną klatką.

## 9. Minimalny renderer: przepis

1. Wczytaj `SPHERE.DAT` (256 × vec3).
2. Wczytaj model wg §3; sprawdź koniec pliku i odwołania.
3. Dla wybranego obiektu weź animację, a z niej klatkę → mesh.
4. Dla każdego linku zbuduj bufor wierzchołków (pos po dekwantyzacji, uv, indeks normalnej) i bufor indeksów `u16`.
5. Teksturę wyszukaj wg §6 i wczytaj TGA. Filtrowanie liniowe, zawijanie REPEAT.
6. Macierz modelu: odbicie osi Y (§4), przesunięcie środka bbox do zera. Kamera w +z patrzy w −z.
7. Shader: `kolor = tekstura(uv) * (ambient + (1-ambient) * max(dot(N, L), 0))`, gdzie `N = sphere[indeks]` obrócone macierzą modelu. Odrzuć piksele z alfą < 0,5 (liście, włosy, kraty).
8. Bez cullingu (modele mają cienkie, dwustronne elementy), z buforem głębi.

## 10. Eksport do glTF 2.0

Tak eksportujemy modele ([`src/lib/gltf_export.cpp`](../../src/lib/gltf_export.cpp), wynik przechodzi walidator Khronos bez uwag):

| `.3db` | glTF |
|---|---|
| obiekt | węzeł główny |
| animacja | węzeł potomny z własną siatką |
| klatka 0 animacji | atrybuty siatki (POSITION, NORMAL, TEXCOORD_0) |
| klatki 1..N−1 | morph targets: różnice POSITION i NORMAL względem klatki 0 |
| link | prymityw z materiałem |
| odtwarzanie | klip: w chwili i/fps waga celu i = 1, interpolacja LINEAR (dokładnie jak liniowa interpolacja klatek) |
| tekstura TGA | obraz PNG osadzony w `.glb`; alfa < 255 → `alphaMode: MASK` (próg 0,5); `doubleSided: true` |

Konwersja współrzędnych: `(x, y, z) → (x, −y, z) · 0,01` (Y w górę, metry; krasnal ma ok. 2 m). UV bez zmian, bo glTF też ma v = 0 u góry. Przy wielu animacjach węzły są domyślnie ukryte skalą 0, a każdy klip pokazuje swój węzeł. Klatki o innej topologii niż klatka 0 (np. 84 z 11 573 w MANN) zastępujemy poprzednią zgodną.

## 11. Przykład: `BABY.3DB`

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

## 12. Otwarte pytania
- Tryby `flags` linku 4, 6, 7, 11 (tabela w §3.2): dokładny wzór mieszania; 9 = ciemniejsze niż 1 (❓ dlaczego).
- Cień: obraz to rzut pod 45° na `shadowQuad` (🔶 §3.6); gra rysuje cienie od lamp (🔶), ❓ jak rozciąga obraz w stronę lampy. Dokładna reguła budowy `cmap` (poszerzenie o kratkę daje IoU 0,70, nie 1).
- (Interpolacja klatek w oryginale: celowo nierozstrzygnięta, u nas zawsze włączona.)

## 9. Punkty zaczepienia ✅

Gra przypina obiekty do punktów modelu (`link_obj dziecko rodzic punkt`): czapki do głowy, narzędzia do rąk. Modele są animowane wierzchołkowo, więc punkty są zapisane osobno w każdej klatce (meshu):

```
rekord = linkRecords[ linkTable[punkt] ]          (linkTable: 32 × u32 w meshu bieżącej klatki)
x = dequant(u16[0]), y = dequant(u16[1]), z = 2 · dequant(u16[2])   (jednostki modelu; z gra skaluje podwójnie)
dequant(v) = (v − 32768) · 0,25
```

Punkt obracamy jak obiekt-rodzica (oś pionowa) i dodajemy do jego pozycji; dziecko dostaje też obrót rodzica. Rekord ma też własną orientację punktu: 3 × `f32` (bajty 8–19) to kąty Eulera X, Y, Z w radianach (Y = obrót wokół osi pionowej), dzięki którym czapka przechyla się z głową (komenda `get_linkrot`; za Diggles-Unity `DigglesLinkPoints.TryGetLocalEuler`, zakres danych ✅). Punkty modelu krasnala `mann`: 0 prawa ręka, 1 lewa ręka, 4 głowa, 6 i 11 tułów, 7 ręka z narzędziem, 10 usta. Przykład: `stand_anim_a`, klatka 0, głowa ≈ (0, −1, 0,05) bloku, czyli blok nad stopami.

Źródło: [Zimennik/Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) `Docs/HAT_LINK_SYSTEM.md` §3–4. ✅ Sprawdzone u nas: czapki z zapisu kampanii (przypięte w zapisie do punktu 4 krasnali) siedzą na głowach i podążają za animacją. Kod: `Model3db::linkOffset`, `MapScene::linkOffset` ([`src/lib/map_scene.cpp`](../../src/lib/map_scene.cpp)).

---
**Diggles-Unity** (ich opracowanie, 🔶 do sprawdzenia w laboratorium): [ANIMATION_CONTROLLER](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/ANIMATION_CONTROLLER.md), [HAT_LINK_SYSTEM](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/HAT_LINK_SYSTEM.md), [FACE_ANIMATION](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/FACE_ANIMATION.md). Porównanie i stan: [mapa Diggles-Unity](../DIGGLES_UNITY.md).
