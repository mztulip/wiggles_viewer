#!/usr/bin/env python3
# Stan komend natywnych gry w naszym silniku Tcl: moduł (kategoria z oficjalnej pomocy komend), czy mamy komendę,
# ile razy skrypty gry jej używają i czy biorą jej wynik jako wartość ([komenda …]: zaślepka „0” zmienia wtedy
# przebieg skryptu, reguła z Diggles-Unity ENGINE_COMMAND_GAP.md), priorytet.
#
# Użycie: tools/tcl_commands_status.py [katalog gry] [ENGINE_TCL_API.txt] > docs/tcl_komendy_stan.md
#   katalog gry: domyślnie ../lab/Wiggles; pomoc komend: ../ref/Diggles-Unity-main/Docs/ENGINE_TCL_API.txt
# Wynik zawiera tylko nazwy komend, liczby i ścieżki plików (bez treści skryptów gry).
import collections
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
game = sys.argv[1] if len(sys.argv) > 1 else os.path.join(REPO, '..', 'lab', 'Wiggles')
api = sys.argv[2] if len(sys.argv) > 2 else os.path.join(REPO, '..', 'ref', 'Diggles-Unity-main', 'Docs', 'ENGINE_TCL_API.txt')

# Komendy gry: docs/natywne_komendy_gry.txt (nazwa, wywołania przy starcie do menu, S/-)
natives = []
for line in open(os.path.join(REPO, 'docs', 'natywne_komendy_gry.txt'), encoding='utf-8'):
    if line.startswith('#') or not line.strip():
        continue
    natives.append(line.rstrip('\n').split('\t')[0])

# Nasze komendy: tabela kCommands w src/lib/script/engine.cpp (nazwa -> funkcja) i moduły cmd_*.cpp
src = open(os.path.join(REPO, 'src', 'lib', 'script', 'engine.cpp'), encoding='utf-8').read()
tab = src[src.index('const Entry kCommands[] = {'):]
tab = tab[:tab.index('\n};')]
ours = dict(re.findall(r'\{"([^"]+)",\s*(\w+)\}', tab))
# Moduły komend (src/lib/script/cmd_*.cpp): nazwy jako literały; tabela właściwości daje parę get_X/set_X.
natset = set(natives)
sd = os.path.join(REPO, 'src', 'lib', 'script')
for f in sorted(os.listdir(sd)):
    if f.startswith('cmd_') and f.endswith('.cpp'):
        for lit in re.findall(r'"([^"\\]+)"', open(os.path.join(sd, f), encoding='utf-8').read()):
            for n in (lit, 'get_' + lit, 'set_' + lit):
                if n in natset and n not in ours:
                    ours[n] = 'module'
TRIVIAL = {'cmdNop', 'cmdLog', 'cmdZero', 'cmdEmpty', 'cmdConst'} # cmdConst: stała odpowiedź do czasu prawdziwego systemu

# Kategorie i pierwsza linia opisu z oficjalnej pomocy („Top<nazwa> (<kategoria>)”)
cat, desc = {}, {}
if os.path.exists(api):
    lines = open(api, encoding='utf-8', errors='replace').read().split('\n')
    for i, l in enumerate(lines):
        m = re.match(r'^Top(\S+) \((\w+)\)\s*$', l)
        if m:
            cat[m.group(1)] = m.group(2)
            if i + 1 < len(lines):
                desc[m.group(1)] = re.sub(r'\s+', ' ', lines[i + 1]).strip()[:110]

