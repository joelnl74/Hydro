#include "hypch.h"
#include "Vfx.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Scene/Prefab.h"
#include "Hydro/Scene/PrefabSerializer.h"
#include "Hydro/Scene/Scene.h"

#include <unordered_set>

namespace Hydro
{
	Entity Vfx::Play(Scene& scene, AssetHandle effect, const glm::vec3& position)
	{
		const Ref<Prefab> prefab = AssetManager::GetLoadedAsset<Prefab>(effect);
		if (prefab == nullptr)
		{
			// Play is called from gameplay code, often: say it once per effect.
			static std::unordered_set<uint64_t> s_Reported;
			if (s_Reported.insert((uint64_t)effect).second)
			{
				HY_CORE_WARN("Vfx::Play: effect {} isn't loaded (load it once with AssetManager::GetAsset<Prefab>, e.g. in OnCreate)", (uint64_t)effect);
			}
			return {};
		}

		Entity entity = PrefabSerializer::Instantiate(scene, *prefab);
		if (entity.IsValid() == false)
		{
			return {};
		}

		if (entity.HasComponent<TransformComponent>() == false)
		{
			entity.AddComponent<TransformComponent>();
		}
		entity.GetComponent<TransformComponent>().Translation = position;
		if (entity.HasComponent<ParticleEmitter2DComponent>())
		{
			entity.GetComponent<ParticleEmitter2DComponent>().DestroyWhenFinished = true;
		}
		return entity;
	}
}
