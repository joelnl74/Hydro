// Game/src/Scripts/Field/NpcWanderScript.h
#pragma once

#include "Hydro.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace Hydro
{
	// A townsperson who walks: back and forth along a line through where it was placed (Axis X: along the street,
	// Y: towards and away from the camera), stopping for a while at each end and sometimes in between, playing the
	// sheet's Walk<Facing> clips while moving and Idle<Facing> while standing. With WorkClip set (for example "Work",
	// the temple monk's sweeping) it plays Work<Facing> instead of standing still, one pause in WorkEvery.
	// Moves the transform directly (no physics body): its collider follows, so the player still bumps into it.
	// Put it on an NPC entity with a SpriteAnimatorComponent whose sheet has Walk<Facing> clips; without them the
	// idle plays while it moves, which is better than a frozen frame.
	HY_SCRIPT()
	class NpcWanderScript : public ScriptableEntity
	{
	public:
		HY_PROPERTY(Range(0, 60), Tooltip("Metres: the whole length of the line it walks, centred on where it was placed"))
		float PathLength = 8.0f;
		HY_PROPERTY(Tooltip("The line's direction: X (along the street, left-right) or Y (up and down the screen)"))
		std::string Axis = "X";
		HY_PROPERTY(Range(0, 5), Tooltip("Metres per second (the player walks at about 3.5)"))
		float Speed = 1.1f;
		HY_PROPERTY(Range(0, 30), Tooltip("Seconds it stands still at a stop, at least"))
		float PauseMin = 2.0f;
		HY_PROPERTY(Range(0, 60), Tooltip("Seconds it stands still at a stop, at most"))
		float PauseMax = 6.0f;
		HY_PROPERTY(Range(0, 1), Tooltip("Chance of a stop part way along the line, per trip"))
		float StopChance = 0.5f;
		HY_PROPERTY(Tooltip("A clip prefix played instead of Idle<Facing> at stops (\"Work\" plays Work<Facing>); empty: none"))
		std::string WorkClip;
		HY_PROPERTY(Range(1, 10), Tooltip("With WorkClip: it works at one stop in this many"))
		int WorkEvery = 2;
		HY_PROPERTY(Range(0, 60), Tooltip("Seconds before it first moves (so a crowd doesn't all set off at once)"))
		float StartDelay = 0.0f;
		HY_PROPERTY(Tooltip("A round: it always walks from the line's start to its end, then reappears at the start (for a long street, with both ends off screen) instead of turning back"))
		bool Loop = false;

	protected:
		void OnCreate() override
		{
			m_Home = GetWorldPosition();
			m_Seed = 88172645u ^ (uint32_t)((uint64_t)GetEntity().GetUUID() >> 5);
			m_Wait = StartDelay + PauseMin + (PauseMax - PauseMin) * Random01();
			m_Along = 0.0f;
			m_Target = (Random01() < 0.5f ? -0.5f : 0.5f) * PathLength;
			if (Loop)
			{
				m_Along = -0.5f * PathLength;
				m_Target = 0.5f * PathLength;
				Place();
			}
			PlayStanding();
		}

		void OnUpdate(float deltaTime) override
		{
			if (m_Wait > 0.0f)
			{
				m_Wait -= deltaTime;
				if (m_Wait <= 0.0f)
				{
					PickNextTarget();
				}
				return;
			}

			const float step = Speed * deltaTime;
			const float remaining = m_Target - m_Along;
			const float direction = remaining < 0.0f ? -1.0f : 1.0f;
			if (std::abs(remaining) <= step)
			{
				m_Along = m_Target;
				if (Loop && m_Target >= 0.5f * PathLength - 0.001f)
				{
					// The end of the round: back to the start (both ends sit off screen), a short pause, off again.
					m_Along = -0.5f * PathLength;
					m_Target = 0.5f * PathLength;
					Place();
					m_Wait = 0.2f + PauseMin;
					m_Stops++;
					PlayStanding();
					return;
				}
				Place();
				m_Wait = PauseMin + (PauseMax - PauseMin) * Random01();
				m_Stops++;
				PlayStanding();
				return;
			}

			m_Along += direction * step;
			Place();
			if (Axis == "Y")
			{
				m_Facing = direction > 0.0f ? "Up" : "Down";
			}
			else
			{
				m_Facing = direction > 0.0f ? "Right" : "Left";
			}
			if (SpriteAnimator::Play(GetEntity(), "Walk" + m_Facing) == false)
			{
				SpriteAnimator::Play(GetEntity(), "Idle" + m_Facing);
			}
		}

	private:
		// Where to walk next: the other end of the line, or (StopChance) a spot between here and there.
		void PickNextTarget()
		{
			if (Loop)
			{
				const float end = 0.5f * PathLength;
				m_Target = (Random01() < StopChance && m_Along < end - 1.0f) ? m_Along + (end - m_Along) * (0.3f + 0.5f * Random01()) : end;
				return;
			}
			const float end = m_Along < 0.0f ? 0.5f * PathLength : -0.5f * PathLength;
			if (Random01() < StopChance)
			{
				m_Target = m_Along + (end - m_Along) * (0.3f + 0.5f * Random01());
			}
			else
			{
				m_Target = end;
			}
		}

		void Place()
		{
			glm::vec3 position = m_Home;
			if (Axis == "Y")
			{
				position.y += m_Along;
			}
			else
			{
				position.x += m_Along;
			}
			SetWorldPosition(position);
		}

		// Standing: the work clip at one stop in WorkEvery (facing the camera, where the broom shows), else idle.
		void PlayStanding()
		{
			if (WorkClip.empty() == false && WorkEvery > 0 && (m_Stops % WorkEvery) == 0)
			{
				if (SpriteAnimator::Play(GetEntity(), WorkClip + "Down"))
				{
					return;
				}
			}
			if (SpriteAnimator::Play(GetEntity(), "Idle" + m_Facing) == false)
			{
				SpriteAnimator::Play(GetEntity(), "Idle");
			}
		}

		// 0 to 1 from a xorshift: every townsperson walks its own way, the same way every run.
		float Random01()
		{
			m_Seed ^= m_Seed << 13;
			m_Seed ^= m_Seed >> 17;
			m_Seed ^= m_Seed << 5;
			return (float)(m_Seed & 0xFFFFFF) / (float)0x1000000;
		}

		glm::vec3 m_Home{ 0.0f };
		float m_Along = 0.0f;      // metres from home along the axis
		float m_Target = 0.0f;
		float m_Wait = 0.0f;
		int m_Stops = 0;
		std::string m_Facing = "Down";
		uint32_t m_Seed = 1;
	};
}
