#!/usr/bin/env bash
# Generates Makefiles on Linux. Linux builds for checks; it is not a supported target yet.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

PREMAKE=vendor/premake/bin/premake5
if [ ! -x "$PREMAKE" ]; then
	PREMAKE="$(command -v premake5 || true)"
fi
if [ -z "$PREMAKE" ]; then
	echo "Premake is missing: run scripts/Setup.sh first." >&2
	exit 1
fi

"$PREMAKE" gmake
