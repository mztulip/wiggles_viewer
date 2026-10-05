#!/usr/bin/env bash
# Zrzut ekranu z gry Wiggles uruchomionej w WIDOCZNYM oknie Wine.
#
# Użycie:
#   tools/lab/capture2.sh menu out.png [sekundy=45]
#       menu startowe gry
#
#   tools/lab/capture2.sh template out.png <szablon> [x y zoom kąt1 kąt2] [sekundy=60]
#       pusta mapa z jednym szablonem w bloku (16,16)
#
#   tools/lab/capture2.sh scene out.png <plik.tcl> [sekundy=60]
#       własny skrypt jako data/userstartup.tcl
#
# Zmienna OW_ARGS:
#   dodatkowe parametry exe, np.:
#     OW_ARGS="-console -nosound"
#
# Wymaga:
#   laboratorium (setup_lab.sh)
#   Wine
#   ImageMagick (import)
#
# Gra jest uruchamiana w widocznym Wine Desktop 1024x768.
# Nie używamy Xvfb.

set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
LAB="$(cd "$HERE/../../../lab" && pwd)"

MODE="${1:?tryb: menu|template|scene}"
OUT="$(realpath -m "${2:?plik wyjściowy}")"
shift 2

export WINEPREFIX="$LAB/prefix"
export WINEDEBUG=-all

US="$LAB/Wiggles/Data/userstartup.tcl"

rm -f "$US"

WAIT=60

case "$MODE" in

	menu)
		WAIT="${1:-30}"
		;;

	template)
		T="${1:?nazwa szablonu}"

		case "${T%%_*}" in
			kris) FOG=Info_Fog_Kris_d ;;
			lava) FOG=Info_Fog_Lava_c ;;
			swf)  FOG=Info_Fog_Swf_a ;;
			*)    FOG=Info_Fog_Urw_d ;;
		esac

		X="${2:-}"
		Y="${3:-}"
		Z="${4:-1.38}"
		A1="${5:--0.355}"
		A2="${6:-0.165}"
		WAIT="${7:-60}"

		cat > "$US" <<TCL
# wygenerowane przez tools/lab/capture.sh

set f [open "owcapture.txt" w]

set code [catch {
	call templates/$T.tcl
	MapTemplateSet 16 16
} r]

puts \$f "szablon $T code=\$code \$r"

catch {
	sel /obj

	set FR [new FogRemover]
	set_pos \$FR {${X:-18} ${Y:-18} 10}

	call_method \$FR fog_remove 0 -50 -50
    

	;# bez timer_delete:
	;# usunięty FogRemover przywraca mgłę wojny
} r

puts \$f "fog: \$r"

# Bez świateł jaskinie w grze są czarne:
# dokładamy źródła światła.
foreach {dx dy} {
	0 0
	-3 -1
	3 -1
	-3 2
	3 2
} {
	catch {
		sel /obj

		set L [new Misc_LightSrc_a "" \
			"[expr {${X:-18} + \$dx}] [expr {${Y:-18} + \$dy}] 14" \
			"0 0 0"]
	} r
}

puts \$f "światła: \$r"

# Mgła i otoczenie świata.
catch {
	sel /obj
	new $FOG "" "${X:-18} ${Y:-18} 0.02" "0 0 0"
} r

puts \$f "mgła $FOG: \$r"

close \$f
TCL

		# Ustawienie kamery.
		echo "set_view ${X:-18} ${Y:-18} $Z $A1 $A2" >> "$US"
		;;

	scene)
		cp "${1:?plik sceny}" "$US"
		WAIT="${2:-10}"
		;;

	*)
		echo "nieznany tryb $MODE"
		exit 2
		;;
esac


###############################################################################
# URUCHOMIENIE GRY
###############################################################################

cd "$LAB/Wiggles"

echo "Uruchamiam Wiggles w widocznym Wine Desktop 1024x768..."
echo "Screenshot zostanie zapisany po ${WAIT}s do:"
echo "  $OUT"

# Czyścimy ewentualny poprzedni wineserver.
wineserver -k 2>/dev/null || true
sleep 1

# Uruchamiamy Wine Desktop.
#
# WAŻNE:
# Nie używamy tutaj Xvfb.
# Wine korzysta z normalnego DISPLAY użytkownika.
#
# Okno Wine Desktop ma 1024x768 i powinno być normalnie widoczne.
wine explorer /desktop=OWLab,1024x768 \
	'C:\Wiggles\Wiggles.exe' \
	${OW_ARGS:-} &

GP=$!

# Dajemy Wine/grze chwilę na start.
sleep 2

# Czekamy na ekran, który chcemy przechwycić.
sleep "$WAIT"


###############################################################################
# SCREENSHOT
###############################################################################

echo "Robię screenshot..."

WIN_ID=""

for i in {1..20}; do
	WIN_ID="$(
		xdotool search --onlyvisible --name 'OWLab' 2>/dev/null |
		head -n1 ||
		true
	)"

	if [ -n "$WIN_ID" ]; then
		break
	fi

	sleep 0.5
done

if [ -z "$WIN_ID" ]; then
	echo "BŁĄD: nie znaleziono widocznego okna OWLab"
	exit 1
fi

echo "Screenshot okna OWLab: $WIN_ID"

import -window "$WIN_ID" "$OUT" 2>/dev/null \
	&& echo "zapisano $OUT" \
	|| echo "BŁĄD zrzutu"



###############################################################################
# SPRZĄTANIE
###############################################################################

echo "Zatrzymuję Wiggles..."

kill "$GP" 2>/dev/null || true
wait "$GP" 2>/dev/null || true

wineserver -k 2>/dev/null || true

rm -f "$US"

if [ -f owcapture.txt ]; then
	cat owcapture.txt
	rm -f owcapture.txt
fi

true