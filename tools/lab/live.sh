#!/usr/bin/env bash
# Gra z laboratorium działająca w tle (niewidoczny Xvfb) z wbudowanym serwerem WebTraq twórców,
# przez który wykonujemy Tcl w działającej grze bez restartów (BAZA_WIEDZY §8.6).
#
# Użycie:
#   tools/lab/live.sh start [scena.tcl]   uruchom grę; scena.tcl staje się data/userstartup.tcl
#                                         (bez sceny: menu startowe); czeka, aż serwer odpowie
#   tools/lab/live.sh tcl 'skrypt'        wykonaj Tcl w głównym interpreterze gry, wypisz wynik
#   tools/lab/live.sh tcl - < plik.tcl    to samo ze standardowego wejścia
#                                         UWAGA: WebTraq ucina skrypt na pierwszym znaku ":", więc bez "::"
#                                         (skrypt i tak działa na poziomie globalnym; zmienne bez przedrostka)
#   tools/lab/live.sh page /objlist       surowa strona WebTraq (np. /objinfo:<nazwa>, /classinfo:<klasa>)
#   tools/lab/live.sh shot out.png        zrzut ekranu gry
#   tools/lab/live.sh stop                zamknij grę i Xvfb
#
# WebTraq włącza wartość rejestru Performance\WebServer=1 (port 8082); start ją ustawia, stop przywraca 0.
# Wymaga: laboratorium (setup_lab.sh), zamontowana płyta (udisksctl mount -b /dev/sr0), Xvfb, curl, ImageMagick.
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
LAB="$(cd "$HERE/../../../lab" && pwd)"
export WINEPREFIX="$LAB/prefix" WINEDEBUG=-all
STATE="$LAB/live.state"
KEY='HKCU\Software\SekOst\Wiggles\Performance'
URL=http://127.0.0.1:8082

decode() { sed -e 's/<br>/\n/g; s/<[^>]*>//g; s/&nbsp;/ /g; s/&lt;/</g; s/&gt;/>/g; s/&quot;/"/g; s/&amp;/\&/g'; }

case "${1:-}" in
	start)
		[ -f "$STATE" ] && { echo "gra już działa ($STATE); najpierw: $0 stop"; exit 1; }
		findmnt -n /dev/sr0 >/dev/null || { echo "płyta nie jest zamontowana: udisksctl mount -b /dev/sr0"; exit 1; }
		US="$LAB/Wiggles/Data/userstartup.tcl"
		rm -f "$US"
		[ -n "${2:-}" ] && cp "$2" "$US"
		wine reg add "$KEY" /v WebServer /d 1 /f >/dev/null; wineserver -w
		D=77; while [ -e "/tmp/.X$D-lock" ]; do D=$((D+1)); done
		Xvfb ":$D" -screen 0 1024x768x24 +extension GLX >/dev/null 2>&1 & XP=$!
		sleep 2
		cd "$LAB/Wiggles"
		DISPLAY=":$D" setsid wine explorer /desktop=OWLab,1024x768 'C:\Wiggles\Wiggles.exe' ${OW_ARGS:-} >/dev/null 2>&1 &
		echo "D=$D XP=$XP" > "$STATE"
		for i in $(seq 1 60); do
			sleep 2
			curl -s -m 3 -o /dev/null "$URL/index" && { echo "gra działa (ekran :$D, WebTraq $URL)"; exit 0; }
		done
		echo "serwer WebTraq nie odpowiedział w 120 s"; exit 1 ;;
	tcl)
		S="${2:?skrypt albo -}"; [ "$S" = - ] && S="$(cat)"
		# Strona: "Script: … Result: <pusta> Ok.|Error: <wynik> <pusta> <echo skryptu>"; wypisujemy status i wynik.
		curl -s -m 60 -G --data-urlencode "script=$S" "$URL/tclshell" | decode |
			awk '/^Result:/{p=1; next} p && /^$/ {if (n) exit; next} p {print; n++}' ;;
	page)
		curl -s -m 30 "$URL${2:-/index}" | decode | sed '/^$/d' ;;
	shot)
		. "$STATE"; DISPLAY=":$D" import -window root "${2:?plik.png}" && echo "zapisano $2" ;;
	stop)
		[ -f "$STATE" ] && . "$STATE"
		wineserver -k 2>/dev/null
		[ -n "${XP:-}" ] && kill "$XP" 2>/dev/null
		rm -f "$STATE" "$LAB/Wiggles/Data/userstartup.tcl"
		wine reg add "$KEY" /v WebServer /d 0 /f >/dev/null; wineserver -w
		echo "zatrzymano" ;;
	*) sed -n '2,16p' "$0"; exit 2 ;;
esac
