#pragma once
#include "Hydro.h"
#include "DioramaCamera.h"
#include "VillageScripts.h"

#include <algorithm>
#include <cmath>
#include <string>

// The Shrine Glade (Assets/Scenes/ShrineGlade.hyscene): the first scene that mixes meshes (terraces,
// retaining walls, stairs, the imported shrine and stone lanterns) with upright sprites (trees, props,
// characters). See docs/TestBed.md, "Shrine Glade".
namespace Hydro
{
	// The walkable ground of a scene, read from its MeshComponents every time it is asked: the top of the
	// highest built-in shape (or model, as its bounding box) under a point. Shapes may be turned about Z only
	// (their Rotation.z), as blockout is. Game code for now; a collision system replaces it.
	struct DioramaGround
	{
		// No ground at all here (outside the map).
		static constexpr float None = -1.0e9f;

		static float HeightAt(Scene& scene, const glm::vec2& point)
		{
			float height = None;
			auto meshes = scene.m_Registry.view<TransformComponent, MeshComponent>();
			for (auto entity : meshes)
			{
				const auto [transform, mesh] = meshes.get<TransformComponent, MeshComponent>(entity);
				height = std::max(height, SurfaceAt(transform, mesh, point));
			}
			return height;
		}

	private:
		// The top of one mesh at `point`, or None when the point isn't over it.
		static float SurfaceAt(const TransformComponent& transform, const MeshComponent& mesh, const glm::vec2& point)
		{
			// The point in the shape's own frame (before its scale).
			const glm::vec2 offset = point - glm::vec2(transform.Translation);
			const float c = std::cos(-transform.Rotation.z);
			const float s = std::sin(-transform.Rotation.z);
			const glm::vec3 scale = glm::max(glm::abs(transform.Scale), glm::vec3(1.0e-4f));
			const glm::vec2 local{ (offset.x * c - offset.y * s) / scale.x, (offset.x * s + offset.y * c) / scale.y };
			const float base = transform.Translation.z;

			if ((uint64_t)mesh.Model != 0)
			{
				const Ref<Mesh> model = AssetManager::GetLoadedAsset<Mesh>(mesh.Model);
				if (model == nullptr)
				{
					return None;
				}
				const glm::vec3& min = model->GetBoundsMin();
				const glm::vec3& max = model->GetBoundsMax();
				if (local.x < min.x || local.x > max.x || local.y < min.y || local.y > max.y)
				{
					return None;
				}
				return base + max.z * scale.z;
			}

			const glm::vec3& size = mesh.Size;
			const glm::vec2 half{ size.x * 0.5f, size.y * 0.5f };
			if (mesh.Shape == MeshShape::Cylinder)
			{
				const float u = half.x > 0.0f ? local.x / half.x : 2.0f;
				const float v = half.y > 0.0f ? local.y / half.y : 2.0f;
				return u * u + v * v <= 1.0f ? base + size.z * scale.z : None;
			}
			if (std::abs(local.x) > half.x || std::abs(local.y) > half.y)
			{
				return None;
			}

			// 0 at the front (-Y) edge, 1 at the back.
			const float along = size.y > 0.0f ? std::clamp((local.y + half.y) / size.y, 0.0f, 1.0f) : 0.0f;
			switch (mesh.Shape)
			{
				case MeshShape::Plane:
					return base;
				case MeshShape::Ramp:
					return base + along * size.z * scale.z;
				case MeshShape::Stairs:
				{
					const uint32_t steps = std::max(mesh.Steps, 1u);
					const float step = std::min(std::floor(along * (float)steps), (float)steps - 1.0f);
					return base + (step + 1.0f) / (float)steps * size.z * scale.z;
				}
				default:
					return base + size.z * scale.z;
			}
		}
	};

	// Walks the player like VillagerWalk (WASD / arrows, Shift to hurry, Walk<Facing> / Idle<Facing> clips),
	// but over the diorama's ground: its feet follow the height of the terraces, stairs and bridge under them
	// (DioramaGround), it can't climb or drop more than a step, and it doesn't walk into the solid box
	// colliders of other entities (trees, lanterns, people). No physics body: put it on an entity without a
	// Rigidbody 2D.
	class DioramaWalkScript : public ScriptableEntity
	{
	protected:
		void OnCreate() override
		{
			Scene& scene = *GetEntity().GetScene();
			auto& transform = GetComponent<TransformComponent>();
			const float ground = DioramaGround::HeightAt(scene, glm::vec2(transform.Translation));
			if (ground > DioramaGround::None)
			{
				transform.Translation.z = ground;
			}
		}

