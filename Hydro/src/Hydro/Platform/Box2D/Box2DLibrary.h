#pragma once

#include <string>

// What is global to Box2D: its assert hook and length unit (1 unit = 1 metre). PhysicsEngine calls these.

namespace Hydro
{
	class Box2DLibrary
	{
	public:
		static void Init();

		// The linked Box2D's version, e.g. "3.1.1".
		static std::string GetVersion();

		// A tiny world, stepped and checked; false means a bad build.
		static bool RunSelfTest();
	};
}
