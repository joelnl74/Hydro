// Hydro/Data/DataTypeRegistration.h
#pragma once

#include "Hydro/Core/Assert.h"
#include "Hydro/Data/DataType.h"

#include <string>
#include <type_traits>

namespace Hydro
{
	template<typename Member>
	struct DataMemberTraits;

	template<typename Class, typename Field>
	struct DataMemberTraits<Field Class::*>
	{
		using ClassType = Class;
		using FieldType = Field;
	};

	// The field `Member` inside an instance of its struct, for DataField::Access.
	template<auto Member>
	void* DataAccessMember(void* object)
	{
		using Traits = DataMemberTraits<decltype(Member)>;
		return &(static_cast<typename Traits::ClassType*>(object)->*Member);
	}

	// What DataTypeRegistry::Register<T> returns. The generator writes these calls from HY_DATA / HY_PROPERTY;
	// each .Field<&T::Member>(...) adds a field, the modifiers after it change the field added last:
	//
	//     DataTypeRegistry::Register<ItemDefinition>("ItemDefinition", "Item")
	//         .Table()
	//         .DisplayField("Name")
	//         .Field<&ItemDefinition::Name>("Name", "Name")
	//         .Field<&ItemDefinition::RestoreHP>("RestoreHP", "Restore HP").Range(0.0f, 999.0f);
	template<typename T>
	class DataTypeRegistration
	{
	public:
		explicit DataTypeRegistration(DataTypeInfo& typeInfo)
			: m_TypeInfo(typeInfo)
		{
		}

		// Can be created as a definition (.hydef).
		DataTypeRegistration& Definition()
		{
			m_TypeInfo.CanBeDefinition = true;
			return *this;
		}

		// Can be created as a data table (.hytable).
		DataTypeRegistration& Table()
		{
			m_TypeInfo.CanBeTable = true;
			return *this;
		}

		// The string field row lists and pickers show.
		DataTypeRegistration& DisplayField(const std::string& fieldName)
		{
			m_TypeInfo.DisplayField = fieldName;
			return *this;
		}

		template<auto Member>
		DataTypeRegistration& Field(const std::string& name, const std::string& displayName)
		{
			using Traits = DataMemberTraits<decltype(Member)>;
			static_assert(std::is_same_v<typename Traits::ClassType, T>, "Field<&T::Member>: the member must belong to the registered type");

			DataField field;
			field.Name = name;
			field.DisplayName = displayName;
			field.Type = MakeDataValueType<typename Traits::FieldType>();
			field.Access = &DataAccessMember<Member>;
			m_TypeInfo.Fields.push_back(std::move(field));
			return *this;
		}

		// Int, Float and vector components (or a list's elements): validation warns outside [min, max].
		DataTypeRegistration& Range(float min, float max)
		{
			DataField& field = Last();
			field.Min = min;
			field.Max = max;
			return *this;
		}

		DataTypeRegistration& Min(float min)
		{
			Last().Min = min;
			return *this;
		}

		DataTypeRegistration& Max(float max)
		{
			Last().Max = max;
			return *this;
		}

		DataTypeRegistration& Tooltip(const std::string& text)
		{
			Last().Tooltip = text;
			return *this;
		}

		DataTypeRegistration& Color()
		{
			Last().IsColor = true;
			return *this;
		}

		// Which assets an AssetHandle field's picker lists.
		DataTypeRegistration& AssetFilter(AssetType type)
		{
			Last().AssetFilter = type;
			return *this;
		}

		// Which tables a DataRowRef field's picker lists: those whose rows are `rowTypeName`.
		DataTypeRegistration& DataRow(const std::string& rowTypeName)
		{
			Last().DataTypeFilter = rowTypeName;
			return *this;
		}

		DataTypeRegistration& ReadOnly()
		{
			Last().ReadOnly = true;
			return *this;
		}

		DataTypeRegistration& Hidden()
		{
			Last().HideInInspector = true;
			return *this;
		}

	private:
		DataField& Last()
		{
			HY_CORE_ASSERT(m_TypeInfo.Fields.empty() == false, "Call Field<...>(...) before its modifiers");
			return m_TypeInfo.Fields.back();
		}

		DataTypeInfo& m_TypeInfo;
	};

	// What DataTypeRegistry::RegisterEnum<E> returns: .Value("Name", E::Name) per enumerator, in declaration order.
	template<typename E>
	class DataEnumRegistration
	{
	public:
		explicit DataEnumRegistration(DataEnumInfo& enumInfo)
			: m_EnumInfo(enumInfo)
		{
		}

		DataEnumRegistration& Value(const std::string& name, E value)
		{
			DataEnumValue entry;
			entry.Name = name;
			entry.Value = (int64_t)value;
			m_EnumInfo.Values.push_back(std::move(entry));
			return *this;
		}

	private:
		DataEnumInfo& m_EnumInfo;
	};
}
