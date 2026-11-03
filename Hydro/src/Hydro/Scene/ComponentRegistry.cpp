#include "hypch.h"
#include "ComponentRegistry.h"

#include <algorithm>

namespace Hydro
{
	static std::vector<ComponentTypeInfo>& GetTypeList()
	{
		static std::vector<ComponentTypeInfo> types;
		return types;
	}

	const std::vector<ComponentTypeInfo>& ComponentRegistry::GetTypes()
	{
		return GetTypeList();
	}

	const ComponentTypeInfo* ComponentRegistry::Find(const std::string& name)
	{
		for (const ComponentTypeInfo& info : GetTypeList())
		{
			if (info.Name == name || std::find(info.FormerNames.begin(), info.FormerNames.end(), name) != info.FormerNames.end())
			{
				return &info;
			}
		}
		return nullptr;
	}

	const ComponentTypeInfo* ComponentRegistry::Find(std::type_index type)
	{
		for (const ComponentTypeInfo& info : GetTypeList())
		{
			if (info.CppType == type)
			{
				return &info;
			}
		}
		return nullptr;
	}

	void ComponentRegistry::Add(ComponentTypeInfo&& info)
	{
		std::vector<ComponentTypeInfo>& types = GetTypeList();
		for (ComponentTypeInfo& existing : types)
		{
			if (existing.CppType == info.CppType || existing.Name == info.Name)
			{
				HY_CORE_WARN("ComponentRegistry: '{}' is registered twice; the later registration replaces it", info.Name);
				existing = std::move(info);
				return;
			}
		}
		types.push_back(std::move(info));
	}
}
