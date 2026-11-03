#pragma once

#include "Hydro.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace Hydro
{
	// A townsperson who walks back and forth along a line through where it was placed, pausing (or working) at the ends and
	// sometimes between. Moves the transform directly; waits while the way ahead is blocked.
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
		HY_PROPERTY(Tooltip("With WorkClip: the work faces the camera (Work Down, where the broom shows); off: the way it stopped (Work<Facing>, else Down)"))
		bool WorkFacesCamera = true;
		HY_PROPERTY(Tooltip("With WorkClip: a clip prefix played once before the work (\"LieDown\" plays LieDown<Facing>, a Once clip); empty: none"))
		std::string WorkStartClip;
		HY_PROPERTY(Tooltip("With WorkClip: a clip prefix played once after the work, before it walks on (\"GetUp\"); empty: none"))
		std::string WorkEndClip;
		HY_PROPERTY(Range(0, 60), Tooltip("Seconds before it first moves (so a crowd doesn't all set off at once)"))
		float StartDelay = 0.0f;
		HY_PROPERTY(Tooltip("A round: it always walks from the line's start to its end, then reappears at the start (for a long street, with both ends off screen) instead of turning back"))
		bool Loop = false;
		HY_PROPERTY(Tooltip("The player: it waits while she stands in its way"))
		EntityRef Player;
		HY_PROPERTY(Range(0, 5), Tooltip("Metres ahead it keeps free: it waits while the player or another townsperson stands there (0: walks through)"))
		float Clearance = 0.9f;
		HY_PROPERTY(Range(0, 30), Tooltip("Seconds it waits for its way to clear before it turns back (a round walker goes on through)"))
		float Patience = 3.0f;

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
				if (m_Work == WorkStage::Starting && SpriteAnimator::IsFinished(GetEntity()))
				{
					PlayWork();
				}
				m_Wait -= deltaTime;
				if (m_Wait <= 0.0f)
				{
					if (m_Work != WorkStage::None && WorkEndClip.empty() == false && PlayFacing(WorkEndClip))
					{
						m_Work = WorkStage::Ending;
						return;
					}
					m_Work = WorkStage::None;
					PickNextTarget();
				}
				return;
			}
			if (m_Work == WorkStage::Ending)
			{
				if (SpriteAnimator::IsFinished(GetEntity()) == false)
				{
					return;
				}
				m_Work = WorkStage::None;
				PickNextTarget();
			}

			const float step = Speed * deltaTime;
			const float remaining = m_Target - m_Along;
			const float direction = remaining < 0.0f ? -1.0f : 1.0f;
			if (WaitWhileBlocked(direction, deltaTime))
			{
				return;
			}
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
			Face(direction);
			if (SpriteAnimator::Play(GetEntity(), "Walk" + m_Facing) == false)
			{
				SpriteAnimator::Play(GetEntity(), "Idle" + m_Facing);
			}
		}

	private:
		void Face(float direction)
		{
			if (Axis == "Y")
			{
				m_Facing = direction > 0.0f ? "Up" : "Down";
			}
			else
			{
				m_Facing = direction > 0.0f ? "Right" : "Left";
			}
		}

		// True while someone is within Clearance ahead; after Patience seconds it turns back, or (Loop) walks on through.
		bool WaitWhileBlocked(float direction, float deltaTime)
		{
			if (m_PassThrough > 0.0f)
			{
				m_PassThrough -= deltaTime;
				return false;
			}
			if (Clearance <= 0.0f || IsBlocked(direction) == false)
			{
				m_Blocked = 0.0f;
				return false;
			}
			if (m_Blocked == 0.0f)
			{
				Face(direction);
				if (SpriteAnimator::Play(GetEntity(), "Idle" + m_Facing) == false)
				{
					SpriteAnimator::Play(GetEntity(), "Idle");
				}
			}
			m_Blocked += deltaTime;
			if (m_Blocked >= Patience)
			{
				m_Blocked = 0.0f;
				if (Loop)
				{
					m_PassThrough = 1.5f;
				}
				else
				{
					m_Target = direction > 0.0f ? -0.5f * PathLength : 0.5f * PathLength;
				}
			}
			return true;
		}

		bool IsBlocked(float direction)
		{
			Entity self = GetEntity();
			Scene& scene = *self.GetScene();
			const glm::vec3 position = GetWorldPosition();
			const glm::vec3 forward = Axis == "Y" ? glm::vec3(0.0f, direction, 0.0f) : glm::vec3(direction, 0.0f, 0.0f);
			if (Entity player = Player.Get())
			{
				const glm::vec3 offset = scene.GetWorldPosition(player) - position;
				const float ahead = glm::dot(offset, forward);
				const glm::vec2 side = glm::vec2(offset) - glm::vec2(forward) * ahead;
				if (ahead > 0.0f && ahead < Clearance + 0.35f && glm::length(side) < 0.6f && std::abs(offset.z) < 1.5f)
				{
					return true;
				}
			}
			m_Nearby.clear();
			const float radius = 0.5f * Clearance;
			Physics3D::OverlapSphere(scene, position + forward * (0.3f + radius) + glm::vec3(0.0f, 0.0f, 0.8f), radius,
				Physics3D::LayerBit(PhysicsLayer::Npc), m_Nearby);
			for (const Entity& other : m_Nearby)
			{
				if ((entt::entity)other != (entt::entity)self)
				{
					return true;
				}
			}
			return false;
		}

		// The other end of the line, or (StopChance) a spot between here and there.
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

		// The work clip at one stop in WorkEvery, after WorkStartClip when it has one, else idle.
		void PlayStanding()
		{
			if (WorkClip.empty() == false && WorkEvery > 0 && (m_Stops % WorkEvery) == 0)
			{
				if (WorkStartClip.empty() == false && PlayFacing(WorkStartClip))
				{
					m_Work = WorkStage::Starting;
					return;
				}
				if (PlayWork())
				{
					return;
				}
			}
			if (SpriteAnimator::Play(GetEntity(), "Idle" + m_Facing) == false)
			{
				SpriteAnimator::Play(GetEntity(), "Idle");
			}
		}

		bool PlayWork()
		{
			const bool played = WorkFacesCamera ? SpriteAnimator::Play(GetEntity(), WorkClip + "Down") : PlayFacing(WorkClip);
			m_Work = played ? WorkStage::Working : WorkStage::None;
			return played;
		}

		// In the way it faces, else facing the camera; from its first frame.
		bool PlayFacing(const std::string& prefix)
		{
			if (SpriteAnimator::Play(GetEntity(), prefix + m_Facing, true))
			{
				return true;
			}
			return SpriteAnimator::Play(GetEntity(), prefix + "Down", true);
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
		enum class WorkStage : uint8_t
		{
			None = 0,
			Starting, // WorkStartClip playing
			Working,  // the work clip
			Ending    // WorkEndClip playing: it walks on when it has finished
		};
		WorkStage m_Work = WorkStage::None;
		int m_Stops = 0;
		std::string m_Facing = "Down";
		float m_Blocked = 0.0f;      // seconds waiting for the way to clear
		float m_PassThrough = 0.0f;  // seconds left walking on regardless (a round walker that waited long enough)
		std::vector<Entity> m_Nearby;
		uint32_t m_Seed = 1;
	};
}
