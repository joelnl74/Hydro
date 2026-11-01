#pragma once
#include "Hydro/Renderer/Camera.h"

#include <glm/glm.hpp>

namespace Hydro
{
	// The projection settings of a camera in a scene (held by CameraComponent). Plain data: the
	// camera's position and rotation come from the entity's WorldTransformComponent, and its aspect ratio
	// from the scene's viewport (Scene::OnViewportResize), so nothing here has to be kept in sync.
	struct SceneCamera
	{
		ProjectionType Projection = ProjectionType::Orthographic;

		float PerspectiveFOV = 45.0f;       // vertical, degrees
		float PerspectiveNear = 0.01f;
		float PerspectiveFar = 1000.0f;

		float OrthographicSize = 10.0f;     // world units visible vertically
		float OrthographicNear = -1.0f;
		float OrthographicFar = 1.0f;

		// Metres along the view to what the camera looks at: the depth of field's focus point (#135). 0 = where the
		// view meets the ground (z = 0), as the fog measures. A camera script that aims above the ground (a diorama
		// camera looking at a terrace) sets it, so the subject stays in the sharp band.
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
