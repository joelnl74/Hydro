#include <hypch.h>
#include "Scene.h"
#include "Entity.h"
#include "ScriptableEntity.h"

#include "../Renderer/Renderer2D.h"
#include <Hydro/Core/UUID.h>
namespace Hydro
{
	Scene::Scene(const std::string& debugName, bool isEditorScene)
		: m_Name(debugName)
	{

	}

	Scene::~Scene()
	{
		// Give scripts a chance to clean up while the registry and their entities still exist.
		auto view = m_Registry.view<NativeScriptComponent>();
		for (auto entityHandle : view)
		{
			auto& scriptComponent = view.get<NativeScriptComponent>(entityHandle);
			for (auto& script : scriptComponent.Scripts)
			{
				if (script.Instance != nullptr)
					script.Instance->OnDestroy();
			}
		}
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
		entity.AddComponent<IDComponent>(uuid);
		
		tag.Tag = name.empty() ? "Entity" : name;

		return entity;
	}

	void Scene::DestroyEntity(Entity entity)
	{
		if ((bool)entity == false || m_Registry.valid(entity) == false)
			return;

		// Let native scripts clean up before their entity disappears.
		if (entity.HasComponent<NativeScriptComponent>())
		{
			for (auto& script : entity.GetComponent<NativeScriptComponent>().Scripts)
			{
				if (script.Instance != nullptr)
					script.Instance->OnDestroy();
			}
		}

		m_Registry.destroy(entity);
	}
	
	void Scene::OnUpdateRuntime(float deltaTime)
	{
		// 1. Native scripts. Instances are created lazily on their first update.
		auto view = m_Registry.view<NativeScriptComponent>();
		for (auto entityHandle : view)
		{
			auto& scriptComponent = view.get<NativeScriptComponent>(entityHandle);
			for (auto& script : scriptComponent.Scripts)
			{
				if (script.Instance == nullptr)
				{
					script.Instance.reset(script.Instantiate());
					script.Instance->m_Entity = Entity{ entityHandle, this };
					script.Instance->OnCreate();
				}

				script.Instance->OnUpdate(deltaTime);
			}
		}

		// 2. Engine systems shared with the editor.
		OnUpdateEditor(deltaTime);
	}

	void Scene::OnUpdateEditor(float deltaTime)
	{
		// Engine systems that run while editing as well as at runtime (e.g. transform propagation,
		// once there is a hierarchy) go here. None yet.
	}

	void Scene::OnViewportResize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0)
			return;

		m_ViewportWidth = width;
		m_ViewportHeight = height;
	}

	Entity Scene::GetPrimaryCameraEntity()
	{
		auto view = m_Registry.view<CameraComponent>();
		for (auto entityHandle : view)
		{
			if (view.get<CameraComponent>(entityHandle).Primary)
				return Entity{ entityHandle, this };
		}

		return {};
	}
	
	void Scene::OnRender()
	{
	}
}
