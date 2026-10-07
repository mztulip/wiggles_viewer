#!/usr/bin/env bash
# System budowania OpenWiggles (bez CMake).
#
# Użycie: ./build.sh [debug|release] [vulkan|opengl|both] [cel...]
#   cele: lib, imgui, wigview, app (program z OW_APP_ROOT), owscript, test3db, export_gltf, all (domyślnie), clean
#   renderer: both (domyślnie: oba backendy, wybór przy starcie --renderer, domyślnie Vulkan),
#             vulkan albo opengl (tylko jeden backend; wynik w build/<tryb>-<renderer>/)
#   zmienne: CXX (domyślnie g++), JOBS (domyślnie nproc), VERBOSE=1
#   program z innego repozytorium (np. gra openwiggles z tym repo jako submodułem):
#     OW_APP_ROOT=<katalog źródeł .cpp> OW_APP_NAME=<nazwa pliku wynikowego>: cel app (też w all)
#     OW_APP_LANG=<katalog .po programu>: tłumaczenia dołączane do wbudowanych (scalane z lang/*.po)
#     OW_BUILD_ROOT=<katalog budowania> (domyślnie build), OW_VERSION_DIR=<repo, z którego wersja>
#
# Kompilacja przyrostowa: plik .o jest przebudowywany, gdy jest starszy od źródła
# albo od któregokolwiek nagłówka z pliku zależności .d (generowanego przez -MMD).
set -euo pipefail
cd "$(dirname "$0")"

MODE=debug
RENDERER=both
TARGETS=()
for a in "$@"; do
	case "$a" in
		debug|release) MODE="$a" ;;
		vulkan|opengl|both) RENDERER="$a" ;;
		*) TARGETS+=("$a") ;;
	esac
done
WITH_VK=0; WITH_GL=0
[ "$RENDERER" != opengl ] && WITH_VK=1
[ "$RENDERER" != vulkan ] && WITH_GL=1
[ ${#TARGETS[@]} -eq 0 ] && TARGETS=(all)

CXX="${CXX:-g++}"
JOBS="${JOBS:-$(nproc)}"
BR="${OW_BUILD_ROOT:-build}" # katalog budowania (wynik, obiekty, pliki generowane)
OUT="$BR/$MODE"
[ "$RENDERER" != both ] && OUT="$BR/$MODE-$RENDERER"
APP_ROOT="${OW_APP_ROOT:-}"
[ -n "$APP_ROOT" ] && APP_ROOT="$(cd "$APP_ROOT" && pwd)"

# Wersja programu (commit, z "-zmiany" przy niezacommitowanych zmianach) w nagłówku build/gen/ow_version.h.
# Plik jest nadpisywany tylko przy zmianie treści, więc przyrostowa kompilacja przebudowuje wtedy
# tylko pliki, które go dołączają.
gen_version() {
	local v d
	local g=(git -C "${OW_VERSION_DIR:-.}")
	v="$("${g[@]}" describe --always --dirty=-zmiany 2>/dev/null || echo nieznana)"
	d="$("${g[@]}" log -1 --format=%cd --date=format:%Y-%m-%d 2>/dev/null || echo '')"
	mkdir -p "$BR/gen"
	local new="#pragma once
#define OW_VERSION \"$v\"
#define OW_VERSION_DATE \"$d\""
	if [ ! -f "$BR/gen/ow_version.h" ] || [ "$(cat "$BR/gen/ow_version.h")" != "$new" ]; then
		printf '%s\n' "$new" > "$BR/gen/ow_version.h"
	fi
}
gen_version

CXXFLAGS=(-std=c++17 -Wall -Wextra -Isrc -I"$BR/gen" -Ithird_party -Ithird_party/imgui -Ithird_party/imgui/backends $(pkg-config --cflags sdl2))
case "$MODE" in
	debug)   CXXFLAGS+=(-O0 -g -DOW_DEBUG) ;;
	release) CXXFLAGS+=(-O2 -DNDEBUG) ;;
esac
LIBS=($(pkg-config --libs sdl2 zlib) -ltcl8.6 -ldl)
[ "$WITH_GL" = 1 ] && CXXFLAGS+=(-DOW_RHI_GL)
if [ "$WITH_VK" = 1 ]; then CXXFLAGS+=(-DOW_RHI_VK); LIBS+=($(pkg-config --libs vulkan)); fi

