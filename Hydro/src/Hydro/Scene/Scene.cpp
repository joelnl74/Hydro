#include <hypch.h>
#include "Scene.h"
#include "Entity.h"

#include "../Renderer/Renderer2D.h"

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
		auto player = CreateEntity("Player Entity");
		player.AddComponent<TransformComponent>(glm::vec3(700, 450, 0), glm::vec3(128, 128, 0));
		player.AddComponent<SpriteRendererComponent>(glm::vec4(1, 0, 0, 1));
		auto& a = player.AddComponent<MovementComponent>();
		auto& b = player.AddComponent<PlayerComponent>();

		m_scripts.push_back(&a);
		m_scripts.push_back(&b);


		for (size_t i = 0; i < 24; i++)
		{
			for (size_t j = 0; j < 24; j++)
			{
				auto Sprite = CreateEntity("Entity" + std::to_string(i + j * i));
				Sprite.AddComponent<TransformComponent>(glm::vec3(j * 64, i * 64, 0), glm::vec3(64, 64, 0));
				Sprite.AddComponent<SpriteRendererComponent>(glm::vec4(j * 0.05, i * 0.05, 1, 1));
			}
		}
	}

	Entity Scene::CreateEntity(const std::string& name)
	{
		return CreateEntityWithUUID(UUID(), name);
	}

	Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name)
	{
		Entity entity = { m_Registry.create(), this };
		auto& tag = entity.AddComponent<TagComponent>();
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
