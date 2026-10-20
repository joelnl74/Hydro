#pragma once

// Platform detection from the compiler's predefined macros. Supported: Windows (x64) and macOS
// (Apple Silicon and Intel). Linux builds too, for scripts/check.sh and the Linux build check, but is
// not a supported target yet. See docs/Architecture.md, "Platforms".
#if defined(_WIN32)
	#if defined(_WIN64)
		#define HY_PLATFORM_WINDOWS
	#else
		#error "x86 builds are not supported!"
	#endif
#elif defined(__APPLE__) || defined(__MACH__)
	#include <TargetConditionals.h>
	// TARGET_OS_MAC is set on every Apple platform, so check the others first.
	#if TARGET_IPHONE_SIMULATOR == 1
		#error "The iOS simulator is not supported!"
	#elif TARGET_OS_IPHONE == 1
		#error "iOS is not supported!"
	#elif TARGET_OS_MAC == 1
		#define HY_PLATFORM_MACOS
	#else
		#error "Unknown Apple platform!"
	#endif
// Android defines __linux__ too, so check it first.
#elif defined(__ANDROID__)
	#error "Android is not supported!"
#elif defined(__linux__)
	#define HY_PLATFORM_LINUX
#else
	#error "Unknown platform!"
#endif
