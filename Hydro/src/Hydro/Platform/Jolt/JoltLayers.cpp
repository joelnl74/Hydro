#include "hypch.h"
#include "JoltLayers.h"

namespace Hydro
{
	JPH::ObjectLayer JoltLayers::ToObjectLayer(PhysicsLayer layer, bool moving)
	{
		return (JPH::ObjectLayer)((uint32_t)layer * 2 + (moving ? 1u : 0u));
	}

	PhysicsLayer JoltLayers::ToPhysicsLayer(JPH::ObjectLayer layer)
	{
		return (PhysicsLayer)(layer / 2);
	}

	bool JoltLayers::IsMoving(JPH::ObjectLayer layer)
	{
		return (layer % 2) == 1;
	}

	bool JoltLayers::Meet(JPH::ObjectLayer a, JPH::ObjectLayer b)
	{
		const PhysicsLayer layerA = ToPhysicsLayer(a);
		const PhysicsLayer layerB = ToPhysicsLayer(b);
		if (layerA == PhysicsLayer::Trigger || layerB == PhysicsLayer::Trigger)
		{
			return false;
		}
		return (layerA == PhysicsLayer::World && layerB == PhysicsLayer::World) == false;
	}

	JPH::uint JoltBroadPhaseLayers::GetNumBroadPhaseLayers() const
	{
		return 3;
	}

	JPH::BroadPhaseLayer JoltBroadPhaseLayers::GetBroadPhaseLayer(JPH::ObjectLayer layer) const
	{
		if (JoltLayers::ToPhysicsLayer(layer) == PhysicsLayer::Trigger)
		{
			return JPH::BroadPhaseLayer(JoltLayers::BroadPhaseTrigger);
		}
		return JPH::BroadPhaseLayer(JoltLayers::IsMoving(layer) ? JoltLayers::BroadPhaseMoving : JoltLayers::BroadPhaseStatic);
	}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
	const char* JoltBroadPhaseLayers::GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const
	{
		switch (layer.GetValue())
		{
			case JoltLayers::BroadPhaseStatic: return "Static";
			case JoltLayers::BroadPhaseMoving: return "Moving";
			default: return "Trigger";
		}
	}
#endif

	bool JoltObjectLayerPairFilter::ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const
	{
		return JoltLayers::Meet(a, b);
	}

	bool JoltObjectVsBroadPhaseFilter::ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broadPhaseLayer) const
	{
		const PhysicsLayer physicsLayer = JoltLayers::ToPhysicsLayer(layer);
		if (physicsLayer == PhysicsLayer::Trigger || broadPhaseLayer.GetValue() == JoltLayers::BroadPhaseTrigger)
		{
			return false;
		}
		// The world only meets what moves and isn't world; Meet sorts out the rest.
		return physicsLayer != PhysicsLayer::World || broadPhaseLayer.GetValue() == JoltLayers::BroadPhaseMoving;
	}

	JoltLayerMaskFilter::JoltLayerMaskFilter(uint32_t mask)
		: m_Mask(mask)
	{
	}

	bool JoltLayerMaskFilter::ShouldCollide(JPH::ObjectLayer layer) const
	{
		return ((m_Mask >> (uint32_t)JoltLayers::ToPhysicsLayer(layer)) & 1u) != 0;
	}
}
