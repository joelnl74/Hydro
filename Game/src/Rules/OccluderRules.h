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

		// True when the box hides the player from the camera: a sight line passes through it (grown by Radius). The
		// lines start a little towards the camera, so a wall the player stands right in front of (camera side) isn't
		// counted, and from her chest, so the ground under her isn't either. What she stands on when it rises higher
		// (a wall under a wall walk) the caller leaves out (CharacterBody::GetGround); a gate she walks under fades.
		static bool Hides(const Sight& sight, const glm::vec3& low, const glm::vec3& high)
		{
			// A box around her feet that ends below the top of her head is something she walks on or along (a wall
			// walk's next section, a low terrace), not something over her.
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

		// True when a standing card (a sprite facing the camera: standing on `foot`, `width` wide and `height` tall,
		// turned to face the camera about its vertical axis) hides the player: a sight line crosses its plane on the
		// camera's side of her, within the card (grown by Radius). A card she stands in front of never hides her.
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
