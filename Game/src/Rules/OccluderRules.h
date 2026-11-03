#pragma once

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace Hydro
{
	// What stands between the camera and the player, for the camera's occluder fade: plain geometry on world boxes.
	class OccluderRules
	{
	public:
		// From the player's chest and head to the camera.
		struct Sight
		{
			glm::vec3 Camera{ 0.0f };
			glm::vec3 Feet{ 0.0f };
			float ChestHeight = 0.9f;
			float HeadHeight = 1.5f;
			// A box closer than this to the lines is in the way.
			float Radius = 0.3f;
		};

		// The lines start a little towards the camera and at her chest, so a wall she stands right in front of and the ground
		// under her don't count. The caller leaves out what she stands on (CharacterBody::GetGround).
		static bool Hides(const Sight& sight, const glm::vec3& low, const glm::vec3& high)
		{
			// A box around her feet that ends below the top of her head is something she walks on or along, not over her.
			const bool aroundFeet = sight.Feet.x >= low.x && sight.Feet.x <= high.x && sight.Feet.y >= low.y && sight.Feet.y <= high.y
				&& sight.Feet.z >= low.z - 0.25f && sight.Feet.z <= high.z;
			if (aroundFeet && high.z <= sight.Feet.z + sight.HeadHeight + 0.5f)
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

		// A standing card faces the camera about its vertical axis. It hides her when a sight line crosses it on the camera's
		// side of her (grown by Radius); a card she stands in front of never does.
		static bool HidesCard(const Sight& sight, const glm::vec3& foot, float width, float height)
		{
			glm::vec3 normal = sight.Camera - foot;
			normal.z = 0.0f;
			const float length = glm::length(normal);
			if (length < 1e-4f)
			{
				return false;
			}
			normal /= length;
			const glm::vec3 side{ -normal.y, normal.x, 0.0f };
			for (float eyeHeight : { sight.ChestHeight, sight.HeadHeight })
			{
				const glm::vec3 eye = sight.Feet + glm::vec3(0.0f, 0.0f, eyeHeight);
				const float eyeDistance = glm::dot(eye - foot, normal);       // negative: behind the card
				const float cameraDistance = glm::dot(sight.Camera - foot, normal);
				if (eyeDistance >= 0.0f || cameraDistance <= 0.0f)
				{
					continue;
				}
				const float t = eyeDistance / (eyeDistance - cameraDistance);
				const glm::vec3 crossing = eye + (sight.Camera - eye) * t;
				const float across = std::abs(glm::dot(crossing - foot, side));
				const float up = crossing.z - foot.z;
				if (across <= 0.5f * width + sight.Radius && up >= -sight.Radius && up <= height + sight.Radius)
				{
					return true;
				}
			}
			return false;
		}

		// The slab test.
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

		// Moves by at most rate * deltaTime (an opacity fading in or out).
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
