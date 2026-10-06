#!/usr/bin/env bash
# Testy regresji OpenWiggles: czy to, co już działa, dalej działa.
#
# Użycie (z katalogu repozytorium):
#   tools/tests/run.sh                 wszystkie testy (budowanie release przed startem)
#   tools/tests/run.sh sim             tylko testy, których nazwa zawiera „sim”
#   tools/tests/run.sh --no-ui         bez testów interakcji (Xvfb + xdotool)
#   tools/tests/run.sh --update-refs   zapisz nowe obrazy wzorcowe (po zamierzonej zmianie wyglądu)
#   tools/tests/run.sh --list          lista testów
#
# Rodzaje testów:
#   - bez okna: test3db, owscript --eval, dokumentacja i tłumaczenia;
#   - scena i symulacja: wigview --screenshot z --print-objects i śladem OW_TRACE_SIM=2;
#   - obrazy wzorcowe: zrzut porównany z ../lab/testref/<nazwa>.png (poza repo: obrazy z danych gry nie
#     mogą trafić do repozytorium; brak wzorca = test zapisuje go i przechodzi);
#   - interakcja: wigview w niewidocznym Xvfb sterowany xdotool (klik, klawisze).
# Gra: WIGGLES_DIR albo laboratorium ../lab/Wiggles (tam są też szablony i zapisy testowe).
# Opis i zasady dopisywania testów: tools/tests/README.md.
set -uo pipefail
cd "$(dirname "$0")/../.."
REPO="$PWD"
GAME="${WIGGLES_DIR:-$(cd ../lab/Wiggles 2>/dev/null && pwd)}"
[ -d "$GAME/Data" ] || { echo "brak katalogu gry (WIGGLES_DIR albo ../lab/Wiggles)"; exit 2; }
REFS="${OW_TESTREF:-$(cd .. && pwd)/lab/testref}"
B="$REPO/build/release"
TMP="$(mktemp -d /tmp/owtest.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

FILTER="" UI=1 UPDATE=0 LIST=0
for a in "$@"; do
	case "$a" in
		--no-ui) UI=0 ;;
		--update-refs) UPDATE=1 ;;
		--list) LIST=1 ;;
		*) FILTER="$a" ;;
	esac
done

PASS=0 FAIL=0 SKIP=0 FAILED=()
# test NAZWA OPIS: zarejestrowana funkcja t_NAZWA; wynik przez ok / fail / skip
TESTS=()
test_() { TESTS+=("$1|$2"); }
ok() { echo "  ok    $CUR${1:+  ($1)}"; PASS=$((PASS + 1)); }
fail() { echo "  BŁĄD  $CUR: $1"; FAIL=$((FAIL + 1)); FAILED+=("$CUR"); }
skip() { echo "  pomin $CUR: $1"; SKIP=$((SKIP + 1)); }
wv() { "$B/wigview" --game "$GAME" --size 640x480 "$@" 2>&1; } # wigview bez okna (zrzut do pliku)

# ---------------------------------------------------------------------------------------------------
test_ docs "dokumentacja: odnośniki (check_docs.py) i komplet tłumaczeń (i18n_update.py --check)"
t_docs() {
	python3 tools/check_docs.py >"$TMP/docs" 2>&1 || { fail "check_docs: $(tail -3 "$TMP/docs")"; return; }
	python3 tools/i18n_update.py --check >"$TMP/i18n" 2>&1 || { fail "brak tłumaczeń: $(grep brakuje "$TMP/i18n")"; return; }
	ok
}

test_ test3db "parsery formatów na wszystkich danych gry (test3db)"
t_test3db() {
	"$B/test3db" "$GAME" >"$TMP/t3" 2>&1 && ok "$(tail -1 "$TMP/t3")" || fail "$(tail -3 "$TMP/t3")"
}

