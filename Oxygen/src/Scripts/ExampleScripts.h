#pragma once
#include "Hydro.h"

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

			if (moving)
			{
				auto& transform = GetComponent<TransformComponent>();
				const glm::vec2 step = glm::normalize(move) * m_Speed * deltaTime;
				transform.Translation.x += step.x;
				transform.Translation.y += step.y;
			}
		}

	private:
		std::string m_Facing = "Down";
		float m_Speed = 3.0f; // units per second
	};

	inline void RegisterExampleScripts()
	{
		// The names are what scenes save: keep them stable.
		ScriptRegistry::Register<SpinScript>("Spin");
		ScriptRegistry::Register<BobScript>("Bob");
		ScriptRegistry::Register<VillagerWalkScript>("VillagerWalk");
	}
}
