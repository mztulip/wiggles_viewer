#!/usr/bin/env bash
# Zrzut ekranu z oryginalnej gry uruchomionej w niewidocznym serwerze X (Xvfb).
# Nic nie pojawia się na pulpicie użytkownika.
#
# Użycie:
#   tools/lab/capture.sh menu  out.png [sekundy=45]
#       menu startowe gry
#   tools/lab/capture.sh template out.png <szablon> [x y zoom kąt1 kąt2] [sekundy=60]
#       pusta mapa z jednym szablonem w bloku (16,16), kamera set_view na jego środek
#   tools/lab/capture.sh scene out.png <plik.tcl> [sekundy=60]
#       własny skrypt jako data/userstartup.tcl
#
# Zmienna OW_ARGS: dodatkowe parametry exe (np. OW_ARGS="-console -nosound").
# Wymaga: laboratorium (setup_lab.sh), Xvfb, ImageMagick (import).
# Gra działa tylko w trybie pełnoekranowym (okno ma 1×1 px), dlatego uruchamiamy ją
# w wirtualnym pulpicie Wine 1024×768, pełną ścieżką C:\Wiggles\Wiggles.exe.
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
LAB="$(cd "$HERE/../../../lab" && pwd)"
MODE="${1:?tryb: menu|template|scene}"; OUT="$(realpath -m "${2:?plik wyjściowy}")"; shift 2
export WINEPREFIX="$LAB/prefix" WINEDEBUG=-all
US="$LAB/Wiggles/Data/userstartup.tcl"
rm -f "$US"
WAIT=60
case "$MODE" in
	menu) WAIT="${1:-45}" ;;
	template)
		T="${1:?nazwa szablonu}"
		case "${T%%_*}" in kris) FOG=Info_Fog_Kris_d ;; lava) FOG=Info_Fog_Lava_c ;; swf) FOG=Info_Fog_Swf_a ;; *) FOG=Info_Fog_Urw_d ;; esac
		X="${2:-}"; Y="${3:-}"; Z="${4:-1.38}"; A1="${5:--0.355}"; A2="${6:-0.165}"; WAIT="${7:-60}"
		cat > "$US" <<TCL
# wygenerowane przez tools/lab/capture.sh
set f [open "owcapture.txt" w]
set code [catch { call templates/$T.tcl ; MapTemplateSet 16 16 } r]
puts \$f "szablon $T code=\$code \$r"
catch {
	sel /obj
	set FR [new FogRemover]
	set_pos \$FR {${X:-18} ${Y:-18} 10}
	call_method \$FR fog_remove 0 50 50
	;# bez timer_delete: usunięty FogRemover przywraca mgłę wojny (czarny obraz)
} r
puts \$f "fog: \$r"
# Bez świateł jaskinie w grze są czarne: dokładamy źródła światła (klasy z Scripts/classes/items/emitter.tcl)
foreach {dx dy} {0 0 -3 -1 3 -1 -3 2 3 2} {
	catch {
		sel /obj
		set L [new Misc_LightSrc_a "" "[expr {${X:-18} + \$dx}] [expr {${Y:-18} + \$dy}] 14" "0 0 0"]
	} r
}
puts \$f "światła: \$r"
# Mgła i otoczenie świata (bez tego obraz jest czarny); klasa Info_Fog_<Świat>_<wariant>
catch { sel /obj ; new $FOG "" "${X:-18} ${Y:-18} 0.02" "0 0 0" } r
puts \$f "mgła $FOG: \$r"
close \$f
TCL
		# Środek kamery: podany albo środek szablonu (z rozmiaru .PMP liczonego w grze nie znamy -> domyślnie 18,18)
		echo "set_view ${X:-18} ${Y:-18} $Z $A1 $A2" >> "$US"
		;;
	scene) cp "${1:?plik sceny}" "$US"; WAIT="${2:-60}" ;;
	*) echo "nieznany tryb $MODE"; exit 2 ;;
esac

# wolny numer ekranu
D=77; while [ -e "/tmp/.X$D-lock" ]; do D=$((D+1)); done
Xvfb ":$D" -screen 0 1024x768x24 +extension GLX >/dev/null 2>&1 & XP=$!
trap 'kill $XP 2>/dev/null' EXIT # sprzątanie także po przerwaniu (timeout)
sleep 2
cd "$LAB/Wiggles"
DISPLAY=":$D" timeout $((WAIT + 20)) wine explorer /desktop=OWLab,1024x768 'C:\Wiggles\Wiggles.exe' ${OW_ARGS:-} >/dev/null 2>&1 & GP=$!
sleep "$WAIT"
DISPLAY=":$D" import -window root "$OUT" 2>/dev/null && echo "zapisano $OUT" || echo "BŁĄD zrzutu"
kill $GP 2>/dev/null; wait $GP 2>/dev/null
DISPLAY=":$D" wineserver -k 2>/dev/null
kill $XP 2>/dev/null; wait $XP 2>/dev/null
rm -f "$US"
[ -f owcapture.txt ] && cat owcapture.txt && rm -f owcapture.txt
true
