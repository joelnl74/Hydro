#include "hypch.h"
#include "ScriptRegistry.h"

#include <map>

namespace Hydro
{
	struct ScriptRegistryData
	{
		std::map<std::string, ScriptTypeInfo> Types;
		std::unordered_map<std::type_index, std::string> Names;
	};

	// Created on first use, so registering from anywhere (even another static) is safe.
	static ScriptRegistryData& GetData()
	{
		static ScriptRegistryData data;
		return data;
	}

	ScriptTypeInfo&
	ScriptRegistry::RegisterType(
		ScriptTypeInfo&& typeInfo)
	{
		HY_CORE_ASSERT(
			typeInfo.Name.empty() == false,
			"A script needs a name");

		HY_CORE_ASSERT(
			typeInfo.Factory != nullptr,
			"A script needs a factory");

		ScriptRegistryData& data =
			GetData();

		const std::string name =
			typeInfo.Name;

		const std::type_index cppType =
			typeInfo.Type;

		if (data.Types.find(name) !=
			data.Types.end())
		{
			HY_CORE_WARN(
				"ScriptRegistry: '{}' was "
				"already registered; "
				"the new script replaces it",
				name);
		}

		data.Types[name] =
			std::move(typeInfo);

		data.Names[cppType] =
			name;

		return data.Types[name];
	}

	ScriptFactory
	ScriptRegistry::Find(
		const std::string& name)
	{
		ScriptRegistryData& data =
			GetData();

		auto it =
			data.Types.find(name);

		if (it == data.Types.end())
		{
			return nullptr;
		}

		return it->second.Factory;
	}

	ScriptTypeInfo*
	ScriptRegistry::FindType(
		const std::string& name)
	{
		ScriptRegistryData& data =
			GetData();

		auto it =
			data.Types.find(name);

		if (it == data.Types.end())
		{
			return nullptr;
		}

		return &it->second;
	}

	const std::string&
	ScriptRegistry::GetName(
		std::type_index type)
	{
		static const std::string empty;

		ScriptRegistryData& data =
			GetData();

		auto it =
			data.Names.find(type);

		return it != data.Names.end()
			? it->second
			: empty;
	}

	std::vector<std::string>
	ScriptRegistry::GetNames()
	{
		std::vector<std::string> names;

		for (const auto& [name, type] :
			GetData().Types)
		{
			names.push_back(name);
		}

		return names;
	}

	void ScriptRegistry::InitializeProperties(
		const std::string& scriptName,
		ScriptPropertyMap& properties)
	{
		ScriptTypeInfo* typeInfo =
			FindType(scriptName);

		if (typeInfo == nullptr)
		{
			return;
		}

		for (const ScriptProperty& property :
			typeInfo->Properties)
		{
			properties[property.Name] =
				property.DefaultValue;
		}
	}
}