# Użycia w skryptach gry (pozycja komendy: początek linii, po [ ; { albo „then”/„else”)
calls, value, files = collections.Counter(), collections.Counter(), collections.defaultdict(set)
names = set(natives)
pat = re.compile(r'(^|[\[;{]|\bthen\b|\belse\b)\s*([A-Za-z_/][\w/]*)', re.M)
for dp, _, fs in os.walk(os.path.join(game, 'Data')):
    for f in fs:
        if not f.lower().endswith('.tcl') or f.startswith('ow_') or f.startswith('owprobe'):
            continue
        p = os.path.join(dp, f)
        t = open(p, encoding='cp1250', errors='replace').read()
        t = re.sub(r'^\s*(//|#|;#).*$', '', t, flags=re.M)
        rel = os.path.relpath(p, os.path.join(game, 'Data'))
        for m in pat.finditer(t):
            w = m.group(2)
            if w in names:
                calls[w] += 1
                files[w].add(rel)
                if m.group(1) == '[':
                    value[w] += 1


def prio(n):
    c, v = calls[n], value[n]
    if c == 0:
        return 'martwa'
    if v >= 1 and c >= 3:
        return 'P1'
    if c >= 5:
        return 'P2'
    return 'P3'


def state(n):
    if n not in ours:
        return '❌'
    return '⚪' if ours[n] in TRIVIAL else '✅'


by_mod = collections.defaultdict(list)
for n in natives:
    by_mod[cat.get(n, 'inne')].append(n)

out = []
out.append('# Stan komend natywnych gry (generowane)\n')
out.append('Wygenerowane przez [`tools/tcl_commands_status.py`](../tools/tcl_commands_status.py); nie edytować ręcznie. '
           'Moduły i opisy z oficjalnej pomocy komend gry w opracowaniu [Zimennik/Diggles-Unity](https://github.com/Zimennik/Diggles-Unity) '
           '(`Docs/ENGINE_TCL_API.txt`). Lista komend: [natywne_komendy_gry.txt](natywne_komendy_gry.txt).\n')
out.append('Stan: ✅ mamy, ⚪ mamy, ale nic nie robi (log, nop, 0, stała odpowiedź), ❌ brak (zaślepka zwraca 0 i trafia do `openwiggles_tcl.log`). '
           'Priorytet: **P1** wynik używany jako wartość (`[komenda …]`) i co najmniej 3 wywołania (zła stała zmienia przebieg skryptu), '
           '**P2** co najmniej 5 wywołań, **P3** rzadka, **martwa** nieużywana w skryptach gry. Wywołania liczone w `Data/**/*.tcl` bez komentarzy.\n')
tot = collections.Counter()
rows = []
for mod in sorted(by_mod, key=lambda m: -sum(calls[n] for n in by_mod[m] if state(n) == '❌')):
    ns = by_mod[mod]
    have = sum(1 for n in ns if state(n) == '✅')
    miss = [n for n in ns if state(n) != '✅']
    p = collections.Counter(prio(n) for n in miss)
    rows.append('| %s | %d | %d | %d | %d | %d | %d |' % (mod, len(ns), have, p['P1'], p['P2'], p['P3'], p['martwa']))
    tot.update({'all': len(ns), 'have': have, **p})
out.append('## Podsumowanie\n')
out.append('| moduł | komend | ✅ | brak P1 | brak P2 | brak P3 | brak martwe |')
out.append('|---|---|---|---|---|---|---|')
out += rows
out.append('| **razem** | %d | %d | %d | %d | %d | %d |\n' % (tot['all'], tot['have'], tot['P1'], tot['P2'], tot['P3'], tot['martwa']))
for mod in sorted(by_mod):
    out.append('## %s\n' % mod)
    out.append('| komenda | stan | priorytet | wywołań | jako wartość | plików | opis |')
    out.append('|---|---|---|---|---|---|---|')
    order = {'P1': 0, 'P2': 1, 'P3': 2, 'martwa': 3}
    for n in sorted(by_mod[mod], key=lambda n: (state(n) == '✅', order[prio(n)], -calls[n], n)):
        d = desc.get(n, '').replace('|', '\\|').replace('‹', '<').replace('›', '>')
        out.append('| `%s` | %s | %s | %d | %d | %d | %s |' % (n.replace('|', '\\|'), state(n), prio(n) if state(n) != '✅' else '',
                                                           calls[n], value[n], len(files[n]), d))
    out.append('')
print('\n'.join(out))
