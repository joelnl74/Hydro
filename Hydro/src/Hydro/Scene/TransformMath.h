#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Hydro
{
	// Turning matrices into translation / rotation / scale and back, as TransformComponent stores them (rotation
	// as Euler angles in radians: glm::quat(Rotation)). Used by the hierarchy's world transforms (Scene::
	// ResolveTransforms, Scene::SetWorldTransform).
	class TransformMath
	{
	public:
		// translation * rotation * scale, the order TransformComponent::GetTransform uses.
		static glm::mat4 Compose(const glm::vec3& translation, const glm::quat& rotation, const glm::vec3& scale);

		// Splits an affine matrix into translation, rotation and scale. A mirroring matrix (negative determinant)
		// gets a negative X scale. A matrix with shear is split into the closest rotation and the lengths of its
		// axes (IsTRS says whether that is exact). False, writing nothing, when an axis has (almost) no length.
		static bool Decompose(const glm::mat4& matrix, glm::vec3& translation, glm::quat& rotation, glm::vec3& scale);

		// True when Compose(Decompose(matrix)) gives the matrix back within a small tolerance: it has no shear.
		static bool IsTRS(const glm::mat4& matrix);

		// Euler angles (radians, TransformComponent::Rotation) for `rotation`. When `previous` already describes
		// the same rotation it is returned as it is, so values the user typed don't jump to an equivalent set.
		static glm::vec3 ToEulerAngles(const glm::quat& rotation, const glm::vec3& previous);
	};
}
