#pragma once

#include "Hydro.h"

#include <algorithm>

// A hero's strike, timed: walk up to the target, strike on the clip's contact frame, walk back. It moves the hero's
// offset and says when things happen; BattleScript plays the clips and lands the hits.

namespace Hydro
{
	class ActionSequence
	{
	public:
		enum class Stage : uint8_t { Approach, Strike, Return };

		// What happened this frame (several can at once).
		struct Events
		{
			bool Arrived = false;    // the walk up ended
			bool EffectDue = false;  // start the strike effect, so its burst meets the impact
			bool Impact = false;     // the blow lands
			bool StrikeOver = false; // the clip ended: the next strike, or the walk back
			bool Returned = false;   // back where the hero stands
		};

		Stage GetStage() const { return m_Stage; }
		int GetStrikes() const { return m_Strikes; } // strikes begun in this action

		void Reset()
		{
			*this = ActionSequence{};
		}

		// `from` and `to` are offsets from where the hero stands; `duration` in seconds.
		void BeginApproach(const glm::vec2& from, const glm::vec2& to, float duration)
		{
			BeginWalk(Stage::Approach, from, to, duration);
		}

		void BeginReturn(const glm::vec2& from, float duration)
		{
			BeginWalk(Stage::Return, from, glm::vec2(0.0f), duration);
		}

		// Seconds into the clip: when the effect starts, when the blow lands, when the clip ends.
		void BeginStrike(float effectTime, float impactTime, float length)
		{
			m_Stage = Stage::Strike;
			m_Time = 0.0f;
			m_EffectTime = effectTime;
			m_ImpactTime = impactTime;
			m_Duration = std::max(length, impactTime + 0.05f);
			m_EffectStarted = false;
			m_Landed = false;
			m_Strikes++;
		}

		// Moves `offset` along a walk: briskly up to the target, easing in and out on the way back.
		Events Update(float deltaTime, glm::vec2& offset)
		{
			Events events;
			m_Time += deltaTime;
			const float k = m_Duration > 0.0f ? std::clamp(m_Time / m_Duration, 0.0f, 1.0f) : 1.0f;
			switch (m_Stage)
			{
				case Stage::Approach:
				{
					const float eased = 1.0f - (1.0f - k) * (1.0f - k);
					offset = m_From + (m_To - m_From) * eased;
					events.Arrived = k >= 1.0f;
					break;
				}
				case Stage::Strike:
					if (m_EffectStarted == false && m_Time >= m_EffectTime)
					{
						m_EffectStarted = true;
						events.EffectDue = true;
					}
					if (m_Landed == false && m_Time >= m_ImpactTime)
					{
						m_Landed = true;
						events.Impact = true;
					}
					events.StrikeOver = m_Time >= m_Duration;
					break;
				case Stage::Return:
				{
					const float eased = k * k * (3.0f - 2.0f * k);
					offset = m_From + (m_To - m_From) * eased;
					if (k >= 1.0f)
					{
						offset = glm::vec2(0.0f);
						events.Returned = true;
					}
					break;
				}
			}
			return events;
		}

	private:
		void BeginWalk(Stage stage, const glm::vec2& from, const glm::vec2& to, float duration)
		{
			m_Stage = stage;
			m_Time = 0.0f;
			m_From = from;
			m_To = to;
			m_Duration = duration;
		}

		Stage m_Stage = Stage::Strike;
		float m_Time = 0.0f;
		float m_Duration = 0.0f;
		float m_EffectTime = 0.0f;
		float m_ImpactTime = 0.0f;
		bool m_EffectStarted = false;
		bool m_Landed = false;
		int m_Strikes = 0;
		glm::vec2 m_From{ 0.0f };
		glm::vec2 m_To{ 0.0f };
	};
}
