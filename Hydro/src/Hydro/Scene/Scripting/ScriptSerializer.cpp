#include "hypch.h"
#include "ScriptSerializer.h"

#include <algorithm>
#include <set>

namespace Hydro
{
	// A kept value as text, so NativeScriptComponent needs no YAML types.
	static std::string ToText(const YAML::Node& node)
	{
		YAML::Emitter out;
		out << node;
		return out.c_str();
	}

	void ScriptSerializer::WriteValue(YAML::Emitter& out, const ScriptPropertyValue& value)
	{
		std::visit(
			[&out](const auto& typed)
			{
				using Value = std::decay_t<decltype(typed)>;
				if constexpr (std::is_same_v<Value, AssetHandle>)
				{
					out << (uint64_t)typed;
				}
				else if constexpr (std::is_same_v<Value, DataRowRef>)
				{
					out << YAML::Flow << YAML::BeginMap;
					out << YAML::Key << "Table" << YAML::Value << (uint64_t)typed.Table;
					out << YAML::Key << "Row" << YAML::Value << typed.Row;
					out << YAML::EndMap;
				}
				else
				{
					out << typed;
				}
			},
			value);
	}

	ScriptPropertyValue ScriptSerializer::ReadValue(const YAML::Node& node, const ScriptProperty& property)
	{
		const ScriptPropertyValue& fallback = property.DefaultValue;
		try
		{
			switch (property.Type)
			{
				case ScriptPropertyType::Bool: return node.as<bool>(std::get<bool>(fallback));
				case ScriptPropertyType::Int: return node.as<int>(std::get<int>(fallback));
				case ScriptPropertyType::Float: return node.as<float>(std::get<float>(fallback));
				case ScriptPropertyType::Vec2: return node.as<glm::vec2>(std::get<glm::vec2>(fallback));
				case ScriptPropertyType::Vec3: return node.as<glm::vec3>(std::get<glm::vec3>(fallback));
				case ScriptPropertyType::Vec4: return node.as<glm::vec4>(std::get<glm::vec4>(fallback));
				case ScriptPropertyType::String: return node.as<std::string>(std::get<std::string>(fallback));
				case ScriptPropertyType::Asset: return AssetHandle(node.as<uint64_t>((uint64_t)std::get<AssetHandle>(fallback)));
				case ScriptPropertyType::RowRef:
				{
					if (node.IsMap() == false)
					{
						return fallback;
					}
					DataRowRef ref;
					ref.Table = AssetHandle(node["Table"].as<uint64_t>(0));
					ref.Row = node["Row"].as<std::string>(std::string());
					return ref;
				}
			}
		}
		catch (const YAML::Exception&)
		{
			// A vector saved with the wrong number of components, or a scalar where a list was expected.
		}
		return fallback;
	}

	void ScriptSerializer::Write(YAML::Emitter& out, const NativeScriptComponent& scripts, const std::string& owner)
	{
		out << YAML::BeginSeq;

		for (const NativeScriptComponent::Script& script : scripts.Scripts)
		{
			if (script.Name.empty())
			{
				HY_CORE_WARN("Entity '{}': a script bound in code without a ScriptRegistry name isn't saved", owner);
				continue;
			}

			out << YAML::BeginMap;
			out << YAML::Key << "Name" << YAML::Value << script.Name;

			if (script.Properties.empty() == false || script.KeptProperties.empty() == false)
			{
				out << YAML::Key << "Properties" << YAML::Value << YAML::BeginMap;

				// Sorted by key, the kept values among the rest, so a scene saves the same way every time (the
				// property map is unordered). A property's own value wins over a kept one under the same key.
				std::vector<std::string> keys;
				keys.reserve(script.Properties.size() + script.KeptProperties.size());
				for (const auto& entry : script.Properties)
				{
					keys.push_back(entry.first);
				}
				for (const auto& entry : script.KeptProperties)
				{
					if (script.Properties.find(entry.first) == script.Properties.end())
					{
						keys.push_back(entry.first);
					}
				}
				std::sort(keys.begin(), keys.end());

				for (const std::string& key : keys)
				{
					auto property = script.Properties.find(key);
					if (property != script.Properties.end())
					{
						out << YAML::Key << key << YAML::Value;
						WriteValue(out, property->second);
						continue;
					}

					try
					{
						const YAML::Node kept = YAML::Load(script.KeptProperties.at(key));
						out << YAML::Key << key << YAML::Value << kept;
					}
					catch (const YAML::Exception&)
					{
						HY_CORE_WARN("Entity '{}': {}'s kept value '{}' couldn't be written back", owner, script.Name, key);
					}
				}

				out << YAML::EndMap;
			}

			out << YAML::EndMap;
		}

		out << YAML::EndSeq;
	}

