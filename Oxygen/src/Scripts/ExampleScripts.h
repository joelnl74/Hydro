#pragma once
#include "Hydro.h"
#include "DioramaCamera.h"
#include "VillageScripts.h"
#include "BattleScripts.h"
#include "MvpScripts.h"
#include "ShrineScripts.h"

#include <algorithm>
#include <cmath>
#include <string>

// Small native scripts to try play mode with. They're registered in OxygenEditorApplication::OnInit,
// so they show up under Add Component > Script in the inspector and are saved with the scene.
namespace Hydro
{
	// Turns the entity around Z.
	HY_SCRIPT()
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
	HY_SCRIPT()
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
	HY_SCRIPT()
	class VillagerWalkScript : public ScriptableEntity
	{
	protected:
		void OnUpdate(float deltaTime) override
		{
			glm::vec2 move{ 0.0f };
			// A modal menu owns the keys: the character stands still (and faces the way it was).
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

			uint32_t increasedSpeed = Input::IsKeyPressed(KeyCode::LeftShift) ? 2 : 1;
			const glm::vec2 velocity = moving ? glm::normalize(move) * (m_Speed + increasedSpeed * VillageState::PlayerSpeedScale()) : glm::vec2(0.0f);
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
	HY_SCRIPT()
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

	// Put it on the camera: the camera glides after the first entity tagged "Player" (the zoom is left alone)
	// and its focus stays inside the bounds. V switches between top-down and the tilted diorama view, O
	// between perspective and orthographic in the diorama (DioramaCamera.h). It follows in play mode only,
	// because scripts only run there.
	HY_SCRIPT()
	class CameraFollowScript : public ScriptableEntity
	{
	protected:
		void OnUpdate(float deltaTime) override
		{
			Entity self = GetEntity();
			Scene* scene = self.GetScene();
			auto& transform = GetComponent<TransformComponent>();

			DioramaView::Update(deltaTime);
			if (m_Target == entt::null || scene->m_Registry.valid(m_Target) == false)
			{
				m_Target = FindTarget(*scene);
				m_Snap = true;
			}

			if (m_Target != entt::null && scene->m_Registry.has<TransformComponent>(m_Target))
			{
				const glm::vec3& position = scene->m_Registry.get<TransformComponent>(m_Target).Translation;
				const float lead = m_LookAhead + DioramaView::SceneFocusLead * DioramaView::Blend;
				const glm::vec2 goal = DioramaView::ClampFocus({ position.x, position.y + lead }, { m_BoundsX, m_BoundsY });

				// Frame-rate independent smoothing: closes most of the gap in about a third of a second.
				if (m_Snap)
				{
					m_Focus = goal;
					m_FocusHeight = position.z;
				}
				const float t = 1.0f - std::exp(-m_Smoothing * deltaTime);
				m_Focus += (goal - m_Focus) * t;
				// The height of the ground the character stands on (a terrace, stairs), so it stays in frame.
				m_FocusHeight += (position.z - m_FocusHeight) * t;
				m_Snap = false;
			}
			else if (m_Snap)
			{
				m_Focus = { transform.Translation.x, transform.Translation.y };
			}

			if (self.HasComponent<CameraComponent>())
			{
				DioramaView::Apply(transform, GetComponent<CameraComponent>(), m_Focus, m_FocusHeight + DioramaView::SceneFocusRise * DioramaView::Blend);
			}
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
		glm::vec2 m_Focus{ 0.0f };  // where the camera looks, on the ground
		float m_FocusHeight = 0.0f; // and how high that ground is (the target's z, smoothed)
		float m_Smoothing = 8.0f;
		float m_LookAhead = 1.5f; // aim at the character's chest, not the feet
		// Used when the scene sets no bounds of its own (DioramaView::SceneBounds*): LightingTest's ground is
		// 60 x 60 units; the view is about 25 x 14.
		float m_BoundsX = 17.0f;
		float m_BoundsY = 23.0f;
	};
}
