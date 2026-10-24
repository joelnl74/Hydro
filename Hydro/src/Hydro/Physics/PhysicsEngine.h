#pragma once
#include <string>

namespace Hydro
{
	// Subsystem: 2D physics (Box2D). Application starts it after the AssetManager and stops it before it.
	// It holds what is global to Box2D: the assert hook and the length unit (1 world unit = 1 metre).
	// The simulation itself will live in one PhysicsScene per running Scene (see the Physics plan),
	// not here.
	class PhysicsEngine
	{
	public:
		static void Init();
		static void Shutdown();

		static bool IsRunning();

		// The version of the Box2D that is linked in, e.g. "3.1.1".
		static std::string GetVersion();

		// Builds a small world (a floor and a falling box), steps it and destroys it. Returns false if
		// Box2D misbehaves, which means a bad build (wrong C standard, mismatched runtime) rather than a
		// bug in the game. Init runs it in Debug builds.
		// TODO(physics): remove once PhysicsScene has real tests.
		static bool RunSelfTest();
	};
}
