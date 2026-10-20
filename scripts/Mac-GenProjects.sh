#!/usr/bin/env bash
# Generates the macOS build files with premake (run scripts/Setup.sh once first).
#   scripts/Mac-GenProjects.sh           Makefiles (build with scripts/Build.sh)
#   scripts/Mac-GenProjects.sh --xcode   an Xcode project as well (Hydro.xcworkspace)
# Needs VULKAN_SDK: `source ~/VulkanSDK/<version>/setup-env.sh` in this terminal or in ~/.zshrc.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

if [ -z "${VULKAN_SDK:-}" ]; then
	echo "VULKAN_SDK is not set. Run: source ~/VulkanSDK/<version>/setup-env.sh" >&2
	exit 1
fi

PREMAKE=vendor/premake/bin/premake5
if [ ! -x "$PREMAKE" ]; then
	echo "Premake is missing: run scripts/Setup.sh first." >&2
	exit 1
fi

"$PREMAKE" gmake
if [ "${1:-}" = "--xcode" ]; then
	"$PREMAKE" xcode4
fi
