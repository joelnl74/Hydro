#include "hypch.h"
#include "ScriptRegistry.h"

#include <map>

namespace Hydro
{
	struct ScriptRegistryData
	{
		std::map<std::string, ScriptTypeInfo> Types;
		std::unordered_map<std::type_index, std::string> Names;
		// A former name -> the script's current name.
		std::unordered_map<std::string, std::string> FormerNames;
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

		for (const std::string& formerName :
			typeInfo.FormerNames)
		{
			if (formerName == name ||
				data.Types.find(formerName) != data.Types.end())
			{
				HY_CORE_WARN(
					"ScriptRegistry: '{}' can't have the "
					"former name '{}': a script is "
					"registered under it",
					name,
					formerName);
				continue;
			}

			auto [it, added] =
				data.FormerNames.emplace(
					formerName,
					name);

			if (added == false &&
				it->second != name)
			{
				HY_CORE_WARN(
					"ScriptRegistry: '{}' and '{}' both "
					"have the former name '{}'; scenes "
					"saved under it bind '{}'",
					it->second,
					name,
					formerName,
					it->second);
			}
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
		ScriptTypeInfo* typeInfo =
			FindType(name);

		return typeInfo != nullptr
			? typeInfo->Factory
			: nullptr;
	}

	ScriptTypeInfo*
	ScriptRegistry::FindType(
		const std::string& name)
	{
		ScriptRegistryData& data =
			GetData();

		auto it =
			data.Types.find(name);

		if (it != data.Types.end())
		{
			return &it->second;
		}

		auto former =
			data.FormerNames.find(name);

		if (former == data.FormerNames.end())
		{
			return nullptr;
		}

		it = data.Types.find(former->second);

		return it != data.Types.end()
			? &it->second
			: nullptr;
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
