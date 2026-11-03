#pragma once
#include "Hydro/Renderer/Camera.h"

#include <glm/glm.hpp>

namespace Hydro
{
	// A scene camera's projection settings (CameraComponent). Position and rotation come from the entity, aspect ratio
	// from the scene's viewport.
	struct SceneCamera
	{
		ProjectionType Projection = ProjectionType::Orthographic;

		float PerspectiveFOV = 45.0f;       // vertical, degrees
		float PerspectiveNear = 0.01f;
		float PerspectiveFar = 1000.0f;

		float OrthographicSize = 10.0f;     // world units visible vertically
		float OrthographicNear = -1.0f;
		float OrthographicFar = 1.0f;

		// Metres along the view to the depth of field's focus (#135). 0 = where the view meets the ground. A camera script
		// aiming above the ground sets it, so the subject stays sharp.
		float FocusDistance = 0.0f;

		glm::mat4 GetProjection(float aspectRatio) const
		{
			if (Projection == ProjectionType::Perspective)
			{
				return Camera::Perspective(glm::radians(PerspectiveFOV), aspectRatio, PerspectiveNear, PerspectiveFar);
			}

			return Camera::Orthographic(OrthographicSize, aspectRatio, OrthographicNear, OrthographicFar);
		}
	};
}
