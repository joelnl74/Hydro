#pragma once

#include "Hydro/Scene/Components/Components.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

#include <cstdint>

// Hydro's PhysicsLayer as Jolt's object and broad-phase layers, and the filters that say which meet.

namespace Hydro
{
	// A Jolt object layer is the PhysicsLayer and whether the body moves: layer * 2 + moving. Broad-phase layers
	// keep what never moves, what does, and triggers (only queries look at those) in separate trees.
	class JoltLayers
	{
	public:
		static constexpr JPH::uint PhysicsLayerCount = 4;
		static constexpr JPH::uint ObjectLayerCount = PhysicsLayerCount * 2;
		static constexpr JPH::uint8 BroadPhaseStatic = 0;
		static constexpr JPH::uint8 BroadPhaseMoving = 1;
		static constexpr JPH::uint8 BroadPhaseTrigger = 2;

		static JPH::ObjectLayer ToObjectLayer(PhysicsLayer layer, bool moving);
		static PhysicsLayer ToPhysicsLayer(JPH::ObjectLayer layer);
		static bool IsMoving(JPH::ObjectLayer layer);

		// Which object layers meet, during the simulation and for characters. Triggers meet nothing (queries find
		// them), the world doesn't meet itself (static and game-moved bodies never push each other).
		static bool Meet(JPH::ObjectLayer a, JPH::ObjectLayer b);
	};

	class JoltBroadPhaseLayers final : public JPH::BroadPhaseLayerInterface
	{
	public:
		JPH::uint GetNumBroadPhaseLayers() const override;
		JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override;
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
		const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override;
#endif
	};

	class JoltObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter
	{
	public:
		bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override;
	};

	class JoltObjectVsBroadPhaseFilter final : public JPH::ObjectVsBroadPhaseLayerFilter
	{
	public:
		bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broadPhaseLayer) const override;
	};

	// Queries: the object layers whose PhysicsLayer is in a mask (bit 1 << layer).
	class JoltLayerMaskFilter final : public JPH::ObjectLayerFilter
	{
	public:
		explicit JoltLayerMaskFilter(uint32_t mask);
		bool ShouldCollide(JPH::ObjectLayer layer) const override;

	private:
		uint32_t m_Mask = 0;
	};
}
