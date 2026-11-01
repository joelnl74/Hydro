// Game/src/Rules/OccluderRules.h
#pragma once

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace Hydro
{
	// What stands between the camera and the player (the camera's occluder fade, OccluderFadeScript): plain geometry
	// on world boxes, no entities.
	class OccluderRules
	{
	public:
		// The sight lines a box is tested against: from the player's chest and head to the camera.
		struct Sight
		{
			glm::vec3 Camera{ 0.0f };
			glm::vec3 Feet{ 0.0f };
			float ChestHeight = 0.9f;
			float HeadHeight = 1.5f;
			// The lines' thickness: a box closer than this to them is in the way.
			float Radius = 0.3f;
		};

		// True when the box hides the player from the camera: a sight line passes through it (grown by Radius) and
		// the player doesn't stand on or in it (the ground, the wall under a wall walk). The lines start a little
		// towards the camera, so a wall the player stands right in front of (camera side) isn't counted.
		static bool Hides(const Sight& sight, const glm::vec3& low, const glm::vec3& high)
		{
			const glm::vec3 margin{ 0.05f, 0.05f, 0.25f };
			if (Contains(low - margin, high + margin, sight.Feet))
			{
				return false;
			}
			const glm::vec3 grow{ sight.Radius };
			for (float height : { sight.ChestHeight, sight.HeadHeight })
			{
				const glm::vec3 eye = sight.Feet + glm::vec3(0.0f, 0.0f, height);
				const glm::vec3 toCamera = sight.Camera - eye;
				const float distance = glm::length(toCamera);
				const float skip = sight.Radius + 0.2f;
				if (distance <= skip)
				{
					continue;
				}
				const glm::vec3 from = eye + toCamera * (skip / distance);
				if (SegmentHitsBox(from, sight.Camera, low - grow, high + grow))
				{
					return true;
				}
			}
			return false;
		}

		// The slab test: true when the segment from `from` to `to` touches the box.
		static bool SegmentHitsBox(const glm::vec3& from, const glm::vec3& to, const glm::vec3& low, const glm::vec3& high)
		{
			const glm::vec3 delta = to - from;
			float enter = 0.0f;
			float leave = 1.0f;
			for (int axis = 0; axis < 3; axis++)
			{
				if (std::abs(delta[axis]) < 1e-6f)
				{
					if (from[axis] < low[axis] || from[axis] > high[axis])
					{
						return false;
					}
					continue;
				}
				float t0 = (low[axis] - from[axis]) / delta[axis];
				float t1 = (high[axis] - from[axis]) / delta[axis];
				if (t0 > t1)
				{
					std::swap(t0, t1);
				}
				enter = std::max(enter, t0);
				leave = std::min(leave, t1);
				if (enter > leave)
				{
					return false;
				}
			}
			return true;
		}

		static bool Contains(const glm::vec3& low, const glm::vec3& high, const glm::vec3& point)
		{
			return point.x >= low.x && point.x <= high.x && point.y >= low.y && point.y <= high.y && point.z >= low.z && point.z <= high.z;
		}

		// `current` moved towards `target` by at most rate * deltaTime (an opacity fading in or out).
		static float Approach(float current, float target, float rate, float deltaTime)
		{
			const float step = std::max(0.0f, rate) * std::max(0.0f, deltaTime);
			if (current < target)
			{
				return std::min(target, current + step);
			}
			return std::max(target, current - step);
		}
	};
}
