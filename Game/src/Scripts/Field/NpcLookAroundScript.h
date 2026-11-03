#pragma once

#include "Hydro.h"

#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace Hydro
{
	// A townsperson who stands and looks around (the gate guards): Idle<Facing> for each of Facings in turn, held a while,
	// facing the player while she is within WatchRange. A facing the sheet lacks is skipped.
	HY_SCRIPT()
	class NpcLookAroundScript : public ScriptableEntity
	{
	public:
		HY_PROPERTY(Tooltip("The facings it looks in, in turn, separated by spaces (Down, Up, Left, Right)"))
		std::string Facings = "Down Left Down Right";
		HY_PROPERTY(Range(0, 30), Tooltip("Seconds it looks one way, at least"))
		float HoldMin = 2.5f;
		HY_PROPERTY(Range(0, 60), Tooltip("Seconds it looks one way, at most"))
		float HoldMax = 6.0f;
		HY_PROPERTY(Tooltip("The player: it watches her while she is near"))
		EntityRef Player;
		HY_PROPERTY(Range(0, 10), Tooltip("Metres: closer than this it faces the player (0: never)"))
		float WatchRange = 2.5f;

	protected:
		void OnCreate() override
		{
			std::istringstream words(Facings);
			std::string word;
			while (words >> word)
			{
				m_Facings.push_back(word);
			}
			m_Seed = 2463534242u ^ (uint32_t)((uint64_t)GetEntity().GetUUID() >> 7);
			m_Hold = HoldMin + (HoldMax - HoldMin) * Random01();
		}

		void OnUpdate(float deltaTime) override
		{
			if (Entity player = Player.Get())
			{
				const glm::vec3 offset = GetEntity().GetScene()->GetWorldPosition(player) - GetWorldPosition();
				if (WatchRange > 0.0f && glm::length(glm::vec2(offset)) < WatchRange && std::abs(offset.z) < 1.5f)
				{
					const std::string facing = std::abs(offset.x) > std::abs(offset.y) ? (offset.x > 0.0f ? "Right" : "Left")
						: (offset.y > 0.0f ? "Up" : "Down");
					Look(facing);
					return;
				}
			}
			if (m_Facings.empty())
			{
				return;
			}
			m_Hold -= deltaTime;
			if (m_Hold > 0.0f && m_Current.empty() == false)
			{
				return;
			}
			m_Hold = HoldMin + (HoldMax - HoldMin) * Random01();
			m_Index = (m_Index + 1) % (int)m_Facings.size();
			Look(m_Facings[m_Index]);
		}

	private:
		void Look(const std::string& facing)
		{
			if (facing == m_Current)
			{
				return;
			}
			if (SpriteAnimator::Play(GetEntity(), "Idle" + facing))
			{
				m_Current = facing;
			}
		}

		// 0 to 1 from a xorshift: each guard keeps its own rhythm, the same every run.
		float Random01()
		{
			m_Seed ^= m_Seed << 13;
			m_Seed ^= m_Seed >> 17;
			m_Seed ^= m_Seed << 5;
			return (float)(m_Seed & 0xFFFFFF) / (float)0x1000000;
		}

		std::vector<std::string> m_Facings;
		std::string m_Current;
		int m_Index = -1;
		float m_Hold = 0.0f;
		uint32_t m_Seed = 1;
	};
}
