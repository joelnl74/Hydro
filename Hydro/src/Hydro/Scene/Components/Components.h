#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <string>

#define GLM_ENABLE_EXPERIMENTAL
#include <Hydro/Core/Base.h>
#include <Hydro/Core/UUID.h>
#include <Hydro/Renderer/SceneCamera.h>
#include <Hydro/Renderer/Texture2D.h>
#include <glm/gtx/quaternion.hpp>

namespace Hydro
{
	class Entity;
	class Scene;
	class UUID;

	// A quad drawn with the entity's TransformComponent: Texture (white when null), repeated
	// TilingFactor times, multiplied by Color (alpha blended).
	struct SpriteRendererComponent
	{
		glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
		// Shared with every sprite that uses the same file. Saved as the file's path (relative to the
		// project's asset folder when it is inside it).
		// TODO(assets): becomes an AssetHandle once the AssetManager exists (Roadmap "Asset system").
		Ref<Texture2D> Texture;
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

	// Every engine component that is plain, copyable data, apart from TagComponent and IDComponent
	// (every entity has those). Scene::Copy copies these; add new copyable components here.
	template<typename... Component>
	struct ComponentGroup
	{
	};

	using CopyableComponents = ComponentGroup<TransformComponent, SpriteRendererComponent, CameraComponent>;
}
