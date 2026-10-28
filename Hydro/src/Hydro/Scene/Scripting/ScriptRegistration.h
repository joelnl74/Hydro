#pragma once

#include "ScriptTypeInfo.h"

namespace Hydro
{
	class ScriptableEntity;

	template<typename>
	inline constexpr bool ScriptAlwaysFalse = false;

	template<typename T>
	class ScriptRegistration
	{
	public:
		explicit ScriptRegistration(
			ScriptTypeInfo& typeInfo)
			: m_TypeInfo(typeInfo)
		{
		}

		// Existing/manual registration:
		//
		// .Property(
		//     "Move Speed",
		//     &DioramaWalkScript::MoveSpeed)
		template<typename Field>
		ScriptRegistration& Property(
			const std::string& name,
			Field T::* member)
		{
			return Property(
				name,
				name,
				member);
		}

		// Generated reflection:
		//
		// .Property(
		//     "MoveSpeed",
		//     "Move Speed",
		//     &DioramaWalkScript::MoveSpeed)
		template<typename Field>
		ScriptRegistration& Property(
			const std::string& name,
			const std::string& displayName,
			Field T::* member)
		{
			ScriptProperty property;

			property.Name = name;
			property.DisplayName = displayName;

			if constexpr (
				std::is_same_v<Field, bool>)
			{
				property.Type =
					ScriptPropertyType::Bool;
			}
			else if constexpr (
				std::is_same_v<Field, int>)
			{
				property.Type =
					ScriptPropertyType::Int;
			}
			else if constexpr (
				std::is_same_v<Field, float>)
			{
				property.Type =
					ScriptPropertyType::Float;
			}
			else
			{
				static_assert(
					ScriptAlwaysFalse<Field>,
					"Unsupported script property type");
			}

			property.DefaultValue =
				m_DefaultInstance.*member;

			property.Apply =
				[member](
					ScriptableEntity& base,
					const ScriptPropertyValue& value)
			{
				T& script =
					static_cast<T&>(base);

				const Field* typedValue =
					std::get_if<Field>(&value);

				if (typedValue != nullptr)
				{
					script.*member =
						*typedValue;
				}
			};

			m_TypeInfo.Properties.push_back(
				std::move(property));

			return *this;
		}

	private:
		ScriptTypeInfo& m_TypeInfo;

		T m_DefaultInstance{};
	};
}
