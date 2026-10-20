#!/usr/bin/env bash
# Runs the editor from Oxygen/, where its assets/ folder is (macOS and Linux).
#   scripts/Run.sh [Debug|Release|Dist]   (default Debug)
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

CONFIG="${1:-Debug}"
if [ "$(uname)" = "Darwin" ]; then
	PLATFORM="macosx-ARM64"
else
	PLATFORM="linux-x86_64"
fi

EXECUTABLE="bin/$CONFIG-$PLATFORM/Oxygen/Oxygen"
if [ ! -x "$EXECUTABLE" ]; then
	echo "$EXECUTABLE not found: run scripts/Build.sh $CONFIG first." >&2
	exit 1
fi

cd Oxygen
exec "../$EXECUTABLE"
