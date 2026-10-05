#!/usr/bin/env python3
"""Spójność dokumentacji: drzewo odnośników od CLAUDE.md i ścieżki do kodu.

Sprawdza wszystkie pliki .md repozytorium (bez build/, third_party/, .git/):
  1. każdy plik .md jest osiągalny odnośnikami z CLAUDE.md (dokumentacja tworzy drzewo),
  2. odnośniki względne prowadzą do istniejących plików i katalogów,
  3. ścieżki w `backtickach` (src/…, tools/…, lang/…, docs/…, lib/…, build.sh) istnieją,
     np. po przeniesieniu pliku nie zostaje stara ścieżka.

  tools/check_docs.py          raport; kod wyjścia 1 przy błędach
  tools/check_docs.py --fix    dodatkowo zamienia ścieżki w backtickach na klikalne odnośniki
                               (`src/lib/vfs.h` -> [`src/lib/vfs.h`](../src/lib/vfs.h)), poza blokami kodu
"""
import collections
import os
import re
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
SKIP = ("build", "third_party", ".git")
LINK = re.compile(r"\]\(([^)\s]+)\)")
# Ścieżka repozytorium w backtickach, której nie poprzedza '[' (czyli nie jest już tekstem odnośnika).
CODE_PATH = re.compile(r"(?<!\[)`((?:src|tools|lang|docs|lib)/[^`\s*{}|]*|build\.sh)`(?!\]\()")


def markdown_files():
    out = []
    for d, dirs, files in os.walk(ROOT):
        rel = os.path.relpath(d, ROOT)
        if rel.split(os.sep)[0] in SKIP:
            dirs[:] = []
            continue
        out += [os.path.normpath(os.path.join(rel, f)) for f in files if f.endswith(".md")]
    return sorted(out)


def repo_path(token):
    """Ścieżka w repozytorium dla ścieżki z tekstu ('lib/x.h' to skrót od 'src/lib/x.h'); None, gdy nie istnieje."""
    p = token.rstrip("/")
    for cand in (p, "src/" + p if p.startswith("lib/") else None):
        if cand and os.path.exists(os.path.join(ROOT, cand)):
            return cand
    return None


def split_fences(text):
    """Części tekstu na przemian: poza blokiem kodu ``` (parzyste) i w bloku (nieparzyste)."""
    return re.split(r"(^```.*?^```[^\n]*$)", text, flags=re.S | re.M)


def main():
    fix = "--fix" in sys.argv
    files = markdown_files()
    graph, broken, stale, linked = {}, [], [], 0
    for md in files:
        path = os.path.join(ROOT, md)
        text = open(path, encoding="utf-8").read()
        if fix:
            parts = split_fences(text)
            for i in range(0, len(parts), 2):
                def repl(m):
                    nonlocal linked
                    target = repo_path(m.group(1))
                    if not target or target == md:
                        return m.group(0)
                    linked += 1
                    return "[`%s`](%s)" % (m.group(1), os.path.relpath(os.path.join(ROOT, target), os.path.dirname(path)))
                parts[i] = CODE_PATH.sub(repl, parts[i])
            new = "".join(parts)
            if new != text:
                open(path, "w", encoding="utf-8").write(new)
                text = new
        out = []
        for target in LINK.findall(text):
            target = target.split("#")[0]
            if not target or re.match(r"[a-z]+:", target):
                continue
            p = os.path.normpath(os.path.join(os.path.dirname(md), target))
            if not os.path.exists(os.path.join(ROOT, p)):
                broken.append((md, target))
            out.append(p)
        graph[md] = out
        for i, part in enumerate(split_fences(text)):
            if i % 2:
                continue
            for m in CODE_PATH.finditer(part):
                if not repo_path(m.group(1)):
                    stale.append((md, m.group(1)))

    depth = {"CLAUDE.md": 0}
    queue = collections.deque(["CLAUDE.md"])
    while queue:
        n = queue.popleft()
        for x in graph.get(n, []):
            if x.endswith(".md") and x not in depth:
                depth[x] = depth[n] + 1
                queue.append(x)
    unreachable = [f for f in files if f not in depth]

    levels = collections.Counter(depth.values())
    print("pliki .md: %d, osiągalne z CLAUDE.md: %d (poziomy: %s)" % (
        len(files), len(depth), ", ".join("%d: %d" % kv for kv in sorted(levels.items()))))
    if fix:
        print("dodane odnośniki do kodu: %d" % linked)
    for f in unreachable:
        print("NIEOSIĄGALNY: %s (dodaj odnośnik z dokumentu nadrzędnego)" % f)
    for md, t in broken:
        print("ZEPSUTY ODNOŚNIK: %s -> %s" % (md, t))
    for md, t in stale:
        print("NIEISTNIEJĄCA ŚCIEŻKA: %s: `%s`" % (md, t))
    ok = not (unreachable or broken or stale)
    print("OK" if ok else "BŁĘDY")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
