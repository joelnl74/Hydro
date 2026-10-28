#pragma once

#include "ScriptProperty.h"
#include "ScriptTypeInfo.h"

#include <string>
#include <type_traits>
#include <typeindex>
#include <vector>

namespace Hydro
{
	class ScriptableEntity;

	template<typename T>
	class ScriptRegistration;

	class ScriptRegistry
	{
	public:
		template<typename T>
		static ScriptRegistration<T> Register(
			const std::string& name);

		static ScriptFactory Find(
			const std::string& name);

		static ScriptTypeInfo* FindType(
			const std::string& name);

		static const std::string& GetName(
			std::type_index type);

		static std::vector<std::string> GetNames();

		static void InitializeProperties(
			const std::string& scriptName,
			ScriptPropertyMap& properties);

	private:
		static ScriptTypeInfo& RegisterType(
			ScriptTypeInfo&& typeInfo);
	};
}

// IMPORTANT:
// The complete ScriptRegistration<T> definition must
// become visible to users of ScriptRegistry.h.
#include "ScriptRegistration.h"

namespace Hydro
{
	template<typename T>
	ScriptRegistration<T>
	ScriptRegistry::Register(
		const std::string& name)
	{
		static_assert(
			std::is_base_of_v<
				ScriptableEntity,
				T>,
			"Scripts must derive from ScriptableEntity");

		ScriptTypeInfo typeInfo;

		typeInfo.Name = name;
		typeInfo.Type = typeid(T);

		typeInfo.Factory =
			[]() -> ScriptableEntity*
		{
			return new T();
		};

		ScriptTypeInfo& registered =
			RegisterType(
				std::move(typeInfo));

		return ScriptRegistration<T>(
			registered);
	}
}