test_ script_clock "Tcl: gametime, gethours, okno pracy, hceil „6.0” (rozdział 11.4)"
t_script_clock() {
	local r
	r=$("$B/owscript" --game "$GAME" Zwerg 5 -o /dev/null \
		--eval 'list [gethours] [get_worktime 1] [get_remaining_sparetime 1] [hceil 5.2] [hfloor 2.7] [hf2i 3.9]' \
		--eval 'gametime factor 2 ; gametime factor' \
		--eval 'set_worktime 1 6 6 ; list [get_worktime 1 start] [get_worktime 1 end] [get_remaining_sparetime 1]' 2>&1)
	echo "$r" >"$TMP/clock"
	grep -q -- '-> 0.03.* {0 6} 0.0 6.0 2.0 3$' "$TMP/clock" || { fail "zegar/okno pracy: $(grep '^eval list \[gethours' "$TMP/clock")"; return; }
	grep -q 'gametime factor -> 2$' "$TMP/clock" || { fail "gametime factor"; return; }
	# okno 6–12 h, godzina ok. 0,03: czas wolny, do pracy 12 − (0,03 − 6 + 12) ≈ 5,97 h
	grep -q -- '-> 6 0 5.9[0-9]*$' "$TMP/clock" || { fail "set_worktime: $(grep set_worktime "$TMP/clock")"; return; }
	ok
}

test_ script_visibility "Tcl: set_visibility / get_visibility"
t_script_visibility() {
	local r
	r=$("$B/owscript" --game "$GAME" Zwerg 1 -o /dev/null --eval 'set a [get_visibility 1] ; set_visibility 1 0 ; list $a [get_visibility 1]' 2>&1)
	echo "$r" | grep -q -- '-> 1 0$' && ok || fail "$(echo "$r" | grep eval)"
}

test_ script_gnome "skrypt krasnala: 120 s bez błędów, przejście do pracy (work_idle)"
t_script_gnome() {
	"$B/owscript" --game "$GAME" Zwerg 120 -o "$TMP/ow.log" >"$TMP/ow" 2>&1
	grep -q "błędy 0" "$TMP/ow" || { fail "$(grep '^po' "$TMP/ow")"; return; }
	grep -q "stan 'work_idle'" "$TMP/ow" && ok "$(grep '^po' "$TMP/ow" | cut -c1-60)" || fail "$(grep '^po' "$TMP/ow")"
}

test_ classes "modele klas: okładziny jaskiń bez modelu, drzwi z set standanim, dekoracje Dummy_*"
t_classes() {
	wv --template kris_caves_1_b --print-objects --screenshot "$TMP/a.png" >"$TMP/c1"
	grep -q "Dummy_Kris_caves_1_b_a model=-" "$TMP/c1" || { fail "okładzina jaskini z modelem (gra jej nie tworzy)"; return; }
	wv --template lava_unq_vampy_tuer --print-objects --screenshot "$TMP/a.png" >"$TMP/c2"
	grep -q "Tuer_kaserne model=tuer_kaserne" "$TMP/c2" || { fail "drzwi bez modelu"; return; }
	grep -q "Dummy_Lava_krst_uv_l model=lava_krst_uv_l" "$TMP/c2" || { fail "dekoracja Dummy_ bez modelu"; return; }
	ok
}

# Ślad OW_TRACE_SIM=2 (co 0,5 s): żadna postać w skale (min ≥ 14,5) ani za ścianą (z < min − 1).
check_trace() { # plik → komunikat błędu albo pusto
	python3 - "$1" <<'EOF'
import re, sys
bad, n, kinds = [], 0, set()
for l in open(sys.argv[1], errors="replace"):
    m = re.search(r"\[sim ([\d.]+)\] (\S+) x ([\d.-]+) .* z ([\d.-]+) \(min ([\d.-]+)\)", l)
    if not m: continue
    n += 1; kinds.add(m.group(2))
    t, z, mn = float(m.group(1)), float(m.group(4)), float(m.group(5))
    if mn >= 14.5 or z < mn - 1.0: bad.append(l.strip())
if n == 0: print("brak śladu postaci")
elif bad: print(f"{len(bad)}/{n} próbek w skale albo za ścianą, np. {bad[0]}")
print("RODZAJE " + " ".join(sorted(kinds)), file=sys.stderr)
EOF
}

