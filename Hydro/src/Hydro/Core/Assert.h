#pragma once
#include "Hydro/Core/Base.h"
#include "Hydro/Core/Log.h"

// HY_CORE_ASSERT(condition, message...): in Debug builds, logs and breaks into the debugger when the condition is false
// (logging only while Log is up). Compiled out in Release and Dist.
#ifdef HY_ENABLE_ASSERTS
	#define HY_CORE_ASSERT(condition, ...) \
		{ \
			if ((condition) == false) \
			{ \
				if (::Hydro::Log::GetCoreLogger() != nullptr) \
				{ \
					HY_CORE_ERROR("Assertion failed: {0}", #condition); \
					HY_CORE_ERROR(__VA_ARGS__); \
				} \
				HY_DEBUGBREAK(); \
			} \
		}
#else
	#define HY_CORE_ASSERT(condition, ...)
#endif
