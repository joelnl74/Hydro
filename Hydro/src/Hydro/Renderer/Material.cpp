#include "hypch.h"
#include "Material.h"
#include "MaterialSerializer.h"

namespace Hydro
{
	Ref<MaterialAsset> MaterialAsset::LoadUncached(const std::filesystem::path& path)
	{
		MaterialData data;
		if (MaterialSerializer::Deserialize(path, data) == false)
		{
			return nullptr;
		}

		return CreateRef<MaterialAsset>(data);
	}
}