	void ScriptSerializer::Read(const YAML::Node& scriptsNode, NativeScriptComponent& scripts, const std::string& owner)
	{
		// A missing key gives a node that throws on anything but IsDefined().
		if (scriptsNode.IsDefined() == false || scriptsNode.IsSequence() == false)
		{
			return;
		}

		for (const YAML::Node& savedScript : scriptsNode)
		{
			std::string savedName;

			// Backwards compatibility with the original string-only format.
			if (savedScript.IsScalar())
			{
				savedName = savedScript.as<std::string>("");
			}
			else if (savedScript.IsMap())
			{
				const YAML::Node nameNode = savedScript["Name"];
				savedName = nameNode.IsDefined() ? nameNode.as<std::string>("") : std::string();
			}

			if (savedName.empty())
			{
				continue;
			}

			// Bind resolves a former name, sets the current one and fills the reflected defaults.
			scripts.Bind(savedName);
			NativeScriptComponent::Script& binding = scripts.Scripts.back();
			const ScriptTypeInfo* typeInfo = ScriptRegistry::FindType(binding.Name);

			if (binding.Name != savedName)
			{
				HY_CORE_INFO("Entity '{}': script '{}' is saved under its former name '{}'; saving writes '{}'",
					owner, binding.Name, savedName, binding.Name);
			}

			// Saved values override the defaults; missing ones keep the current C++ defaults.
			const YAML::Node propertiesNode = savedScript.IsMap() ? savedScript["Properties"] : YAML::Node();
			if (propertiesNode.IsDefined() && propertiesNode.IsMap())
			{
				std::set<std::string> read;

				if (typeInfo != nullptr)
				{
					for (const ScriptProperty& property : typeInfo->Properties)
					{
						// Its current key first; a former key only when the scene has no value under the current one.
						std::string key;
						if (propertiesNode[property.Name])
						{
							key = property.Name;
						}
						for (size_t i = 0; key.empty() && i < property.FormerNames.size(); i++)
						{
							if (propertiesNode[property.FormerNames[i]])
							{
								key = property.FormerNames[i];
							}
						}
						if (key.empty())
						{
							continue;
						}

						binding.Properties[property.Name] = ReadValue(propertiesNode[key], property);
						read.insert(key);
						if (key != property.Name)
						{
							HY_CORE_INFO("Entity '{}': {}.{} is saved under its former name '{}'; saving writes '{}'",
								owner, binding.Name, property.Name, key, property.Name);
						}
					}
				}

				for (const auto& entry : propertiesNode)
				{
					const std::string key = entry.first.as<std::string>("");
					if (key.empty() || read.count(key) > 0)
					{
						continue;
					}

					binding.KeptProperties[key] = ToText(entry.second);
					if (typeInfo == nullptr)
					{
						continue;
					}
					const ScriptProperty* renamed = nullptr;
					for (const ScriptProperty& property : typeInfo->Properties)
					{
						if (std::find(property.FormerNames.begin(), property.FormerNames.end(), key) != property.FormerNames.end())
						{
							renamed = &property;
						}
					}
					if (renamed != nullptr)
					{
						HY_CORE_WARN("Entity '{}': {} has values under both '{}' and its former name '{}'; '{}' is used, '{}' is kept unused",
							owner, binding.Name, renamed->Name, key, renamed->Name, key);
					}
					else
					{
						HY_CORE_WARN("Entity '{}': {} has no property '{}' now; its saved value is kept in the file",
							owner, binding.Name, key);
					}
				}
			}

			if (binding.Instantiate == nullptr)
			{
				HY_CORE_WARN("Entity '{}': script '{}' isn't registered; it is kept (with its saved values) but won't run",
					owner, savedName);
			}
		}
	}
}
