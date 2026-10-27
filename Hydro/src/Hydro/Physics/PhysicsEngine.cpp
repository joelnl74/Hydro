#include "hypch.h"
#include "PhysicsEngine.h"

#include "Hydro/Core/Assert.h"
#include "Hydro/Core/Base.h"

#include <box2d/box2d.h>

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/RegisterTypes.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>

namespace Hydro
{
	struct PhysicsEngineData
	{
		b2Version Version{};
	};

	static Scope<PhysicsEngineData> s_Data;

	// Box2D reports a broken precondition here. Returning 1 breaks into the debugger (0 would skip it).
	static int OnBox2DAssert(const char* condition, const char* fileName, int lineNumber)
	{
		HY_CORE_ERROR("Box2D assertion failed: {0} ({1}:{2})", condition, fileName, lineNumber);
		return 1;
	}

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

	void PhysicsEngine::Init()
	{
		HY_CORE_ASSERT(s_Data == nullptr, "PhysicsEngine::Init was already called");

		s_Data = CreateScope<PhysicsEngineData>();
		s_Data->Version = b2GetVersion();

		b2SetAssertFcn(OnBox2DAssert);

		// 1 world unit = 1 metre: Box2D is tuned for objects between 0.1 and 10 metres.
		b2SetLengthUnitsPerMeter(1.0f);

		HY_CORE_INFO("Physics: Box2D {0}", GetVersion());

		// Jolt's globals: the allocator, the trace and assert hooks, the type factory and the shape types.
		// RegisterTypes also checks that this file and the Jolt library were compiled with the same
		// defines (see JoltDefines in Dependencies.lua).
		JPH::RegisterDefaultAllocator();
		JPH::Trace = OnJoltTrace;
#ifdef JPH_ENABLE_ASSERTS
		JPH::AssertFailed = OnJoltAssert;
#endif
		JPH::Factory::sInstance = new JPH::Factory();
		JPH::RegisterTypes();

		HY_CORE_INFO("Physics: Jolt {0}", GetJoltVersion());

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
		if (s_Data == nullptr)
		{
			return;
		}

		JPH::UnregisterTypes();
		delete JPH::Factory::sInstance;
		JPH::Factory::sInstance = nullptr;

		s_Data.reset();
	}

	bool PhysicsEngine::IsRunning()
	{
		return s_Data != nullptr;
	}

	std::string PhysicsEngine::GetVersion()
	{
		const b2Version version = s_Data != nullptr ? s_Data->Version : b2GetVersion();
		return std::to_string(version.major) + "." + std::to_string(version.minor) + "." + std::to_string(version.revision);
	}

	std::string PhysicsEngine::GetJoltVersion()
	{
		return std::to_string(JPH_VERSION_MAJOR) + "." + std::to_string(JPH_VERSION_MINOR) + "." + std::to_string(JPH_VERSION_PATCH);
	}

	bool PhysicsEngine::RunSelfTest()
	{
		b2WorldDef worldDef = b2DefaultWorldDef();
		worldDef.gravity = { 0.0f, -9.81f };
		const b2WorldId world = b2CreateWorld(&worldDef);
		if (b2World_IsValid(world) == false)
		{
			return false;
		}

		const b2ShapeDef shapeDef = b2DefaultShapeDef();

		// A static floor whose top is at y = 0.
		b2BodyDef floorDef = b2DefaultBodyDef();
		floorDef.position = { 0.0f, -0.5f };
		const b2BodyId floorBody = b2CreateBody(world, &floorDef);
		const b2Polygon floorShape = b2MakeBox(20.0f, 0.5f);
		b2CreatePolygonShape(floorBody, &shapeDef, &floorShape);

		// A 1 x 1 box dropped from y = 3.
		b2BodyDef boxDef = b2DefaultBodyDef();
		boxDef.type = b2_dynamicBody;
		boxDef.position = { 0.0f, 3.0f };
		const b2BodyId boxBody = b2CreateBody(world, &boxDef);
		const b2Polygon boxShape = b2MakeBox(0.5f, 0.5f);
		b2CreatePolygonShape(boxBody, &shapeDef, &boxShape);

		for (int step = 0; step < 180; step++)
		{
			b2World_Step(world, 1.0f / 60.0f, 4);
		}

		// It should rest on the floor: its centre half a unit above y = 0.
		const b2Vec2 position = b2Body_GetPosition(boxBody);
		const bool restsOnFloor = std::fabs(position.y - 0.5f) < 0.05f && std::fabs(position.x) < 0.05f;

		b2DestroyWorld(world);
		const bool destroyed = b2World_IsValid(world) == false;

		return restsOnFloor && destroyed;
	}
}
