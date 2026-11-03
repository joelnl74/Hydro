#include "hypch.h"
#include "DataRecord.h"

#include <typeindex>

namespace Hydro
{
	DataKeptValues::DataKeptValues(const DataKeptValues& other)
		: Unknown(YAML::Clone(other.Unknown)), Unreadable(YAML::Clone(other.Unreadable)), Nested(other.Nested)
	{
	}

	DataKeptValues& DataKeptValues::operator=(const DataKeptValues& other)
	{
		if (this != &other)
		{
			Unknown = YAML::Clone(other.Unknown);
			Unreadable = YAML::Clone(other.Unreadable);
			Nested = other.Nested;
		}
		return *this;
	}

	bool DataKeptValues::IsEmpty() const
	{
		if (Unknown.size() > 0 || Unreadable.size() > 0)
		{
			return false;
		}
		for (const auto& [name, nested] : Nested)
		{
			if (nested.IsEmpty() == false)
			{
				return false;
			}
		}
		return true;
	}

	void DataKeptValues::ForgetUnreadable(const std::string& fieldName)
	{
		if (Unreadable.IsMap() && Unreadable[fieldName])
		{
			Unreadable.remove(fieldName);
		}
	}

	void DataKeptValues::Forget(const std::string& fieldName)
	{
		ForgetUnreadable(fieldName);

		// "Reward" and every "Lines[n]" under the field.
		for (auto it = Nested.begin(); it != Nested.end();)
		{
			const std::string& key = it->first;
			const bool isField = key == fieldName;
			const bool isEntry = key.size() > fieldName.size() && key.compare(0, fieldName.size(), fieldName) == 0
				&& key[fieldName.size()] == '[';
			if (isField || isEntry)
			{
				it = Nested.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	DataRecord::DataRecord(const DataTypeInfo& type)
		: m_Type(&type), m_Instance(type.Construct())
	{
	}

	DataRecord::~DataRecord()
	{
		Release();
	}

	DataRecord::DataRecord(const DataRecord& other)
		: m_Type(other.m_Type), m_Kept(other.m_Kept)
	{
		if (other.m_Instance != nullptr)
		{
			m_Instance = m_Type->Construct();
			m_Type->Copy(m_Instance, other.m_Instance);
		}
	}

	DataRecord& DataRecord::operator=(const DataRecord& other)
	{
		if (this == &other)
		{
			return *this;
		}

		if (m_Instance != nullptr && other.m_Instance != nullptr && m_Type == other.m_Type)
		{
			m_Type->Copy(m_Instance, other.m_Instance);
		}
		else
		{
			Release();
			m_Type = other.m_Type;
			if (other.m_Instance != nullptr)
			{
				m_Instance = m_Type->Construct();
				m_Type->Copy(m_Instance, other.m_Instance);
			}
		}
		m_Kept = other.m_Kept;
		return *this;
	}

	DataRecord::DataRecord(DataRecord&& other) noexcept
		: m_Type(other.m_Type), m_Instance(other.m_Instance), m_Kept(std::move(other.m_Kept))
	{
		other.m_Type = nullptr;
		other.m_Instance = nullptr;
	}

	DataRecord& DataRecord::operator=(DataRecord&& other) noexcept
	{
		if (this != &other)
		{
			Release();
			m_Type = other.m_Type;
			m_Instance = other.m_Instance;
			m_Kept = std::move(other.m_Kept);
			other.m_Type = nullptr;
			other.m_Instance = nullptr;
		}
		return *this;
	}

	bool DataRecord::IsOfType(const std::type_info& cppType) const
	{
		return m_Type != nullptr && m_Instance != nullptr && m_Type->CppType == std::type_index(cppType);
	}

	void DataRecord::Release()
	{
		if (m_Instance != nullptr)
		{
			m_Type->Destroy(m_Instance);
			m_Instance = nullptr;
		}
	}
}
