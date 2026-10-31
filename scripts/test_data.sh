#!/usr/bin/env bash
# Headless tests of the data system (definitions and data tables, #168), for Linux, WSL or a CI/agent container:
#   1. the reflection generator's HY_DATA / HY_ENUM checks (Tools/DataTests/test_generator.py)
#   2. the generated registration of Tools/DataTests/DataTestTypes.h, then Tools/DataTests/DataRoundTripTest.cpp
#      built with g++ against the engine's Data/ sources, yaml-cpp and spdlog, and run
# No window, GPU, project or MSVC. MSVC /W4 stays the real compile check.
#
# Usage: scripts/test_data.sh
# Needs: python3, g++ (C++17). Builds yaml-cpp once into $HYDRO_CHECK_CACHE (default /tmp/hydro-check).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CACHE="${HYDRO_CHECK_CACHE:-/tmp/hydro-check}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"
OUT="$CACHE/data-tests.$$"
trap 'rm -rf "$OUT"' EXIT
mkdir -p "$OUT"

log() { printf '\033[1m== %s\033[0m\n' "$*"; }

V="$ROOT/Hydro/vendor"
if [ -z "$(ls -A "$V/glm" 2>/dev/null)" ] || [ -z "$(ls -A "$V/spdlog" 2>/dev/null)" ]; then
	log "Initialising submodules (shallow)"
	git -C "$ROOT" submodule update --init --depth 1 Hydro/vendor/glm Hydro/vendor/spdlog
fi

# ---- yaml-cpp, built once ---------------------------------------------------------------------
YAML_LIB="$CACHE/yaml-cpp/libyaml-cpp.a"
if [ ! -f "$YAML_LIB" ]; then
	log "Building yaml-cpp (once)"
	mkdir -p "$CACHE/yaml-cpp/obj"
	find "$V/yaml-cpp/src" -name '*.cpp' | xargs -P "$JOBS" -I{} sh -c \
		'g++ -std=c++17 -O1 -c -DYAML_CPP_STATIC_DEFINE -I"$1/include" -I"$1/src" "$2" -o "$3/obj/$(basename "$2" .cpp).o"' \
		_ "$V/yaml-cpp" {} "$CACHE/yaml-cpp"
	ar rcs "$YAML_LIB" "$CACHE"/yaml-cpp/obj/*.o
fi

# ---- 1. Generator ------------------------------------------------------------------------------
log "Reflection generator (HY_DATA / HY_ENUM)"
python3 "$ROOT/Tools/DataTests/test_generator.py"

# ---- 2. Round trips ----------------------------------------------------------------------------
log "Generating the test types' registration"
python3 "$ROOT/Tools/ReflectionGenerator/generate_reflection.py" "$ROOT" --source "$ROOT/Tools/DataTests" \
	--output "$OUT/gen/GeneratedScriptReflection.cpp" --data-output "$OUT/gen/GeneratedDataReflection.cpp"
cp "$ROOT/Game/src/Generated/GeneratedDataReflection.h" "$OUT/gen/"

# The engine refuses to compile on Linux (PlatformDetection.h #error): build a copy with that line turned into a define.
cp -r "$ROOT/Hydro/src" "$OUT/src"
sed -i 's/#error "Linux is not supported!"/#define HY_PLATFORM_LINUX/' "$OUT/src/Hydro/Core/PlatformDetection.h"

FLAGS=(-std=c++17 -O0 -g -Wall -Wextra -DHY_DEBUG -DYAML_CPP_STATIC_DEFINE
	-I"$OUT/src" -I"$OUT/gen" -I"$ROOT/Tools/DataTests"
	-isystem "$V/glm" -isystem "$V/spdlog/include" -isystem "$V/yaml-cpp/include")
SOURCES=(
	"$OUT/src/Hydro/Core/Log.cpp"
	"$OUT/src/Hydro/Core/UUID.cpp"
	"$OUT/src/Hydro/Utils/YAMLConversions.cpp"
	"$OUT"/src/Hydro/Data/*.cpp
	"$OUT/gen/GeneratedDataReflection.cpp"
	"$ROOT/Tools/DataTests/DataRoundTripTest.cpp"
)

log "Building the round-trip test"
mkdir -p "$OUT/obj"
printf '%s\n' "${SOURCES[@]}" | xargs -P "$JOBS" -I{} sh -c \
	'g++ "$@" -include "'"$OUT"'/src/hypch.h" -c "$0" -o "'"$OUT"'/obj/$(basename "$0" .cpp).o" 2>&1 | grep -v "^$" || true' \
	{} "${FLAGS[@]}" > "$OUT/build.log"
if grep -qE "error|warning" "$OUT/build.log"; then
	sed "s|$OUT/src/|Hydro/src/|g" "$OUT/build.log" | grep -E "error|warning" | head -40
	if grep -q "error" "$OUT/build.log"; then
		exit 1
	fi
fi
g++ "$OUT"/obj/*.o "$YAML_LIB" -lpthread -o "$OUT/DataRoundTripTest"

log "Running"
"$OUT/DataRoundTripTest"
