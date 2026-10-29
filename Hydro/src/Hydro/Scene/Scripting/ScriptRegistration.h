#pragma once

#include "ScriptTypeInfo.h"

#include "Hydro/Core/Assert.h"

#include <string>
#include <type_traits>

namespace Hydro
{
	class ScriptableEntity;

	template<typename>
	inline constexpr bool ScriptAlwaysFalse = false;

	// The ScriptPropertyType of a C++ field type, checked at compile time.
	template<typename Field>
	constexpr ScriptPropertyType GetScriptPropertyType()
	{
		if constexpr (std::is_same_v<Field, bool>)
		{
			return ScriptPropertyType::Bool;
		}
		else if constexpr (std::is_same_v<Field, int>)
		{
			return ScriptPropertyType::Int;
		}
		else if constexpr (std::is_same_v<Field, float>)
		{
			return ScriptPropertyType::Float;
		}
		else if constexpr (std::is_same_v<Field, glm::vec2>)
		{
			return ScriptPropertyType::Vec2;
		}
		else if constexpr (std::is_same_v<Field, glm::vec3>)
		{
			return ScriptPropertyType::Vec3;
		}
		else if constexpr (std::is_same_v<Field, glm::vec4>)
		{
			return ScriptPropertyType::Vec4;
		}
		else if constexpr (std::is_same_v<Field, std::string>)
		{
			return ScriptPropertyType::String;
		}
		else if constexpr (std::is_same_v<Field, AssetHandle>)
		{
			return ScriptPropertyType::Asset;
		}
		else
		{
			static_assert(
				ScriptAlwaysFalse<Field>,
				"Unsupported script property type: bool, int, float, glm::vec2/3/4, std::string or AssetHandle");
			return ScriptPropertyType::Float;
		}
	}

	// What ScriptRegistry::Register<T> returns. Each .Property(...) adds a field; the modifiers after it
	// (.Range, .Tooltip, .Color, .AssetFilter, .ReadOnly, .Hidden) change the property added last:
	//
	//     ScriptRegistry::Register<DoorScript>("DoorScript")
	//         .Property("OpenSpeed", "Open Speed", &DoorScript::OpenSpeed).Range(0.0f, 10.0f).Tooltip("Metres per second")
	//         .Property("Glow", "Glow", &DoorScript::Glow).Color()
	//         .Property("Sign", "Sign", &DoorScript::Sign).AssetFilter(AssetType::Texture2D);
	template<typename T>
	class ScriptRegistration
	{
	public:
		explicit ScriptRegistration(
			ScriptTypeInfo& typeInfo)
			: m_TypeInfo(typeInfo)
		{
		}

		// Manual registration: the label is the name.
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

		// Generated reflection (and manual registration with a label).
		template<typename Field>
		ScriptRegistration& Property(
			const std::string& name,
			const std::string& displayName,
			Field T::* member)
		{
			ScriptProperty property;

			property.Name = name;
			property.DisplayName = displayName;
			property.Type = GetScriptPropertyType<Field>();
			property.DefaultValue = m_DefaultInstance.*member;

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

			property.Read =
				[member](
					const ScriptableEntity& base) -> ScriptPropertyValue
			{
				const T& script =
					static_cast<const T&>(base);

				return ScriptPropertyValue(script.*member);
			};

			m_TypeInfo.Properties.push_back(
				std::move(property));

			return *this;
		}

		// Int, Float and vector components are clamped to [min, max] when the field is left.
		ScriptRegistration& Range(float min, float max)
		{
			ScriptProperty& property = Last();
			property.Min = min;
			property.Max = max;
			return *this;
		}

		ScriptRegistration& Min(float min)
		{
			Last().Min = min;
			return *this;
		}

		ScriptRegistration& Max(float max)
		{
			Last().Max = max;
			return *this;
		}

		ScriptRegistration& Tooltip(const std::string& text)
		{
			Last().Tooltip = text;
			return *this;
		}

		// A Vec3 / Vec4 edited with a colour picker.
		ScriptRegistration& Color()
		{
			ScriptProperty& property = Last();
			HY_CORE_ASSERT(property.Type == ScriptPropertyType::Vec3 || property.Type == ScriptPropertyType::Vec4,
				"Color() is for glm::vec3 / glm::vec4 properties");
			property.IsColor = true;
			return *this;
		}

		// Which assets an AssetHandle property's picker lists.
		ScriptRegistration& AssetFilter(AssetType type)
		{
			ScriptProperty& property = Last();
			HY_CORE_ASSERT(property.Type == ScriptPropertyType::Asset, "AssetFilter() is for AssetHandle properties");
			property.AssetFilter = type;
			return *this;
		}

		ScriptRegistration& ReadOnly()
		{
			Last().ReadOnly = true;
			return *this;
		}

		ScriptRegistration& Hidden()
		{
			Last().HideInInspector = true;
			return *this;
		}

	private:
		ScriptProperty& Last()
		{
			HY_CORE_ASSERT(m_TypeInfo.Properties.empty() == false, "Call Property(...) before its modifiers");
			return m_TypeInfo.Properties.back();
		}

		ScriptTypeInfo& m_TypeInfo;

		// Where the defaults come from: the field initialisers of a default-constructed script.
		T m_DefaultInstance{};
	};
}
