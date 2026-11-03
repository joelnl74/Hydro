#pragma once
#include <glm/glm.hpp>

#include <cstdint>

namespace Hydro
{
	enum class ProjectionType : uint8_t
	{
		Perspective = 0,
		Orthographic = 1
	};

	// A projection matrix. Every projection is built by the functions below, so Vulkan's clip space (depth 0..1, Y down)
	// is handled once, here. Right-handed: the camera looks down -Z.
	class Camera
	{
	public:
		Camera() = default;
		explicit Camera(const glm::mat4& projection)
			: m_Projection(projection) {
		}

		const glm::mat4& GetProjection() const { return m_Projection; }

		// verticalFOV in radians; aspectRatio = width / height.
		static glm::mat4 Perspective(float verticalFOV, float aspectRatio, float nearClip, float farClip);
		// size = world units visible vertically; the width follows from aspectRatio.
		static glm::mat4 Orthographic(float size, float aspectRatio, float nearClip, float farClip);
		static glm::mat4 Orthographic(float left, float right, float bottom, float top, float nearClip, float farClip);

	protected:
		glm::mat4 m_Projection{ 1.0f };
	};
}
