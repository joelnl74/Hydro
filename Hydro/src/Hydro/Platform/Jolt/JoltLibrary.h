#pragma once

#include <string>

// What is global to Jolt: its allocator, trace and assert hooks, and the type factory. PhysicsEngine calls these.

namespace Hydro
{
	class JoltLibrary
	{
	public:
		static void Init();
		static void Shutdown();

		// The linked Jolt's version, e.g. "5.6.0".
		static std::string GetVersion();

		// A falling box and a character walking up a ramp, Z up; false means a bad build or a Y-up default left in.
		static bool RunSelfTest();
	};
}
