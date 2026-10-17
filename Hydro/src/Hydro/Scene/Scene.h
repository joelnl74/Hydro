#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "../../../vendor/entt/entt.hpp"

#include "Components/Components.h"

namespace Hydro
{
	class Entity;
	class UUID;

	class Scene
	{
	public:
		Scene(const std::string& debugName = "Scene", bool isEditorScene = false);
		~Scene();

		void Init();

		// Runtime (the game is running): native scripts, then the other engine systems.
		void OnUpdateRuntime(float deltaTime);
		// Editing: the engine systems that keep the scene consistent, without scripts.
		void OnUpdateEditor(float deltaTime);
		void OnRender();

		// The size the scene is viewed at; scene cameras take their aspect ratio from it.
		void OnViewportResize(uint32_t width, uint32_t height);
		uint32_t GetViewportWidth() const { return m_ViewportWidth; }
		uint32_t GetViewportHeight() const { return m_ViewportHeight; }
		float GetViewportAspectRatio() const { return (float)m_ViewportWidth / (float)m_ViewportHeight; }

		// The first entity with a CameraComponent marked Primary, or an empty Entity.
		Entity GetPrimaryCameraEntity();

		Entity CreateEntity(const std::string& name = std::string());
		Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());
		void DestroyEntity(Entity entity);

		const std::string& GetName() const { return m_Name; }
		void SetName(const std::string& name) { m_Name = name; }

	public:
		// THIS SHOULD BE PRIVATE AND EXPOSE A METHOD TO GET COMPONENTS/ENTITIES FROM REGISTRY.
		entt::registry m_Registry;

	private:
		std::string m_Name;
		uint32_t m_ViewportWidth = 1280;
		uint32_t m_ViewportHeight = 720;

		friend class Entity;
		friend class SceneRenderer;
	};
}