test_ sim_walls "symulacja: postacie przy krawędziach jaskini nie wchodzą w skałę ani za ścianę (kris_caves_1_b)"
t_sim_walls() {
	OW_TRACE_SIM=2 wv --template kris_caves_1_b --simulate --populate 3 1 --warmup 60 --screenshot "$TMP/a.png" >"$TMP/s1"
	local e
	e=$(check_trace "$TMP/s1" 2>"$TMP/s1k")
	[ -z "$e" ] || { fail "$e"; return; }
	grep -q "wuker" "$TMP/s1k" || { fail "brak wukera w symulacji"; return; }
	ok "$(grep -c '\[sim [0-9]' "$TMP/s1") próbek"
}

test_ sim_corridor "symulacja: krasnale i wuker w płytkim korytarzu przed tylną ścianą, wuker z modelem (lava_unq_vampy_tuer)"
t_sim_corridor() {
	OW_TRACE_SIM=2 wv --template lava_unq_vampy_tuer --simulate --populate 3 1 --warmup 20 --print-objects --screenshot "$TMP/a.png" >"$TMP/s2"
	local e
	e=$(check_trace "$TMP/s2" 2>/dev/null)
	[ -z "$e" ] || { fail "$e"; return; }
	[ "$(grep -c '^postać' "$TMP/s2")" = 4 ] || { fail "postaci: $(grep -c '^postać' "$TMP/s2"), oczekiwane 4"; return; }
	grep -q "Wuker model=wuker" "$TMP/s2" || { fail "wuker bez modelu"; return; }
	grep '^postać' "$TMP/s2" | awk '{split($7,a,"="); split($8,b,"="); if (a[2] < b[2] - 0.6) bad=1} END {exit bad}' ||
		{ fail "postać za ścianą: $(grep '^postać' "$TMP/s2" | head -2)"; return; }
	ok
}

test_ sim_gender "zapis kampanii: kobiety modelem frau, mężczyźni mann"
t_sim_gender() {
	[ -f "$GAME/Data/Gamesave/save_kampania.sav" ] || { skip "brak save_kampania.sav"; return; }
	wv --save save_kampania.sav --simulate --warmup 1 --print-objects --screenshot "$TMP/a.png" >"$TMP/g"
	local f m
	f=$(grep -c "Zwerg model=frau" "$TMP/g"); m=$(grep -c "Zwerg model=mann" "$TMP/g")
	[ "$f" -ge 1 ] && [ "$m" -ge 1 ] && ok "frau $f, mann $m" || fail "frau $f, mann $m"
}

test_ lab_tools "narzędzie laboratorium set_link_flags.py --list (bez zmian w plikach)"
t_lab_tools() {
	[ -f "$GAME/Data/3db/LAVAWELT.3DB" ] || { skip "brak LAVAWELT.3DB"; return; }
	tools/lab/set_link_flags.py LAVAWELT.3DB --list lava_krst_ov_i 2>&1 | grep -q "8:lava_seulen_128" && ok ||
		fail "tryb 8 kolumn lawy nie odczytany (plik podmieniony w laboratorium? --restore)"
}

