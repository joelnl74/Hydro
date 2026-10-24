#pragma once
#include "Hydro.h"
#include "VillageScripts.h"

#include <algorithm>
#include <cmath>
#include <string>

// Small native scripts to try play mode with. They're registered in OxygenEditorApplication::OnInit,
// so they show up under Add Component > Script in the inspector and are saved with the scene.
namespace Hydro
{
	// Turns the entity around Z.
	class SpinScript : public ScriptableEntity
	{
	protected:
		void OnUpdate(float deltaTime) override
		{
			auto& transform = GetComponent<TransformComponent>();
			transform.Rotation.z += m_Speed * deltaTime;
		}

	private:
		float m_Speed = 1.5f; // radians per second
	};

	// Moves the entity up and down around where it started.
	class BobScript : public ScriptableEntity
	{
	protected:
		void OnCreate() override
		{
			m_StartY = GetComponent<TransformComponent>().Translation.y;
		}

		void OnUpdate(float deltaTime) override
		{
			m_Time += deltaTime;
			auto& transform = GetComponent<TransformComponent>();
			transform.Translation.y = m_StartY + std::sin(m_Time * 2.0f) * 0.5f;
		}

	private:
		float m_StartY = 0.0f;
		float m_Time = 0.0f;
	};

	// Walks a character with WASD or the arrow keys and picks its animation by direction:
	// "Walk<Facing>" while moving, "Idle<Facing>" when standing, Facing = Down/Right/Up/Left.
	// Needs a Sprite Animator whose set has those clips (Assets/Characters/*.hyanim in the Sandbox).
	class VillagerWalkScript : public ScriptableEntity
	{
	protected:
		void OnUpdate(float deltaTime) override
		{
			glm::vec2 move{ 0.0f };
			if (Input::IsKeyPressed(KeyCode::A) || Input::IsKeyPressed(KeyCode::Left))
			{
				move.x -= 1.0f;
			}
			if (Input::IsKeyPressed(KeyCode::D) || Input::IsKeyPressed(KeyCode::Right))
			{
				move.x += 1.0f;
			}
			if (Input::IsKeyPressed(KeyCode::W) || Input::IsKeyPressed(KeyCode::Up))
			{
				move.y += 1.0f;
			}
			if (Input::IsKeyPressed(KeyCode::S) || Input::IsKeyPressed(KeyCode::Down))
			{
				move.y -= 1.0f;
			}

			// Sideways wins when moving diagonally.
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
			// Safe every frame: a clip that is already playing carries on.
			SpriteAnimator::Play(GetEntity(), (moving ? "Walk" : "Idle") + m_Facing);

			// With a physics body the player moves by velocity, so walls stop it and it pushes crates;
			// without one (or while no world runs) it just slides the transform.
			const glm::vec2 velocity = moving ? glm::normalize(move) * (m_Speed * VillageState::PlayerSpeedScale()) : glm::vec2(0.0f);
			if (Physics2D::SetLinearVelocity(GetEntity(), velocity) == false && moving)
			{
				auto& transform = GetComponent<TransformComponent>();
				transform.Translation.x += velocity.x * deltaTime;
				transform.Translation.y += velocity.y * deltaTime;
			}
		}

	private:
		std::string m_Facing = "Down";
		float m_Speed = 3.0f; // units per second
	};

	// Sorts a sprite among the others by its height on screen: the lower it stands, the more in front it is
	// (SortOrder = round(10 - y), the same rule the LightingTest scene uses for its static sprites). For
	// characters that move; put it next to a script that walks them.
	class YSortScript : public ScriptableEntity
	{
	protected:
		void OnUpdate([[maybe_unused]] float deltaTime) override
		{
			Entity entity = GetEntity();
			if (entity.HasComponent<SpriteRendererComponent>() == false)
			{
				return;
			}

			const float y = GetComponent<TransformComponent>().Translation.y;
			GetComponent<SpriteRendererComponent>().SortOrder = (int32_t)std::lround(10.0f - y);
		}
	};

	// Put it on the camera: the camera glides after the first entity tagged "Player" (x and y only; the
	// Z and the zoom are left alone) and stays inside the bounds. It follows in play mode only, because
	// scripts only run there.
	class CameraFollowScript : public ScriptableEntity
	{
	protected:
		void OnUpdate(float deltaTime) override
		{
			Entity self = GetEntity();
			Scene* scene = self.GetScene();
			auto& transform = GetComponent<TransformComponent>();

			if (m_Target == entt::null || scene->m_Registry.valid(m_Target) == false)
			{
				m_Target = FindTarget(*scene);
				m_Snap = true;
			}

			if (m_Target == entt::null || scene->m_Registry.has<TransformComponent>(m_Target) == false)
			{
				return;
			}

			const glm::vec3& position = scene->m_Registry.get<TransformComponent>(m_Target).Translation;
			const glm::vec2 goal{ std::clamp(position.x, -m_BoundsX, m_BoundsX), std::clamp(position.y + m_LookAhead, -m_BoundsY, m_BoundsY) };

			// Frame-rate independent smoothing: closes most of the gap in about a third of a second.
			const float t = m_Snap ? 1.0f : 1.0f - std::exp(-m_Smoothing * deltaTime);
			transform.Translation.x += (goal.x - transform.Translation.x) * t;
			transform.Translation.y += (goal.y - transform.Translation.y) * t;
			m_Snap = false;
		}

	private:
		entt::entity FindTarget(Scene& scene) const
		{
			auto view = scene.m_Registry.view<TagComponent, TransformComponent>();
			for (auto entity : view)
			{
				if (view.get<TagComponent>(entity).Tag == "Player")
				{
					return entity;
				}
			}

			return entt::null;
		}

		entt::entity m_Target = entt::null;
		bool m_Snap = true;
		float m_Smoothing = 8.0f;
		float m_LookAhead = 1.5f;   // aim at the character's chest, not the feet
		float m_BoundsX = 17.0f;    // LightingTest's ground is 60 x 60 units; the view is about 25 x 14
		float m_BoundsY = 23.0f;
	};

	inline void RegisterExampleScripts()
	{
		// The names are what scenes save: keep them stable.
		ScriptRegistry::Register<SpinScript>("Spin");
		ScriptRegistry::Register<BobScript>("Bob");
		ScriptRegistry::Register<VillagerWalkScript>("VillagerWalk");
		ScriptRegistry::Register<YSortScript>("YSort");
		ScriptRegistry::Register<CameraFollowScript>("CameraFollow");
		RegisterVillageScripts();
	}
}
