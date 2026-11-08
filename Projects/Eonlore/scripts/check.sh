#!/usr/bin/env bash
# Eonlore's checks with Hydro's tools (Linux/WSL/CI): g++ syntax + conventions + project validation (check.sh) and the
# data tests with Eonlore's rule tests (test_data.sh). Hydro: HYDRO_DIR, else ../Hydro, else two folders up.
#   scripts/check.sh [files...]       # Hydro's check.sh on this workspace
#   scripts/check.sh --tests          # the data tests with Tools/Tests/GameRulesTest.cpp
set -euo pipefail
WORKSPACE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
for candidate in "${HYDRO_DIR:-}" "$WORKSPACE/../Hydro" "$WORKSPACE/../.."; do
	if [ -n "$candidate" ] && [ -f "$candidate/Engine.lua" ]; then HYDRO="$(cd "$candidate" && pwd)"; break; fi
done
if [ -z "${HYDRO:-}" ]; then echo "Eonlore: Hydro not found: set HYDRO_DIR or clone it next to this repository." >&2; exit 2; fi
if [ "${1:-}" = "--tests" ]; then
	exec bash "$HYDRO/scripts/test_data.sh" --workspace "$WORKSPACE" --game-tests "$WORKSPACE/Tools/Tests/GameRulesTest.cpp"
fi
exec bash "$HYDRO/scripts/check.sh" --workspace "$WORKSPACE" "$@"
