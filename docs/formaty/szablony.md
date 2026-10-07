# Szablony map: `.pmp`, `.l2m`, `.tcl`, `.ENT`

Pliki w `Data/Templates/` (gra czyta z tego katalogu; podkatalogi `pmp/`, `l2m/`, `tcl/`, `ent/` to robocze kopie edytora). Jak szablony składają się w świat: [rozdział 3 przewodnika](../silnik/03_swiat_z_szablonow.md). Implementacja wzorcowa: [`src/lib/map_template.cpp`](../../src/lib/map_template.cpp).

Wszystkie liczby są little-endian. ✅ Wszystkie pliki `.pmp` i niepuste `.l2m` z gry czytają się dokładnie do ostatniego bajtu (sprawdza to `test3db`).

## 1. `.pmp`: siatka bloków

```
u32  w, h            rozmiar w blokach
u8   typ[w·h]        typ pola, wiersz po wierszu od góry
u32  blok[w·h]       ID kształtu w BlockDB (0 … 118687)
u32  n
u16  maska[n]        przebiegi płaszczyzny ruchu (niżej)
u8   warstwa2[w·h]   światło bazowe pola
```

Indeks pola (x, y) to `y · w + x` we wszystkich tablicach.

| Pole | Znaczenie | Rozdział |
|---|---|---|
| typ & 0x0F | 0 i 2 = grunt C (piasek), 3 = B (ziemia), 4 = A (skała), 5 = otwór do tła ✅ | [4.2](../silnik/04_teren.md), [4.5](../silnik/04_teren.md) |
| typ & 0x7F == 0 | pole puste: przy nakładaniu może być przezroczyste ✅ | [3.3](../silnik/03_swiat_z_szablonow.md) |
| typ & 0x7F | materiał do kopania: 2 → ziemia (2), 3 → piasek (1), 4 → skała nie do kopania (0), 5 → otwarte (3); 0 i 1 dziedziczą po polu niżej / z prawej ✅ (`get_material` w grze) | [19.2](../silnik/19_kopanie.md) |
| typ & 0x80 | wymuszona ściana dla ruchu: podpola nie do chodzenia, cztery takie pola 2×2 blokują pole (za Diggles-Unity, `tile+0x18` bit 7); w obrazie bez efektu | [17.2](../silnik/17_ruch.md) |
| blok | kształt wnętrza bloku ✅ | [blockdb.md](blockdb.md) |
| warstwa2 | światło bazowe 0…8 (4 = zwykła skała) ✅ | [7.2](../silnik/07_oswietlenie.md) |
| maska | ✅ płaszczyzna ruchu postaci (podpola 0 zablokowane, 1 ściana, 3 podłoga), wiersz po wierszu drobnej siatki szablonu (w · 8 × h · 8). Przebieg u16: wartość = bity 0..1, długość = bity 2..15 (0 = 0x4000); suma długości = w · h · 64. Gra ją zapisuje, ale po wczytaniu świata wylicza od nowa z wysokości bloków; nasza reguła zgadza się z nią w 99,96% podpól 231 szablonów | [17.2](../silnik/17_ruch.md) |

Nazwy `.pmp` opisują tylko kształt (`gng_021.pmp`), a jeden kształt służy wielu szablonom różnych światów.

## 2. `.l2m`: detale terenu

```
char[4] "M2L\0"
u32     w, h          jak w .pmp tego szablonu
u16     wartość[w·h]
```

| Bity | Znaczenie |
|---|---|
| 15–14 | narożnik lewy górny: 0 brak, 1 zestaw `<świat>`, 2 zestaw `<świat>2` ✅ |
| 13–12 | narożnik prawy górny ✅ |
| 11–10 | narożnik lewy dolny ✅ |
| 9–8 | narożnik prawy dolny ✅ |
| 7–0 | ❓ wartości 0x00, 0x20, 0x40, 0x60, 0xC0, rzadko 1…4 |

Plik może mieć **0 bajtów** (8 przypadków): brak detali. Wybór tekstur i mieszanie: [rozdział 4.4](../silnik/04_teren.md).

## 3. `.tcl`: skrypt szablonu

Plik definiuje jedną procedurę Tcl. Budowa jest stała, bo pliki generował edytor map:

```tcl
proc MapTemplateSet {xoffset yoffset} {
	 loading_objdetail high
	map_template data/templates/gng_021.pmp $xoffset $yoffset
	map_setlayer2 $xoffset $yoffset data/templates/urw_gng_021_a.l2m
	 loading_objdetail medium
	sel /obj
	set objhandle [new Dummy_Moos_um_a "" "[expr 2.53098 + $xoffset] [expr 2.5 + $yoffset] 13.54" "0 6.12319 0"]
	set_owner $objhandle -1
	set_physic $objhandle true
	set_autolight $objhandle false
	set_boxed $objhandle false
	...
}
```

| Komenda | Argumenty |
|---|---|
| `map_template` | plik `.pmp`, x, y (ścieżka czasem bez `data/templates/`) |
| `map_setlayer2` | x, y, plik `.l2m` (uwaga: inna kolejność niż w `map_template`) |
| `loading_objdetail` | `high`, `medium`, `low`, `off`: poziom szczegółów kolejnych obiektów |
| `new` | klasa, nazwa (zwykle `""`), `"x y z"`, `"rx ry rz"` |

Pozycje obiektów mają postać `[expr <liczba> + $xoffset]` (czasem z klamrami `[expr {… + $xoffset}]`), więc prosty parser może wyciągnąć liczbę i dodać przesunięcie bez interpretera Tcl. Składowa z nie ma przesunięcia i podlega przeliczeniu `z · 0,5 + 7` ([1.5](../silnik/01_przeglad.md)).

Nazwy szablonów: `<świat>_<rodzaj>_<nr>_<wariant>.tcl` (świat: `urw`, `kris`, `lava`, `swf`, `obw`, `unq`, `multi`; rodzaj: `gng`, `hol`, `gsg`, `caves`, `start`).

## 4. `.ENT`

Zwykle 0 bajtów. ❓

## 5. Inne pliki w katalogu

`CAVE0–3.TGA` i plik `.txt` w `Data/Templates/` ❓ (prawdopodobnie pomoce edytora).

---
**Diggles-Unity** (ich opracowanie, 🔶 do sprawdzenia w laboratorium): [RENDERING_PIPELINE_COMPLETE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/RENDERING_PIPELINE_COMPLETE.md), [WORLDGEN_ORIGINAL_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/WORLDGEN_ORIGINAL_RE.md). Porównanie i stan: [mapa Diggles-Unity](../DIGGLES_UNITY.md).
