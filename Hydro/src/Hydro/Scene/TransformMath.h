#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Hydro
{
	// Matrices to translation / rotation / scale and back, as TransformComponent stores them (Euler radians).
	// Used by the hierarchy's world transforms.
	class TransformMath
	{
	public:
		// translation * rotation * scale, the order TransformComponent::GetTransform uses.
		static glm::mat4 Compose(const glm::vec3& translation, const glm::quat& rotation, const glm::vec3& scale);

		// Splits an affine matrix; a mirror gets a negative X scale and shear the closest rotation (IsTRS says whether it
		// is exact). False, writing nothing, when an axis has almost no length.
		static bool Decompose(const glm::mat4& matrix, glm::vec3& translation, glm::quat& rotation, glm::vec3& scale);

		// True when Compose(Decompose(matrix)) gives the matrix back within a small tolerance: it has no shear.
		static bool IsTRS(const glm::mat4& matrix);

		// Euler angles for `rotation`; `previous` is returned when it already describes it, so typed values don't jump.
		static glm::vec3 ToEulerAngles(const glm::quat& rotation, const glm::vec3& previous);
	};
}
