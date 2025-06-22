#pragma once
#include <string>
#include <vector>
#include "../../../vendor/entt/entt.hpp"

#include "../Core/UUID.h"
#include "Components/Components.h"

namespace Hydro
{
	class NativeScriptComponent;
	class Entity;
	class Scene
	{
	public:
		Scene(const std::string& debugName = "Scene", bool isEditorScene = false);
		~Scene();

		void Init();

		void OnUpdate();
		void OnRender();

		Entity CreateEntity(const std::string& name = std::string());
		Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());

	public:
		// THIS SHOULD BE PRIVATE AND EXPOSE A METHOD TO GET COMPONENTS/ENTITIES FROM REGISTRY.
		entt::registry m_Registry;
		std::vector<NativeScriptComponent*> m_scripts;

	private:

		friend class Entity;
		friend class SceneRenderer;
	};
}
