#include "hypch.h"
#include "RenderProfile.h"
#include "RenderProfileSerializer.h"

namespace Hydro
{
	Ref<RenderProfile> RenderProfile::LoadUncached(const std::filesystem::path& path)
	{
		RenderProfileData data;
		if (RenderProfileSerializer::Deserialize(path, data) == false)
		{
			return nullptr;
		}

		return CreateRef<RenderProfile>(data);
	}
}
