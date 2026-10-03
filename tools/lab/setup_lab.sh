#!/usr/bin/env bash
# Tworzy "laboratorium": kopię gry w osobnym prefiksie Wine, z sondą Tcl (owprobe.tcl)
# wstrzykniętą do systeminit.tcl. Oryginalna instalacja nie jest modyfikowana.
#
# Użycie: tools/lab/setup_lab.sh [katalog_gry] [katalog_lab]
#   katalog_gry  domyślnie ~/.wine/drive_c/Program Files (x86)/Innonics/Wiggles
#   katalog_lab  domyślnie ../lab (obok repozytorium)
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
GAME="${1:-$HOME/.wine/drive_c/Program Files (x86)/Innonics/Wiggles}"
LAB="${2:-$HERE/../../../lab}"
mkdir -p "$LAB"
LAB="$(cd "$LAB" && pwd)"
export WINEPREFIX="$LAB/prefix" WINEDEBUG=-all

if [ ! -d "$LAB/Wiggles" ]; then
	echo ">> kopiuję grę do $LAB/Wiggles"
	cp -r --reflink=auto "$GAME" "$LAB/Wiggles"
fi

if [ ! -f "$WINEPREFIX/system.reg" ]; then
	echo ">> tworzę prefiks Wine"
	wineboot -i >/dev/null 2>&1
fi
ln -sfn "$LAB/Wiggles" "$WINEPREFIX/drive_c/Wiggles"

echo ">> rejestr gry (pełny ekran 1024x768 w wirtualnym pulpicie, bez filmów, WorkingDir=C:\\Wiggles)"
cat > "$LAB/wiggles_lab.reg" <<'REG'
Windows Registry Editor Version 5.00

[HKEY_CURRENT_USER\Software\SekOst\Wiggles]
"Language"="pl"
"TCLDEFINES"=""
"WorkingDir"="C:\\Wiggles"

[HKEY_CURRENT_USER\Software\SekOst\Wiggles\Setup]
"INSTDIR"="C:\\Wiggles"

[HKEY_CURRENT_USER\Software\SekOst\Wiggles\Screen]
"Fullscreen"="1"
"Width"="1024"
"Height"="768"

[HKEY_CURRENT_USER\Software\SekOst\Wiggles\Misc]
"ShowFMV"="0"
"PauseOnDeactivate"="0"
REG
wine regedit /S "$LAB/wiggles_lab.reg"
wineserver -w

echo ">> instaluję sondę"
cp "$HERE/owprobe.tcl" "$LAB/Wiggles/Data/owprobe.tcl"
SI="$LAB/Wiggles/Data/systeminit.tcl"
[ -f "$SI.orig" ] || cp "$SI" "$SI.orig"
{
	printf 'catch {source data/owprobe.tcl} owprobe_err\r\n'
	sed 's/^log "systeminit script succesfully parsed"/&\r\ncatch {owprobe_dump end}\r\ncatch {owtrace_counts}/' "$SI.orig"
} > "$SI"

echo ">> gotowe. Uruchomienie: tools/lab/run_lab.sh"
