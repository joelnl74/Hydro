#include <hypch.h>
#include "Camera.h"

#include <glm/ext/matrix_clip_space.hpp>

namespace Hydro
{
	namespace
	{
		// Vulkan clip space has +Y pointing down. Flipping here (and nowhere else) keeps world +Y up
		// on screen and lets every image be shown without flipping its UVs.
		glm::mat4 FlipY(glm::mat4 projection)
		{
			projection[1][1] *= -1.0f;
			return projection;
		}
	}

	glm::mat4 Camera::Perspective(float verticalFOV, float aspectRatio, float nearClip, float farClip)
	{
		if (aspectRatio <= 0.0f)
			aspectRatio = 1.0f;

		return FlipY(glm::perspectiveRH_ZO(verticalFOV, aspectRatio, nearClip, farClip));
	}

	glm::mat4 Camera::Orthographic(float size, float aspectRatio, float nearClip, float farClip)
	{
		if (aspectRatio <= 0.0f)
			aspectRatio = 1.0f;

		const float halfHeight = size * 0.5f;
		const float halfWidth = halfHeight * aspectRatio;
		return Orthographic(-halfWidth, halfWidth, -halfHeight, halfHeight, nearClip, farClip);
	}

	glm::mat4 Camera::Orthographic(float left, float right, float bottom, float top, float nearClip, float farClip)
	{
		return FlipY(glm::orthoRH_ZO(left, right, bottom, top, nearClip, farClip));
	}
}
