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

	// A projection matrix. The base of EditorCamera, and what SceneRenderer builds from a scene's
	// CameraComponent (SceneCamera::GetProjection) to call Renderer2D::BeginScene.
	//
	// Every projection in Hydro is built by the static functions below, so the Vulkan clip-space
	// conventions are handled once, here:
	//  - depth range [0, 1] (GLM's *_ZO functions; no GLM_FORCE_DEPTH_ZERO_TO_ONE needed),
	//  - Y flip: Vulkan's clip-space Y points down; world +Y points up on screen.
	// Right-handed: the camera looks down -Z, +X is right, +Y is up.
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
