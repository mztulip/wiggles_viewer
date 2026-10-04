# 13. Płyny: woda, siarka, lawa

Woda w basenach i rzekach, żółta siarka i lawa to „płyny”. Każde pole świata może zawierać jeden płyn z wypełnieniem. Płyn przelewa się między polami 10 razy na sekundę, a źródła (obiekty `Wasser`) go dolewają. Ten rozdział opisuje dane płynu, jego kształt w polu i rysowanie. Przepływ liczymy prawem gry (§13.5), sprawdzonym na zapisie z laboratorium.

**Źródło i status.** Prawa za [Zimennik/Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) (`Docs/FLUID_SYSTEM_RE.md`, `Docs/WATER_PHASE1_RE.md`). ✅ Sprawdziliśmy na danych deskryptor płynu w BlockDB oraz zapis stanu w `.sav` (zapis z wodą z laboratorium). Rysowanie porównaliśmy ze zrzutem gry tylko wzrokowo (🔶).

## 13.1 Skąd bierze się płyn

Szablony nie zawierają płynu. Tworzą obiekt-źródło, np. w `kris_unq_060`:

```tcl
set objhandle [new Wasser "" "8.5 14.5 13.01" "0 0 0"]
call_method $objhandle Editor_Set_Info {{volume 1000} {vpf 5}}
```

Klasa `Wasser` (`Scripts/classes/items/wasser.tcl`) sekundę po starcie woła `set_water 1` i `set_fsource this -pos {0 -1 0} -type water -vpf $vpf -volume $volume`: źródło jedno pole nad obiektem dolewa `vpf` na klatkę logiki, aż wyczerpie `volume`. Siarkę i lawę tworzą klasy obok, a typ płynu podaje `-type`.

Stan płynów zapisuje się w każdym polu zapisu gry: typ i wypełnienie ([formaty/sav.md](../formaty/sav.md)).

## 13.2 Kształt płynu w polu

Każdy blok terenu ma w BlockDB deskryptor płynu ([formaty/blockdb.md](../formaty/blockdb.md) §2): pojemność oraz granicę górną `U` i dolną `L` w ósmych częściach pola. Dla otwartego bloku `U = 8`, `L = 0`. Przy wypełnieniu `f` (y w dół):

```
dno płynu        = y + U/8
powierzchnia     = dno + (L − U) · min(f, 1) / 8
```

Przykład: otwarte pole w wierszu 23 z `f = 0,85`: dno 24, powierzchnia 24 − 0,85 = 23,15. Wypełnienie powyżej 1 nie podnosi powierzchni, tylko oznacza ciśnienie słupa wody nad polem (w zapisie: 0,85 przy powierzchni, 1,85 wiersz niżej itd.).

## 13.3 Rysowanie

Każde pole z płynem (`typ ≠ 0`, `f > 0,001`):
- **ściana przednia**: prostokąt `x..x+1`, od powierzchni do dna, na głębokości **14,4** bloku (tuż za przednią płaszczyzną terenu 14,5);
- **powierzchnia**, gdy nad polem nie ma płynu albo `f < 1`: poziomy prostokąt na wysokości powierzchni, od głębokości 14,4 w tył o `(16 − (wariant bloku & 15)) / 2` bloku (wariant: bajt 84 rekordu BlockDB). Gra składa go z tylu pasów o szerokości 1;
- **przezroczystość**: woda i lawa `min(255, max(f − 1, 1) · 64)/255`, czyli przy powierzchni 0,25 i coraz mniej przezroczyście w głębi; siarka stałe 64/255;
- **tekstura**: `Texture/m064/wasser_env.tga`, `schwef_nahtlos.tga`, `lava_nahtlos.tga` (typ 3 i 4); u nas przesuwana powoli w czasie;
- **mgła wojny**: pole pomijane, gdy wszystkie 4 narożniki mają bajt mgły 0.

Mieszanie alfa, test głębi bez zapisu, po terenie i modelach, przed cząsteczkami. 🔶 Oświetlenie: u nas stała jasność 0,75; gra liczy płyn jak teren (rozdział 7).

## 13.4 Czego brakuje

- 🔶 Falowanie powierzchni, oświetlenie płynu, dokładny wygląd `wasser_env` (to mapa otoczenia); u nas woda jest mniej niebieska niż w grze.
- ❓ Zatyczki (`Wasserstopper*`, `set_fstopper`), odpływy (ujemne `vpf`), opcje `-height` i `-ventil`, mieszanie różnych płynów.
- ❓ Źródła w zapisach gry: ich stan (pozostała objętość) jest w zapisie obiektu, którego jeszcze nie czytamy; woda z zapisu tylko się wyrównuje.

## 13.5 Przepływ ✅

Za Diggles-Unity (`WATER_PHASE1_RE.md` §4). Takt co 0,1 s (`src/lib/fluid_sim.cpp`):

1. **Źródła.** Obiekt `Wasser` (`Sulfur`, `Lava`) z `Editor_Set_Info {volume V} {vpf R}`: sekundę po starcie dolewa co takt `R·128` jednostek do pola `(int(x), int(y − 1))`, aż wyda `V·128`. Wypełnienie pola rośnie o `ilość / pojemność`.
2. **Migawka** wypełnień `F` wszystkich pól.
3. **Jeden przebieg** po polach w kolejności wierszy. Dla pola bieżącego c i jego sąsiada n wyżej (pionowo) oraz z lewej (poziomo):

```
deskryptor c: przepuszczalność A (pionowa albo pozioma); A = 0 albo pojemność 0 → ściana
A = max(A, 8), pojemności = max(C, 32)              (lawa: 16 i 128)
H(F, przesunięcie) = (U − L)·F + (8 + przesunięcie − U)     przesunięcie = 8 dla pola wyżej, inaczej 0
P(H) = H dla H ≤ 8, inaczej (H − 8)·8 + 8           (ciśnienie słupa ponad pełne pole rośnie 8× szybciej)
T = (P(Hn) − P(Hc)) · 0,125 · 0,125 · A · relaksacja + praca · pęd
T przycięte do [−zawartość c, zawartość n]; zawartość = F · pojemność
n −= T, c += T; wypełnienie < 0,001 → 0; praca = T (gdy |T| > 0,004)
```

| płyn | relaksacja | pęd |
|---|---|---|
| woda (1), lawa z wodą (4) | 1,5 | 0,5 |
| siarka (2) | 1,4 | 0,3 |
| lawa (3) | 1,3 | 0,125 |

Przykład: dwa otwarte pola (C = 960, A = 120, U/L = 8/0), pełne obok pustego: ΔP = 8, T = 8 · 0,015625 · 120 · 1,5 = 22,5 jednostki, czyli 0,023 pola w jednym takcie.

**Sprawdzenie.** Szablon `kris_unq_060` w polu (16, 16), źródło `{volume 1000} {vpf 5}`, 410 taktów (41 s). Nasza symulacja daje w basenie x 20..28 dokładnie te same wypełnienia co zapis gry z tej chwili: 0,85 w wierszu powierzchni, potem 1,85, 2,85 … 7,85 i 10,13 w wierszu dna (zapis `../lab/zapisy/ow_woda.sav`, program porównujący w historii badań: BAZA_WIEDZY §9.5).

🔶 Uproszczenia: przebieg po całym prostokącie z płynem zamiast rzadkiej listy pól, praca kierunkowa bez „starzenia”.
