# 8. Tło: strefy horyzontu i panorama

Za terenem nic nie ma: przez otwory typu 5 (rozdział 4.5) i nad powierzchnią widać tło. Gra ma dwa mechanizmy tła:

1. **Strefy horyzontu**: jednolity kolor czyszczenia ekranu i mgła, w której nikną modele stojące daleko za terenem. Tak wygląda tło jaskiń (np. jezioro w menu).
2. **Panorama**: malowane niebo i krajobraz z paralaksą oraz „dywan” łąki. Tylko na powierzchni.

## 8.1 Strefy horyzontu ✅

**Skąd się biorą.** Obiekty klas `Info_Fog_*` (niewidoczne znaczniki w szablonach) wołają w ciele klasy:

```tcl
horizon_set [get_pos this] r g b głębokość zasięg
```

Przykłady z `Scripts/classes/items/INFOOBJS.TCL`:

| klasa | kolor (r g b) | głębokość | zasięg |
|---|---|---|---|
| `Info_Fog_Urw_a` | 0,305 0,375 0,429 | 32 | 70 |
| `Info_Fog_Urw_b` | 0,305 0,375 0,429 | 32 | 40 |
| `Info_Fog_Urw_d` | 0,305 0,375 0,429 | 32 | 15 |
| `Info_Fog_Obw_Nacht_a` | 0,085 0,093 0,32 | 32 | 40 |

Każde `horizon_set` dopisuje do listy stref rekord {pozycja, kolor, głębokość, zasięg}. Wszystkie klasy w danych mają głębokość 32, a zasięg od 15 do 100.

**Wybór strefy, w każdej klatce:** aktywna jest strefa **najbliższa ognisku kamery** (punkt `x, y` z `set_view`), ale tylko taka, której odległość 2D od ogniska jest **mniejsza niż jej zasięg**. Jeśli żadna nie spełnia warunku, strefy nie ma.

```
aktywna = brak; najlepsza = ∞
dla strefy s:
    d = |(s.x, s.y) − (ognisko.x, ognisko.y)|
    jeśli d < s.zasięg i d < najlepsza: aktywna = s; najlepsza = d
```

✅ W OBERWELT kamera startowa jest daleko od wszystkich stref, a drzewa w tle nie mają mgły; bez warunku zasięgu przechodziły w niebieskoszary.

**Co robi aktywna strefa:**
1. **Kolor czyszczenia ekranu** to kolor strefy. Bez strefy ekran czyści się na czarno. Ten kolor widać w otworach typu 5 tam, gdzie nie ma modeli tła.
2. **Zanikanie w głąb**: wierzchołki modeli stojących daleko za terenem, czyli o głębokości świata **z < 0** bloków (ponad 7 bloków za najgłębszym dnem), przechodzą w kolor strefy:
   ```
   jeśli z < 0:
       f = clamp(−z / głębokość, 0, 1)
       rozproszone = rozproszone · (1 − f)
       dodatek     = dodatek · (1 − f) + kolor_strefy · f
   ```
   Przy f = 1 wierzchołek ma czysty kolor strefy, więc odległe dekoracje (jezioro, drzewa za otworem) płynnie znikają w tle. Teren nigdy tu nie trafia, bo leży w z ∈ [7; 14,5].

Źródło: [Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) `Docs/BACKDROP_PARALLAX_RE.md` §3–4. ✅ Kadr menu zgadza się ze zrzutem gry.

## 8.2 Panorama powierzchni ✅

Panorama to sześć pasów krajobrazu (`Data/Texture/Misc/Panorama_layer_0.tga` … `_5.tga`): niebo, odległe góry, bliższe góry, las… Rysuje się je **przed terenem, bez testu i zapisu głębi**, z mieszaniem alfa (`SRC_ALPHA, ONE_MINUS_SRC_ALPHA`), po kolei od 0 (najdalszy) do 5 (najbliższy). Teren i modele zamalowują je potem tam, gdzie leżą.

**Siatka ekranowa.** Każdą warstwę rysuje się jako siatkę 9 × 9 punktów rozpiętą na całym ekranie (8 × 8 czworokątów). Pozycje wierzchołków są stałe (rogi i równe podziały ekranu), a co klatkę zmieniają się tylko UV. Dla każdego punktu siatki:

```
promień przez punkt ekranu w układzie gry (bloki):
    bliski = odrzutuj(punkt, głębia 0)        punkt na bliskiej płaszczyźnie (0,5 bloku od oka)
    daleki = odrzutuj(punkt, głębia 1)
    d = daleki − bliski
    nachylenieX = d.x / d.z,  nachylenieY = d.y / d.z
    xb = bliski.x,  yb = bliski.y
```

**Prawo paralaksy** dla warstwy o głębokości D, skali S i przesunięciach (offU, offV):