# ---------------------------------------------------------------------------------------------------
# Obrazy wzorcowe: RMSE (0..255) pomniejszonych obrazów; próg z marginesem na różnice sterowników.
compare_ref() { # nazwa plik_zrzutu
	local ref="$REFS/$1.png"
	if [ "$UPDATE" = 1 ] || [ ! -f "$ref" ]; then
		mkdir -p "$REFS" && cp "$2" "$ref"
		ok "zapisano wzorzec $ref"
		return
	fi
	local r
	r=$(python3 - "$ref" "$2" <<'EOF'
import sys
from PIL import Image, ImageChops, ImageStat
a = Image.open(sys.argv[1]).convert("RGB").resize((160, 120))
b = Image.open(sys.argv[2]).convert("RGB").resize((160, 120))
print("%.1f" % (sum(ImageStat.Stat(ImageChops.difference(a, b)).rms) / 3))
EOF
)
	if python3 -c "import sys; sys.exit(0 if float('$r') <= ${OW_REF_TOL:-2} else 1)"; then ok "RMSE $r"; else
		cp "$2" "$TMP/../owtest_$1.png" 2>/dev/null
		fail "RMSE $r > ${OW_REF_TOL:-2} (zrzut: /tmp/owtest_$1.png, wzorzec: $ref; zamierzona zmiana: --update-refs)"
	fi
}
test_ ref_lava "obraz: lava_unq_vampy_tuer (tryby rysowania 8/3, detale lawy, drzwi, lampy laboratorium)"
t_ref_lava() {
	"$B/wigview" --game "$GAME" --template lava_unq_vampy_tuer --lab-lamps --view 8 2 1.8 -0.25 0.1 --size 1024x768 --screenshot "$TMP/r.png" >/dev/null 2>&1
	compare_ref lava "$TMP/r.png"
}
test_ ref_cave "obraz: kris_caves_1_b (bez okładzin jaskini)"
t_ref_cave() {
	"$B/wigview" --game "$GAME" --template kris_caves_1_b --lab-lamps --view 6 4 1.5 -0.25 0.1 --size 1024x768 --screenshot "$TMP/r.png" >/dev/null 2>&1
	compare_ref cave "$TMP/r.png"
}
test_ ref_hats "obraz: krasnale z zapisu z bliska (czapki na głowach, frau)"
t_ref_hats() {
	[ -f "$GAME/Data/Gamesave/save_kampania.sav" ] || { skip "brak save_kampania.sav"; return; }
	"$B/wigview" --game "$GAME" --save save_kampania.sav --view 245 29.8 1.0 0 0 --size 1024x768 --screenshot "$TMP/r.png" >/dev/null 2>&1
	compare_ref hats "$TMP/r.png"
}

# ---------------------------------------------------------------------------------------------------
# Interakcja: wigview w niewidocznym Xvfb (OpenGL), xdotool. Mysz: ruch, pauza, klik (natychmiastowy klik
# ImGui może przypisać panelowi z poprzedniej klatki).
UIPID="" XPID="" DISP=""
ui_start() { # argumenty wigview → uruchomiony, okno gotowe
	DISP=":$((90 + RANDOM % 9))"
	while [ -e "/tmp/.X${DISP#:}-lock" ]; do DISP=":$((90 + RANDOM % 9))"; done
	Xvfb "$DISP" -screen 0 1280x800x24 +extension GLX >/dev/null 2>&1 & XPID=$!
	sleep 2
	DISPLAY="$DISP" OW_TRACE_SIM=2 "$B/wigview" --game "$GAME" --renderer opengl --size 1280x800 "$@" >"$TMP/ui.log" 2>&1 & UIPID=$!
	for i in $(seq 1 60); do grep -q '\[sim [0-9]' "$TMP/ui.log" && break; sleep 1; done
	sleep 2
}
ui_stop() { kill "$UIPID" 2>/dev/null; wait "$UIPID" 2>/dev/null; kill "$XPID" 2>/dev/null; wait "$XPID" 2>/dev/null; UIPID="" XPID=""; }
xd() { DISPLAY="$DISP" xdotool "$@"; }
click_at() { xd mousemove "$1" "$2"; sleep 0.5; xd click "${3:-1}"; sleep 0.5; }
last_x() { grep "\[sim [0-9].*krasnal" "$TMP/ui.log" | awk -v n="$1" '{c++; if (c % 2 == n % 2) v=$5} END {print v}'; }
simtime() { grep -o '\[sim [0-9.]*' "$TMP/ui.log" | tail -1 | cut -c6-; }
ui_ok() {
	[ "$UI" = 1 ] || { skip "--no-ui"; return 1; }
	command -v Xvfb >/dev/null && command -v xdotool >/dev/null || { skip "brak Xvfb albo xdotool"; return 1; }
}