# Shadery: src/shaders/*.vert|*.frag (GLSL 450, #include "x.glsl" rozwijane tutaj).
#  - OpenGL: tekst z "#define OW_OPENGL" w build/gen/ow_shaders_gl.inc,
#  - Vulkan: SPIR-V z glslc ("#define OW_VULKAN") w build/gen/ow_shaders_vk.inc.
# Pliki .inc są nadpisywane tylko przy zmianie treści.
expand_includes() { # expand_includes <plik>
	local line inc
	while IFS= read -r line || [ -n "$line" ]; do
		if [[ "$line" =~ ^#include\ \"([^\"]+)\" ]]; then
			inc="src/shaders/${BASH_REMATCH[1]}"
			[ -f "$inc" ] || { log "brak $inc" >&2; exit 1; }
			expand_includes "$inc"
		else
			printf '%s\n' "$line"
		fi
	done < "$1"
}
write_if_changed() { # write_if_changed <plik> (treść na stdin)
	local tmp="$1.tmp"
	cat > "$tmp"
	if [ -f "$1" ] && cmp -s "$tmp" "$1"; then rm -f "$tmp"; else mv "$tmp" "$1"; fi
}
gen_shaders() {
	local dir="$BR/gen/shaders" f name stage body
	mkdir -p "$dir"
	{
		echo "// Wygenerowane przez build.sh z src/shaders/. Nie edytować."
		for f in src/shaders/*.vert src/shaders/*.frag; do
			name="$(basename "${f%.*}")"; stage="${f##*.}"
			body="$(expand_includes "$f" | sed '1s/^#version 450.*/#version 450 core\n#define OW_OPENGL 1/')"
			printf '{"%s", "%s", R"OWSH(%s\n)OWSH"},\n' "$name" "$stage" "$body"
		done
	} | write_if_changed "$BR/gen/ow_shaders_gl.inc"
	[ "$WITH_VK" = 1 ] || return 0
	command -v glslc >/dev/null || { log "brak glslc (pakiet shaderc) - potrzebny dla Vulkana"; exit 1; }
	local entries=""
	{
		echo "// Wygenerowane przez build.sh z src/shaders/ (SPIR-V z glslc). Nie edytować."
		for f in src/shaders/*.vert src/shaders/*.frag; do
			name="$(basename "${f%.*}")"; stage="${f##*.}"
			expand_includes "$f" | sed '1s/^#version 450.*/#version 450\n#define OW_VULKAN 1/' \
				| write_if_changed "$dir/$name.$stage.vk.glsl"
			if [ ! -f "$dir/$name.$stage.spv" ] || [ "$dir/$name.$stage.vk.glsl" -nt "$dir/$name.$stage.spv" ]; then
				log "  GLSLC $f" >&2
				glslc -fshader-stage="$stage" -O "$dir/$name.$stage.vk.glsl" -o "$dir/$name.$stage.spv" || { log "BŁĄD shadera $f" >&2; exit 1; }
			fi
			printf 'static const uint32_t spv_%s_%s[] = {\n' "$name" "$stage"
			od -An -v -tx4 "$dir/$name.$stage.spv" | tr -s ' ' '\n' | sed '/^$/d;s/^/0x/;s/$/,/' | paste -sd' ' | fold -w 120 -s
			printf '};\n'
			entries+="{\"$name\", \"$stage\", spv_${name}_${stage}, sizeof(spv_${name}_${stage}) / 4},"$'\n'
		done
		echo "static const VkShaderEntry kVkShaders[] = {"
		printf '%s' "$entries"
		echo "};"
	} | write_if_changed "$BR/gen/ow_shaders_vk.inc"
}

# Tłumaczenia: lang/*.po osadzane jako tekst w build/gen/ow_lang.inc (src/lib/i18n.cpp).
gen_lang() {
	local f
	mkdir -p "$BR/gen"
	{
		echo "// Wygenerowane przez build.sh z lang/*.po (i OW_APP_LANG/*.po). Nie edytować."
		echo "static const BuiltinLanguage kBuiltinLanguages[] = {"
		for f in lang/*.po ${OW_APP_LANG:+"$OW_APP_LANG"/*.po}; do
			[ -f "$f" ] || continue
			printf '{"%s", R"OWPO(%s\n)OWPO"},\n' "$(basename "$f")" "$(cat "$f")"
		done
		echo "{nullptr, nullptr}};"
	} | write_if_changed "$BR/gen/ow_lang.inc"
}

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
		local rel="$s"
		[ -n "$APP_ROOT" ] && [ "${s#"$APP_ROOT"/}" != "$s" ] && rel="app/${s#"$APP_ROOT"/}" # program z innego repo
		local o="$OUT/obj/${rel%.cpp}.o"
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

# Biblioteka nie może zależeć od programów: src/lib nie dołącza niczego z src/apps.
check_layers() {
	local bad
	bad="$(grep -rn '#include "apps/' src/lib || true)"
	[ -z "$bad" ] || { log "BŁĄD: src/lib dołącza kod programów (src/apps):"; log "$bad"; exit 1; }
}

build_lib() {
	log "[libwiggles]"
	check_layers
	compile -- $(find src/lib -name '*.cpp' | sort)
	archive "$OUT/libwiggles.a" "${OBJS[@]}"
}

build_imgui() {
	log "[imgui]"
	compile -w -- third_party/imgui/imgui.cpp third_party/imgui/imgui_draw.cpp \
		third_party/imgui/imgui_tables.cpp third_party/imgui/imgui_widgets.cpp \
		third_party/imgui/imgui_demo.cpp \
		third_party/imgui/backends/imgui_impl_sdl2.cpp \
		$([ "$WITH_GL" = 1 ] && echo third_party/imgui/backends/imgui_impl_opengl3.cpp) \
		$([ "$WITH_VK" = 1 ] && echo third_party/imgui/backends/imgui_impl_vulkan.cpp)
	archive "$OUT/libimgui.a" "${OBJS[@]}"
}

build_wigview() {
	build_lib; build_imgui
	log "[wigview]"
	compile -- $(find src/apps/wigview -name '*.cpp' | sort)
	link "$OUT/wigview" "${OBJS[@]}" "$OUT/libwiggles.a" "$OUT/libimgui.a"
}

build_app() { # program z innego repozytorium (OW_APP_ROOT, OW_APP_NAME)
	[ -n "$APP_ROOT" ] || { log "cel app wymaga OW_APP_ROOT i OW_APP_NAME"; exit 1; }
	build_lib; build_imgui
	log "[${OW_APP_NAME:?brak OW_APP_NAME}]"
	compile -- $(find "$APP_ROOT" -name '*.cpp' | sort)
	link "$OUT/$OW_APP_NAME" "${OBJS[@]}" "$OUT/libwiggles.a" "$OUT/libimgui.a"
}

build_owscript() {
	build_lib
	log "[owscript]"
	compile -- src/tools/owscript.cpp
	link "$OUT/owscript" "${OBJS[@]}" "$OUT/libwiggles.a"
}

build_export_gltf() {
	build_lib
	log "[export_gltf]"
	compile -- src/tools/export_gltf.cpp
	link "$OUT/export_gltf" "${OBJS[@]}" "$OUT/libwiggles.a"
}

build_test3db() {
	build_lib
	log "[test3db]"
	compile -- src/tools/test3db.cpp
	link "$OUT/test3db" "${OBJS[@]}" "$OUT/libwiggles.a"
}

[ "${TARGETS[*]}" = clean ] || { gen_shaders; gen_lang; }

for t in "${TARGETS[@]}"; do
	case "$t" in
		clean) log "usuwam $BR/"; rm -rf "$BR" ;;
		lib) build_lib ;;
		imgui) build_imgui ;;
		wigview) build_wigview ;;
		app) build_app ;;
		owscript) build_owscript ;;
		test3db) build_test3db ;;
		export_gltf) build_export_gltf ;;
		all) build_wigview; [ -n "$APP_ROOT" ] && build_app; build_test3db; build_export_gltf; build_owscript ;;
		*) log "nieznany cel: $t"; exit 1 ;;
	esac
done
log "OK ($MODE, renderer: $RENDERER)"
