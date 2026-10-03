#!/usr/bin/env bash
# System budowania OpenWiggles (bez CMake).
#
# Użycie: ./build.sh [debug|release] [cel...]
#   cele: lib, imgui, wigview, test3db, all (domyślnie), clean
#   zmienne: CXX (domyślnie g++), JOBS (domyślnie nproc), VERBOSE=1
#
# Kompilacja przyrostowa: plik .o jest przebudowywany, gdy jest starszy od źródła
# albo od któregokolwiek nagłówka z pliku zależności .d (generowanego przez -MMD).
set -euo pipefail
cd "$(dirname "$0")"

MODE=debug
TARGETS=()
for a in "$@"; do
	case "$a" in
		debug|release) MODE="$a" ;;
		*) TARGETS+=("$a") ;;
	esac
done
[ ${#TARGETS[@]} -eq 0 ] && TARGETS=(all)

CXX="${CXX:-g++}"
JOBS="${JOBS:-$(nproc)}"
OUT="build/$MODE"

CXXFLAGS=(-std=c++17 -Wall -Wextra -Isrc -Ithird_party/imgui -Ithird_party/imgui/backends $(pkg-config --cflags sdl2))
case "$MODE" in
	debug)   CXXFLAGS+=(-O0 -g -DOW_DEBUG) ;;
	release) CXXFLAGS+=(-O2 -DNDEBUG) ;;
esac
LIBS=($(pkg-config --libs sdl2) -ldl)

log() { printf '%s\n' "$*"; }
run() { [ "${VERBOSE:-0}" = 1 ] && log "$*"; "$@"; }

# Czy obiekt $1 wymaga przebudowy względem źródła $2?
needs_build() {
	local obj="$1" src="$2" dep="${1%.o}.d"
	[ -f "$obj" ] || return 0
	[ "$src" -nt "$obj" ] && return 0
	[ -f "$dep" ] || return 0
	local f
	# .d ma postać "obj: src hdr1 hdr2 \" itd.
	for f in $(sed -e 's/^[^:]*://' -e 's/\\$//' "$dep"); do
		[ "$f" -nt "$obj" ] && return 0
	done
	return 1
}

# compile <dodatkowe flagi...> -- <źródła...>; wypisuje listę obiektów do zmiennej OBJS
compile() {
	local extra=() srcs=() s
	while [ "$1" != "--" ]; do extra+=("$1"); shift; done; shift
	srcs=("$@")
	OBJS=()
	local pending=0 failed=0
	for s in "${srcs[@]}"; do
		local o="$OUT/obj/${s%.cpp}.o"
		OBJS+=("$o")
		if needs_build "$o" "$s"; then
			mkdir -p "$(dirname "$o")"
			log "  CXX  $s"
			run "$CXX" "${CXXFLAGS[@]}" "${extra[@]}" -MMD -MP -c "$s" -o "$o" &
			pending=$((pending + 1))
			if [ "$pending" -ge "$JOBS" ]; then wait -n || failed=1; pending=$((pending - 1)); fi
		fi
	done
	while [ "$pending" -gt 0 ]; do wait -n || failed=1; pending=$((pending - 1)); done
	[ "$failed" = 0 ] || { log "BŁĄD kompilacji"; exit 1; }
}

archive() { # archive <plik.a> <obiekty...>
	local a="$1"; shift
	local o rebuild=0
	[ -f "$a" ] || rebuild=1
	for o in "$@"; do [ "$o" -nt "$a" ] && rebuild=1; done
	if [ "$rebuild" = 1 ]; then log "  AR   $a"; rm -f "$a"; run ar rcs "$a" "$@"; fi
}

link() { # link <wyjście> <obiekty i biblioteki...>
	local out="$1"; shift
	local o rebuild=0
	[ -f "$out" ] || rebuild=1
	for o in "$@"; do [ -f "$o" ] && [ "$o" -nt "$out" ] && rebuild=1; done
	if [ "$rebuild" = 1 ]; then
		log "  LD   $out"; mkdir -p "$(dirname "$out")"
		run "$CXX" -o "$out" "$@" "${LIBS[@]}"
	fi
}

build_lib() {
	log "[libwiggles]"
	compile -- $(find src/lib -name '*.cpp' | sort)
	archive "$OUT/libwiggles.a" "${OBJS[@]}"
}

build_imgui() {
	log "[imgui]"
	compile -w -- third_party/imgui/imgui.cpp third_party/imgui/imgui_draw.cpp \
		third_party/imgui/imgui_tables.cpp third_party/imgui/imgui_widgets.cpp \
		third_party/imgui/imgui_demo.cpp \
		third_party/imgui/backends/imgui_impl_sdl2.cpp third_party/imgui/backends/imgui_impl_opengl3.cpp
	archive "$OUT/libimgui.a" "${OBJS[@]}"
}

build_wigview() {
	build_lib; build_imgui
	log "[wigview]"
	compile -- $(find src/wigview -name '*.cpp' | sort)
	link "$OUT/wigview" "${OBJS[@]}" "$OUT/libwiggles.a" "$OUT/libimgui.a"
}

build_test3db() {
	build_lib
	log "[test3db]"
	compile -- src/tools/test3db.cpp
	link "$OUT/test3db" "${OBJS[@]}" "$OUT/libwiggles.a"
}

for t in "${TARGETS[@]}"; do
	case "$t" in
		clean) log "usuwam build/"; rm -rf build ;;
		lib) build_lib ;;
		imgui) build_imgui ;;
		wigview) build_wigview ;;
		test3db) build_test3db ;;
		all) build_wigview; build_test3db ;;
		*) log "nieznany cel: $t"; exit 1 ;;
	esac
done
log "OK ($MODE)"
