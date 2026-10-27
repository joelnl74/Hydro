#include <hypch.h>
#include "Scene.h"
#include "Entity.h"
#include "ScriptableEntity.h"
#include "ScriptSystem.h"
#include "SpriteAnimator.h"
#include "SystemContext.h"
#include "Hydro/Physics/PhysicsScene.h"
#include "ParticleScene.h"

#include "../Renderer/Renderer2D.h"
#include "Hydro/Asset/AssetManager.h"
#include <Hydro/Core/Time.h>
#include <Hydro/Core/UUID.h>

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
namespace Hydro
{
	Scene::Scene(const std::string& debugName, [[maybe_unused]] bool isEditorScene)
		: m_Name(debugName)
	{

	}

	Scene::~Scene()
	{
		// Give scripts a chance to clean up while the registry and their entities still exist. A stage
		// is open so that anything a script's OnDestroy asks for (DestroyEntity...) is queued and then
		// dropped instead of changing the registry under this loop.
		m_StageDepth++;

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

		m_Deferred.clear();
		m_PendingDestroy.clear();
		m_Particles.reset();
		m_Physics.reset();
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

	Ref<RenderProfile> Scene::GetRenderProfile() const
	{
		return (uint64_t)m_Look.Profile != 0 ? AssetManager::GetLoadedAsset<RenderProfile>(m_Look.Profile) : nullptr;
	}

	SceneEnvironment Scene::GetActiveEnvironment() const
	{
		if (m_Look.Environment == RenderLookSource::Profile)
		{
			if (const Ref<RenderProfile> profile = GetRenderProfile(); profile != nullptr)
			{
				return profile->GetData().Environment;
			}
		}
		return m_Environment;
	}

	ScenePostProcess Scene::GetActivePostProcess() const
	{
		if (m_Look.PostProcess == RenderLookSource::Profile)
		{
			if (const Ref<RenderProfile> profile = GetRenderProfile(); profile != nullptr)
			{
				return profile->GetData().PostProcess;
			}
		}
		return m_PostProcess;
	}

	Ref<Scene> Scene::Copy(const Ref<Scene>& source)
	{
		Ref<Scene> copy = CreateRef<Scene>(source->GetName());
		copy->m_ViewportWidth = source->m_ViewportWidth;
		copy->m_ViewportHeight = source->m_ViewportHeight;
		copy->m_RenderSettings = source->m_RenderSettings;
		copy->m_Environment = source->m_Environment;
		copy->m_PostProcess = source->m_PostProcess;
		copy->m_Look = source->m_Look;

		const entt::registry& sourceRegistry = source->m_Registry;

		// Created in the source's list order, so the copy lists entities the same way.
		std::unordered_map<entt::entity, entt::entity> entityMap;
		for (entt::entity sourceEntity : source->GetEntityOrder())
		{
			const UUID uuid = sourceRegistry.get<IDComponent>(sourceEntity).Id;
			std::string name;
			if (sourceRegistry.has<TagComponent>(sourceEntity))
			{
				name = sourceRegistry.get<TagComponent>(sourceEntity).Tag;
			}

			entityMap[sourceEntity] = copy->CreateEntityWithUUID(uuid, name);
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
		MoveEntity(duplicate, entity, true);
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

	const std::vector<entt::entity>& Scene::GetEntityOrder()
	{
		// Drop entities that were destroyed straight through the registry.
		m_EntityOrder.erase(std::remove_if(m_EntityOrder.begin(), m_EntityOrder.end(), [&](entt::entity handle)
		{
			return m_Registry.valid(handle) == false || m_Registry.has<IDComponent>(handle) == false;
		}), m_EntityOrder.end());

		// Pick up entities that got an IDComponent without going through CreateEntity.
		if (m_Registry.size<IDComponent>() != m_EntityOrder.size())
		{
			const std::unordered_set<entt::entity> known(m_EntityOrder.begin(), m_EntityOrder.end());
			std::vector<entt::entity> missing;
			for (entt::entity handle : m_Registry.view<IDComponent>())
			{
				if (known.find(handle) == known.end())
				{
					missing.push_back(handle);
				}
			}

			// The view lists newest first; the list is oldest first.
			m_EntityOrder.insert(m_EntityOrder.end(), missing.rbegin(), missing.rend());
		}

		return m_EntityOrder;
	}

	void Scene::MoveEntity(Entity entity, Entity target, bool placeAfter)
	{
		if ((bool)entity == false || (bool)target == false || entity == target)
		{
			return;
		}

		GetEntityOrder();

		const entt::entity entityHandle = (entt::entity)entity;
		const entt::entity targetHandle = (entt::entity)target;
		if (std::find(m_EntityOrder.begin(), m_EntityOrder.end(), entityHandle) == m_EntityOrder.end()
			|| std::find(m_EntityOrder.begin(), m_EntityOrder.end(), targetHandle) == m_EntityOrder.end())
		{
			return;
		}

		m_EntityOrder.erase(std::remove(m_EntityOrder.begin(), m_EntityOrder.end(), entityHandle), m_EntityOrder.end());

		auto position = std::find(m_EntityOrder.begin(), m_EntityOrder.end(), targetHandle);
		if (placeAfter)
		{
			++position;
		}

		m_EntityOrder.insert(position, entityHandle);
	}

	Entity Scene::CreateEntity(const std::string& name)
	{
		return CreateEntityWithUUID({}, name);
	}

	Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name)
	{
		// An entity with this UUID is already here (an entry for a destroyed one doesn't count).
		const auto existing = m_EntityMap.find(uuid);
		if (existing != m_EntityMap.end())
		{
			const entt::entity handle = existing->second;
			if (m_Registry.valid(handle) && m_Registry.has<IDComponent>(handle) && m_Registry.get<IDComponent>(handle).Id == uuid)
			{
				HY_CORE_ERROR("Scene '{0}': UUID {1} is already used by an entity; '{2}' gets a new one", m_Name, (uint64_t)uuid, name);
				do
				{
					uuid = UUID();
				}
				while (m_EntityMap.find(uuid) != m_EntityMap.end());
			}
		}

		Entity entity = { m_Registry.create(), this };
		auto& tag = entity.AddComponent<TagComponent>();
		entity.AddComponent<IDComponent>(uuid);
		
		tag.Tag = name.empty() ? "Entity" : name;

		m_EntityOrder.push_back((entt::entity)entity);
		m_EntityMap[uuid] = (entt::entity)entity;

		return entity;
	}

	Entity Scene::GetEntityByUUID(UUID uuid)
	{
		for (int attempt = 0; attempt < 2; attempt++)
		{
			const auto found = m_EntityMap.find(uuid);
			if (found != m_EntityMap.end())
			{
				const entt::entity handle = found->second;
				if (m_Registry.valid(handle) && m_Registry.has<IDComponent>(handle) && m_Registry.get<IDComponent>(handle).Id == uuid)
				{
					return Entity{ handle, this };
				}

				// Destroyed (or its ID changed) behind the scene's back.
				m_EntityMap.erase(found);
			}

			// A miss: unless the map matches the registry exactly, an entity may have gotten an
			// IDComponent without CreateEntity, so look at the registry once before saying no.
			if (attempt == 0)
			{
				if (m_EntityMap.size() == m_Registry.size<IDComponent>())
				{
					break;
				}

				RebuildEntityMap();
			}
		}

		return {};
	}

	void Scene::RebuildEntityMap()
	{
		m_EntityMap.clear();
		for (entt::entity handle : m_Registry.view<IDComponent>())
		{
			// The first entity with a UUID keeps it if (through the registry) two share one.
			m_EntityMap.emplace(m_Registry.get<IDComponent>(handle).Id, handle);
		}
	}

	void Scene::DestroyEntity(Entity entity)
	{
		if ((bool)entity == false || m_Registry.valid(entity) == false)
			return;

		if (m_StageDepth > 0)
		{
			// A script may be running on this very entity: stop it from running again, remove it later.
			if (m_PendingDestroy.insert((entt::entity)entity).second)
			{
				m_Deferred.push_back({ DeferredCommand::Type::DestroyEntity, (entt::entity)entity });
			}
			return;
		}

		DestroyEntityNow(entity);
	}

	void Scene::DestroyEntityNow(Entity entity)
	{
		if ((bool)entity == false || m_Registry.valid(entity) == false)
			return;

		m_PendingDestroy.erase((entt::entity)entity);
		// Its physics body goes with it (a body whose entity is gone would be written back to nothing).
		if (m_Physics != nullptr)
			m_Physics->OnEntityDestroyed((entt::entity)entity);
		// Let native scripts clean up before their entity disappears.
		if (entity.HasComponent<NativeScriptComponent>())
		{
			for (auto& script : entity.GetComponent<NativeScriptComponent>().Scripts)
			{
				if (script.Instance != nullptr)
					script.Instance->OnDestroy();
			}
		}

		// Looked up until now, so a script's OnDestroy can still find its own entity by UUID.
		if (m_Registry.has<IDComponent>(entity))
		{
			const UUID uuid = m_Registry.get<IDComponent>(entity).Id;
			const auto found = m_EntityMap.find(uuid);
			if (found != m_EntityMap.end() && found->second == (entt::entity)entity)
			{
				m_EntityMap.erase(found);
			}
		}

		m_EntityOrder.erase(std::remove(m_EntityOrder.begin(), m_EntityOrder.end(), (entt::entity)entity), m_EntityOrder.end());
		m_Registry.destroy(entity);
	}

	bool Scene::IsPendingDestroy(Entity entity) const
	{
		return (bool)entity && m_PendingDestroy.find((entt::entity)entity) != m_PendingDestroy.end();
	}

	void Scene::RemoveScript(Entity entity, size_t index)
	{
		if (entity.HasComponent<NativeScriptComponent>() == false)
			return;

		auto& scripts = entity.GetComponent<NativeScriptComponent>().Scripts;
		if (index >= scripts.size())
			return;

		if (m_StageDepth > 0)
		{
			scripts[index].PendingRemove = true;
			m_Deferred.push_back({ DeferredCommand::Type::RemoveFlaggedScripts, (entt::entity)entity });
			return;
		}

		if (scripts[index].Instance != nullptr)
			scripts[index].Instance->OnDestroy();

		scripts.erase(scripts.begin() + index);
	}

	void Scene::RemoveScripts(Entity entity)
	{
		if (entity.HasComponent<NativeScriptComponent>() == false)
			return;

		if (m_StageDepth > 0)
		{
			for (auto& script : entity.GetComponent<NativeScriptComponent>().Scripts)
			{
				script.PendingRemove = true;
			}

			m_Deferred.push_back({ DeferredCommand::Type::RemoveFlaggedScripts, (entt::entity)entity });
			return;
		}

		for (auto& script : entity.GetComponent<NativeScriptComponent>().Scripts)
		{
			if (script.Instance != nullptr)
				script.Instance->OnDestroy();
		}

		entity.RemoveComponent<NativeScriptComponent>();
	}

	void Scene::RemoveFlaggedScriptsNow(entt::entity handle)
	{
		if (m_Registry.valid(handle) == false || m_Registry.has<NativeScriptComponent>(handle) == false)
			return;

		// OnDestroy can call back into the scene (queued, since a stage is open) and can bind scripts,
		// so no reference into the vector is kept across it: take one flagged script at a time out first.
		for (;;)
		{
			auto& scripts = m_Registry.get<NativeScriptComponent>(handle).Scripts;
			auto flagged = std::find_if(scripts.begin(), scripts.end(), [](const NativeScriptComponent::Script& script)
			{
				return script.PendingRemove;
			});
			if (flagged == scripts.end())
				break;

			std::unique_ptr<ScriptableEntity> instance = std::move(flagged->Instance);
			scripts.erase(flagged);
			if (instance != nullptr)
				instance->OnDestroy();
		}
	}

	void Scene::BeginStage()
	{
		m_StageDepth++;
	}

	void Scene::EndStage()
	{
		m_StageDepth--;
		if (m_StageDepth == 0)
		{
			ApplyDeferred();
		}
	}

	void Scene::ApplyDeferred()
	{
		// A stage stays open while this runs, so commands queued by OnDestroy callbacks wait in
		// m_Deferred and are picked up by the next round of the loop.
		m_StageDepth++;

		while (m_Deferred.empty() == false)
		{
			std::vector<DeferredCommand> batch = std::move(m_Deferred);
			m_Deferred.clear();

			for (const DeferredCommand& command : batch)
			{
				if (command.Kind == DeferredCommand::Type::RemoveFlaggedScripts)
					RemoveFlaggedScriptsNow(command.Handle);
				else
					DestroyEntityNow(Entity{ command.Handle, this });
			}
		}

		m_StageDepth--;
	}

	// One context per update, shared by every stage of that frame.
	static SystemContext MakeSystemContext(float deltaTime, bool isPlaying)
	{
		SystemContext context;
		context.DeltaTime = deltaTime;
		context.FrameIndex = Time::GetFrameCount();
		context.IsPlaying = isPlaying;
		return context;
	}

	void Scene::NotifyScripts(entt::entity self, entt::entity other, void (ScriptableEntity::*hook)(Entity))
	{
		if (m_Registry.valid(self) == false || m_Registry.valid(other) == false || m_Registry.has<NativeScriptComponent>(self) == false)
		{
			return;
		}
		if (IsPendingDestroy(Entity{ self, this }))
		{
			return;
		}

		// A snapshot: a script may add components, which can move the script list while it is walked.
		// Scripts removed meanwhile stay alive until the stage ends (RemoveScript is deferred).
		std::vector<ScriptableEntity*> instances;
		for (auto& script : m_Registry.get<NativeScriptComponent>(self).Scripts)
		{
			if (script.Instance != nullptr && script.PendingRemove == false)
			{
				instances.push_back(script.Instance.get());
			}
		}

		for (ScriptableEntity* instance : instances)
		{
			(instance->*hook)(Entity{ other, this });
		}
	}

	void Scene::DispatchPhysicsEvents()
	{
		if (m_Physics == nullptr || m_Physics->GetEvents().empty())
		{
			return;
		}

		// Copied: nothing may change the list while scripts run, but a copy costs little and can't dangle.
		const std::vector<PhysicsScene::Event> events = m_Physics->GetEvents();

		BeginStage();
		for (const PhysicsScene::Event& event : events)
		{
			switch (event.Kind)
			{
			case PhysicsScene::Event::Type::CollisionBegin:
				NotifyScripts(event.A, event.B, &ScriptableEntity::OnCollisionEnter);
				NotifyScripts(event.B, event.A, &ScriptableEntity::OnCollisionEnter);
				break;
			case PhysicsScene::Event::Type::CollisionEnd:
				NotifyScripts(event.A, event.B, &ScriptableEntity::OnCollisionExit);
				NotifyScripts(event.B, event.A, &ScriptableEntity::OnCollisionExit);
				break;
			case PhysicsScene::Event::Type::TriggerBegin:
				NotifyScripts(event.A, event.B, &ScriptableEntity::OnTriggerEnter);
				NotifyScripts(event.B, event.A, &ScriptableEntity::OnTriggerEnter);
				break;
			case PhysicsScene::Event::Type::TriggerEnd:
				NotifyScripts(event.A, event.B, &ScriptableEntity::OnTriggerExit);
				NotifyScripts(event.B, event.A, &ScriptableEntity::OnTriggerExit);
				break;
			}
		}
		EndStage();
	}

	void Scene::OnRuntimeStart()
	{
		if (m_Physics == nullptr)
		{
			m_Physics = CreateScope<PhysicsScene>(*this);
		}
		if (m_Particles == nullptr)
		{
			m_Particles = CreateScope<ParticleScene>();
		}
		// The play copy carries the editor scene's runtime state; the game starts from the saved settings.
		ResetSpriteAnimations(*this);
	}

	void Scene::OnRuntimeStop()
	{
		m_Particles.reset();
		m_Physics.reset();
	}

	void Scene::OnUpdateRuntime(float deltaTime)
	{
		const SystemContext context = MakeSystemContext(deltaTime, true);

		// Nobody called OnRuntimeStart: do it before the first stage, so it can't undo what the
		// scripts start in their first update (it resets the animators).
		if (m_Physics == nullptr)
		{
			OnRuntimeStart();
		}

		// 1. Native scripts. Instances are created lazily on their first update.
		UpdateScripts(*this, context);

		// 2. Physics: after the scripts have set velocities and moved kinematic bodies, before animation
		// and transforms read the result.
		m_Physics->Step(context);
		DispatchPhysicsEvents();

		// 3. Sprite animation, after the scripts so what a script started this frame shows this frame.
		UpdateSpriteAnimations(*this, context);

		// 4. Transforms, shared with the editor.
		UpdateTransforms(context);

		// 5. Particles, after transforms so emitters read their final position this frame. Finished
		// one-shot effects (DestroyWhenFinished) are destroyed here, after the emitter loop.
		UpdateParticles(*this, context);
	}

	void Scene::OnUpdateEditor(float deltaTime)
	{
		// Only the stages that keep the scene consistent while editing: no scripts, physics or animation.
		const SystemContext context = MakeSystemContext(deltaTime, false);

		UpdateTransforms(context);
	}

	void Scene::UpdateTransforms([[maybe_unused]] const SystemContext& context)
	{
		// World-transform propagation goes here once there is a parent/child hierarchy (S3). None yet.
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
	
	Entity Scene::GetMainLight()
	{
		Entity first;
		Entity main;
		uint32_t marked = 0;
		// Entity order (what the hierarchy shows), so "the first" is stable.
		for (entt::entity handle : m_EntityOrder)
		{
			if (m_Registry.valid(handle) == false || m_Registry.has<DirectionalLightComponent>(handle) == false)
			{
				continue;
			}
			if ((bool)first == false)
			{
				first = Entity{ handle, this };
			}
			if (m_Registry.get<DirectionalLightComponent>(handle).IsMainLight)
			{
				if (marked == 0)
				{
					main = Entity{ handle, this };
				}
				marked++;
			}
		}

		if (marked > 1 && m_MainLightWarned == false)
		{
			HY_CORE_WARN("Scene '{}': {} directional lights are marked as the main light; the first is used", m_Name, marked);
			m_MainLightWarned = true;
		}
		return marked > 0 ? main : first;
	}

	void Scene::OnRender()
	{
	}
}
