#include "hypch.h"
#include "TransformMath.h"

#include <glm/gtc/matrix_transform.hpp>
#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtx/quaternion.hpp>

#include <algorithm>
#include <cmath>

namespace Hydro
{
	// Below this an axis counts as having no length (a zero scale), and the matrix can't be split.
	static constexpr float MinAxisLength = 1e-6f;

	glm::mat4 TransformMath::Compose(const glm::vec3& translation, const glm::quat& rotation, const glm::vec3& scale)
	{
		return glm::translate(glm::mat4(1.0f), translation) * glm::toMat4(rotation) * glm::scale(glm::mat4(1.0f), scale);
	}

	bool TransformMath::Decompose(const glm::mat4& matrix, glm::vec3& translation, glm::quat& rotation, glm::vec3& scale)
	{
		glm::vec3 axes[3] = { glm::vec3(matrix[0]), glm::vec3(matrix[1]), glm::vec3(matrix[2]) };
		glm::vec3 lengths(glm::length(axes[0]), glm::length(axes[1]), glm::length(axes[2]));
		if (lengths.x < MinAxisLength || lengths.y < MinAxisLength || lengths.z < MinAxisLength)
		{
			return false;
		}

		// A mirror is one negative scale; X carries it.
		if (glm::determinant(glm::mat3(matrix)) < 0.0f)
		{
			lengths.x = -lengths.x;
		}

		const glm::mat3 rotationMatrix(axes[0] / lengths.x, axes[1] / lengths.y, axes[2] / lengths.z);
		translation = glm::vec3(matrix[3]);
		rotation = glm::normalize(glm::quat_cast(rotationMatrix));
		scale = lengths;
		return true;
	}

	bool TransformMath::IsTRS(const glm::mat4& matrix)
	{
		glm::vec3 translation;
		glm::quat rotation;
		glm::vec3 scale;
		if (Decompose(matrix, translation, rotation, scale) == false)
		{
			return false;
		}

		const glm::mat4 rebuilt = Compose(translation, rotation, scale);
		float largest = 1.0f;
		float difference = 0.0f;
		for (int column = 0; column < 4; column++)
		{
			for (int row = 0; row < 3; row++)
			{
				largest = std::max(largest, std::abs(matrix[column][row]));
				difference = std::max(difference, std::abs(matrix[column][row] - rebuilt[column][row]));
			}
		}
		return difference <= 1e-4f * largest;
	}

	glm::vec3 TransformMath::ToEulerAngles(const glm::quat& rotation, const glm::vec3& previous)
	{
		// q and -q are the same rotation: compare by the absolute dot product.
		const float alignment = std::abs(glm::dot(glm::normalize(glm::quat(previous)), glm::normalize(rotation)));
		if (alignment > 1.0f - 1e-6f)
		{
			return previous;
		}
		return glm::eulerAngles(rotation);
	}
}
