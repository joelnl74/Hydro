#include "hypch.h"
#include "ScriptSystem.h"

#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/ScriptableEntity.h"
#include "Hydro/Scene/SystemContext.h"

#include <vector>

namespace Hydro
{
	void UpdateScripts(Scene& scene, const SystemContext& context)
	{
		if (context.IsPlaying == false)
		{
			return;
		}

		// Walk a snapshot, not the live view: a script may create entities, add components or bind
		// scripts, and any of those can move the registry's storage under a view. Entities created during
		// the stage are not in the snapshot, so their scripts start on the next update.
		std::vector<entt::entity> entities;
		{
			auto view = scene.m_Registry.view<NativeScriptComponent>();
			entities.reserve(view.size());
			for (auto entityHandle : view)
			{
				entities.push_back(entityHandle);
			}
		}

		// Destroying an entity or removing a script is queued while the stage is open and applied
		// by EndStage, so a script never loses its entity (or itself) in the middle of a callback.
		scene.BeginStage();

		for (const entt::entity entityHandle : entities)
		{
			const Entity entity{ entityHandle, &scene };

			if (scene.m_Registry.valid(entityHandle) == false || scene.m_Registry.has<NativeScriptComponent>(entityHandle) == false
				|| scene.IsPendingDestroy(entity))
			{
				continue;
			}

			// Scripts bound during this stage are appended and start next update, so only the ones that
			// existed when the entity's turn began run now. The component is fetched again for every
			// script: a callback can add components, which may move it.
			const size_t scriptCount = scene.m_Registry.get<NativeScriptComponent>(entityHandle).Scripts.size();
			for (size_t index = 0; index < scriptCount; index++)
			{
				if (scene.IsPendingDestroy(entity))
				{
					break;
				}

				auto& scripts = scene.m_Registry.get<NativeScriptComponent>(entityHandle).Scripts;
				if (index >= scripts.size())
				{
					break;
				}

				NativeScriptComponent::Script& script = scripts[index];

				// Unknown script (not registered): kept so the scene saves it back, but it can't run.
				if (script.Instantiate == nullptr || script.PendingRemove)
				{
					continue;
				}

				if (script.Instance == nullptr)
				{
					script.Instance.reset(script.Instantiate());
					script.Instance->m_Entity = entity;

					// The instance lives on the heap, so this pointer stays valid when the script list
					// moves; nothing is removed before the stage ends.
					ScriptableEntity* instance = script.Instance.get();
					instance->OnCreate();

					if (scene.IsPendingDestroy(entity))
					{
						break;
					}

					instance->OnUpdate(context.DeltaTime);
				}
				else
				{
					script.Instance->OnUpdate(context.DeltaTime);
				}
			}
		}

		scene.EndStage();
	}
}
