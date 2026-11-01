#include "hypch.h"
#include "ScriptSystem.h"

#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/SystemContext.h"

#include "Hydro/Scene/Scripting/ScriptableEntity.h"
#include "Hydro/Scene/Scripting/ScriptProperty.h"
#include "Hydro/Scene/Scripting/ScriptRegistry.h"
#include "Hydro/Scene/Scripting/ScriptTypeInfo.h"

#include <vector>

namespace Hydro
{
	void UpdateScripts(Scene& scene, const SystemContext& context)
	{
		if (context.IsPlaying == false)
		{
			return;
		}

		std::vector<entt::entity> entities;

		{
			auto view =
				scene.m_Registry.view<NativeScriptComponent>();

			entities.reserve(view.size());

			for (auto entityHandle : view)
			{
				entities.push_back(entityHandle);
			}
		}

		scene.BeginStage();

		// The creation order (Scripting plan SC3): first every new script of this frame is made and given its
		// properties, then their OnCreate run, then every script's OnUpdate. So an OnCreate can find another
		// entity's script (Entity::GetComponent<T>) whatever the order of the entities, with its properties set
		// (its own OnCreate may not have run yet).
		struct NewScript
		{
			entt::entity Handle;
			size_t Index;
			ScriptableEntity* Instance;
		};
		std::vector<NewScript> created;

		for (const entt::entity entityHandle : entities)
		{
			const Entity entity{ entityHandle, &scene };

			if (scene.m_Registry.valid(entityHandle) == false || scene.m_Registry.has<NativeScriptComponent>(entityHandle) == false || scene.IsPendingDestroy(entity))
			{
				continue;
			}

			auto& scripts =
				scene.m_Registry
					.get<NativeScriptComponent>(entityHandle)
					.Scripts;

			for (size_t index = 0; index < scripts.size(); index++)
			{
				NativeScriptComponent::Script& script =
					scripts[index];

				if (script.Instantiate == nullptr || script.PendingRemove || script.Instance != nullptr)
				{
					continue;
				}

				script.Instance.reset(
					script.Instantiate());

				if (script.Instance == nullptr)
				{
					continue;
				}

				script.Instance->m_Entity = entity;

				// Apply exposed values before any OnCreate.
				if (ScriptTypeInfo* typeInfo =
						ScriptRegistry::FindType(script.Name))
				{
					for (const ScriptProperty& property :
						typeInfo->Properties)
					{
						auto it =
							script.Properties.find(
								property.Name);

						if (it == script.Properties.end())
						{
							continue;
						}

						if (property.Apply == nullptr)
						{
							continue;
						}

						property.Apply(
							*script.Instance,
							it->second);
					}
				}

				created.push_back({ entityHandle, index, script.Instance.get() });
			}
		}

		// The script at (handle, index) is still the one made above: an earlier OnCreate may have destroyed its
		// entity or removed it.
		auto stillThere = [&scene](const NewScript& item) -> bool
		{
			const Entity entity{ item.Handle, &scene };
			if (scene.m_Registry.valid(item.Handle) == false || scene.m_Registry.has<NativeScriptComponent>(item.Handle) == false || scene.IsPendingDestroy(entity))
			{
				return false;
			}
			const auto& scripts = scene.m_Registry.get<NativeScriptComponent>(item.Handle).Scripts;
			return item.Index < scripts.size() && scripts[item.Index].PendingRemove == false && scripts[item.Index].Instance.get() == item.Instance;
		};

		for (const NewScript& item : created)
		{
			if (stillThere(item))
			{
				item.Instance->OnCreate();
			}
		}

		for (const entt::entity entityHandle : entities)
		{
			const Entity entity{ entityHandle, &scene };

			if (scene.m_Registry.valid(entityHandle) == false || scene.m_Registry.has<NativeScriptComponent>(entityHandle) == false || scene.IsPendingDestroy(entity))
			{
				continue;
			}

			const size_t scriptCount =
				scene.m_Registry
					.get<NativeScriptComponent>(entityHandle)
					.Scripts.size();

			for (size_t index = 0; index < scriptCount; index++)
			{
				if (scene.IsPendingDestroy(entity))
				{
					break;
				}

				auto& scripts =
					scene.m_Registry
						.get<NativeScriptComponent>(entityHandle)
						.Scripts;

				if (index >= scripts.size())
				{
					break;
				}

				NativeScriptComponent::Script& script =
					scripts[index];

				// A script bound during this frame's OnCreate / OnUpdate is made next frame.
				if (script.Instance == nullptr || script.PendingRemove)
				{
					continue;
				}

				script.Instance->OnUpdate(
					context.DeltaTime);
			}
		}

		scene.EndStage();
	}
}