```
u = S · ( Ku · (nachylenieX · (D − 1) − xb) + 5 ) + offU
v = S · ( −Kv · (nachylenieY · (D − 1) − yb) + 5 ) + offV
Ku = 0,00075,  Kv = 0,0015
```

| warstwa | D | S | offU | offV |
|---|---|---|---|---|
| 0 (najdalsza, niebo) | 21 100 | 0,53 | 0 | 0,75 |
| 1 | 20 500 | 0,04 | 0 | 1,0 |
| 2 | 11 300 | 0,11 | 0 | 0,5 |
| 3 | 1 000 | 1,12 | 0 | −4,9 |
| 4 | 200 | 6,68 | 0,7 | −33,0 |
| 5 (najbliższa) | 100 | 18,0 | 1,0 | −90,25 |

Intuicja: promień przedłużony do głębokości D trafia w punkt, którego współrzędne wyznaczają UV. Bliskie warstwy (małe D, duże S) przesuwają się mocno przy ruchu kamery, a dalekie prawie wcale. To daje głębię.

**Adresowanie tekstur:** U zawsze zawijane (REPEAT), bo pasy powtarzają się w poziomie. V zawijane tylko w warstwie 0, w pozostałych **przycięte do krawędzi** (CLAMP). Pasy mają przezroczystą górę i nieprzezroczysty dół, więc poza pasem jest czyste niebo albo jednolity grunt, który i tak przykryje teren.

Źródło prawa i stałych: [Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) `Docs/BACKDROP_PARALLAX_RE.md` §2. ✅ Kadr OBERWELT na wprost zgadza się ze zrzutem gry; przy kamerze startowej panorama wypada u nas nieco niżej niż w grze (🔶 do zbadania).

## 8.3 Dywan łąki ✅/🔶

Po sześciu warstwach gra rysuje `Panorama_layer_6.tga`: pas łąki leżący poziomo w świecie, od terenu w głąb do horyzontu. W odróżnieniu od warstw jest to geometria w układzie świata (bloki), też bez testu i zapisu głębi:

```
wiersz_gruntu = G (8.4)
x0 = floor(ognisko.x / 20) · 20 − 140
dla kawałka k = 0..14:                     kawałki po 20 bloków, razem ±140 wokół ogniska
    siatka 4 × 4 wierzchołków (kolumna c = 0..3, rząd r = 0..3):
        x = x0 + 20·k + c · 20/3
        y = G − 2,5 − podniesienie[r]       podniesienie = (0,75; 0,25; 0; 0)
        z = (−60; −37,5; −15; 7,5)[r]       od horyzontu do tuż pod dnem terenu
        u = c · 0,33,  v = r · 0,33
```

Rząd 0 leży najdalej (z = −60), a rząd 3 tuż pod terenem (z = 7,5). Adresowanie: U zawijane, V przycięte.

## 8.4 Kiedy rysować panoramę i gdzie jest grunt ✅

Ustalone obserwacją gry (zrzuty `../lab/zrzuty/gra/oberwelt_*.png`, poza repozytorium):

- **Bez nieba nie ma panoramy.** Kadr OBERWELT z `set_light_begin -10000` ma czarne tło. W jaskiniach (menu) panoramy też nie ma.
- **`set_ground_begin N`** („Y-beginning of the ground”) ustawia wiersz gruntu G dywanu. Przy N = 15 i 24 duża część tła jest czarna (dywan i linia przycięcia nad kamerą), przy 30 dywan widać nad terenem, a 36 i 45 dają obraz jak bez komendy. **Dane gry nigdzie jej nie używają.** Bez niej G = `set_light_begin` (33 w OBERWELT); dywan jest wtedy schowany za terenem, a widać tylko jego dalekie rzędy na horyzoncie.

Reguła dla silnika:

```
jeśli scenariusz ustawił set_ground_begin:   G = ta wartość
w przeciwnym razie, jeśli set_light_begin:   G = ta wartość
w przeciwnym razie:                           panoramy nie rysuj
rysuj, gdy ognisko.y − 20 ≤ G                 (bramka wg Diggles-Unity, §2.2)
```

❓ Gra rysuje tylko te komórki siatki panoramy, które leżą nad rzutem płaszczyzny dywanu (linia przycięcia). Przy normalnym G niczego to nie zmienia, bo resztę i tak zamalowuje teren. Różnica jest widoczna tylko wtedy, gdy grunt jest nad kamerą (czarne pola przy `set_ground_begin 15`). Nie odtwarzamy tego.

---
**W kodzie OpenWiggles** (pomocniczo, zasada działania jest opisana wyżej): [`lib/render/panorama_renderer.h`](../../src/lib/render/panorama_renderer.h), [`lib/map_scene.cpp`](../../src/lib/map_scene.cpp) (`updateZone`). Wszystkie moduły: [mapa kodu](../KOD.md).
