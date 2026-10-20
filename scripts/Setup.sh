#!/usr/bin/env bash
# macOS / Linux setup: Python packages, premake, Vulkan SDK check, submodules, then the projects.
# Windows: scripts/Setup.bat.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
python3 Setup.py
