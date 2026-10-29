#!/usr/bin/env bash
# Quick checks that don't need Windows, MSVC or a GPU (for Linux, WSL or a CI/agent container):
#   1. syntax check of engine and editor sources with g++ -fsyntax-only -Wall -Wextra
#   2. compile every shader in Oxygen/assets/shaders to SPIR-V with glslang
# It is not a build: nothing links and Windows-only files are skipped. MSVC /W4 stays the real check.
#
# Usage:
#   scripts/check.sh                 # all sources + shaders
#   scripts/check.sh <file.cpp>...   # only these sources (+ shaders)
#   scripts/check.sh --shaders       # only shaders
#
# Needs: git, g++ (C++17), cmake + a C++ compiler for glslang (built once).
# Downloads (once, into $HYDRO_CHECK_CACHE, default /tmp/hydro-check): Vulkan-Headers, shaderc
# (headers only), glslang.
# Uses the repository's own vendored libraries; initialises the glm, imgui, spdlog, glfw, box2d and
# JoltPhysics submodules shallowly if they are empty. Several runs can share the cache at once: each
# has its own copy of the sources and its own log.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CACHE="${HYDRO_CHECK_CACHE:-/tmp/hydro-check}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"
mkdir -p "$CACHE"

log() { printf '\033[1m== %s\033[0m\n' "$*"; }

# ---- Dependencies -------------------------------------------------------------
if [ -z "$(ls -A "$ROOT/Hydro/vendor/glm" 2>/dev/null)" ] || [ -z "$(ls -A "$ROOT/Hydro/vendor/imgui" 2>/dev/null)" ] || [ -z "$(ls -A "$ROOT/Hydro/vendor/spdlog" 2>/dev/null)" ]; then
	log "Initialising submodules (shallow)"
	git -C "$ROOT" submodule update --init --depth 1 Hydro/vendor/glm Hydro/vendor/imgui Hydro/vendor/spdlog Hydro/vendor/glfw
fi
if [ -z "$(ls -A "$ROOT/Hydro/vendor/box2d" 2>/dev/null)" ] || [ -z "$(ls -A "$ROOT/Hydro/vendor/JoltPhysics" 2>/dev/null)" ]; then
	log "Initialising the physics submodules (shallow)"
	git -C "$ROOT" submodule update --init --depth 1 Hydro/vendor/box2d Hydro/vendor/JoltPhysics
fi

if [ ! -f "$CACHE/Vulkan-Headers/include/vulkan/vulkan.h" ]; then
	log "Fetching Vulkan-Headers"
	git clone -q --depth 1 https://github.com/KhronosGroup/Vulkan-Headers.git "$CACHE/Vulkan-Headers"
fi

if [ ! -f "$CACHE/shaderc/libshaderc/include/shaderc/shaderc.hpp" ]; then
	log "Fetching shaderc headers"
	rm -rf "$CACHE/shaderc"
	git clone -q --depth 1 https://github.com/google/shaderc.git "$CACHE/shaderc"
fi

GLSLANG="$(command -v glslangValidator || command -v glslang || true)"
if [ -z "$GLSLANG" ]; then
	GLSLANG="$CACHE/glslang/build/StandAlone/glslang"
	if [ ! -x "$GLSLANG" ]; then
		log "Building glslang (once, a few minutes)"
		[ -d "$CACHE/glslang" ] || git clone -q --depth 1 https://github.com/KhronosGroup/glslang.git "$CACHE/glslang"
		cmake -S "$CACHE/glslang" -B "$CACHE/glslang/build" -DENABLE_OPT=OFF -DGLSLANG_TESTS=OFF -DCMAKE_BUILD_TYPE=Release > "$CACHE/glslang-configure.log"
		cmake --build "$CACHE/glslang/build" -j"$JOBS" --target glslang-standalone > "$CACHE/glslang-build.log"
	fi
fi

