# 15. Dźwięk

Kroki Fenrira, stukanie młotka, okrzyki krasnali, muzyka w tle i dialogi w przerywnikach. Silnik nie wie, który dźwięk zagrać przy której czynności: wszystko opisują skrypty Tcl. Ten rozdział wyjaśnia, jakie pliki ma gra, jak skrypty nadają dźwiękom nazwy i jak przypinają je do klatek animacji, akcji i sekwencji.

**Źródło i status.** ✅ Pliki, formaty i składnię komend sprawdziliśmy na danych gry: `test3db` dekoduje każdy plik, a indeks odwołań buduje statyczny skaner skryptów ([`src/lib/sound_refs.cpp`](../../src/lib/sound_refs.cpp)). Znaczenie dalszych pól `sound anim` (materiał, pomieszczenie, wysokość) i komend `adaptive_sound` pochodzi z [Zimennik/Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) (`Docs/CUTSCENE_SYSTEM.md`, „Audio”) i jest 🔶, dopóki nie porównamy go z grą. ❓ Jak gra wycisza dźwięk z odległością i jak miesza głośności, nie badaliśmy.

## 15.1 Pliki

| Katalog | Zawartość | Format |
|---|---|---|
| `Data/Sound/` i podkatalogi postaci (`Fenris`, `Troll`, `Baby`, `Drache`…), `InGame`, `Landschaft`, `Produktion`, `Specials`, `Sprache` | efekty, odgłosy postaci, okrzyki | WAV PCM mono, 22050 Hz, 16 bit (892 pliki) albo 8 bit (298); pojedyncze 11025 i 44100 Hz |
| `Data/Sound/Sequenzen/` | dialogi przerywników i samouczka (`clip0092.mp3`, `1260a2040i.mp3`) | MP3 |
| `Data/Music/` | muzyka (`M001.MP3`…, `na001.mp3`…, `Hoehle.mp3`) | MP3 44,1 kHz albo 22,05 kHz |
| 4 pliki `.wav` | MP3 zapisane w kontenerze RIFF (format `0x55`) | WAV/MP3 |

W sumie 1420 plików i około 267 minut. Nazwy mają mieszaną wielkość liter (`fe_schritt1.WAV`), a skrypty piszą ścieżki małymi literami (`data/sound/fenris/fe_schritt1.wav`), więc wyszukiwanie musi ignorować wielkość liter (jak w [rozdziale 2](02_start_i_skrypty.md)).

**Dekodowanie WAV.** Plik RIFF składa się z kawałków `id(4) długość(4) dane`. W kawałku `fmt ` są: format (2 B), kanały (2 B), częstotliwość (4 B) i bity na próbkę (2 B, przesunięcie 14). Próbki leżą w kawałku `data`. Próbka 8-bitowa jest bez znaku (cisza = 128), więc `int16 = (b − 128) · 256`. Próbka 16-bitowa ma znak (little-endian). Niektóre pliki deklarują w kawałku `data` długość większą niż reszta pliku, dlatego czytamy tylko tyle, ile faktycznie jest.

## 15.2 Nazwy logiczne: `sound bind`

Skrypty nie odwołują się do plików bezpośrednio. Najpierw nadają plikowi nazwę:

```tcl
sound bind fe_schritt1 data/sound/fenris/fe_schritt1.wav
sound bind interface_click data/sound/Specials/interface_klick.wav
```

Tabelę nazw buduje `Scripts/init/soundinit.tcl`, wołany z `systeminit.tcl` (krok `soundinit`, [rozdział 2](02_start_i_skrypty.md)). Ten plik wywołuje też `z_soundinit.tcl` (krasnale) i `t_soundinit.tcl` (trolle). Łącznie jest 1015 wywołań `sound bind`. 1001 plików ma co najmniej jedną nazwę, a jeden plik może mieć ich kilka. 11 nazw wskazuje na pliki, których nie ma w danych: to głównie żarty `mp_chat_*` (krowa, owca, „terrorists win”) w bloku `catch {}`, więc brak pliku nie przerywa startu gry.

