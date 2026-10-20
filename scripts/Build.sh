#!/usr/bin/env bash
# Builds everything with the generated Makefiles (macOS and Linux).
#   scripts/Build.sh [Debug|Release|Dist]   (default Debug)
# Run scripts/Mac-GenProjects.sh (or Linux-GenProjects.sh) first, and again after adding files.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

CONFIG="${1:-Debug}"
case "$CONFIG" in
	Debug|Release|Dist) ;;
	*) echo "Usage: scripts/Build.sh [Debug|Release|Dist]" >&2; exit 1 ;;
esac

if [ ! -f Makefile ]; then
	echo "No Makefile: run scripts/Mac-GenProjects.sh first." >&2
	exit 1
fi

if [ "$(uname)" = "Darwin" ]; then
	JOBS="$(sysctl -n hw.ncpu)"
else
	JOBS="$(nproc)"
fi

make config="$(echo "$CONFIG" | tr '[:upper:]' '[:lower:]')" -j"$JOBS"
