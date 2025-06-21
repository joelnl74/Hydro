#pragma once
#include <string>
#include <vector>
#include "../../../vendor/entt/entt.hpp"

#include "../Core/UUID.h"

namespace Hydro
{
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

	private:
		entt::registry m_Registry;

		friend class Entity;
		friend class SceneRenderer;
	};
}
