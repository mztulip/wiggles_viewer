#!/usr/bin/env python3
"""Tłumaczenia: wyciąga napisy z kodu i aktualizuje pliki lang/*.po.

Napisy do tłumaczenia to literały w tr("…") i N_("…") w src/ (sąsiednie literały są łączone,
przyrostek identyfikatora ImGui "##…" jest odcinany, jak w src/lib/i18n.cpp).

    tools/i18n_update.py            # zapisuje lang/openwiggles.pot i scala każdy lang/*.po
    tools/i18n_update.py --new de "Deutsch"   # nowy język: lang/de.po z pustymi tłumaczeniami
    tools/i18n_update.py --check    # tylko raport; kod wyjścia 1, gdy w którymś .po czegoś brakuje
    tools/i18n_update.py --root KATALOG [...]   # inne repozytorium (np. gra z tym repo jako submodułem):
                                               # jego src/ i lang/ (plik .pot: lang/<nazwa katalogu bez _git>.pot)

Scalanie zachowuje istniejące tłumaczenia; nowe napisy dostają pusty msgstr (program pokaże wtedy
tekst polski), a napisy usunięte z kodu trafiają na koniec pliku jako przestarzałe (#~).
Bez zależności od pakietu gettext. Opis dla tłumaczy: lang/README.md.
"""
import os
import re
import sys
from datetime import datetime, timezone

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
POT_NAME = "openwiggles.pot"
if "--root" in sys.argv:  # inne repozytorium: jego src/ i lang/
    i = sys.argv.index("--root")
    ROOT = os.path.abspath(sys.argv[i + 1])
    POT_NAME = re.sub(r"_git$", "", os.path.basename(ROOT.rstrip("/"))) + ".pot"
    del sys.argv[i:i + 2]
LANG = os.path.join(ROOT, "lang")
POT = os.path.join(LANG, POT_NAME)

LIT = r'"(?:[^"\\\n]|\\.)*"'
CALL = re.compile(r'\b(?:tr|N_)\(\s*((?:%s\s*)+)\)' % LIT)
SPEC = re.compile(r'%[-+ #0-9.*]*[hljztL]*[a-zA-Z%]')


def c_unescape(s):
    out, i = [], 0
    while i < len(s):
        c = s[i]
        if c == "\\" and i + 1 < len(s):
            e = s[i + 1]
            out.append({"n": "\n", "t": "\t", '"': '"', "\\": "\\"}.get(e, e))
            i += 2
        else:
            out.append(c)
            i += 1
    return "".join(out)


def po_escape(s):
    return s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n").replace("\t", "\\t")


def extract():
    """{msgid: [plik:linia, …]} w kolejności pierwszego wystąpienia."""
    found = {}
    for base, _, files in sorted(os.walk(os.path.join(ROOT, "src"))):
        for f in sorted(files):
            if not f.endswith((".cpp", ".h")) or f.startswith("i18n."):  # i18n.* ma przykłady w komentarzach
                continue
            path = os.path.join(base, f)
            text = open(path, encoding="utf-8").read()
            for m in CALL.finditer(text):
                lits = re.findall(LIT, m.group(1))
                s = c_unescape("".join(l[1:-1] for l in lits))
                s = s.split("##")[0]
                if not s:
                    continue
                line = text.count("\n", 0, m.start()) + 1
                found.setdefault(s, []).append("%s:%d" % (os.path.relpath(path, ROOT), line))
    return found


def write_entry(out, msgid, msgstr, refs=(), fuzzy=False, obsolete=False):
    p = "#~ " if obsolete else ""
    for r in refs:
        out.append("#: " + r)
    flags = []
    if fuzzy:
        flags.append("fuzzy")
    if SPEC.search(msgid.replace("%%", "")):
        flags.append("c-format")
    if flags and not obsolete:
        out.append("#, " + ", ".join(flags))
    out.append('%smsgid "%s"' % (p, po_escape(msgid)))
    out.append('%smsgstr "%s"' % (p, po_escape(msgstr)))
    out.append("")


