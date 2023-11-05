#pragma once
#include <glm/glm.hpp>

namespace Hydro
{
	class Camera
	{
	public:
		Camera(float left, float right, float bottom, float top);
		void SetPosition(const glm::vec3& position);
		void SetRotation(float rotation);
		const glm::mat4& GetViewProjectMatrix() const { return m_viewProjectionMatrix; };
	private:
		void RecalculateViewProjectionMatrix();

	private:
		glm::mat4 m_viewMatrix;
		glm::mat4 m_projectionMatrix;
		glm::mat4 m_viewProjectionMatrix;

		glm::vec3 m_position;
		float m_rotation;
	};
}
