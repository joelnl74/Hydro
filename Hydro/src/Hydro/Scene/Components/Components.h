#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <string>

#define GLM_ENABLE_EXPERIMENTAL
#include <Hydro/Core/UUID.h>
#include <Hydro/Renderer/SceneCamera.h>
#include <glm/gtx/quaternion.hpp>

namespace Hydro
{
	class Entity;
	class Scene;
	class UUID;

	struct SpriteRendererComponent
	{
		glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
		// Ref<Texture2D> Texture;
		float TilingFactor = 1.0f;

		SpriteRendererComponent() = default;
		SpriteRendererComponent(const SpriteRendererComponent&) = default;
		SpriteRendererComponent(const glm::vec4& color)
			: Color(color) {
		}
	};

	struct TagComponent
	{
		std::string Tag;

		TagComponent() = default;
		TagComponent(const TagComponent& other) = default;
		TagComponent(const std::string& tag)
			: Tag(tag) {
		}

		operator std::string& () { return Tag; }
		operator const std::string& () const { return Tag; }
	};

	struct IDComponent
	{
		UUID Id = {};

		IDComponent() = default;
		IDComponent(const IDComponent&) = default;
		IDComponent(const UUID& id)
			: Id(id) {
		}
	};

	struct TransformComponent
	{
		glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };
		glm::vec3 Rotation = { 0.0f, 0.0f, 0.0f };

		TransformComponent() = default;
		TransformComponent(const TransformComponent&) = default;
		TransformComponent(const glm::vec3& translation)
			: Translation(translation) {
		}
		TransformComponent(const glm::vec3& translation, const glm::vec3 scale)
			: Translation(translation), Scale(scale) {
		}
		TransformComponent(const glm::vec3& translation, const glm::vec3 scale, const glm::vec3 rotation)
			: Translation(translation), Scale(scale), Rotation(rotation) {
		}

		glm::mat4 GetTransform() const
		{
			glm::mat4 rotation = glm::toMat4(glm::quat(Rotation));

			return glm::translate(glm::mat4(1.0f), Translation)
				* rotation
				* glm::scale(glm::mat4(1.0f), Scale);
		}
	};

	// A camera the scene can be viewed through. Its position and rotation come from the entity's
	// TransformComponent (scale is ignored), and its aspect ratio from the scene's viewport.
	// SceneRenderer::RenderRuntime draws the scene from the first camera with Primary set.
	struct CameraComponent
	{
		SceneCamera Camera;
		bool Primary = true;

		CameraComponent() = default;
		CameraComponent(const CameraComponent&) = default;
	};
}
