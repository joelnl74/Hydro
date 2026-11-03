#include "hypch.h"
#include "DataTypeRegistry.h"

#include <map>
#include <unordered_map>

namespace Hydro
{
	struct DataTypeRegistryData
	{
		// std::map: entries never move, so the pointers Find hands out stay valid.
		std::map<std::string, DataTypeInfo> Types;
		std::map<std::string, DataEnumInfo> Enums;
		std::unordered_map<std::type_index, std::string> TypeNames;
		std::unordered_map<std::type_index, std::string> EnumNames;
		// A former type name -> the type's current name.
		std::unordered_map<std::string, std::string> FormerNames;
	};

	// Created on first use, so registering from anywhere (even another static) is safe.
	static DataTypeRegistryData& GetData()
	{
		static DataTypeRegistryData data;
		return data;
	}

	DataTypeInfo& DataTypeRegistry::RegisterType(DataTypeInfo&& typeInfo)
	{
		HY_CORE_ASSERT(typeInfo.Name.empty() == false, "A data type needs a name");

		DataTypeRegistryData& data = GetData();
		const std::string name = typeInfo.Name;
		HY_CORE_ASSERT(data.Enums.find(name) == data.Enums.end(), "A data type and an enum can't share a name");

		if (data.Types.find(name) != data.Types.end())
		{
			HY_CORE_WARN("DataTypeRegistry: '{}' was already registered; the new type replaces it", name);
		}

		for (const std::string& formerName : typeInfo.FormerNames)
		{
			if (formerName == name || data.Types.find(formerName) != data.Types.end())
			{
				HY_CORE_WARN("DataTypeRegistry: '{}' can't have the former name '{}': a data type is registered under it", name,
					formerName);
				continue;
			}
			auto [it, added] = data.FormerNames.emplace(formerName, name);
			if (added == false && it->second != name)
			{
				HY_CORE_WARN("DataTypeRegistry: '{}' and '{}' both have the former name '{}'; files saved under it load as '{}'",
					it->second, name, formerName, it->second);
			}
		}

		data.TypeNames[typeInfo.CppType] = name;
		data.Types[name] = std::move(typeInfo);
		return data.Types[name];
	}

	DataEnumInfo& DataTypeRegistry::RegisterEnumType(DataEnumInfo&& enumInfo)
	{
		HY_CORE_ASSERT(enumInfo.Name.empty() == false, "An enum needs a name");

		DataTypeRegistryData& data = GetData();
		const std::string name = enumInfo.Name;
		HY_CORE_ASSERT(data.Types.find(name) == data.Types.end(), "A data type and an enum can't share a name");

		if (data.Enums.find(name) != data.Enums.end())
		{
			HY_CORE_WARN("DataTypeRegistry: enum '{}' was already registered; the new one replaces it", name);
		}

		data.EnumNames[enumInfo.CppType] = name;
		data.Enums[name] = std::move(enumInfo);
		return data.Enums[name];
	}

	const DataTypeInfo* DataTypeRegistry::Find(const std::string& name)
	{
		DataTypeRegistryData& data = GetData();
		auto it = data.Types.find(name);
		if (it == data.Types.end())
		{
			return nullptr;
		}
		return &it->second;
	}

	const DataTypeInfo* DataTypeRegistry::FindByFormerName(const std::string& name)
	{
		DataTypeRegistryData& data = GetData();
		auto it = data.FormerNames.find(name);
		if (it == data.FormerNames.end())
		{
			return nullptr;
		}
		return Find(it->second);
	}

	const DataTypeInfo* DataTypeRegistry::Find(std::type_index cppType)
	{
		DataTypeRegistryData& data = GetData();
		auto it = data.TypeNames.find(cppType);
		if (it == data.TypeNames.end())
		{
			return nullptr;
		}
		return Find(it->second);
	}

	const DataEnumInfo* DataTypeRegistry::FindEnum(const std::string& name)
	{
		DataTypeRegistryData& data = GetData();
		auto it = data.Enums.find(name);
		if (it == data.Enums.end())
		{
			return nullptr;
		}
		return &it->second;
	}

	const DataEnumInfo* DataTypeRegistry::FindEnum(std::type_index cppType)
	{
		DataTypeRegistryData& data = GetData();
		auto it = data.EnumNames.find(cppType);
		if (it == data.EnumNames.end())
		{
			return nullptr;
		}
		return FindEnum(it->second);
	}

	std::vector<const DataTypeInfo*> DataTypeRegistry::GetTypes()
	{
		std::vector<const DataTypeInfo*> types;
		for (const auto& [name, typeInfo] : GetData().Types)
		{
			types.push_back(&typeInfo);
		}
		return types;
	}
}
