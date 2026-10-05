#!/usr/bin/env bash
# Przeglądarka szablonów map w oryginalnej grze: gra z laboratorium w WIDOCZNYM oknie (Wine Desktop 1024×768)
# z serwerem WebTraq; szablony podmieniane w działającej grze w ok. 2 s, bez restartu (BAZA_WIEDZY §8.6).
#
# Użycie:
#   tools/lab/live2.sh start [szablon]     uruchom grę w oknie i pokaż szablon (domyślnie pierwszy z listy)
#   tools/lab/live2.sh show <szablon>      pokaż szablon (nazwa pliku z Data/Templates bez .tcl)
#   tools/lab/live2.sh next [filtr]        następny szablon z listy; filtr (np. lava_unq) zawęża listę i zostaje zapamiętany
#   tools/lab/live2.sh prev [filtr]        poprzedni szablon
#   tools/lab/live2.sh list [filtr]        lista szablonów (bieżący oznaczony *)
#   tools/lab/live2.sh view X Y [ZOOM DX DY]   kamera jak set_view, współrzędne względem lewego górnego rogu szablonu
#   tools/lab/live2.sh tcl 'skrypt'        Tcl w działającej grze (jak live.sh; bez znaku ":" w skrypcie)
#   tools/lab/live2.sh shot out.png        zrzut okna gry
#   tools/lab/live2.sh stop                zamknij grę
#
# Jak to działa: userstartup.tcl definiuje w grze procedurę ow_show. Ta usuwa obiekty Tcl poprzedniego
# szablonu, czyści mapę (map create), wstawia szablon w blok (16,16), odsłania go prostokątem FogRemover
# (set_fogofwar z ujemnym zasięgiem = prostokąt przez skałę, rozdział 10.4), stawia siatkę lamp co 5 bloków
# (bez lamp jaskinie są czarne), obiekt mgły świata Info_Fog_<świat> i kamerę na środek szablonu.
# Wymiar szablonu skrypt czyta z pliku .pmp (pierwsze 8 bajtów: szerokość i wysokość, u32).
# Gra rusza w zoom 1,0–2,5 (rozdział 6.1). W grze nie przesuwaj kamery myszą zbyt daleko: poza szablonem jest pusta mapa.
# Nie działa równocześnie z live.sh (ten sam port WebTraq 8082).
# Wymaga: laboratorium (setup_lab.sh), zamontowana płyta, curl, ImageMagick, xdotool (zrzut okna).
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
LAB="$(cd "$HERE/../../../lab" && pwd)"
export WINEPREFIX="$LAB/prefix" WINEDEBUG=-all
STATE="$LAB/live2.state"
TPL="$LAB/Wiggles/Data/Templates"
KEY='HKCU\Software\SekOst\Wiggles\Performance'
URL=http://127.0.0.1:8082

decode() { sed -e 's/<br>/\n/g; s/<[^>]*>//g; s/&nbsp;/ /g; s/&lt;/</g; s/&gt;/>/g; s/&quot;/"/g; s/&amp;/\&/g'; }
tcl() {
	curl -s -m 60 -G --data-urlencode "script=$1" "$URL/tclshell" | decode |
		awk '/^Result:/{p=1; next} p && /^$/ {if (n) exit; next} p {print; n++}'
}
templates() { # nazwy szablonów (bez .tcl), opcjonalnie z filtrem
	find "$TPL" -maxdepth 1 -iname '*.tcl' -printf '%f\n' | sed 's/\.[tT][cC][lL]$//' | sort -f | grep -i -- "${1:-}" || true
}
state_get() { [ -f "$STATE" ] && grep "^$1=" "$STATE" | head -1 | cut -d= -f2-; }
state_set() { grep -v "^$1=" "$STATE" 2>/dev/null > "$STATE.tmp"; echo "$1=$2" >> "$STATE.tmp"; mv "$STATE.tmp" "$STATE"; }

# Wymiar szablonu z .pmp podanego w pierwszym map_template pliku .tcl: "szer wys" (bloki).
tsize() {
	local f p pmp
	f="$(find "$TPL" -maxdepth 1 -iname "$1.tcl" | head -1)"
	[ -n "$f" ] || { echo "brak szablonu $1" >&2; return 1; }
	p="$(grep -o -i 'map_template data/templates/[^ ]*\.pmp' "$f" | head -1 | sed 's#.*/##')"
	pmp="$(find "$TPL" -maxdepth 1 -iname "$p" | head -1)"
	[ -n "$pmp" ] || { echo "brak pliku $p" >&2; return 1; }
	od -An -tu4 -N8 "$pmp" | awk '{print $1, $2}'
}

show() {
	local T="$1" w h fog
	read -r w h < <(tsize "$T") || return 1
	case "$(echo "${T%%_*}" | tr A-Z a-z)" in kris) fog=Info_Fog_Kris_d ;; lava) fog=Info_Fog_Lava_c ;; swf) fog=Info_Fog_Swf_a ;; *) fog=Info_Fog_Urw_d ;; esac
	echo "$T  (${w}×${h} bloków, mgła świata $fog)"
	tcl "ow_show $T $w $h $fog" | sed 's/^/  /'
	state_set cur "$T"
}

