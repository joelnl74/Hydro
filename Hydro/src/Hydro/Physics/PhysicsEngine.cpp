#include "hypch.h"
#include "PhysicsEngine.h"

#include "Hydro/Core/Assert.h"
#include "Hydro/Core/Base.h"
#include "Hydro/Platform/Box2D/Box2DLibrary.h"
#include "Hydro/Platform/Jolt/JoltLibrary.h"

namespace Hydro
{
	static bool s_Running = false;

	void PhysicsEngine::Init()
	{
		HY_CORE_ASSERT(s_Running == false, "PhysicsEngine::Init was already called");
		s_Running = true;
		Box2DLibrary::Init();
		JoltLibrary::Init();

#ifdef HY_DEBUG
		if (RunSelfTest() == false)
		{
			HY_CORE_ERROR("Physics: the Box2D self test failed; the library is not built correctly");
			HY_DEBUGBREAK();
		}

		if (RunJoltSelfTest() == false)
		{
			HY_CORE_ERROR("Physics: the Jolt self test failed; the library is not built correctly");
			HY_DEBUGBREAK();
		}
#endif
	}

	void PhysicsEngine::Shutdown()
	{
		if (s_Running == false)
		{
			return;
		}
		JoltLibrary::Shutdown();
		s_Running = false;
	}

	bool PhysicsEngine::IsRunning()
	{
		return s_Running;
	}

	std::string PhysicsEngine::GetVersion()
	{
		return Box2DLibrary::GetVersion();
	}

	std::string PhysicsEngine::GetJoltVersion()
	{
		return JoltLibrary::GetVersion();
	}

	bool PhysicsEngine::RunSelfTest()
	{
		return Box2DLibrary::RunSelfTest();
	}

	bool PhysicsEngine::RunJoltSelfTest()
	{
		return JoltLibrary::RunSelfTest();
	}
}
