#include "hypch.h"
#include "JoltLibrary.h"

#include "Hydro/Core/Base.h"

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/RegisterTypes.h>

#include <cstdarg>
#include <cstdio>

namespace Hydro
{
	// Jolt prints its own trace lines through a printf-style hook.
	static void OnJoltTrace(const char* format, ...)
	{
		char buffer[1024];
		va_list arguments;
		va_start(arguments, format);
		vsnprintf(buffer, sizeof(buffer), format, arguments);
		va_end(arguments);

		HY_CORE_INFO("Jolt: {0}", buffer);
	}

#ifdef JPH_ENABLE_ASSERTS
	// Returning true breaks into the debugger.
	static bool OnJoltAssert(const char* expression, const char* message, const char* fileName, JPH::uint lineNumber)
	{
		HY_CORE_ERROR("Jolt assertion failed: {0} {1} ({2}:{3})", expression, message != nullptr ? message : "", fileName, lineNumber);
		return true;
	}
#endif

	void JoltLibrary::Init()
	{
		// RegisterTypes also checks that Jolt was compiled with the same defines (JoltDefines in Dependencies.lua).
		JPH::RegisterDefaultAllocator();
		JPH::Trace = OnJoltTrace;
#ifdef JPH_ENABLE_ASSERTS
		JPH::AssertFailed = OnJoltAssert;
#endif
		JPH::Factory::sInstance = new JPH::Factory();
		JPH::RegisterTypes();
		HY_CORE_INFO("Physics: Jolt {0}", GetVersion());
	}

	void JoltLibrary::Shutdown()
	{
		JPH::UnregisterTypes();
		delete JPH::Factory::sInstance;
		JPH::Factory::sInstance = nullptr;
	}

	std::string JoltLibrary::GetVersion()
	{
		return std::to_string(JPH_VERSION_MAJOR) + "." + std::to_string(JPH_VERSION_MINOR) + "." + std::to_string(JPH_VERSION_PATCH);
	}
}
