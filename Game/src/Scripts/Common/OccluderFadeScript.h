// Game/src/Scripts/Common/OccluderFadeScript.h
#pragma once

#include "Hydro.h"
#include "Hydro/Scene/MeshBounds.h"
#include "Hydro/Scene/SpriteBounds.h"
#include "Rules/OccluderRules.h"

#include <algorithm>
#include <vector>

namespace Hydro
{
	// Put it on the camera (after CameraFollowScript): meshes and standing sprites between the camera and the Target
	// (the player) fade to FadedOpacity, and back when she comes out from behind them, so she is never lost behind a
	// roof, a wall (the wall walk behind the inn), a gate she walks under or a tree. A mesh is "in the way" when the
	// line from her chest or head to the camera passes through its world box (MeshBounds), unless it is what she
	// stands on (CharacterBody::GetGround); a standing sprite when that line crosses its card on the camera's side of
	// her (SpriteBounds).
	// Anything larger than MaxSize (ground slabs, terraces, backdrops) never fades, nor do animated sprites
	// (characters, flames) or the camera's own children (foreground framing).
	// The fade is MeshComponent::Opacity / SpriteRendererComponent::Opacity, drawn by the engine's fade shader variants
	// (mesh_fade.frag, sprite_fade.frag; shadows stay). Play mode runs on a copy of the scene, so nothing is saved.
	// Known gap: the fade shaders are engine files the renderers pick for opacity below 1; once games can register
	// their own shader variants they move here (docs/SceneAndECS.md, Known gaps).
	HY_SCRIPT()
	class OccluderFadeScript : public ScriptableEntity
	{
	public:
		HY_PROPERTY(Tooltip("The one kept in sight (the player)"))
		EntityRef Target;
		HY_PROPERTY(Range(0, 1), Tooltip("How much of a mesh in the way still shows (0: gone, 1: no fade)"))
		float FadedOpacity = 0.3f;
		HY_PROPERTY(Range(0, 1), Tooltip("How much of a sprite in the way still shows: lower than meshes, since a near tree is also in the depth of field's foreground blur, which spreads what is left of it over the player"))
		float SpriteFadedOpacity = 0.12f;
		HY_PROPERTY(Range(0, 20), Tooltip("Opacity per second while fading out and back in"))
		float FadeSpeed = 4.0f;
		HY_PROPERTY(Range(0, 2), Tooltip("Metres: how close to the line of sight a thing is in the way"))
		float Radius = 0.3f;
		HY_PROPERTY(Range(0, 4), Tooltip("Metres above the target's feet of the highest point kept in sight (its head)"))
		float HeadHeight = 1.5f;
		HY_PROPERTY(Min(1), Tooltip("Metres: a thing larger than this on any side never fades (the ground, terraces)"))
		float MaxSize = 30.0f;
		HY_PROPERTY(Tooltip("Fade standing sprites too (trees, signs, props); meshes always fade"))
		bool FadeSprites = true;
		HY_PROPERTY(Tooltip("Log each thing as it starts to fade (its name and world box), to tune the radius and size"))
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
				// What she stands on never fades (the wall under a wall walk rises past her chest).
				CharacterGround ground;
				const entt::entity standingOn = CharacterBody::GetGround(target, ground) && ground.GroundEntity
					? (entt::entity)ground.GroundEntity : entt::null;
				auto meshes = scene->m_Registry.view<MeshComponent, WorldTransformComponent>();
				for (auto entity : meshes)
				{
					if (entity != standingOn)
					{
						Test(*scene, sight, entity, Kind::Mesh);
					}
				}
				if (FadeSprites)
				{
					const UUID self = GetEntity().GetUUID();
					auto sprites = scene->m_Registry.view<SpriteRendererComponent, WorldTransformComponent>();
					for (auto entity : sprites)
					{
						if (entity == (entt::entity)target || scene->m_Registry.has<SpriteAnimatorComponent>(entity))
						{
							continue;
						}
						const RelationshipComponent* relation = scene->m_Registry.try_get<RelationshipComponent>(entity);
						if (relation != nullptr && (uint64_t)relation->Parent == (uint64_t)self)
						{
							continue;
						}
						Test(*scene, sight, entity, Kind::Sprite);
					}
				}
			}

