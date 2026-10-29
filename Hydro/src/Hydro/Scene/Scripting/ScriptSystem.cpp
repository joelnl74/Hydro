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

				if (script.Instantiate == nullptr || script.PendingRemove)
				{
					continue;
				}

				if (script.Instance == nullptr)
				{
					script.Instance.reset(
						script.Instantiate());

					if (script.Instance == nullptr)
					{
						continue;
					}

					script.Instance->m_Entity = entity;

					// Apply exposed values before OnCreate.
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

					ScriptableEntity* instance =
						script.Instance.get();

					instance->OnCreate();

					if (scene.IsPendingDestroy(entity))
					{
						break;
					}

					instance->OnUpdate(
						context.DeltaTime);
				}
				else
				{
					script.Instance->OnUpdate(
						context.DeltaTime);
				}
			}
		}

		scene.EndStage();
	}
}