USERSTARTUP='# wygenerowane przez tools/lab/live2.sh
proc ow_show {T w h fog} {
	global ow_objs
	set log {}
	# obiekty Tcl poprzedniego szablonu (dekoracje statyczne znikają razem z mapą)
	foreach o [obj_query 0 -limit 100000] { catch { del $o } }
	catch { map create 128 96 } r
	set code [catch { call templates/$T.tcl ; MapTemplateSet 16 16 } r]
	lappend log "szablon: [expr {$code ? "BLAD $r" : "OK"}]"
	set cx [expr {16 + $w / 2.0}]
	set cy [expr {16 + $h / 2.0}]
	set box [expr {-(($w > $h ? $w : $h) / 2 + 10)}]
	catch { sel /obj ; set FR [new FogRemover] ; set_pos $FR "$cx $cy 10" ; call_method $FR fog_remove 0 $box $box } r
	set n 0
	for {set y 16} {$y <= 16 + $h} {incr y 5} {
		for {set x 16} {$x <= 16 + $w} {incr x 5} {
			if {[catch { sel /obj ; new Misc_LightSrc_a "" "[expr {$x + 1}] [expr {$y + 1}] 14" "0 0 0" }] == 0} { incr n }
		}
	}
	lappend log "lampy: $n"
	catch { sel /obj ; new $fog "" "$cx $cy 0.02" "0 0 0" } r
	set z [expr {$w > 24 ? 2.2 : ($w > 14 ? 1.8 : 1.5)}]
	set_view $cx $cy $z -0.25 0.1
	return [join $log ", "]
}
proc ow_view {x y z dx dy} { set_view [expr {16 + $x}] [expr {16 + $y}] $z $dx $dy }
'

case "${1:-}" in
	start)
		findmnt -n /dev/sr0 >/dev/null || { echo "płyta nie jest zamontowana: udisksctl mount -b /dev/sr0"; exit 1; }
		curl -s -m 2 -o /dev/null "$URL/index" && { echo "gra już działa (port 8082); najpierw: $0 stop albo tools/lab/live.sh stop"; exit 1; }
		T="${2:-$(state_get cur)}"; [ -n "$T" ] || T="$(templates | head -1)"
		US="$LAB/Wiggles/Data/userstartup.tcl"
		printf '%s' "$USERSTARTUP" > "$US"
		wine reg add "$KEY" /v WebServer /d 1 /f >/dev/null; wineserver -w
		cd "$LAB/Wiggles"
		setsid wine explorer /desktop=OWLab2,1024x768 'C:\Wiggles\Wiggles.exe' ${OW_ARGS:-} >/dev/null 2>&1 &
		echo -n "czekam na grę"
		for i in $(seq 1 60); do
			sleep 2; echo -n .
			if curl -s -m 3 -o /dev/null "$URL/index"; then echo; sleep 3; show "$T"; exit 0; fi
		done
		echo; echo "serwer WebTraq nie odpowiedział w 120 s"; exit 1 ;;
	show) show "${2:?nazwa szablonu}" ;;
	next|prev)
		[ $# -ge 2 ] && state_set filter "$2"
		F="$(state_get filter)"; mapfile -t L < <(templates "$F")
		[ ${#L[@]} -gt 0 ] || { echo "brak szablonów dla filtra '$F'"; exit 1; }
		cur="$(state_get cur)"; i=-1
		for k in "${!L[@]}"; do [ "${L[$k]}" = "$cur" ] && i=$k; done
		if [ "$1" = next ]; then i=$(( (i + 1) % ${#L[@]} )); else i=$(( (i - 1 + ${#L[@]}) % ${#L[@]} )); fi
		echo "[$((i + 1))/${#L[@]}${F:+, filtr $F}]"
		show "${L[$i]}" ;;
	list)
		cur="$(state_get cur)"
		templates "${2:-}" | while read -r t; do [ "$t" = "$cur" ] && echo "* $t" || echo "  $t"; done ;;
	view) tcl "ow_view ${2:?x} ${3:?y} ${4:-1.6} ${5:--0.25} ${6:-0.1}" ;;
	tcl) S="${2:?skrypt albo -}"; [ "$S" = - ] && S="$(cat)"; tcl "$S" ;;
	shot)
		# tylko okno gry (nigdy cały ekran użytkownika)
		W="$(xdotool search --name '^OWLab2' 2>/dev/null | head -1)"
		[ -n "$W" ] || { echo "nie znaleziono okna OWLab2"; exit 1; }
		xwininfo -id "$W" | grep -q IsViewable || { echo "okno gry jest niewidoczne (inny pulpit albo zminimalizowane): pokaż je i powtórz"; exit 1; }
		# menedżer okien może rozciągnąć okno; gra rysuje 1024×768 w jego lewym górnym rogu
		import -window "$W" -crop 1024x768+0+0 +repage "${2:?plik.png}" && echo "zapisano $2" ;;
	stop)
		wineserver -k 2>/dev/null
		rm -f "$LAB/Wiggles/Data/userstartup.tcl"
		wine reg add "$KEY" /v WebServer /d 0 /f >/dev/null; wineserver -w
		echo "zatrzymano (ostatni szablon: $(state_get cur))" ;;
	*) sed -n '2,25p' "$0"; exit 2 ;;
esac