# ---- Shaders ------------------------------------------------------------------
check_shaders() {
	log "Shaders"
	local failed=0
	for shader in "$ROOT"/Oxygen/assets/shaders/*.vert "$ROOT"/Oxygen/assets/shaders/*.frag; do
		[ -f "$shader" ] || continue
		if "$GLSLANG" -V "$shader" -o "$CACHE/$(basename "$shader").$$.spv" > "$CACHE/shader.$$.log" 2>&1; then
			echo "ok    ${shader#$ROOT/}"
		else
			echo "FAIL  ${shader#$ROOT/}"; sed 's/^/      /' "$CACHE/shader.$$.log"; failed=1
		fi
	done
	rm -f "$CACHE"/*.$$.spv "$CACHE/shader.$$.log"
	return $failed
}

if [ "${1:-}" = "--shaders" ]; then
	check_shaders
	exit $?
fi

# ---- Sources ------------------------------------------------------------------
# The engine refuses to compile on Linux (PlatformDetection.h #error), so check a copy of the
# sources with that one line turned into a define. The copy is refreshed every run.
SRC="$CACHE/src.$$"
LOG="$CACHE/sources.$$.log"
trap 'rm -rf "$SRC"' EXIT
rm -rf "$SRC"; mkdir -p "$SRC"
cp -r "$ROOT/Hydro/src" "$SRC/Hydro"
cp -r "$ROOT/Oxygen/src" "$SRC/Oxygen"
sed -i 's/#error "Linux is not supported!"/#define HY_PLATFORM_LINUX/' "$SRC/Hydro/Hydro/Core/PlatformDetection.h"

V="$ROOT/Hydro/vendor"
COMMON=(-std=c++17 -fsyntax-only -Wall -Wextra -DHY_DEBUG -DYAML_CPP_STATIC_DEFINE -DGLFW_INCLUDE_NONE
	-isystem "$CACHE/Vulkan-Headers/include" -isystem "$V" -isystem "$V/glm" -isystem "$V/imgui" -isystem "$V/imgui/backends"
	-isystem "$V/spdlog/include" -isystem "$V/yaml-cpp/include" -isystem "$V/entt" -isystem "$V/stb/include"
	-isystem "$V/cgltf" -isystem "$V/VulkanMemoryAllocator" -isystem "$V/glfw/include"
	-isystem "$V/box2d/include" -isystem "$V/JoltPhysics" -isystem "$V/ImGuizmo"
	-isystem "$CACHE/shaderc/libshaderc/include"
	-DJPH_ENABLE_ASSERTS) # Jolt's Debug defines: JoltDefines() in Dependencies.lua
export SRC
HYDRO_FLAGS="${COMMON[*]} -I$SRC/Hydro -include $SRC/Hydro/hypch.h"
OXYGEN_FLAGS="${COMMON[*]} -I$SRC/Oxygen -I$SRC/Hydro"
export HYDRO_FLAGS OXYGEN_FLAGS

# Windows-only and vendored copies are skipped.
skip() {
	case "$1" in
		*/Platform/Windows/*|*/imgui_impl_*|*/hypch.cpp) return 0 ;;
	esac
	return 1
}

files=()
if [ $# -gt 0 ]; then
	for f in "$@"; do files+=("$(cd "$(dirname "$f")" && pwd)/$(basename "$f")"); done
else
	while IFS= read -r f; do files+=("$f"); done < <(find "$ROOT/Hydro/src" "$ROOT/Oxygen/src" -name '*.cpp' | sort)
fi

log "Sources (${#files[@]} files, $JOBS jobs)"
printf '%s\n' "${files[@]}" | while IFS= read -r f; do skip "$f" || echo "${f#$ROOT/}"; done > "$CACHE/files.$$.txt"

check_one() {
	local rel="$1" copy flags
	case "$rel" in
		Hydro/src/*)  copy="$SRC/Hydro/${rel#Hydro/src/}";   flags="$HYDRO_FLAGS" ;;
		Oxygen/src/*) copy="$SRC/Oxygen/${rel#Oxygen/src/}"; flags="$OXYGEN_FLAGS" ;;
		*) echo "skip  $rel (not under Hydro/src or Oxygen/src)"; return 0 ;;
	esac
	# shellcheck disable=SC2086
	local out; out="$(g++ $flags "$copy" 2>&1 | grep -E 'error|warning' | grep -v '/vendor/\|Vulkan-Headers' || true)"
	if [ -z "$out" ]; then echo "ok    $rel"; else echo "FAIL  $rel"; echo "$out" | sed "s|$SRC/Hydro/|Hydro/src/|; s|$SRC/Oxygen/|Oxygen/src/|; s/^/      /" | head -20; fi
}
export -f check_one
xargs -a "$CACHE/files.$$.txt" -P "$JOBS" -I{} bash -c 'check_one "$@"' _ {} | tee "$LOG" | grep -v '^ok' || true
echo "$(grep -c '^ok' "$LOG") ok, $(grep -c '^FAIL' "$LOG" || true) with errors or warnings (full log: $LOG)"

shader_status=0
check_shaders || shader_status=1
rm -f "$CACHE/files.$$.txt"
if grep -q '^FAIL' "$LOG" || [ $shader_status -ne 0 ]; then
	exit 1
fi
