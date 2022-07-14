#pragma once

#include "Hydro/Core/PlatformDetection.h"

#include <memory>

#ifdef HY_DEBUG
#if defined(HY_PLATFORM_WINDOWS)
#define HY_DEBUGBREAK() __debugbreak()
#elif defined(HY_PLATFORM_LINUX)
#include <signal.h>
#define HY_DEBUGBREAK() raise(SIGTRAP)
#else
#error "Platform doesn't support debugbreak yet!"
#endif
#define HY_ENABLE_ASSERTS
#else
#define HY_DEBUGBREAK()
#endif

#define HY_EXPAND_MACRO(x) x
#define HY_STRINGIFY_MACRO(x) #x

#define BIT(x) (1 << x)

#define HY_BIND_EVENT_FN(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }

namespace Hydro {

	template<typename T>
	using Scope = std::unique_ptr<T>;
	template<typename T, typename ... Args>
	constexpr Scope<T> CreateScope(Args&& ... args)
	{
		return std::make_unique<T>(std::forward<Args>(args)...);
	}

	template<typename T>
	using Ref = std::shared_ptr<T>;
	template<typename T, typename ... Args>
	constexpr Ref<T> CreateRef(Args&& ... args)
	{
		return std::make_shared<T>(std::forward<Args>(args)...);
	}

}
