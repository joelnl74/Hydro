#pragma once

#include "Hydro/Scene/Scripting/ReflectionMacros.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace Hydro
{
	// Saved by name, so routes saved with the older string property keep their mode.
	HY_ENUM()
	enum class RouteMode : uint8_t
	{
		Loop = 0,  // the last waypoint leads back to the first
		PingPong,  // first to last and back again
		Once,      // first to last, then it stays
		Restart    // first to last, then it reappears at the first (a round through a street)
	};

	// Plain rules with every random draw passed in, no entities.
	class RouteRules
	{
	public:
		using Mode = RouteMode;

		// Anything unknown is Loop.
		static Mode ParseMode(const std::string& name)
		{
			if (name == "PingPong")
			{
				return Mode::PingPong;
			}
			if (name == "Once")
			{
				return Mode::Once;
			}
			if (name == "Restart")
			{
				return Mode::Restart;
			}
			return Mode::Loop;
		}

		struct Step
		{
			int Index = 0;          // the waypoint to go to next
			int Direction = 1;      // +1 forwards, -1 backwards (PingPong)
			bool Teleport = false;  // Restart: appear there instead of walking
			bool Finished = false;  // Once: the last one is reached, stay
		};

		// After reaching waypoint `index` (going `direction`) of `count`.
		static Step Next(int index, int direction, int count, Mode mode)
		{
			Step step;
			step.Direction = direction < 0 ? -1 : 1;
			if (count <= 1)
			{
				step.Index = 0;
				step.Finished = true;
				return step;
			}
			const int next = index + step.Direction;
			if (next >= 0 && next < count)
			{
				step.Index = next;
				return step;
			}
			switch (mode)
			{
				case Mode::PingPong:
					step.Direction = -step.Direction;
					step.Index = index + step.Direction;
					return step;
				case Mode::Once:
					step.Index = index;
					step.Finished = true;
					return step;
				case Mode::Restart:
					step.Index = 0;
					step.Direction = 1;
					step.Teleport = true;
					return step;
				default:
					step.Index = next < 0 ? count - 1 : 0;
					return step;
			}
		}

		static bool RollStop(float chance, float random01)
		{
			return random01 < std::clamp(chance, 0.0f, 1.0f);
		}

		static float RollWait(float waitMin, float waitMax, float random01)
		{
			const float low = std::max(0.0f, std::min(waitMin, waitMax));
			const float high = std::max(0.0f, std::max(waitMin, waitMax));
			return low + (high - low) * std::clamp(random01, 0.0f, 1.0f);
		}

		// "Up" is away from the camera (+Y), "Right" is +X; `current` when the direction is too short to tell.
		static std::string FacingOf(const glm::vec2& direction, const std::string& current)
		{
			if (std::abs(direction.x) < 1e-4f && std::abs(direction.y) < 1e-4f)
			{
				return current;
			}
			if (std::abs(direction.x) >= std::abs(direction.y))
			{
				return direction.x > 0.0f ? "Right" : "Left";
			}
			return direction.y > 0.0f ? "Up" : "Down";
		}

		// True when `other` stands within `clearance` ahead along `forward`, close to its line and at about the same height.
		static bool IsInTheWay(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& other, float clearance)
		{
			const glm::vec3 offset = other - position;
			const float ahead = glm::dot(offset, forward);
			const glm::vec2 side = glm::vec2(offset) - glm::vec2(forward) * ahead;
			return ahead > 0.0f && ahead < clearance + 0.35f && glm::length(side) < 0.6f && std::abs(offset.z) < 1.5f;
		}
	};
}
