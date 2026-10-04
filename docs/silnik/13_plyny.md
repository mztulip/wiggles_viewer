# 13. Płyny: woda, siarka, lawa

Woda w basenach i rzekach, żółta siarka i lawa to „płyny”. Każde pole świata może zawierać jeden płyn z wypełnieniem. Płyn przelewa się między polami 10 razy na sekundę, a źródła (obiekty `Wasser`) go dolewają. Ten rozdział opisuje dane płynu, jego kształt w polu i rysowanie. Symulacji przepływu jeszcze nie mamy.

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

- ❓ Symulacja przepływu (10 Hz): prawa i współczynniki są w `Docs/FLUID_SYSTEM_RE.md` („Flow contract”). Bez niej szablon z obiektem `Wasser` pokazuje się bez wody; wodę widać tylko w zapisach.
- 🔶 Falowanie powierzchni, oświetlenie płynu, dokładny wygląd `wasser_env` (to mapa otoczenia).
