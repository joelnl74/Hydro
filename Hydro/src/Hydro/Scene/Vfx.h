#pragma once
#include "Hydro/Asset/Asset.h"
#include "Hydro/Scene/Entity.h"

#include <glm/glm.hpp>

namespace Hydro
{
	class Scene;

	// Plays visual effects (prefabs with a ParticleEmitter2DComponent) from game code. Load the prefab once in OnCreate
	// (AssetManager::GetAsset<Prefab>), then Vfx::Play never reads the disk.
	class Vfx
	{
	public:
		// Instantiates the effect at `position` (safe in scripts and callbacks) as a one-shot that destroys itself when
		// done, so give effects a non-looping emitter. Never loads: an unloaded prefab logs once and returns {}.
		static Entity Play(Scene& scene, AssetHandle effect, const glm::vec3& position);
	};
}
