#include "hypch.h"
#include "PhysicsEngine.h"

#include "Hydro/Core/Assert.h"
#include "Hydro/Core/Base.h"

#include <Jolt/Jolt.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Math/Real.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <cmath>

// MVP0 spike (see the Jolt physics MVP page): proves Jolt builds and links, and that its Y-up defaults
// are all replaced for a Z-up world. The layer setup below is the smallest Jolt accepts; the real one
// belongs to PhysicsScene3D (MVP1) and is not built from this.
// TODO(physics): remove once PhysicsScene3D has real tests.
namespace Hydro
{
	static constexpr JPH::ObjectLayer s_SpikeLayerStatic = 0;
	static constexpr JPH::ObjectLayer s_SpikeLayerMoving = 1;

	class JoltSpikeBroadPhaseLayers final : public JPH::BroadPhaseLayerInterface
	{
	public:
		JPH::uint GetNumBroadPhaseLayers() const override
		{
			return 2;
		}

		JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
		{
			return JPH::BroadPhaseLayer(static_cast<JPH::uint8>(layer));
		}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
		const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override
		{
			return layer.GetValue() == 0 ? "Static" : "Moving";
		}
#endif
	};

	class JoltSpikeObjectVsBroadPhaseFilter final : public JPH::ObjectVsBroadPhaseLayerFilter
	{
	public:
		bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broadPhaseLayer) const override
		{
			if (layer == s_SpikeLayerStatic)
			{
				return broadPhaseLayer.GetValue() == s_SpikeLayerMoving;
			}

			return true;
		}
	};

	class JoltSpikeObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter
	{
	public:
		bool ShouldCollide(JPH::ObjectLayer layerA, JPH::ObjectLayer layerB) const override
		{
			return layerA == s_SpikeLayerMoving || layerB == s_SpikeLayerMoving;
		}
	};

	bool PhysicsEngine::RunJoltSelfTest()
	{
		constexpr float stepTime = 1.0f / 60.0f;
		constexpr float rampAngle = JPH::DegreesToRadians(20.0f);
		constexpr float rampHalfLength = 6.0f;
		constexpr float rampHalfThickness = 0.1f;
		constexpr float rampStartX = 3.0f;
		constexpr float characterRadius = 0.25f;
		constexpr float characterHeight = 1.4f;
		const JPH::Vec3 gravity(0.0f, 0.0f, -9.81f);
		const JPH::Vec3 up = JPH::Vec3::sAxisZ();

		JPH::TempAllocatorImpl tempAllocator(10 * 1024 * 1024);
		JPH::JobSystemSingleThreaded jobSystem(JPH::cMaxPhysicsJobs);

		JoltSpikeBroadPhaseLayers broadPhaseLayers;
		JoltSpikeObjectVsBroadPhaseFilter objectVsBroadPhaseFilter;
		JoltSpikeObjectLayerPairFilter objectLayerPairFilter;

		JPH::PhysicsSystem physicsSystem;
		physicsSystem.Init(1024, 0, 1024, 1024, broadPhaseLayers, objectVsBroadPhaseFilter, objectLayerPairFilter);
		physicsSystem.SetGravity(gravity);

		JPH::BodyInterface& bodies = physicsSystem.GetBodyInterface();

		// A static floor whose top is at z = 0.
		const JPH::BodyID floorBody = bodies.CreateAndAddBody(
			JPH::BodyCreationSettings(new JPH::BoxShape(JPH::Vec3(20.0f, 20.0f, 0.5f)), JPH::RVec3(0.0f, 0.0f, -0.5f),
				JPH::Quat::sIdentity(), JPH::EMotionType::Static, s_SpikeLayerStatic),
			JPH::EActivation::DontActivate);

		// A 1 x 1 x 1 box dropped from z = 3, off to the side of the character's path.
		const JPH::BodyID boxBody = bodies.CreateAndAddBody(
			JPH::BodyCreationSettings(new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f)), JPH::RVec3(0.0f, 5.0f, 3.0f),
				JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, s_SpikeLayerMoving),
			JPH::EActivation::Activate);

		// A slab that rises along +X: its top surface meets the floor at x = rampStartX and climbs at
		// rampAngle. A rotation about +Y by a negative angle lifts the +X end.
		const float sinAngle = std::sin(rampAngle);
		const float cosAngle = std::cos(rampAngle);
		const JPH::RVec3 rampCentre(rampStartX + rampHalfLength * cosAngle + rampHalfThickness * sinAngle, 0.0f,
			rampHalfLength * sinAngle - rampHalfThickness * cosAngle);
		const JPH::BodyID rampBody = bodies.CreateAndAddBody(
			JPH::BodyCreationSettings(new JPH::BoxShape(JPH::Vec3(rampHalfLength, 3.0f, rampHalfThickness)), rampCentre,
				JPH::Quat::sRotation(JPH::Vec3::sAxisY(), -rampAngle), JPH::EMotionType::Static, s_SpikeLayerStatic),
			JPH::EActivation::DontActivate);

		// The character: Jolt's capsule stands along Y, so it is turned to stand along Z and lifted so
		// that the character's position is its feet.
		const float cylinderHalfHeight = 0.5f * (characterHeight - 2.0f * characterRadius);
		const JPH::ShapeRefC characterShape = JPH::RotatedTranslatedShapeSettings(
			JPH::Vec3(0.0f, 0.0f, 0.5f * characterHeight), JPH::Quat::sRotation(JPH::Vec3::sAxisX(), 0.5f * JPH::JPH_PI),
			new JPH::CapsuleShape(cylinderHalfHeight, characterRadius)).Create().Get();

		JPH::CharacterVirtualSettings characterSettings;
		characterSettings.mUp = up;
		characterSettings.mShape = characterShape;
		characterSettings.mMaxSlopeAngle = JPH::DegreesToRadians(50.0f);
		// Contacts below the capsule's lower sphere hold the character up (Jolt's default plane is for Y up).
		characterSettings.mSupportingVolume = JPH::Plane(up, -characterRadius);

		JPH::Ref<JPH::CharacterVirtual> character =
			new JPH::CharacterVirtual(&characterSettings, JPH::RVec3(0.0f, 0.0f, 0.0f), JPH::Quat::sIdentity(), 0, &physicsSystem);

		// Jolt's defaults for stepping up stairs and sticking to the floor are also Y up.
		JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;
		updateSettings.mStickToFloorStepDown = JPH::Vec3(0.0f, 0.0f, -0.5f);
		updateSettings.mWalkStairsStepUp = JPH::Vec3(0.0f, 0.0f, 0.4f);

		const JPH::Vec3 walkVelocity(2.0f, 0.0f, 0.0f);

		for (int step = 0; step < 300; step++)
		{
			physicsSystem.Update(stepTime, 1, &tempAllocator, &jobSystem);

			const bool onGround = character->GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;
			JPH::Vec3 velocity = walkVelocity;
			if (onGround)
			{
				velocity += character->GetGroundVelocity();
			}
			else
			{
				velocity += character->GetLinearVelocity().Dot(up) * up + gravity * stepTime;
			}
			character->SetLinearVelocity(velocity);

			character->ExtendedUpdate(stepTime, gravity, updateSettings,
				physicsSystem.GetDefaultBroadPhaseLayerFilter(s_SpikeLayerMoving),
				physicsSystem.GetDefaultLayerFilter(s_SpikeLayerMoving), JPH::BodyFilter(), JPH::ShapeFilter(), tempAllocator);
		}

		// The box should rest on the floor: its centre half a unit above z = 0.
		const JPH::RVec3 boxPosition = bodies.GetCenterOfMassPosition(boxBody);
		const bool boxRests = std::fabs(boxPosition.GetZ() - 0.5f) < 0.05f;

		// The character should have walked up the ramp: grounded, with its feet on the ramp's surface.
		const JPH::RVec3 feet = character->GetPosition();
		const float surfaceZ = (static_cast<float>(feet.GetX()) - rampStartX) * std::tan(rampAngle);
		const bool onRamp = static_cast<float>(feet.GetX()) > rampStartX + 2.0f && std::fabs(static_cast<float>(feet.GetZ()) - surfaceZ) < 0.1f;
		const bool grounded = character->GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;

		HY_CORE_INFO("Physics: Jolt self test: box z {0} (expect 0.5), character feet ({1}, {2}, {3}), ramp surface z {4}, grounded {5}",
			static_cast<float>(boxPosition.GetZ()), static_cast<float>(feet.GetX()), static_cast<float>(feet.GetY()),
			static_cast<float>(feet.GetZ()), surfaceZ, grounded);

		character = nullptr;
		bodies.RemoveBody(rampBody);
		bodies.DestroyBody(rampBody);
		bodies.RemoveBody(boxBody);
		bodies.DestroyBody(boxBody);
		bodies.RemoveBody(floorBody);
		bodies.DestroyBody(floorBody);

		return boxRests && onRamp && grounded;
	}
}