## 15.3 Dźwięk na klatce animacji: `sound anim`

```tcl
sound anim <obiekt>.<animacja> <klatka> <nazwa> [<materiał> <pomieszczenie> <głośność> <wysokość> …]
sound anim mann.ablegen_a 4 ablegen_a any any 0.9 1.0 0 0
sound anim fenrir.gehen_loop 9 fe_schritt1 any any 0.9 1.0 0 0
```

Gdy animacja `<animacja>` obiektu `<obiekt>` dojdzie do klatki `<klatka>`, gra odtwarza dźwięk `<nazwa>`.

- **Klucz `obiekt.animacja`** to dokładnie nazwa obiektu i animacji z pliku `.3db` ([format](../formaty/3db.md)). ✅ Wszystkie 1594 klucze z niezakomentowanych linii pasują do animacji w modelach (np. obiekt `fenrir` z `FENRIR.3DB`). Dwa niepasujące to szablony w ciele procedury (`$animname`).
- **Klatki liczone od 0.** 🔶 Wskazują na to dane: 493 z 2866 odwołań dotyczy klatki 0, tylko jedno klatki równej długości animacji, a trzy wychodzą poza animację (`troll.sitzen_saufen_b` klatka 23 przy 18 klatkach). Te trzy wyglądają na pomyłki w danych.
- **Pola dodatkowe** (🔶 Diggles-Unity): materiał podłoża `any/sand/soil/stone/wood/metal/water/lava` (np. inny dźwięk kroku na kamieniu i na drewnie), pomieszczenie `any/tunnel/small/medium/big`, głośność i wysokość.
- **`sound animnoseq`** ma te same pola, ale dźwięk nie gra w trakcie sekwencji (przerywników), w których dialog ma pierwszeństwo.
- Pomocnicze procedury `snd_anim` i `snd_animlist` (początek `soundinit.tcl`) tylko wołają `sound anim`. Duże listy `snd_animlist mann {…}` w `z_soundinit.tcl` są w większości zakomentowane.

Wynik w liczbach: 2171 `sound anim` i 735 `sound animnoseq`.

## 15.4 Dźwięk z akcji: `sound play`, `sound playglobal`

```tcl
sound play <nazwa> <głośność> [<pozycja>]   ;# w świecie: głośniej, gdy kamera jest blisko (❓ krzywa)
sound playglobal <nazwa> <głośność>         ;# bez pozycji (interfejs, komunikaty)
```

Przykłady: kroki Fenrira w sekwencjach (`sound play fe_schritt1 1`), bomba w trybie wieloosobowym (`sound play c4_plant 1 {x y z}`). Nazwa bywa składana ze zmiennej (`c4_beep$b`), więc indeks łączy takie wywołanie ze wszystkimi nazwami o tym samym przedrostku (`c4_beep1`, `c4_beep2`…).

## 15.5 Muzyka: `adaptive_sound`

`Scripts/init/adaptiveinit.tcl` definiuje motywy muzyczne. Motyw ma utwór na wejście, utwór do walki i kilka zwykłych utworów (🔶 Diggles-Unity):

```tcl
adaptive_sound setintrostream atmooberwelt data/music/na002.mp3
adaptive_sound setfightstream atmooberwelt data/music/na001.mp3
adaptive_sound addstandardstream atmooberwelt data/music/na001.mp3
```

## 15.6 Dialogi sekwencji

Przerywniki mają mowę w jednym pliku MP3, z którego grane są fragmenty:

- `Scripts/sequences/soundmarker/<numer>_<język>.txt` zaczyna się od `soundfile clip0092.mp3`, a po słowie `marker` ma wiersze `<słowo> <czasy w sekundach…>` (np. `Ohoh 0.87 2.913 4.062 5.973`; 🔶 granice fragmentów, dokładne znaczenie kolumn ❓),
- sekwencer otwiera plik przez `seq_audiostream open <plik>` i gra fragment `seq_audiostream play <start> <stop>`,
- samouczek i turniej mają własną procedurę `soundfile_open <nazwa>` (`Trigger_Tutorial.tcl`), która dokleja `.mp3`.

