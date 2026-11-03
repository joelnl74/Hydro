#pragma once

#include "Hydro.h"
#include "Scripts/Common/DioramaCamera.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace Hydro
{
	// Walks the player (WASD / arrows, Shift to hurry) over the 3D collision through its CharacterBodyComponent. The script
	// only says where it wants to go (CharacterBody::Move); the physics stage moves it.
	HY_SCRIPT()
	class DioramaWalkScript : public ScriptableEntity
	{
	protected:
		void OnCreate() override
		{
			if (GetEntity().HasComponent<CharacterBodyComponent>() == false)
			{
				HY_WARN("DioramaWalk: '{}' has no Character Body, so it can't walk; add one or rebuild the scene with its build script",
					GetComponent<TagComponent>().Tag);
			}
		}

		void OnUpdate([[maybe_unused]] float deltaTime) override
		{
			glm::vec2 move{ 0.0f };
			// A UI canvas that captures input owns the keys: she stands still.
			const bool inputAllowed = UI::CapturesInput(*GetEntity().GetScene()) == false;
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
			// Standing still she keeps facing the way she walked; a sheet with one "Idle" still works.
			if (moving)
			{
				SpriteAnimator::Play(GetEntity(), "Walk" + m_Facing);
			}
			else if (SpriteAnimator::Play(GetEntity(), "Idle" + m_Facing) == false)
			{
				SpriteAnimator::Play(GetEntity(), "Idle");
			}

			glm::vec3 velocity{ 0.0f };
			if (moving)
			{
				const float speed = Input::IsKeyPressed(KeyCode::LeftShift) ? m_Speed * RunMultiplier : m_Speed;
				// The keys are screen directions, turned by the camera's yaw, so up is always into the view.
				velocity = glm::vec3(DioramaView::ScreenToGround(glm::normalize(move)) * speed, 0.0f);
			}
			CharacterBody::Move(GetEntity(), velocity);
		}

	public:
		std::string m_Facing = "Up";
		HY_PROPERTY(Range(0, 20), Tooltip("Walking speed in metres per second"))
		float m_Speed = 3.5f; // the saved key stays m_Speed (renaming it needs #143's FormerNames)
		HY_PROPERTY(Range(1, 5), Tooltip("Shift multiplies the speed by this"))
		float RunMultiplier = 1.7f;
	};
}
