#include <hypch.h>
#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>

namespace Hydro
{
	Camera::Camera(float left, float right, float bottom, float top)
		: m_projectionMatrix(glm::ortho(left, right, bottom, top, -10.0f, 1.0f)), m_viewMatrix(1.0f)
	{
		// m_projectionMatrix[1][1] *= -1;
		m_viewProjectionMatrix = m_projectionMatrix * m_viewMatrix;
	}
	
	void Camera::SetPosition(const glm::vec3& position)
	{
		m_position = position;
	}
	
	void Camera::SetRotation(float rotation)
	{
		m_rotation = rotation;
	}
	
	void Camera::RecalculateViewProjectionMatrix()
	{
		glm::mat4 transform = glm::translate(glm::mat4(1), m_position)
			* glm::rotate(glm::mat4(1), m_rotation, glm::vec3(0, 0, 1));

		m_viewProjectionMatrix = m_projectionMatrix * m_viewMatrix;
	}
}