		void OnUpdate(float deltaTime) override
		{
			Scene& scene = *GetEntity().GetScene();
			auto& transform = GetComponent<TransformComponent>();

			glm::vec2 move{ 0.0f };
			const bool inputAllowed = UIState::MenuOpen == false;
			if (inputAllowed && (Input::IsKeyPressed(KeyCode::A) || Input::IsKeyPressed(KeyCode::Left)))
			{
				move.x -= 1.0f;
			}
			if (inputAllowed && (Input::IsKeyPressed(KeyCode::D) || Input::IsKeyPressed(KeyCode::Right)))
			{
				move.x += 1.0f;
			}
			if (inputAllowed && (Input::IsKeyPressed(KeyCode::W) || Input::IsKeyPressed(KeyCode::Up)))
			{
				move.y += 1.0f;
			}
			if (inputAllowed && (Input::IsKeyPressed(KeyCode::S) || Input::IsKeyPressed(KeyCode::Down)))
			{
				move.y -= 1.0f;
			}

			if (move.x < 0.0f)
			{
				m_Facing = "Left";
			}
			else if (move.x > 0.0f)
			{
				m_Facing = "Right";
			}
			else if (move.y > 0.0f)
			{
				m_Facing = "Up";
			}
			else if (move.y < 0.0f)
			{
				m_Facing = "Down";
			}

			const bool moving = move.x != 0.0f || move.y != 0.0f;
			SpriteAnimator::Play(GetEntity(), (moving ? "Walk" : "Idle") + m_Facing);

			const glm::vec2 position{ transform.Translation };
			if (moving)
			{
				const float speed = Input::IsKeyPressed(KeyCode::LeftShift) ? m_Speed * 1.7f : m_Speed;
				const glm::vec2 step = glm::normalize(move) * speed * std::min(deltaTime, 0.05f);
				// The whole step, else slide along a wall on one axis.
				const glm::vec2 tries[3] = { position + step, position + glm::vec2(step.x, 0.0f), position + glm::vec2(0.0f, step.y) };
				for (const glm::vec2& next : tries)
				{
					if (CanStand(scene, next, transform.Translation.z))
					{
						transform.Translation.x = next.x;
						transform.Translation.y = next.y;
						break;
					}
				}
			}

			// Feet on the ground: eased a little, so a stair step is a quick lift instead of a jump.
			const float ground = DioramaGround::HeightAt(scene, glm::vec2(transform.Translation));
			if (ground > DioramaGround::None)
			{
				const float t = 1.0f - std::exp(-m_HeightEasing * deltaTime);
				transform.Translation.z += (ground - transform.Translation.z) * t;
				if (std::abs(ground - transform.Translation.z) < 0.005f)
				{
					transform.Translation.z = ground;
				}
			}
		}

	private:
		bool CanStand(Scene& scene, const glm::vec2& point, float currentHeight) const
		{
			const float ground = DioramaGround::HeightAt(scene, point);
			if (ground <= DioramaGround::None || std::abs(ground - currentHeight) > m_MaxStep)
			{
				return false;
			}

			// Solid box colliders of other entities, as boxes on the ground (their scale applied, rotation not).
			const entt::entity self = GetEntity();
			auto colliders = scene.m_Registry.view<TransformComponent, BoxCollider2DComponent>();
			for (auto entity : colliders)
			{
				if (entity == (entt::entity)self)
				{
					continue;
				}
				const auto [transform, collider] = colliders.get<TransformComponent, BoxCollider2DComponent>(entity);
				if (collider.IsSensor)
				{
					continue;
				}
				// Only things standing at about the player's height block it (a tree on the terrace above doesn't).
				if (std::abs(transform.Translation.z - currentHeight) > 1.0f)
				{
					continue;
				}
				const glm::vec2 scale = glm::abs(glm::vec2(transform.Scale));
				const glm::vec2 centre = glm::vec2(transform.Translation) + collider.Offset * scale;
				const glm::vec2 half = collider.Size * scale * 0.5f + glm::vec2(m_FootRadius);
				if (std::abs(point.x - centre.x) < half.x && std::abs(point.y - centre.y) < half.y)
				{
					return false;
				}
			}
			return true;
		}

		std::string m_Facing = "Up";
		float m_Speed = 3.5f;        // metres per second
		float m_MaxStep = 0.35f;     // the highest step up or down (stairs are 0.2)
		float m_FootRadius = 0.15f;
		float m_HeightEasing = 18.0f;
	};

	// On the Shrine Glade's director entity: the scene starts in the tilted diorama view (V still switches).
	class ShrineGladeScript : public ScriptableEntity
	{
	protected:
		void OnCreate() override
		{
			DioramaView::Enabled = true;
			DioramaView::Blend = 1.0f;
			HY_INFO("Shrine Glade: WASD walk up the stairs to the shrine, E interact, V top-down / tilted, O perspective / orthographic");
		}
	};

	inline void RegisterShrineScripts()
	{
		// The names are what scenes save: keep them stable.
		ScriptRegistry::Register<DioramaWalkScript>("DioramaWalk");
		ScriptRegistry::Register<ShrineGladeScript>("ShrineGlade");
	}
}
