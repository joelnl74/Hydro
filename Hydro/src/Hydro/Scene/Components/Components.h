#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Hydro
{
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

	struct TransformComponent
	{
		glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Rotation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };

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

	class NativeScriptComponent 
	{
	public:
		virtual void OnCreate() const = 0;
		virtual void OnUpdate(float deltaTime) const = 0;
		virtual void OnDestroy() const = 0;

	private:
		// Entity* m_entity;
		// friend class Scene;
	};

	class MovementComponent : public NativeScriptComponent
	{
	public:
		MovementComponent() = default;
		MovementComponent(const MovementComponent&) = default;

		void OnCreate() const override
		{
			HY_CORE_INFO("MovementComponent::OnCreate() called");
		}

		void OnUpdate(float ts) const override
		{
			HY_CORE_INFO("MovementComponent::OnUpdate() called");
		}

		void OnDestroy() const override
		{
		}
	};

	class PlayerComponent : public NativeScriptComponent
	{
	public:
		PlayerComponent() = default;
		PlayerComponent(const PlayerComponent&) = default;


		void OnCreate() const override
		{
			HY_CORE_INFO("PlayerComponent::OnCreate() called");
		}

		void OnUpdate(float ts) const override
		{
			HY_CORE_INFO("PlayerComponent::OnUpdate() called");
		}

		void OnDestroy() const override
		{
		}
	};
}
