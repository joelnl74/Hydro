#include <hypch.h>
#include "Scene.h"
#include "Entity.h"
#include "ScriptableEntity.h"
#include "SpriteAnimator.h"

#include "../Renderer/Renderer2D.h"
#include <Hydro/Core/UUID.h>

#include <unordered_map>
namespace Hydro
{
	Scene::Scene(const std::string& debugName, [[maybe_unused]] bool isEditorScene)
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

	template<typename... Component>
	static void CopyComponents(ComponentGroup<Component...>, entt::registry& destination, const entt::registry& source,
		const std::unordered_map<entt::entity, entt::entity>& entityMap)
	{
		([&]()
		{
			auto view = source.view<const Component>();
			for (auto sourceEntity : view)
			{
				auto it = entityMap.find(sourceEntity);
				if (it != entityMap.end())
					destination.emplace_or_replace<Component>(it->second, view.template get<const Component>(sourceEntity));
			}
		}(), ...);
	}

	Ref<Scene> Scene::Copy(const Ref<Scene>& source)
	{
		Ref<Scene> copy = CreateRef<Scene>(source->GetName());
		copy->m_ViewportWidth = source->m_ViewportWidth;
		copy->m_ViewportHeight = source->m_ViewportHeight;

		const entt::registry& sourceRegistry = source->m_Registry;

		// Same order as the source (entt iterates newest first), so the copy lists entities the same way.
		std::vector<entt::entity> entities;
		sourceRegistry.each([&](auto entityHandle)
		{
			if (sourceRegistry.has<IDComponent>(entityHandle))
				entities.push_back(entityHandle);
		});

		std::unordered_map<entt::entity, entt::entity> entityMap;
		for (auto it = entities.rbegin(); it != entities.rend(); ++it)
		{
			const UUID uuid = sourceRegistry.get<IDComponent>(*it).Id;
			std::string name;
			if (sourceRegistry.has<TagComponent>(*it))
				name = sourceRegistry.get<TagComponent>(*it).Tag;

			entityMap[*it] = copy->CreateEntityWithUUID(uuid, name);
		}

		CopyComponents(CopyableComponents{}, copy->m_Registry, sourceRegistry, entityMap);

		// Native scripts: copy the bindings, not the instances (those belong to the source scene).
		auto scripts = sourceRegistry.view<const NativeScriptComponent>();
		for (auto sourceEntity : scripts)
		{
			auto it = entityMap.find(sourceEntity);
			if (it == entityMap.end())
				continue;

			NativeScriptComponent& copied = copy->m_Registry.emplace<NativeScriptComponent>(it->second);
			for (const auto& script : scripts.get<const NativeScriptComponent>(sourceEntity).Scripts)
			{
				NativeScriptComponent::Script binding;
				binding.Name = script.Name;
				binding.Instantiate = script.Instantiate;
				copied.Scripts.push_back(std::move(binding));
			}
		}

		return copy;
	}

	template<typename... Component>
	static void CopyComponentsOfEntity(ComponentGroup<Component...>, entt::registry& registry, entt::entity source, entt::entity destination)
	{
		([&]()
		{
			if (registry.has<Component>(source))
			{
				// Copy first: emplacing can reallocate the pool the source component lives in.
				Component copy = registry.get<Component>(source);
				registry.emplace_or_replace<Component>(destination, std::move(copy));
			}
		}(), ...);
	}

	Entity Scene::DuplicateEntity(Entity entity)
	{
		if ((bool)entity == false || m_Registry.valid(entity) == false || m_Registry.has<IDComponent>(entity) == false)
		{
			return {};
		}

		Entity duplicate = CreateEntity(entity.GetName());
		CopyComponentsOfEntity(CopyableComponents{}, m_Registry, entity, duplicate);

		// Native scripts: copy the bindings, not the instances (those belong to the source entity).
		if (m_Registry.has<NativeScriptComponent>(entity))
		{
			std::vector<NativeScriptComponent::Script> bindings;
			for (const auto& script : m_Registry.get<NativeScriptComponent>(entity).Scripts)
			{
				NativeScriptComponent::Script binding;
				binding.Name = script.Name;
				binding.Instantiate = script.Instantiate;
				bindings.push_back(std::move(binding));
			}

			m_Registry.emplace<NativeScriptComponent>(duplicate).Scripts = std::move(bindings);
		}

		return duplicate;
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
	
	void Scene::RemoveScript(Entity entity, size_t index)
	{
		if (entity.HasComponent<NativeScriptComponent>() == false)
			return;

		auto& scripts = entity.GetComponent<NativeScriptComponent>().Scripts;
		if (index >= scripts.size())
			return;

		if (scripts[index].Instance != nullptr)
			scripts[index].Instance->OnDestroy();

		scripts.erase(scripts.begin() + index);
	}

	void Scene::RemoveScripts(Entity entity)
	{
		if (entity.HasComponent<NativeScriptComponent>() == false)
			return;

		for (auto& script : entity.GetComponent<NativeScriptComponent>().Scripts)
		{
			if (script.Instance != nullptr)
				script.Instance->OnDestroy();
		}

		entity.RemoveComponent<NativeScriptComponent>();
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
				// Unknown script (not registered): kept so the scene saves it back, but it can't run.
				if (script.Instantiate == nullptr)
					continue;

				if (script.Instance == nullptr)
				{
					script.Instance.reset(script.Instantiate());
					script.Instance->m_Entity = Entity{ entityHandle, this };
					script.Instance->OnCreate();
				}

				script.Instance->OnUpdate(deltaTime);
			}
		}

		// 2. Game-only engine systems, after the scripts so what a script started this frame shows
		// this frame.
		UpdateSpriteAnimations(m_Registry, deltaTime);

		// 3. Engine systems shared with the editor.
		OnUpdateEditor(deltaTime);
	}

	void Scene::OnUpdateEditor([[maybe_unused]] float deltaTime)
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
