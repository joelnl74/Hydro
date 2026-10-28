#include "hypch.h"
#include "PhysicsScene3D.h"

#include "Hydro/Core/Assert.h"
#include "Hydro/Scene/ColliderShapes.h"
#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/SystemContext.h"
#include "Hydro/Scene/Scripting/ScriptableEntity.h"

#include <Jolt/Jolt.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Hydro
{
	// ---- Layers ----------------------------------------------------------------------------------------------
	// A Jolt object layer is the PhysicsLayer and whether the body moves: layer * 2 + moving. Broad-phase layers
	// keep what never moves, what does, and triggers (only queries look at those) in separate trees.

	static constexpr JPH::uint s_PhysicsLayerCount = 4;
	static constexpr JPH::uint s_ObjectLayerCount = s_PhysicsLayerCount * 2;
	static constexpr JPH::uint8 s_BroadPhaseStatic = 0;
	static constexpr JPH::uint8 s_BroadPhaseMoving = 1;
	static constexpr JPH::uint8 s_BroadPhaseTrigger = 2;

	static JPH::ObjectLayer ToObjectLayer(PhysicsLayer layer, bool moving)
	{
		return (JPH::ObjectLayer)((uint32_t)layer * 2 + (moving ? 1u : 0u));
	}

	static PhysicsLayer ToPhysicsLayer(JPH::ObjectLayer layer)
	{
		return (PhysicsLayer)(layer / 2);
	}

	static bool IsMoving(JPH::ObjectLayer layer)
	{
		return (layer % 2) == 1;
	}

	class PhysicsScene3DBroadPhaseLayers final : public JPH::BroadPhaseLayerInterface
	{
	public:
		JPH::uint GetNumBroadPhaseLayers() const override
		{
			return 3;
		}

		JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
		{
			if (ToPhysicsLayer(layer) == PhysicsLayer::Trigger)
			{
				return JPH::BroadPhaseLayer(s_BroadPhaseTrigger);
			}
			return JPH::BroadPhaseLayer(IsMoving(layer) ? s_BroadPhaseMoving : s_BroadPhaseStatic);
		}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
		const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override
		{
			switch (layer.GetValue())
			{
				case s_BroadPhaseStatic: return "Static";
				case s_BroadPhaseMoving: return "Moving";
				default: return "Trigger";
			}
		}
#endif
	};

	// Which object layers meet, during the simulation and for characters. Triggers meet nothing (queries find them),
	// the world doesn't meet itself (static and game-moved bodies never push each other), everything else meets.
	static bool LayersMeet(JPH::ObjectLayer a, JPH::ObjectLayer b)
	{
		const PhysicsLayer layerA = ToPhysicsLayer(a);
		const PhysicsLayer layerB = ToPhysicsLayer(b);
		if (layerA == PhysicsLayer::Trigger || layerB == PhysicsLayer::Trigger)
		{
			return false;
		}
		return (layerA == PhysicsLayer::World && layerB == PhysicsLayer::World) == false;
	}

	class PhysicsScene3DObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter
	{
	public:
		bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override
		{
			return LayersMeet(a, b);
		}
	};

	class PhysicsScene3DObjectVsBroadPhaseFilter final : public JPH::ObjectVsBroadPhaseLayerFilter
	{
	public:
		bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broadPhaseLayer) const override
		{
			const PhysicsLayer physicsLayer = ToPhysicsLayer(layer);
			if (physicsLayer == PhysicsLayer::Trigger || broadPhaseLayer.GetValue() == s_BroadPhaseTrigger)
			{
				return false;
			}
			// The world only meets what moves and isn't world; LayersMeet sorts out the rest.
			return physicsLayer != PhysicsLayer::World || broadPhaseLayer.GetValue() == s_BroadPhaseMoving;
		}
	};

	// Queries: the object layers whose PhysicsLayer is in a mask (bit 1 << layer).
	class PhysicsScene3DLayerMaskFilter final : public JPH::ObjectLayerFilter
	{
	public:
		explicit PhysicsScene3DLayerMaskFilter(uint32_t mask)
			: m_Mask(mask)
		{
		}

		bool ShouldCollide(JPH::ObjectLayer layer) const override
		{
			return ((m_Mask >> (uint32_t)ToPhysicsLayer(layer)) & 1u) != 0;
		}

	private:
		uint32_t m_Mask = 0;
	};

	// ---- Data ------------------------------------------------------------------------------------------------

	struct PhysicsScene3D::Data
	{
		struct EntityBodies
		{
			JPH::BodyID Solid;
			JPH::BodyID Trigger;
			bool Kinematic = false;
			glm::vec3 Translation{ 0.0f }; // the transform the bodies were last put at (kinematic)
			glm::vec3 Rotation{ 0.0f };
		};

		struct Character
		{
			entt::entity Handle{ entt::null };
			JPH::Ref<JPH::CharacterVirtual> Body;
			JPH::ObjectLayer Layer = 0;
			float Radius = 0.25f;
			float Height = 1.4f;
			float StepHeight = 0.35f;
			bool StaysOnLedges = false;
			glm::vec3 Desired{ 0.0f };
			glm::vec3 Previous{ 0.0f }; // positions after the last two steps, blended for the transform
			glm::vec3 Current{ 0.0f };
			glm::vec3 Written{ 0.0f };  // what was last written to the transform: anything else is a teleport
			std::vector<entt::entity> Triggers; // the triggers it is inside
		};

		Data()
			: TempAllocator(4 * 1024 * 1024), JobSystem(JPH::cMaxPhysicsJobs)
		{
		}

		// The layer interfaces must outlive the PhysicsSystem (it keeps references): declared first.
		PhysicsScene3DBroadPhaseLayers BroadPhaseLayers;
		PhysicsScene3DObjectVsBroadPhaseFilter ObjectVsBroadPhaseFilter;
		PhysicsScene3DObjectLayerPairFilter ObjectLayerPairFilter;
		JPH::TempAllocatorImpl TempAllocator;
		JPH::JobSystemSingleThreaded JobSystem;
		JPH::PhysicsSystem System;

		std::unordered_map<entt::entity, EntityBodies> Bodies;
		std::vector<Character> Characters;
		std::unordered_map<entt::entity, size_t> CharacterIndex; // entity -> position in Characters
		std::unordered_set<entt::entity> WarnedMissingModel;
		size_t BodyCount = 0;
		float Accumulator = 0.0f;
	};

	// ---- Conversions -----------------------------------------------------------------------------------------

	static JPH::Vec3 ToJolt(const glm::vec3& value)
	{
		return JPH::Vec3(value.x, value.y, value.z);
	}

	static JPH::RVec3 ToJoltPosition(const glm::vec3& value)
	{
		return JPH::RVec3(value.x, value.y, value.z);
	}

	static glm::vec3 ToGlm(JPH::Vec3Arg value)
	{
		return { value.GetX(), value.GetY(), value.GetZ() };
	}

	static glm::vec3 ToGlmPosition(JPH::RVec3Arg value)
	{
		return { (float)value.GetX(), (float)value.GetY(), (float)value.GetZ() };
	}

	// The transform's rotation (Euler angles in radians, as TransformComponent::GetTransform uses them).
	static JPH::Quat ToJoltRotation(const glm::vec3& euler)
	{
		const glm::quat rotation(euler);
		return JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w).Normalized();
	}

	static entt::entity GetBodyEntity(const JPH::BodyInterface& bodies, const JPH::BodyID& id)
	{
		return (entt::entity)(uint32_t)bodies.GetUserData(id);
	}

	// Jolt's capsules and cylinders stand along Y; Hydro's along Z.
	static JPH::Quat StandUp()
	{
		return JPH::Quat::sRotation(JPH::Vec3::sAxisX(), 0.5f * JPH::JPH_PI);
	}

	// A convex radius rounds a shape's edges; it must fit inside the shape.
	static float ConvexRadiusFor(float smallestHalfSize)
	{
		return std::min(JPH::cDefaultConvexRadius, 0.5f * smallestHalfSize);
	}

	// The Jolt shape of the entity's shapes that are (or aren't) triggers, or null when there are none.
	static JPH::ShapeRefC MakeShape(const std::vector<ColliderShape>& shapes, bool triggers, const char* entityName)
	{
		JPH::StaticCompoundShapeSettings compound;
		uint32_t count = 0;
		for (const ColliderShape& shape : shapes)
		{
			if (shape.IsTrigger != triggers)
			{
				continue;
			}

			JPH::Quat rotation = JPH::Quat::sIdentity();
			JPH::Vec3 position = ToJolt(shape.Centre);
			JPH::Ref<JPH::Shape> part;
			switch (shape.Type)
			{
				case ColliderShapeType::Box:
				{
					const float smallest = std::min(shape.HalfExtents.x, std::min(shape.HalfExtents.y, shape.HalfExtents.z));
					part = new JPH::BoxShape(ToJolt(shape.HalfExtents), ConvexRadiusFor(smallest));
					break;
				}
				case ColliderShapeType::Sphere:
					part = new JPH::SphereShape(shape.Radius);
					break;
				case ColliderShapeType::Capsule:
					part = new JPH::CapsuleShape(shape.HalfHeight, shape.Radius);
					rotation = StandUp();
					break;
				case ColliderShapeType::Cylinder:
					part = new JPH::CylinderShape(shape.HalfHeight, shape.Radius, ConvexRadiusFor(std::min(shape.HalfHeight, shape.Radius)));
					rotation = StandUp();
					break;
				case ColliderShapeType::TriangleMesh:
				{
					// Walkable ground as it was modelled (a bridge's arch, a slope): hollow triangles, which only
					// static and game-moved bodies may have (which is all there are).
					JPH::VertexList vertices;
					vertices.reserve(shape.Points.size());
					for (const glm::vec3& point : shape.Points)
					{
						vertices.push_back(JPH::Float3(point.x, point.y, point.z));
					}
					JPH::IndexedTriangleList triangles;
					triangles.reserve(shape.Indices.size() / 3);
					for (size_t i = 0; i + 2 < shape.Indices.size(); i += 3)
					{
						triangles.push_back(JPH::IndexedTriangle(shape.Indices[i], shape.Indices[i + 1], shape.Indices[i + 2]));
					}
					position = JPH::Vec3::sZero();
					const JPH::ShapeSettings::ShapeResult result = JPH::MeshShapeSettings(std::move(vertices), std::move(triangles)).Create();
					if (result.HasError())
					{
						HY_CORE_WARN("Physics 3D: '{}': a walk mesh could not be built ({})", entityName, result.GetError().c_str());
						continue;
					}
					part = result.Get();
					break;
				}
				case ColliderShapeType::Prism:
				case ColliderShapeType::ConvexHull:
				{
					JPH::Array<JPH::Vec3> points;
					for (const glm::vec3& point : shape.Points)
					{
						points.push_back(ToJolt(point));
					}
					// The points are where the shape is: no extra offset. A sharp hull (no rounding) keeps ramps exact.
					position = JPH::Vec3::sZero();
					const JPH::ShapeSettings::ShapeResult result = JPH::ConvexHullShapeSettings(points, 0.0f).Create();
					if (result.HasError())
					{
						HY_CORE_WARN("Physics 3D: '{}': a collision shape could not be built ({})", entityName, result.GetError().c_str());
						continue;
					}
					part = result.Get();
					break;
				}
			}

			compound.AddShape(position, rotation, part);
			count++;
		}

		if (count == 0)
		{
			return nullptr;
		}

		const JPH::ShapeSettings::ShapeResult result = compound.Create();
		if (result.HasError())
		{
			HY_CORE_WARN("Physics 3D: '{}': its collision shapes could not be combined ({})", entityName, result.GetError().c_str());
			return nullptr;
		}
		return result.Get();
	}

	// ---- PhysicsScene3D --------------------------------------------------------------------------------------

	PhysicsScene3D::PhysicsScene3D(Scene& scene)
		: m_Data(CreateScope<Data>()), m_Scene(&scene)
	{
		// Room for the largest field scene with margin; Jolt allocates these tables up front.
		const JPH::uint maxBodies = 16384;
		const JPH::uint maxBodyPairs = 8192;
		const JPH::uint maxContactConstraints = 4096;
		m_Data->System.Init(maxBodies, 0, maxBodyPairs, maxContactConstraints, m_Data->BroadPhaseLayers,
			m_Data->ObjectVsBroadPhaseFilter, m_Data->ObjectLayerPairFilter);
		m_Data->System.SetGravity(JPH::Vec3(0.0f, 0.0f, Gravity));

		// The entities that exist when the game starts, in scene order. Later ones get theirs in Step.
		for (entt::entity handle : scene.GetEntityOrder())
		{
			CreateBodies(handle);
		}
		// Most bodies were just added and never move: build the broad phase's trees once, now.
		m_Data->System.OptimizeBroadPhase();
	}

	PhysicsScene3D::~PhysicsScene3D()
	{
		if (m_Data == nullptr)
		{
			return;
		}

		m_Data->Characters.clear();
		JPH::BodyInterface& bodies = m_Data->System.GetBodyInterface();
		for (const auto& [handle, entry] : m_Data->Bodies)
		{
			for (const JPH::BodyID& id : { entry.Solid, entry.Trigger })
			{
				if (id.IsInvalid() == false)
				{
					bodies.RemoveBody(id);
					bodies.DestroyBody(id);
				}
			}
		}
		m_Data->Bodies.clear();
	}

	bool PhysicsScene3D::CreateBodies(entt::entity handle)
	{
		entt::registry& registry = m_Scene->m_Registry;
		if (registry.valid(handle) == false || registry.has<TransformComponent>(handle) == false)
		{
			return false;
		}
		// About to be destroyed (at the end of the stage that is running): never gets a body.
		if (m_Scene->IsPendingDestroy(Entity{ handle, m_Scene }))
		{
			return false;
		}

		const TransformComponent& transform = registry.get<TransformComponent>(handle);
		const char* name = registry.has<TagComponent>(handle) ? registry.get<TagComponent>(handle).Tag.c_str() : "Entity";
		bool created = false;

		// A character: its capsule stands on the entity's position. The transform's scale (a sprite's size) is not used.
		if (registry.has<CharacterBodyComponent>(handle))
		{
			if (HasCharacter(handle))
			{
				return false;
			}

			const CharacterBodyComponent& component = registry.get<CharacterBodyComponent>(handle);
			Data::Character character;
			character.Handle = handle;
			character.Radius = std::max(component.Radius, 0.05f);
			character.Height = std::max(component.Height, 2.0f * character.Radius + 0.05f);
			character.StepHeight = std::max(component.StepHeight, 0.0f);
			character.StaysOnLedges = component.StaysOnLedges;
			character.Layer = ToObjectLayer(component.Layer == PhysicsLayer::Trigger ? PhysicsLayer::Player : component.Layer, true);

			const float cylinderHalfHeight = 0.5f * character.Height - character.Radius;
			const JPH::ShapeSettings::ShapeResult shape = JPH::RotatedTranslatedShapeSettings(JPH::Vec3(0.0f, 0.0f, 0.5f * character.Height), StandUp(),
				new JPH::CapsuleShape(cylinderHalfHeight, character.Radius)).Create();
			if (shape.HasError())
			{
				HY_CORE_WARN("Physics 3D: '{}': its character capsule could not be built ({})", name, shape.GetError().c_str());
				return false;
			}

			JPH::CharacterVirtualSettings settings;
			settings.mUp = JPH::Vec3::sAxisZ();
			settings.mShape = shape.Get();
			settings.mMaxSlopeAngle = JPH::DegreesToRadians(std::clamp(component.MaxSlopeDegrees, 0.0f, 89.0f));
			// Contacts below the lower half sphere's centre hold the character up (Jolt's default plane is for Y up).
			settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisZ(), -character.Radius);

			character.Current = transform.Translation;
			character.Previous = transform.Translation;
			character.Written = transform.Translation;
			character.Body = new JPH::CharacterVirtual(&settings, ToJoltPosition(transform.Translation), JPH::Quat::sIdentity(),
				(JPH::uint64)(uint32_t)handle, &m_Data->System);

			m_Data->CharacterIndex[handle] = m_Data->Characters.size();
			m_Data->Characters.push_back(std::move(character));
			// A character's own colliders would hold it in place: it has none.
			return true;
		}

		if (HasBody(handle) || ColliderShapes::HasColliders(registry, handle) == false)
		{
			return false;
		}

		std::vector<ColliderShape> shapes;
		if (ColliderShapes::Get(registry, handle, shapes) == false)
		{
			// A model that isn't loaded yet: try again next step (say so once, in case it never is).
			if (m_Data->WarnedMissingModel.insert(handle).second)
			{
				HY_CORE_WARN("Physics 3D: '{}': its model isn't loaded; it collides once it is", name);
			}
			return false;
		}
		if (shapes.empty())
		{
			return false;
		}

		const bool kinematic = registry.has<NativeScriptComponent>(handle);
		const JPH::RVec3 position = ToJoltPosition(transform.Translation);
		const JPH::Quat rotation = ToJoltRotation(transform.Rotation);
		JPH::BodyInterface& bodies = m_Data->System.GetBodyInterface();

		Data::EntityBodies entry;
		entry.Kinematic = kinematic;
		entry.Translation = transform.Translation;
		entry.Rotation = transform.Rotation;

		for (bool triggers : { false, true })
		{
			const JPH::ShapeRefC shape = MakeShape(shapes, triggers, name);
			if (shape == nullptr)
			{
				continue;
			}

			// The first solid shape's layer is the body's (one layer per entity).
			PhysicsLayer layer = PhysicsLayer::Trigger;
			if (triggers == false)
			{
				const auto solid = std::find_if(shapes.begin(), shapes.end(), [](const ColliderShape& s) { return s.IsTrigger == false; });
				layer = solid->Layer == PhysicsLayer::Trigger ? PhysicsLayer::World : solid->Layer;
			}

			JPH::BodyCreationSettings settings(shape, position, rotation, kinematic ? JPH::EMotionType::Kinematic : JPH::EMotionType::Static,
				ToObjectLayer(layer, kinematic));
			settings.mIsSensor = triggers;
			settings.mUserData = (JPH::uint64)(uint32_t)handle;

			const JPH::BodyID id = bodies.CreateAndAddBody(settings, JPH::EActivation::DontActivate);
			if (id.IsInvalid())
			{
				HY_CORE_WARN("Physics 3D: '{}': no room for another body", name);
				continue;
			}
			(triggers ? entry.Trigger : entry.Solid) = id;
			m_Data->BodyCount++;
			created = true;
		}

		if (created)
		{
			m_Data->Bodies[handle] = entry;
			m_Data->WarnedMissingModel.erase(handle);
		}
		return created;
	}

	void PhysicsScene3D::CreateNewBodies()
	{
		// Entities that got their components after the world was built. Only entities with a 3D collider or a
		// character are looked at; CreateBodies skips the ones that have theirs.
		entt::registry& registry = m_Scene->m_Registry;
		for (entt::entity handle : registry.view<CharacterBodyComponent>())
		{
			CreateBodies(handle);
		}
		for (entt::entity handle : registry.view<BoxColliderComponent>())
		{
			CreateBodies(handle);
		}
		for (entt::entity handle : registry.view<SphereColliderComponent>())
		{
			CreateBodies(handle);
		}
		for (entt::entity handle : registry.view<CapsuleColliderComponent>())
		{
			CreateBodies(handle);
		}
		for (entt::entity handle : registry.view<CylinderColliderComponent>())
		{
			CreateBodies(handle);
		}
		for (entt::entity handle : registry.view<MeshColliderComponent>())
		{
			CreateBodies(handle);
		}
	}

	void PhysicsScene3D::MoveKinematicBodies()
	{
		entt::registry& registry = m_Scene->m_Registry;
		JPH::BodyInterface& bodies = m_Data->System.GetBodyInterface();
		for (auto& [handle, entry] : m_Data->Bodies)
		{
			if (entry.Kinematic == false || registry.valid(handle) == false || registry.has<TransformComponent>(handle) == false)
			{
				continue;
			}

			const TransformComponent& transform = registry.get<TransformComponent>(handle);
			if (transform.Translation == entry.Translation && transform.Rotation == entry.Rotation)
			{
				continue;
			}

			// The game moved it: the bodies are put where it is. Nothing is pushed by them, so no velocity is needed.
			const JPH::RVec3 position = ToJoltPosition(transform.Translation);
			const JPH::Quat rotation = ToJoltRotation(transform.Rotation);
			for (const JPH::BodyID& id : { entry.Solid, entry.Trigger })
			{
				if (id.IsInvalid() == false)
				{
					bodies.SetPositionAndRotation(id, position, rotation, JPH::EActivation::DontActivate);
				}
			}
			entry.Translation = transform.Translation;
			entry.Rotation = transform.Rotation;
		}
	}

	void PhysicsScene3D::StepCharacters(float fixed)
	{
		const JPH::Vec3 gravity(0.0f, 0.0f, Gravity);
		const JPH::Vec3 up = JPH::Vec3::sAxisZ();

		for (Data::Character& character : m_Data->Characters)
		{
			JPH::CharacterVirtual& body = *character.Body;
			character.Previous = character.Current;

			const auto broadPhaseFilter = m_Data->System.GetDefaultBroadPhaseLayerFilter(character.Layer);
			const auto layerFilter = m_Data->System.GetDefaultLayerFilter(character.Layer);

			// Jolt's defaults for stepping up stairs and sticking to the floor are for Y up.
			JPH::CharacterVirtual::ExtendedUpdateSettings update;
			update.mStickToFloorStepDown = JPH::Vec3(0.0f, 0.0f, -character.StepHeight);
			update.mWalkStairsStepUp = JPH::Vec3(0.0f, 0.0f, character.StepHeight);

			const bool wasOnGround = body.GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;
			const JPH::RVec3 start = body.GetPosition();
			const JPH::Vec3 startVelocity = body.GetLinearVelocity();

			// Ground right under the feet, within a step. The ledge guard trusts this rather than the ground state:
			// the rounded capsule still stands on a terrace's corner with its middle over the drop, and just inside
			// the edge it can count as not supported while there is ground under it.
			const auto groundUnder = [&](JPH::RVec3Arg feet)
			{
				const JPH::RRayCast down{ feet + JPH::Vec3(0.0f, 0.0f, 0.1f), JPH::Vec3(0.0f, 0.0f, -(character.StepHeight + 0.15f)) };
				JPH::RayCastResult hit;
				return m_Data->System.GetNarrowPhaseQuery().CastRay(down, hit, broadPhaseFilter, layerFilter);
			};
			const bool guarded = character.StaysOnLedges && groundUnder(start);

			// One move with `walk` (the wanted velocity along the ground). On the ground the character also moves with
			// what it stands on; in the air gravity keeps pulling it down. False when it would have walked off a ledge
			// it must stay on: the move is undone.
			const auto tryWalk = [&](const glm::vec2& walk)
			{
				JPH::Vec3 velocity(walk.x, walk.y, 0.0f);
				if (wasOnGround)
				{
					velocity += body.GetGroundVelocity();
				}
				else
				{
					velocity += startVelocity.Dot(up) * up + gravity * fixed;
				}
				body.SetLinearVelocity(velocity);
				body.ExtendedUpdate(fixed, gravity, update, broadPhaseFilter, layerFilter, JPH::BodyFilter(), JPH::ShapeFilter(), m_Data->TempAllocator);

				const bool walking = walk.x != 0.0f || walk.y != 0.0f;
				if (guarded == false || walking == false || groundUnder(body.GetPosition()))
				{
					return true;
				}

				body.SetPosition(start);
				body.SetLinearVelocity(startVelocity);
				body.RefreshContacts(broadPhaseFilter, layerFilter, JPH::BodyFilter(), JPH::ShapeFilter(), m_Data->TempAllocator);
				return false;
			};

			// At a ledge: slide along it on one axis, else stop (as DioramaWalk did before the character body).
			const glm::vec2 desired{ character.Desired.x, character.Desired.y };
			if (tryWalk(desired) == false && tryWalk({ desired.x, 0.0f }) == false && tryWalk({ 0.0f, desired.y }) == false)
			{
				tryWalk(glm::vec2(0.0f));
			}

			character.Current = ToGlmPosition(body.GetPosition());
		}
	}

	void PhysicsScene3D::CollectTriggerEvents()
	{
		const JPH::NarrowPhaseQuery& query = m_Data->System.GetNarrowPhaseQuery();
		const JPH::BodyInterface& bodies = m_Data->System.GetBodyInterface();
		const PhysicsScene3DLayerMaskFilter triggersOnly(1u << (uint32_t)PhysicsLayer::Trigger);

		for (Data::Character& character : m_Data->Characters)
		{
			JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
			JPH::CollideShapeSettings settings;
			query.CollideShape(character.Body->GetShape(), JPH::Vec3::sOne(), character.Body->GetCenterOfMassTransform(), settings,
				JPH::RVec3::sZero(), collector, JPH::BroadPhaseLayerFilter(), triggersOnly);

			std::vector<entt::entity> inside;
			for (const JPH::CollideShapeResult& hit : collector.mHits)
			{
				const entt::entity trigger = GetBodyEntity(bodies, hit.mBodyID2);
				if (std::find(inside.begin(), inside.end(), trigger) == inside.end())
				{
					inside.push_back(trigger);
				}
			}

			for (entt::entity trigger : inside)
			{
				if (std::find(character.Triggers.begin(), character.Triggers.end(), trigger) == character.Triggers.end())
				{
					m_Events.push_back({ Event::Type::TriggerBegin, trigger, character.Handle });
				}
			}
			for (entt::entity trigger : character.Triggers)
			{
				if (std::find(inside.begin(), inside.end(), trigger) == inside.end())
				{
					m_Events.push_back({ Event::Type::TriggerEnd, trigger, character.Handle });
				}
			}
			character.Triggers = std::move(inside);
		}
	}

	void PhysicsScene3D::WriteCharacterTransforms(float blend)
	{
		entt::registry& registry = m_Scene->m_Registry;
		for (Data::Character& character : m_Data->Characters)
		{
			if (registry.valid(character.Handle) == false || registry.has<TransformComponent>(character.Handle) == false)
			{
				continue;
			}
			const glm::vec3 position = glm::mix(character.Previous, character.Current, blend);
			registry.get<TransformComponent>(character.Handle).Translation = position;
			character.Written = position;
		}
	}

	void PhysicsScene3D::Step(const SystemContext& context)
	{
		if (context.IsPlaying == false || context.FixedDeltaTime <= 0.0f)
		{
			return;
		}

		m_Events.clear();

		// Bodies and characters for entities that appeared since the last update.
		CreateNewBodies();
		MoveKinematicBodies();

		// A character whose transform isn't what physics wrote was moved by the game (a spawn point, a scene
		// change): it is teleported there, and stops falling.
		entt::registry& registry = m_Scene->m_Registry;
		for (Data::Character& character : m_Data->Characters)
		{
			if (registry.valid(character.Handle) == false || registry.has<TransformComponent>(character.Handle) == false)
			{
				continue;
			}
			const glm::vec3& translation = registry.get<TransformComponent>(character.Handle).Translation;
			if (glm::length(translation - character.Written) > 1.0e-4f)
			{
				character.Body->SetPosition(ToJoltPosition(translation));
				character.Body->SetLinearVelocity(JPH::Vec3::sZero());
				character.Previous = translation;
				character.Current = translation;
				character.Written = translation;
			}
		}

		const float fixed = context.FixedDeltaTime;
		m_Data->Accumulator += context.DeltaTime;

		int steps = 0;
		while (m_Data->Accumulator >= fixed && steps < MaxStepsPerFrame)
		{
			m_Data->Accumulator -= fixed;
			steps++;

			m_Data->System.Update(fixed, 1, &m_Data->TempAllocator, &m_Data->JobSystem);
			StepCharacters(fixed);
			CollectTriggerEvents();
		}
		if (m_Data->Accumulator >= fixed)
		{
			// A long frame (a breakpoint, a hitch): drop the backlog instead of running behind forever.
			m_Data->Accumulator = 0.0f;
		}

		// Interpolation: the transform is between the last two steps by how far the time is into the next one, so
		// a frame without a step still moves (one step behind the simulation).
		WriteCharacterTransforms(std::clamp(m_Data->Accumulator / fixed, 0.0f, 1.0f));
	}

	void PhysicsScene3D::OnEntityDestroyed(entt::entity handle)
	{
		const auto bodies = m_Data->Bodies.find(handle);
		if (bodies != m_Data->Bodies.end())
		{
			JPH::BodyInterface& bodyInterface = m_Data->System.GetBodyInterface();
			for (const JPH::BodyID& id : { bodies->second.Solid, bodies->second.Trigger })
			{
				if (id.IsInvalid() == false)
				{
					bodyInterface.RemoveBody(id);
					bodyInterface.DestroyBody(id);
					m_Data->BodyCount--;
				}
			}
			m_Data->Bodies.erase(bodies);
		}

		const auto found = m_Data->CharacterIndex.find(handle);
		if (found != m_Data->CharacterIndex.end())
		{
			// Swap-remove, keeping the index map in step.
			const size_t index = found->second;
			const size_t last = m_Data->Characters.size() - 1;
			if (index != last)
			{
				m_Data->Characters[index] = std::move(m_Data->Characters[last]);
				m_Data->CharacterIndex[m_Data->Characters[index].Handle] = index;
			}
			m_Data->Characters.pop_back();
			m_Data->CharacterIndex.erase(handle);
		}

		// Characters inside a trigger that is gone are no longer in it (no event: the trigger isn't there to get one).
		for (Data::Character& character : m_Data->Characters)
		{
			character.Triggers.erase(std::remove(character.Triggers.begin(), character.Triggers.end(), handle), character.Triggers.end());
		}
		m_Data->WarnedMissingModel.erase(handle);
	}

	bool PhysicsScene3D::HasBody(entt::entity handle) const
	{
		return m_Data->Bodies.find(handle) != m_Data->Bodies.end();
	}

	bool PhysicsScene3D::HasCharacter(entt::entity handle) const
	{
		return m_Data->CharacterIndex.find(handle) != m_Data->CharacterIndex.end();
	}

	size_t PhysicsScene3D::GetBodyCount() const
	{
		return m_Data->BodyCount;
	}

	size_t PhysicsScene3D::GetCharacterCount() const
	{
		return m_Data->Characters.size();
	}

	bool PhysicsScene3D::SetCharacterVelocity(entt::entity handle, const glm::vec3& velocity)
	{
		if (HasCharacter(handle) == false)
		{
			CreateBodies(handle);
		}
		const auto found = m_Data->CharacterIndex.find(handle);
		if (found == m_Data->CharacterIndex.end())
		{
			return false;
		}
		m_Data->Characters[found->second].Desired = glm::vec3(velocity.x, velocity.y, 0.0f);
		return true;
	}

	bool PhysicsScene3D::GetCharacterGround(entt::entity handle, CharacterGround& ground) const
	{
		const auto found = m_Data->CharacterIndex.find(handle);
		if (found == m_Data->CharacterIndex.end())
		{
			return false;
		}

		const JPH::CharacterVirtual& body = *m_Data->Characters[found->second].Body;
		ground = CharacterGround();
		ground.IsGrounded = body.GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;
		if (body.GetGroundState() != JPH::CharacterBase::EGroundState::InAir)
		{
			ground.Normal = ToGlm(body.GetGroundNormal());
			ground.SlopeDegrees = glm::degrees(std::acos(std::clamp(ground.Normal.z, -1.0f, 1.0f)));
			if (body.GetGroundBodyID().IsInvalid() == false)
			{
				ground.GroundEntity = Entity{ GetBodyEntity(m_Data->System.GetBodyInterface(), body.GetGroundBodyID()), m_Scene };
			}
		}
		return true;
	}

	bool PhysicsScene3D::GetCharacterCapsule(entt::entity handle, glm::vec3& feet, float& radius, float& height) const
	{
		const auto found = m_Data->CharacterIndex.find(handle);
		if (found == m_Data->CharacterIndex.end())
		{
			return false;
		}
		const Data::Character& character = m_Data->Characters[found->second];
		feet = character.Current;
		radius = character.Radius;
		height = character.Height;
		return true;
	}

	bool PhysicsScene3D::Raycast(const glm::vec3& from, const glm::vec3& to, uint32_t layerMask, RaycastHit3D& hit) const
	{
		const glm::vec3 direction = to - from;
		if (glm::dot(direction, direction) <= 0.0f)
		{
			return false;
		}

		const JPH::RRayCast ray{ ToJoltPosition(from), ToJolt(direction) };
		JPH::RayCastResult result;
		const PhysicsScene3DLayerMaskFilter layers(layerMask);
		if (m_Data->System.GetNarrowPhaseQuery().CastRay(ray, result, JPH::BroadPhaseLayerFilter(), layers) == false)
		{
			return false;
		}

		const JPH::RVec3 point = ray.GetPointOnRay(result.mFraction);
		hit = RaycastHit3D();
		hit.Point = ToGlmPosition(point);
		hit.Distance = result.mFraction * glm::length(direction);

		const JPH::BodyLockRead lock(m_Data->System.GetBodyLockInterface(), result.mBodyID);
		if (lock.Succeeded())
		{
			const JPH::Body& body = lock.GetBody();
			hit.Normal = ToGlm(body.GetWorldSpaceSurfaceNormal(result.mSubShapeID2, point));
			hit.HitEntity = Entity{ (entt::entity)(uint32_t)body.GetUserData(), m_Scene };
		}
		return true;
	}

	void PhysicsScene3D::OverlapSphere(const glm::vec3& centre, float radius, uint32_t layerMask, std::vector<Entity>& entities) const
	{
		if (radius <= 0.0f)
		{
			return;
		}

		JPH::SphereShape sphere(radius);
		sphere.SetEmbedded(); // on the stack: never reference counted
		JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
		const PhysicsScene3DLayerMaskFilter layers(layerMask);
		m_Data->System.GetNarrowPhaseQuery().CollideShape(&sphere, JPH::Vec3::sOne(), JPH::RMat44::sTranslation(ToJoltPosition(centre)),
			JPH::CollideShapeSettings(), JPH::RVec3::sZero(), collector, JPH::BroadPhaseLayerFilter(), layers);

		const JPH::BodyInterface& bodies = m_Data->System.GetBodyInterface();
		for (const JPH::CollideShapeResult& hit : collector.mHits)
		{
			const entt::entity handle = GetBodyEntity(bodies, hit.mBodyID2);
			const auto same = [handle](const Entity& entity) { return (entt::entity)entity == handle; };
			if (std::find_if(entities.begin(), entities.end(), same) == entities.end())
			{
				entities.push_back(Entity{ handle, m_Scene });
			}
		}
	}
}
