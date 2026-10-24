#include "hypch.h"
#include "ScriptSystem.h"

#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/ScriptableEntity.h"
#include "Hydro/Scene/SystemContext.h"

namespace Hydro
{
	void UpdateScripts(Scene& scene, const SystemContext& context)
	{
		if (context.IsPlaying == false)
		{
			return;
		}

		auto view = scene.m_Registry.view<NativeScriptComponent>();
		for (auto entityHandle : view)
		{
			auto& scriptComponent = view.get<NativeScriptComponent>(entityHandle);
			for (auto& script : scriptComponent.Scripts)
			{
				// Unknown script (not registered): kept so the scene saves it back, but it can't run.
				if (script.Instantiate == nullptr)
				{
					continue;
				}

				if (script.Instance == nullptr)
				{
					script.Instance.reset(script.Instantiate());
					script.Instance->m_Entity = Entity{ entityHandle, &scene };
					script.Instance->OnCreate();
				}

				script.Instance->OnUpdate(context.DeltaTime);
			}
		}
	}
}