test_ ui_control "interakcja: klik w krasnala przejmuje go, → przesuwa o pola, ↑ nie przełącza szablonu, Shift puszcza"
t_ui_control() {
	ui_ok || return
	ui_start --template kris_caves_1_b --simulate --populate 2 0
	click_at 80 190                # lista szablonów ma fokus (jak u użytkownika)
	sleep 3
	click_at 895 490               # krasnal na polu 5 (kadr startowy wigview 1280×800)
	grep -q "\[ui\] sterowanie: postać" "$TMP/ui.log" || { ui_stop; fail "klik w krasnala nie przejął sterowania"; return; }
	local loads x0 x1
	loads=$(grep -c "wczytano mapę" "$TMP/ui.log")
	sleep 1; x0=$(grep "\[sim [0-9].*krasnal" "$TMP/ui.log" | tail -2 | awk '{print $5}' | sort -n | head -1)
	for i in 1 2 3; do xd key Right; sleep 0.4; done
	sleep 3
	xd key Up; sleep 1
	[ "$(grep -c "wczytano mapę" "$TMP/ui.log")" = "$loads" ] || { ui_stop; fail "↑ przełączyła szablon zamiast prowadzić krasnala"; return; }
	x1=$(grep "\[sim [0-9].*krasnal" "$TMP/ui.log" | tail -2 | awk '{print $5}' | sort -n | tail -1)
	xd key shift; sleep 1
	grep -q "\[ui\] sterowanie: puszczone" "$TMP/ui.log" || { ui_stop; fail "Shift nie puścił postaci"; return; }
	ui_stop
	python3 -c "import sys; sys.exit(0 if float('$x1') - float('$x0') >= 2.5 else 1)" && ok "x $x0 → $x1" || fail "krasnal nie przeszedł 3 pól: x $x0 → $x1"
}

test_ ui_keys "interakcja: ←/→ bez sterowania nie zatrzymują symulacji, Esc nie zamyka, Q zamyka"
t_ui_keys() {
	ui_ok || return
	ui_start --template kris_caves_1_b --simulate --populate 1 0
	xd mousemove 900 300; sleep 0.5
	local t0 t1
	xd key Right Left; t0=$(simtime); sleep 3; t1=$(simtime)
	python3 -c "import sys; sys.exit(0 if float('$t1') - float('$t0') >= 2 else 1)" || { ui_stop; fail "symulacja stanęła po ←/→ (czas $t0 → $t1)"; return; }
	xd key Escape; sleep 1
	kill -0 "$UIPID" 2>/dev/null || { ui_stop; fail "Esc zamknął program"; return; }
	xd key q
	for i in $(seq 1 10); do kill -0 "$UIPID" 2>/dev/null || break; sleep 0.5; done
	if kill -0 "$UIPID" 2>/dev/null; then ui_stop; fail "Q nie zamknął programu"; else ui_stop; ok; fi
}

# ---------------------------------------------------------------------------------------------------
if [ "$LIST" = 1 ]; then
	for t in "${TESTS[@]}"; do printf '  %-18s %s\n' "${t%%|*}" "${t#*|}"; done
	exit 0
fi
echo "budowanie (release)…"
./build.sh release >"$TMP/build" 2>&1 || { tail -20 "$TMP/build"; echo "BŁĄD budowania"; exit 1; }
echo "gra: $GAME, wzorce: $REFS"
for t in "${TESTS[@]}"; do
	CUR="${t%%|*}"
	[ -n "$FILTER" ] && [[ "$CUR" != *"$FILTER"* ]] && continue
	"t_$CUR"
done
echo
echo "wynik: $PASS ok, $FAIL błędów, $SKIP pominiętych"
[ "$FAIL" = 0 ] || { printf '  nie przeszły: %s\n' "${FAILED[@]}"; exit 1; }
