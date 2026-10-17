#pragma once
#include <string>
#include <vector>
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

		void OnUpdate(float deltaTime);
		void OnRender();

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

		friend class Entity;
		friend class SceneRenderer;
	};
}
