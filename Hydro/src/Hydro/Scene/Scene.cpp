#include <hypch.h>
#include "Scene.h"
#include "Entity.h"

#include "../Renderer/Renderer2D.h"
#include <Hydro/Core/UUID.h>
namespace Hydro
{
	Scene::Scene(const std::string& debugName, bool isEditorScene)
	{

	}

	Scene::~Scene()
	{
	}
	
	void Scene::Init()
	{

	}

	Entity Scene::CreateEntity(const std::string& name)
	{
		return CreateEntityWithUUID({}, name);
	}

	Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name)
	{
		Entity entity = { m_Registry.create(), this };
		auto& tag = entity.AddComponent<TagComponent>();
		auto& id = entity.AddComponent<IDComponent>();
		
		tag.Tag = name.empty() ? "Entity" : name;

		return entity;
	}
	
	void Scene::OnUpdate()
	{
		{
			for (size_t i = 0; i < m_scripts.size(); i++)
			{
				m_scripts[i]->OnUpdate(0.0f);
			}
		}

	}
	
	void Scene::OnRender()
	{
	}
}
