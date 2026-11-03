#pragma once
#include <string>

namespace Hydro
{
	// Subsystem: what is global to Box2D (2D) and Jolt (3D): hooks, allocator, length unit (1 unit = 1 metre). The
	// simulation lives in each running Scene's PhysicsScene and PhysicsScene3D.
	class PhysicsEngine
	{
	public:
		static void Init();
		static void Shutdown();

		static bool IsRunning();

		// The linked Box2D's version, e.g. "3.1.1".
		static std::string GetVersion();

		// The linked Jolt's version, e.g. "5.6.0".
		static std::string GetJoltVersion();

		// A tiny Box2D world, stepped and checked; false means a bad build. Init runs it in Debug builds.
		// TODO(physics): remove once PhysicsScene has real tests.
		static bool RunSelfTest();

		// The Jolt counterpart (a falling box, a character walking up a ramp, Z up); false means a bad build or a Y-up
		// default left in. TODO(physics): remove once PhysicsScene3D has real tests.
		static bool RunJoltSelfTest();
	};
}
