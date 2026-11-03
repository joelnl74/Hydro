// Hydro/Data/DataTypeRegistry.h
#pragma once

#include "Hydro/Data/DataType.h"
#include "Hydro/Data/DataTypeRegistration.h"

#include <string>
#include <type_traits>
#include <typeindex>
#include <vector>

namespace Hydro
{
	// The data types (HY_DATA) and enums (HY_ENUM) by name: a lookup table filled by the generated
	// RegisterGeneratedDataReflection() before any data is read. The types themselves are the game's.
	class DataTypeRegistry
	{
	public:
		// `name` is saved in files: rename it only with a former name (`formerNames`).
		template<typename T>
		static DataTypeRegistration<T> Register(const std::string& name, const std::string& displayName,
			const std::vector<std::string>& formerNames = {});

		template<typename E>
		static DataEnumRegistration<E> RegisterEnum(const std::string& name);

		// nullptr when nothing is registered under that name or C++ type.
		static const DataTypeInfo* Find(const std::string& name);
		static const DataTypeInfo* Find(std::type_index cppType);
		// The type that had `name` as a former name (a file saved before a rename); nullptr when none.
		static const DataTypeInfo* FindByFormerName(const std::string& name);
		static const DataEnumInfo* FindEnum(const std::string& name);
		static const DataEnumInfo* FindEnum(std::type_index cppType);

		// Every registered type, sorted by name.
		static std::vector<const DataTypeInfo*> GetTypes();

	private:
		static DataTypeInfo& RegisterType(DataTypeInfo&& typeInfo);
		static DataEnumInfo& RegisterEnumType(DataEnumInfo&& enumInfo);
	};

	template<typename T>
	DataTypeRegistration<T> DataTypeRegistry::Register(const std::string& name, const std::string& displayName,
		const std::vector<std::string>& formerNames)
	{
		static_assert(std::is_class_v<T>, "Data types are structs");
		static_assert(std::is_default_constructible_v<T>, "Data types must be default-constructible (field initialisers are the defaults)");
		static_assert(std::is_copy_assignable_v<T> && std::is_move_assignable_v<T>, "Data types must be copyable and movable");

		DataTypeInfo typeInfo;
		typeInfo.Name = name;
		typeInfo.DisplayName = displayName;
		typeInfo.FormerNames = formerNames;
		typeInfo.CppType = typeid(T);
		typeInfo.Construct = []() -> void*
		{
			return new T();
		};
		typeInfo.Destroy = [](void* object)
		{
			delete static_cast<T*>(object);
		};
		typeInfo.Copy = [](void* destination, const void* source)
		{
			*static_cast<T*>(destination) = *static_cast<const T*>(source);
		};
		typeInfo.Move = [](void* destination, void* source)
		{
			*static_cast<T*>(destination) = std::move(*static_cast<T*>(source));
		};

		return DataTypeRegistration<T>(RegisterType(std::move(typeInfo)));
	}

	template<typename E>
	DataEnumRegistration<E> DataTypeRegistry::RegisterEnum(const std::string& name)
	{
		static_assert(std::is_enum_v<E>, "HY_ENUM marks an enum class");

		DataEnumInfo enumInfo;
		enumInfo.Name = name;
		enumInfo.CppType = typeid(E);
		return DataEnumRegistration<E>(RegisterEnumType(std::move(enumInfo)));
	}
}