**Linie sekwencji z `-` i `+`.** ✅ Pliki `Scripts/sequences/**/*.tcl` czyta procedura `parse_next` w `classes/story/sequencer.tcl` linia po linii. Minus na początku linii **nie** wyłącza jej: linia jest wykonywana normalnie, ale pomijana, gdy gracz przerwie sekwencję (np. `-sound play fe_schritt1 0.9` razem z `-sq_screenvibe steps`). Plus oznacza „wykonaj także po przerwaniu”. Linie z `#` są pomijane.

## 15.7 Indeks odwołań (przeglądarka)

Zakładka **Dźwięki** w `wigview` pokazuje przy każdym pliku, gdzie skrypty go używają. Indeks powstaje bez wykonywania Tcl (ok. 0,2 s na 834 skrypty i pliki znaczników):

1. Każda linia skryptu jest dzielona na słowa Tcl. Rozpoznawane są `sound bind/anim/animnoseq/play/playglobal`, `adaptive_sound … *.mp3`, `soundfile`, `soundfile_open` i `seq_audiostream open`. Linia może zawierać kilka poleceń (np. `ExecBroadcast "sound play …; sound playglobal …"`).
2. Plik łączy się z nazwami przez `sound bind`, a nazwy z użyciami przez `sound anim/play`. Klipy dialogów łączą się przez nazwę pliku bez rozszerzenia (`clip0092`, `1260a2040i`).
3. Pozostałe linie są przeszukiwane pod kątem słów równych nazwie logicznej albo nazwie pliku („wzmianki”, np. zmienne i listy z nazwą dźwięku). Słowa występujące więcej niż 12 razy (`anim`, `hamster`, `labor`, `brauerei`…) to zwykłe słowa skryptów, a nie odwołania, więc są pomijane.

Wynik: 1227 z 1420 plików ma co najmniej jedno odwołanie; pozostałe (szare na liście) są w danych, ale skrypty ich nie używają.

W zakładce **Modele** sekcja „Dźwięki animacji” pokazuje `sound anim` bieżącej animacji. Opcja „Odtwarzaj dźwięki klatek” gra je w trakcie animacji: w każdym kroku zegara odtwarzane są dźwięki klatek z przedziału (poprzednia klatka, bieżąca klatka], z uwzględnieniem przejścia przez koniec pętli.

## 15.8 Odtwarzanie w OpenWiggles

`src/lib/sound.{h,cpp}`: `decodeSound` zamienia WAV i MP3 na próbki int16 (MP3 dekoduje [dr_mp3](https://github.com/mackron/dr_libs)), `toOutputFormat` przelicza je na 44100 Hz stereo (interpolacja liniowa), a `AudioOut` to mikser kilku głosów na SDL2 (głośność, pętla, pauza, przewijanie). Gra użyje tego samego miksera. Brakuje jeszcze praw głośności od odległości i materiałów (❓, [niewiadome](niewiadome.md)).

---
**Diggles-Unity** (ich opracowanie, 🔶 do sprawdzenia w laboratorium): [SOUND_SYSTEM_RE](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/SOUND_SYSTEM_RE.md), [AUDIO_SETUP](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/AUDIO_SETUP.md), [CUTSCENE_SYSTEM](https://github.com/Zimennik/Diggles-Unity/blob/main/Docs/CUTSCENE_SYSTEM.md). Porównanie i stan: [mapa Diggles-Unity](../DIGGLES_UNITY.md).

**W kodzie OpenWiggles** (pomocniczo, zasada działania jest opisana wyżej): [`lib/sound.h`](../../src/lib/sound.h), [`lib/sound_refs.h`](../../src/lib/sound_refs.h), [`apps/wigview/sound_view.h`](../../src/apps/wigview/sound_view.h). Wszystkie moduły: [mapa kodu](../KOD.md).
