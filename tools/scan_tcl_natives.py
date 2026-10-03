#!/usr/bin/env python3
# Skaner skryptów Tcl gry: wypisuje komendy używane, ale niezdefiniowane przez proc/method/def_class ani przez Tcl 8.3.
# Użycie: tools/scan_tcl_natives.py "<katalog gry>/Data" > docs/tcl_natywne_kandydaci.txt
# Wynik jest zaszumiony (łapie też słowa z list i stringów); służy jako punkt startowy.
import os,re,sys,collections
root=sys.argv[1]
TCL83=set("""after append array binary break case catch cd clock close concat continue encoding eof error eval exec exit expr fblocked fconfigure fcopy file fileevent flush for foreach format gets glob global history if incr info interp join lappend lindex linsert list llength load lrange lreplace lsearch lsort namespace open package pid proc puts pwd read regexp regsub rename return scan seek set socket source split string subst switch tell time trace unknown unset update uplevel upvar variable vwait while else elseif then default""".split())
defined=set();used=collections.Counter();files=collections.defaultdict(set)
code=[]
for dp,_,fs in os.walk(root):
  for f in fs:
    if f.lower().endswith('.tcl'):
      t=open(os.path.join(dp,f),encoding='cp1250',errors='replace').read()
      code.append((os.path.relpath(os.path.join(dp,f),root),t))
for p,t in code:
  for m in re.finditer(r'^\s*(?:proc|method)\s+([\w:]+)',t,re.M): defined.add(m.group(1))
  for m in re.finditer(r'^\s*def_class\s+(\w+)',t,re.M): defined.add(m.group(1))
for p,t in code:
  t=re.sub(r'^\s*(//|#|;#).*$','',t,flags=re.M)
  for m in re.finditer(r'(?:^|[\[;{])\s*([a-z_][A-Za-z0-9_]*)\b',t,re.M):
    w=m.group(1)
    if w in TCL83 or w in defined: continue
    used[w]+=1; files[w].add(p.split('/')[0])
nat=[(w,c) for w,c in used.most_common() if c>=2]
print(len(code),'files; defined procs/methods:',len(defined),'; candidate natives (>=2 uses):',len(nat))
for w,c in nat: print(f'{w} {c}')
