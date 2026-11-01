#include "hypch.h"
#include "MeshBounds.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Renderer/Mesh.h"
#include "Hydro/Renderer/Renderer3D.h"
#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Scene/Entity.h"

#include <limits>

namespace Hydro
{
	bool MeshBounds::GetLocal(const MeshComponent& mesh, glm::vec3& low, glm::vec3& high)
	{
		const Ref<Mesh> model = (uint64_t)mesh.Model != 0 ? AssetManager::GetLoadedAsset<Mesh>(mesh.Model) : Renderer3D::GetShapeMesh(mesh);
		if (model == nullptr)
		{
			return false;
		}
		low = model->GetBoundsMin();
		high = model->GetBoundsMax();
		return true;
	}

	bool MeshBounds::GetWorld(Entity entity, glm::vec3& low, glm::vec3& high)
	{
		const MeshComponent* mesh = entity.TryGetComponent<MeshComponent>();
		const WorldTransformComponent* transform = entity.TryGetComponent<WorldTransformComponent>();
		if (mesh == nullptr || transform == nullptr)
		{
			return false;
		}
		glm::vec3 localLow{ 0.0f };
		glm::vec3 localHigh{ 0.0f };
		if (GetLocal(*mesh, localLow, localHigh) == false)
		{
			return false;
		}
		low = glm::vec3(std::numeric_limits<float>::max());
		high = glm::vec3(-std::numeric_limits<float>::max());
		for (int i = 0; i < 8; i++)
		{
			const glm::vec3 local{ (i & 1) ? localHigh.x : localLow.x, (i & 2) ? localHigh.y : localLow.y, (i & 4) ? localHigh.z : localLow.z };
			const glm::vec3 corner = glm::vec3(transform->Matrix * glm::vec4(local, 1.0f));
			low = glm::min(low, corner);
			high = glm::max(high, corner);
		}
		return true;
	}
}
