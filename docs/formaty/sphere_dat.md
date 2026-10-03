# `Data/SPHERE.DAT`: tablica normalnych

| Właściwość | Wartość |
|---|---|
| Rozmiar | 3072 B |
| Zawartość | 256 × `vec3` (3 × `f32` little-endian), bez nagłówka |
| Wartości | wektory jednostkowe (długość ≈ 1), rozłożone na sferze |

```
for i in 0..255:
    sphere[i] = (f32, f32, f32)
```

Pierwszy wpis: bajty `43 67 A2 3E 9B 6C 18 3F F8 F7 3C 3F` → (0,317194; 0,595407; 0,738159).

Zastosowanie ✅: kwantyzacja normalnych. Modele `.3db` zapisują normalną wierzchołka jako jeden bajt, czyli indeks do tej tablicy ([3db.md](3db.md) §5).

Źródło: [Wiggles Forum](https://wiggles.ruka.at/forum/), wątek „Game internal information” (cech12, 2021, za informacjami od zespołu Mirage). Zastosowanie w `.3db` ustaliliśmy sami.
