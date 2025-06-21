#include <hypch.h>
#include "Scene.h"
#include "Entity.h"
#include "ScriptableEntity.h"

#include "../Renderer/Renderer2D.h"

#include "Components/Components.h"

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

		for (size_t i = 0; i < 24; i++)
		{
			for (size_t j = 0; j < 24; j++)
			{
				auto Sprite = CreateEntity("Sprite");
				Sprite.AddComponent<TransformComponent>(glm::vec3(j * 64, i * 64, 0), glm::vec3(64, 64, 0));
				Sprite.AddComponent<SpriteRendererComponent>(glm::vec4(j * 0.05, i * 0.05, 1, 1));
			}
		}

		auto player = CreateEntity("Sprite");
		player.AddComponent<TransformComponent>(glm::vec3(700, 450, 0), glm::vec3(64, 64, 0));
		player.AddComponent<SpriteRendererComponent>(glm::vec4(1, 0, 0, 1));
		// player.AddComponent<NativeScriptComponent>().Bind<MovementComponent>();
		player.AddComponent<NativeScriptComponent>().Bind<PlayerComponent>();
	}

	Entity Scene::CreateEntity(const std::string& name)
	{
		return CreateEntityWithUUID(UUID(), name);
	}

	Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name)
	{
		Entity entity = { m_Registry.create(), this };
		// entity.AddComponent<IDComponent>(uuid);
		// entity.AddComponent<TransformComponent>();
		// auto& tag = entity.AddComponent<TagComponent>();
		// tag.Tag = name.empty() ? "Entity" : name;

		// m_EntityMap[uuid] = entity;

		return entity;
	}
	
	void Scene::OnUpdate()
	{
		{
			m_Registry.view<NativeScriptComponent>().each([=](auto entity, auto& script)
			{
				if (script.Instance == nullptr)
				{
					script.OnInstantiateFunction();
					script.Instance->OnCreate();
				}

				script.Instance->OnUpdate(0.0f);
			});
		}

	}
	
	void Scene::OnRender()
	{
	}
}
