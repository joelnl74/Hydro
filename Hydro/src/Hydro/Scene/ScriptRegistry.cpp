#include "hypch.h"
#include "ScriptRegistry.h"

#include <map>

namespace Hydro
{
	struct ScriptRegistryData
	{
		std::map<std::string, ScriptFactory> Factories;   // sorted, so GetNames is too
		std::unordered_map<std::type_index, std::string> Names;
	};

	// Created on first use, so registering from anywhere (even another static) is safe.
	static ScriptRegistryData& GetData()
	{
		static ScriptRegistryData data;
		return data;
	}

	void ScriptRegistry::Register(const std::string& name, std::type_index type, ScriptFactory factory)
	{
		HY_CORE_ASSERT(name.empty() == false, "A script needs a name");
		HY_CORE_ASSERT(factory != nullptr, "A script needs a factory");

		ScriptRegistryData& data = GetData();
		if (data.Factories.find(name) != data.Factories.end())
			HY_CORE_WARN("ScriptRegistry: '{}' was already registered; the new script replaces it", name);

		data.Factories[name] = factory;
		data.Names[type] = name;
	}

	ScriptFactory ScriptRegistry::Find(const std::string& name)
	{
		ScriptRegistryData& data = GetData();
		auto it = data.Factories.find(name);
		return it != data.Factories.end() ? it->second : nullptr;
	}

	const std::string& ScriptRegistry::GetName(std::type_index type)
	{
		static const std::string empty;

		ScriptRegistryData& data = GetData();
		auto it = data.Names.find(type);
		return it != data.Names.end() ? it->second : empty;
	}

	std::vector<std::string> ScriptRegistry::GetNames()
	{
		std::vector<std::string> names;
		for (const auto& [name, factory] : GetData().Factories)
			names.push_back(name);
		return names;
	}
}
