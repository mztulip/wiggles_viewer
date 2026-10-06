#!/usr/bin/env python3
"""Laboratorium: zmiana trybu rysowania (flags) linków w kopii pliku .3db gry.

    tools/lab/set_link_flags.py LAVAWELT.3DB 'lava_krst_' lava_seulen_128 1   # obiekty z „lava_krst_” w nazwie, materiał
    tools/lab/set_link_flags.py LAVAWELT.3DB 'lava_krst_' '*' 1               # wszystkie materiały tych obiektów
    tools/lab/set_link_flags.py LAVAWELT.3DB --list 'lava_krst_ov_i'          # tylko wypisz linki (tryb, materiał)
    tools/lab/set_link_flags.py LAVAWELT.3DB --restore                        # przywróć oryginał z kopii .orig

Plik jest szukany w ../lab/Wiggles/Data/3db (nazwa bez rozróżniania wielkości liter). Przed pierwszą zmianą
zapisuje kopię <plik>.orig; zmiany zawsze liczone od oryginału (poprzednie podmiany znikają). Filtr obiektu
to podciąg nazwy (małe litery). Gra wczytuje modele przy starcie: po zmianie `live2.sh stop` i `start`.
Układ pliku: docs/formaty/3db.md §3 (link = 6 × u16, flags to drugie słowo).
"""
import os
import shutil
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DIR = os.path.join(ROOT, "..", "lab", "Wiggles", "Data", "3db")


class R:
    def __init__(self, b):
        self.b, self.p = b, 0

    def u16(self):
        v = struct.unpack_from("<H", self.b, self.p)[0]
        self.p += 2
        return v

    def u32(self):
        v = struct.unpack_from("<I", self.b, self.p)[0]
        self.p += 4
        return v

    def s(self):
        n = self.u32()
        v = self.b[self.p:self.p + n].decode("latin-1")
        self.p += n
        return v


def parse(b):
    """(materiały, meshe: lista [(offset linku, material, flags)], obiekty: {nazwa: zbiór meshy})."""
    r = R(b)
    r.s(), r.s()
    mats = []
    for _ in range(r.u16()):
        mats.append(r.s())
        r.s()
        r.u32()
    meshes = []
    for _ in range(r.u32()):
        links = []
        for _ in range(r.u16()):
            off = r.p
            mat, fl = struct.unpack_from("<HH", b, off)
            links.append((off, mat, fl))
            r.p += 12
        r.p += 24 + 128 + 2 + 48 + 2  # bbox, linkTable, shadowIndex, shadowQuad, cmapIndex
        meshes.append(links)
    objs = []
    for _ in range(r.u16()):
        name = r.s()
        objs.append((name, [r.u32() for _ in range(r.u16())]))
    anims = []
    for _ in range(r.u16()):
        r.s()
        frames = [r.u32() for _ in range(r.u16())]
        r.u16()
        r.p += 4
        r.s()
        r.p += 24
        anims.append(frames)
    return mats, meshes, {n: {f for a in al for f in anims[a]} for n, al in objs}


def find(name):
    for f in os.listdir(DIR):
        if f.lower() == name.lower():
            return os.path.join(DIR, f)
    sys.exit(f"brak {name} w {DIR}")


def main():
    a = sys.argv[1:]
    if not a:
        sys.exit(__doc__)
    path = find(a[0])
    orig = path + ".orig"
    if a[1:2] == ["--restore"]:
        if os.path.exists(orig):
            shutil.copyfile(orig, path)
            os.remove(orig)
            print("przywrócono", path)
        else:
            print("brak kopii .orig, plik jest oryginałem")
        return
    listing = a[1:2] == ["--list"]
    src = orig if os.path.exists(orig) and not listing else path  # --list: plik w obecnym stanie
    b = bytearray(open(src, "rb").read())
    mats, meshes, objs = parse(b)
    if listing:
        filt = a[2].lower() if len(a) > 2 else ""
        for n, ms in objs.items():
            if filt in n.lower():
                s = sorted({(fl, mats[m]) for i in ms for _, m, fl in meshes[i]})
                print(n, " ".join(f"{fl}:{m}" for fl, m in s))
        return
    filt, mat, flag = a[1].lower(), a[2], int(a[3])
    done = set()
    for n, ms in objs.items():
        if filt not in n.lower():
            continue
        for i in ms:
            for off, m, fl in meshes[i]:
                if (mat == "*" or mats[m].lower() == mat.lower()) and off not in done:
                    struct.pack_into("<H", b, off + 2, flag)
                    done.add(off)
    if not done:
        sys.exit("nic nie pasuje")
    if not os.path.exists(orig):
        shutil.copyfile(path, orig)
    open(path, "wb").write(b)
    print(f"{os.path.basename(path)}: {len(done)} linków -> tryb {flag} (oryginał w .orig)")


if __name__ == "__main__":
    main()
