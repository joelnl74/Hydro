#pragma once
#include "Hydro/Asset/Asset.h"
#include "Hydro/Scene/Entity.h"

#include <glm/glm.hpp>

namespace Hydro
{
	class Scene;

	// Plays visual effects from scripts and game code. An effect is a prefab (.hyprefab) whose entity has a
	// ParticleEmitter2DComponent (later also a light, a sound or an animated sprite):
	//
	//     // OnCreate: load it once, so Play never reads the disk.
	//     m_Dust = AssetManager::FindHandle("Prefabs/Effects/DustPuff.hyprefab");
	//     AssetManager::GetAsset<Prefab>(m_Dust);
	//     // When it happens:
	//     Vfx::Play(*GetEntity().GetScene(), m_Dust, position);
	//
	// A domain facade of static functions, like Particles2D and Physics2D.
	class Vfx
	{
	public:
		// Instantiates the effect at `position` and returns the new entity (allowed while scripts and
		// collision callbacks run; it emits in this frame's particle stage). Its emitter is made a one-shot
		// that destroys the entity once it stopped and its particles are gone (DestroyWhenFinished), so
		// give effect prefabs a non-looping emitter. Never loads: a prefab nobody loaded logs once and
		// returns {}.
		static Entity Play(Scene& scene, AssetHandle effect, const glm::vec3& position);
	};
}