def parse_po(path):
    """(nagłówek, {msgid: (msgstr, fuzzy)}) – ta sama podstawowa składnia co src/lib/i18n.cpp."""
    entries, header = {}, ""
    cur, field, fuzzy, pending_fuzzy = None, None, False, False

    def flush():
        nonlocal header
        if cur is not None:
            if cur["id"] == "":
                header = cur["str"]
            else:
                entries[cur["id"]] = (cur["str"], cur["fuzzy"])

    for raw in open(path, encoding="utf-8"):
        line = raw.strip()
        if not line or line.startswith("#~"):
            continue
        if line.startswith("#"):
            if line.startswith("#,") and "fuzzy" in line:
                pending_fuzzy = True
            continue
        m = re.match(r'(msgid|msgstr)\s+(".*")$', line)
        if m:
            if m.group(1) == "msgid":
                flush()
                cur = {"id": "", "str": "", "fuzzy": pending_fuzzy}
                pending_fuzzy = False
            field = "id" if m.group(1) == "msgid" else "str"
            cur[field] += c_unescape(m.group(2)[1:-1])
        elif line.startswith('"') and cur is not None:
            cur[field] += c_unescape(line[1:-1])
        else:
            sys.exit("%s: nieznana linia: %s" % (path, line))
    flush()
    return header, entries


def header_text(code, name, date):
    return (
        "Project-Id-Version: OpenWiggles\n"
        "PO-Revision-Date: %s\n"
        "Language: %s\n"
        "X-Language-Name: %s\n"
        "MIME-Version: 1.0\n"
        "Content-Type: text/plain; charset=UTF-8\n"
        "Content-Transfer-Encoding: 8bit\n" % (date, code, name)
    )


def write_po(path, header, found, old, comment):
    out = comment[:]
    out.append('msgid ""')
    out.append('msgstr ""')
    for line in header.splitlines(keepends=True):
        out.append('"%s"' % po_escape(line))
    out.append("")
    for msgid, refs in found.items():
        msgstr, fuzzy = old.get(msgid, ("", False))
        write_entry(out, msgid, msgstr, refs, fuzzy)
    stale = [k for k in old if k not in found and old[k][0]]
    for msgid in stale:
        write_entry(out, msgid, old[msgid][0], obsolete=True)
    open(path, "w", encoding="utf-8").write("\n".join(out))
    done = sum(1 for k in found if old.get(k, ("", False))[0] and not old[k][1])
    return done, len(stale)


def main():
    args = sys.argv[1:]
    found = extract()
    now = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M+0000")
    os.makedirs(LANG, exist_ok=True)
    check = "--check" in args
    if "--new" in args:
        i = args.index("--new")
        code, name = args[i + 1], args[i + 2]
        path = os.path.join(LANG, code + ".po")
        if os.path.exists(path):
            sys.exit("%s już istnieje" % path)
        write_po(path, header_text(code, name, now), found, {},
                 ["# Tłumaczenie OpenWiggles: %s (%s). Opis: lang/README.md." % (name, code)])
        print("utworzono", os.path.relpath(path, ROOT))
    if not check:
        write_po(POT, header_text("", "", "YEAR-MO-DA HO:MI+ZONE").replace("Language: \nX-Language-Name: \n", ""),
                 found, {}, ["# Szablon tłumaczeń OpenWiggles (wygenerowany przez tools/i18n_update.py). Nie edytować."])
    missing_any = False
    for f in sorted(os.listdir(LANG)):
        if not f.endswith(".po"):
            continue
        path = os.path.join(LANG, f)
        header, old = parse_po(path)
        comment = [l.rstrip("\n") for l in open(path, encoding="utf-8") if l.startswith("# ")][:5]
        if check:
            done = sum(1 for k in found if old.get(k, ("", False))[0] and not old[k][1])
            stale = sum(1 for k in old if k not in found)
        else:
            done, stale = write_po(path, header, found, old, comment)
        missing = len(found) - done
        missing_any |= missing > 0
        print("%-8s %4d/%d przetłumaczonych%s%s" % (f, done, len(found),
              ", brakuje %d" % missing if missing else "", ", przestarzałych %d" % stale if stale else ""))
    print("napisów w kodzie: %d" % len(found))
    if check and missing_any:
        sys.exit(1)


if __name__ == "__main__":
    main()
