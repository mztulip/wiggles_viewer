#!/usr/bin/env bash
# Uruchamia grę z laboratorium. Wyniki sondy: lab/Wiggles/owprobe_*.txt, błędy Tcl: lab/Wiggles/tclerrors.txt
# Użycie: tools/lab/run_lab.sh [sekundy=90] [dodatkowe parametry exe, np. -console -nosound]
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
LAB="$(cd "$HERE/../../../lab" && pwd)"
SECS="${1:-90}"; shift || true
export WINEPREFIX="$LAB/prefix" WINEDEBUG=-all
cd "$LAB/Wiggles"
rm -f owprobe_*.txt tclerrors.txt
timeout "$SECS" wine Wiggles.exe -logerrors "$@" > "$LAB/run.log" 2>&1
echo "kod wyjścia: $? (124 = zamknięte przez timeout)"
ls -la owprobe_*.txt tclerrors.txt 2>/dev/null