			for (const Faded& hit : m_InTheWay)
			{
				if (Find(hit.Entity, hit.Type) == m_Faded.end())
				{
					float* opacity = OpacityOf(*scene, hit.Entity, hit.Type);
					m_Faded.push_back({ hit.Entity, hit.Type, opacity != nullptr ? *opacity : 1.0f });
					if (LogFades)
					{
						Log(*scene, hit);
					}
				}
			}
			for (auto it = m_Faded.begin(); it != m_Faded.end();)
			{
				float* opacity = scene->m_Registry.valid(it->Entity) ? OpacityOf(*scene, it->Entity, it->Type) : nullptr;
				if (opacity == nullptr)
				{
					it = m_Faded.erase(it);
					continue;
				}
				const Faded& faded = *it;
				const bool inTheWay = std::any_of(m_InTheWay.begin(), m_InTheWay.end(),
					[&faded](const Faded& hit) { return hit.Entity == faded.Entity && hit.Type == faded.Type; });
				const float fadeTo = it->Type == Kind::Sprite ? SpriteFadedOpacity : FadedOpacity;
				const float goal = inTheWay ? std::min(fadeTo, it->Authored) : it->Authored;
				*opacity = OccluderRules::Approach(*opacity, goal, FadeSpeed, deltaTime);
				if (inTheWay == false && *opacity == it->Authored)
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
				float* opacity = scene->m_Registry.valid(faded.Entity) ? OpacityOf(*scene, faded.Entity, faded.Type) : nullptr;
				if (opacity != nullptr)
				{
					*opacity = faded.Authored;
				}
			}
			m_Faded.clear();
		}

	private:
		enum class Kind : uint8_t { Mesh, Sprite };

		struct Faded
		{
			entt::entity Entity = entt::null;
			Kind Type = Kind::Mesh;
			float Authored = 1.0f; // its Opacity before the fade, restored after
		};

		void Test(Scene& scene, const OccluderRules::Sight& sight, entt::entity entity, Kind type)
		{
			const Entity handle{ entity, &scene };
			glm::vec3 low{ 0.0f };
			glm::vec3 high{ 0.0f };
			if (type == Kind::Mesh)
			{
				if (MeshBounds::GetWorld(handle, low, high) == false)
				{
					return;
				}
				const glm::vec3 size = high - low;
				if (std::max({ size.x, size.y, size.z }) <= MaxSize && OccluderRules::Hides(sight, low, high))
				{
					m_InTheWay.push_back({ entity, type, 1.0f });
				}
				return;
			}
			glm::vec3 foot{ 0.0f };
			float width = 0.0f;
			float height = 0.0f;
			if (SpriteBounds::GetStanding(handle, foot, width, height) && std::max(width, height) <= MaxSize
				&& OccluderRules::HidesCard(sight, foot, width, height))
			{
				m_InTheWay.push_back({ entity, type, 1.0f });
			}
		}

		static float* OpacityOf(Scene& scene, entt::entity entity, Kind type)
		{
			if (type == Kind::Mesh)
			{
				MeshComponent* mesh = scene.m_Registry.try_get<MeshComponent>(entity);
				return mesh != nullptr ? &mesh->Opacity : nullptr;
			}
			SpriteRendererComponent* sprite = scene.m_Registry.try_get<SpriteRendererComponent>(entity);
			return sprite != nullptr ? &sprite->Opacity : nullptr;
		}

		std::vector<Faded>::iterator Find(entt::entity entity, Kind type)
		{
			return std::find_if(m_Faded.begin(), m_Faded.end(), [entity, type](const Faded& faded) { return faded.Entity == entity && faded.Type == type; });
		}

		static void Log(Scene& scene, const Faded& hit)
		{
			Entity faded{ hit.Entity, &scene };
			glm::vec3 low{ 0.0f };
			glm::vec3 high{ 0.0f };
			float width = 0.0f;
			float height = 0.0f;
			if (hit.Type == Kind::Mesh)
			{
				MeshBounds::GetWorld(faded, low, high);
			}
			else if (SpriteBounds::GetStanding(faded, low, width, height))
			{
				high = low + glm::vec3(width, 0.0f, height);
			}
			HY_INFO("OccluderFadeScript: fading {} '{}' ({:.1f}, {:.1f}, {:.1f}) .. ({:.1f}, {:.1f}, {:.1f})", hit.Type == Kind::Mesh ? "mesh" : "sprite",
				faded.GetName(), low.x, low.y, low.z, high.x, high.y, high.z);
		}

		std::vector<Faded> m_Faded;
		std::vector<Faded> m_InTheWay; // this frame's (kept so the update allocates nothing once warm)
	};
}
