// Game/src/Scripts/Examples/SpawnerScript.h
#pragma once

#include "Hydro.h"

#include <algorithm>
#include <vector>

// An example (Scripting plan SC8b, rewritten from the deleted village's crate spawner): spawning and removing
// entities while the game runs. It shows a prefab property (an asset set in the editor), the input edge (pressed
// this frame, not held), entities created and destroyed during play, and a one-shot effect where they appear.
//
// Put it on any entity. Pressing the Key (E by default) instantiates the Prefab in front of the entity tagged
// "Player" (or at this entity when there is none); at most MaxAlive copies exist, the oldest is removed first.
namespace Hydro
{
	HY_SCRIPT()
	class SpawnerScript : public ScriptableEntity
	{
	public:
		HY_PROPERTY(Asset(Prefab), Tooltip("What to spawn"))
		AssetHandle Prefab = 0;
		HY_PROPERTY(Asset(Prefab), Tooltip("Played where a copy appears or is removed (a particle effect prefab); empty: none"))
		AssetHandle Effect = 0;
		HY_PROPERTY(Range(1, 100), Tooltip("Copies kept at once; the oldest is removed"))
		int MaxAlive = 10;
		HY_PROPERTY(Range(0, 10), Tooltip("Metres in front of the player a copy appears"))
		float Distance = 1.4f;

	protected:
		void OnCreate() override
		{
			// Load once here: Vfx::Play and spawning from a loaded prefab never read the disk.
			if ((uint64_t)Effect != 0)
			{
				AssetManager::GetAsset<Hydro::Prefab>(Effect);
			}
			m_Previous = true; // a key held when the scene starts doesn't count
		}

		void OnUpdate([[maybe_unused]] float deltaTime) override
		{
			const bool down = Input::IsKeyPressed(KeyCode::E);
			const bool pressed = down && m_Previous == false;
			m_Previous = down;
			if (pressed == false || (uint64_t)Prefab == 0)
			{
				return;
			}

			Scene& scene = *GetEntity().GetScene();
			Entity copy = PrefabSerializer::Instantiate(scene, Prefab);
			if (copy.IsValid() == false)
			{
				return;
			}
			scene.SetWorldPosition(copy, SpawnPoint(scene));
			PlayEffect(scene, scene.GetWorldPosition(copy));

			m_Alive.push_back(copy.GetUUID());
			while ((int)m_Alive.size() > std::max(MaxAlive, 1))
			{
				Entity oldest = scene.GetEntityByUUID(m_Alive.front());
				if (oldest.IsValid())
				{
					PlayEffect(scene, scene.GetWorldPosition(oldest));
					scene.DestroyEntity(oldest);
				}
				m_Alive.erase(m_Alive.begin());
			}
			HY_INFO("{}: spawned one ({} of {})", GetEntity().GetName(), m_Alive.size(), MaxAlive);
		}

	private:
		glm::vec3 SpawnPoint(Scene& scene)
		{
			Entity player = scene.FindEntityByTag("Player");
			if (player.IsValid() == false)
			{
				return GetWorldPosition();
			}
			const glm::vec3 at = scene.GetWorldPosition(player);
			return at + glm::vec3(0.0f, -Distance, 0.0f);
		}

		void PlayEffect(Scene& scene, const glm::vec3& position)
		{
			if ((uint64_t)Effect != 0)
			{
				Vfx::Play(scene, Effect, position);
			}
		}

		std::vector<UUID> m_Alive; // the copies, oldest first (by ID: a copy may be destroyed by something else)
		bool m_Previous = false;
	};
}
