// Hydro/Data/DataRecord.h
#pragma once

#include "Hydro/Data/DataType.h"

#include <yaml-cpp/yaml.h>

#include <map>
#include <string>
#include <typeinfo>

namespace Hydro
{
	// What a record's file had that its type couldn't take, kept so that loading and saving loses nothing
	// (except comments and formatting). Written back by the DataSerializer; reported by validation.
	struct DataKeptValues
	{
		// Keys the type has no field for (a field renamed or removed in C++), as a map. Written after the fields.
		YAML::Node Unknown;
		// Values that couldn't be read (wrong kind, unknown enum name), by field name. The field holds its
		// default meanwhile, and the file keeps the old value until the field is edited (Forget).
		YAML::Node Unreadable;
		// The same for nested structs ("Reward") and list entries ("Lines[2]").
		std::map<std::string, DataKeptValues> Nested;

		DataKeptValues() = default;
		// Deep copies: YAML::Node copies would share their data.
		DataKeptValues(const DataKeptValues& other);
		DataKeptValues& operator=(const DataKeptValues& other);
		DataKeptValues(DataKeptValues&& other) = default;
		DataKeptValues& operator=(DataKeptValues&& other) = default;

		bool IsEmpty() const;
		// The field was set by hand (the inspector): its unreadable value and everything kept under it go.
		void Forget(const std::string& fieldName);
		// Only the field's unreadable value goes (a list was changed: its entries keep what they kept).
		void ForgetUnreadable(const std::string& fieldName);
	};

	// One value of a data type that owns its memory: a definition's values, or one table row. The type is fixed
	// at creation. Copies are deep (through the type's Copy function, never bytes).
	class DataRecord
	{
	public:
		DataRecord() = default;
		// A record of `type` holding the type's defaults (its field initialisers).
		explicit DataRecord(const DataTypeInfo& type);
		~DataRecord();

		DataRecord(const DataRecord& other);
		DataRecord& operator=(const DataRecord& other);
		DataRecord(DataRecord&& other) noexcept;
		DataRecord& operator=(DataRecord&& other) noexcept;

		bool IsValid() const { return m_Instance != nullptr; }
		const DataTypeInfo* GetType() const { return m_Type; }

		void* Get() { return m_Instance; }
		const void* Get() const { return m_Instance; }

		// The record as T; nullptr when it isn't one.
		template<typename T>
		T* As()
		{
			return IsOfType(typeid(T)) ? static_cast<T*>(m_Instance) : nullptr;
		}

		template<typename T>
		const T* As() const
		{
			return IsOfType(typeid(T)) ? static_cast<const T*>(m_Instance) : nullptr;
		}

		DataKeptValues& GetKept() { return m_Kept; }
		const DataKeptValues& GetKept() const { return m_Kept; }

	private:
		bool IsOfType(const std::type_info& cppType) const;
		void Release();

		const DataTypeInfo* m_Type = nullptr;
		void* m_Instance = nullptr;
		DataKeptValues m_Kept;
	};
}
