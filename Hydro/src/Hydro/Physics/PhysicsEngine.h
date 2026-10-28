#pragma once
#include <string>

namespace Hydro
{
	// Subsystem: physics (Box2D for 2D, Jolt for 3D). Application starts it after the AssetManager and
	// stops it before it. It holds what is global to the libraries: Box2D's assert hook and length unit
	// (1 world unit = 1 metre), and Jolt's allocator, type registration and hooks. The simulation itself
	// lives in one PhysicsScene (2D) and later one PhysicsScene3D per running Scene, not here.
	class PhysicsEngine
	{
	public:
		static void Init();
		static void Shutdown();

		static bool IsRunning();

		// The version of the Box2D that is linked in, e.g. "3.1.1".
		static std::string GetVersion();

		// The version of the Jolt that is linked in, e.g. "5.6.0".
		static std::string GetJoltVersion();

		// Builds a small world (a floor and a falling box), steps it and destroys it. Returns false if
		// Box2D misbehaves, which means a bad build (wrong C standard, mismatched runtime) rather than a
		// bug in the game. Init runs it in Debug builds.
		// TODO(physics): remove once PhysicsScene has real tests.
		static bool RunSelfTest();

		// The Jolt counterpart (JoltSelfTest.cpp): a floor, a falling box and a character walking up a
		// ramp, in a Z-up world. Returns false if Jolt misbehaves: a bad build (mismatched defines or
		// runtime) or a Y-up default that was left in. Init runs it in Debug builds.
		// TODO(physics): remove once PhysicsScene3D has real tests.
		static bool RunJoltSelfTest();
	};
}
