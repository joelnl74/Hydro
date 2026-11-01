// Game/src/Scripts/Common/OccluderFadeScript.h
#pragma once

#include "Hydro.h"
#include "Hydro/Scene/MeshBounds.h"
#include "Rules/OccluderRules.h"

#include <algorithm>
#include <vector>

namespace Hydro
{
	// Put it on the camera (after CameraFollowScript): meshes standing between the camera and the Target (the player)
	// fade to FadedOpacity, and back when she comes out from behind them, so she is never lost behind a roof or a
	// wall (the wall walk behind the inn). A mesh is "in the way" when the line from her chest or head to the camera
	// passes through its world box (MeshBounds), unless she stands on or in it (the ground, a wall's walk); meshes
	// larger than MaxSize (ground slabs, terraces) never fade. Sprites aren't faded.
	// The fade is MeshComponent::Opacity (dithered in mesh.frag; shadows stay). Play mode runs on a copy of the
	// scene, so nothing it changes is saved.
	// Known gap: a game script can only set engine-defined material inputs; adding its own shader effect needs the
	// engine (docs/SceneAndECS.md, Known gaps).
	HY_SCRIPT()
	class OccluderFadeScript : public ScriptableEntity
	{
	public:
		HY_PROPERTY(Tooltip("The one kept in sight (the player)"))
		EntityRef Target;
		HY_PROPERTY(Range(0, 1), Tooltip("How much of a mesh in the way still shows (0: gone, 1: no fade)"))
		float FadedOpacity = 0.3f;
		HY_PROPERTY(Range(0, 20), Tooltip("Opacity per second while fading out and back in"))
		float FadeSpeed = 4.0f;
		HY_PROPERTY(Range(0, 2), Tooltip("Metres: how close to the line of sight a mesh is in the way"))
		float Radius = 0.3f;
		HY_PROPERTY(Range(0, 4), Tooltip("Metres above the target's feet of the highest point kept in sight (its head)"))
		float HeadHeight = 1.5f;
		HY_PROPERTY(Min(1), Tooltip("Metres: a mesh larger than this on any side never fades (the ground, terraces)"))
		float MaxSize = 30.0f;
		HY_PROPERTY(Tooltip("Log each mesh as it starts to fade (its name and world box), to tune the radius and size"))
		bool LogFades = false;

	protected:
		void OnUpdate(float deltaTime) override
		{
			Scene* scene = GetEntity().GetScene();
			m_InTheWay.clear();
			Entity target = Target.Get();
			if ((bool)target)
			{
				OccluderRules::Sight sight;
				sight.Camera = GetWorldPosition();
				sight.Feet = scene->GetWorldPosition(target);
				sight.HeadHeight = HeadHeight;
				sight.ChestHeight = HeadHeight * 0.6f;
				sight.Radius = Radius;
				auto view = scene->m_Registry.view<MeshComponent, WorldTransformComponent>();
				for (auto entity : view)
				{
					glm::vec3 low{ 0.0f };
					glm::vec3 high{ 0.0f };
					if (MeshBounds::GetWorld(Entity{ entity, scene }, low, high) == false)
					{
						continue;
					}
					const glm::vec3 size = high - low;
					if (std::max({ size.x, size.y, size.z }) > MaxSize)
					{
						continue;
					}
					if (OccluderRules::Hides(sight, low, high))
					{
						m_InTheWay.push_back(entity);
					}
				}
			}

			for (entt::entity entity : m_InTheWay)
			{
				if (FindFaded(entity) == m_Faded.end())
				{
					m_Faded.push_back({ entity, scene->m_Registry.get<MeshComponent>(entity).Opacity });
					if (LogFades)
					{
						Entity faded{ entity, scene };
						glm::vec3 low{ 0.0f };
						glm::vec3 high{ 0.0f };
						MeshBounds::GetWorld(faded, low, high);
						HY_INFO("OccluderFadeScript: fading '{}' ({:.1f}, {:.1f}, {:.1f}) .. ({:.1f}, {:.1f}, {:.1f})", faded.GetName(),
							low.x, low.y, low.z, high.x, high.y, high.z);
					}
				}
			}
			for (auto it = m_Faded.begin(); it != m_Faded.end();)
			{
				if (scene->m_Registry.valid(it->Entity) == false || scene->m_Registry.has<MeshComponent>(it->Entity) == false)
				{
					it = m_Faded.erase(it);
					continue;
				}
				const bool inTheWay = std::find(m_InTheWay.begin(), m_InTheWay.end(), it->Entity) != m_InTheWay.end();
				float& opacity = scene->m_Registry.get<MeshComponent>(it->Entity).Opacity;
				const float goal = inTheWay ? std::min(FadedOpacity, it->Authored) : it->Authored;
				opacity = OccluderRules::Approach(opacity, goal, FadeSpeed, deltaTime);
				if (inTheWay == false && opacity == it->Authored)
				{
					it = m_Faded.erase(it);
					continue;
				}
				++it;
			}
		}

		// Removed (or Stop): what it faded shows again at once.
		void OnDestroy() override
		{
			Scene* scene = GetEntity().GetScene();
			for (const Faded& faded : m_Faded)
			{
				if (scene->m_Registry.valid(faded.Entity) && scene->m_Registry.has<MeshComponent>(faded.Entity))
				{
					scene->m_Registry.get<MeshComponent>(faded.Entity).Opacity = faded.Authored;
				}
			}
			m_Faded.clear();
		}

	private:
		struct Faded
		{
			entt::entity Entity = entt::null;
			float Authored = 1.0f; // its Opacity before the fade, restored after
		};

		std::vector<Faded>::iterator FindFaded(entt::entity entity)
		{
			return std::find_if(m_Faded.begin(), m_Faded.end(), [entity](const Faded& faded) { return faded.Entity == entity; });
		}

		std::vector<Faded> m_Faded;
		std::vector<entt::entity> m_InTheWay; // this frame's (kept so the update allocates nothing once warm)
	};
}
