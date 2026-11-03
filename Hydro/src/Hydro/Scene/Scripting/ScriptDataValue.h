#pragma once

#include "Hydro/Data/DataType.h"

#include <typeindex>
#include <utility>

namespace Hydro
{
	// How to make, copy and free a value of one C++ type without knowing it: what a ScriptDataValue holds.
	struct ScriptDataOps
	{
		DataValueType Type;
		void* (*Construct)() = nullptr;                                // new T()
		void (*Destroy)(void* value) = nullptr;                        // delete (T*)value
		void (*Copy)(void* destination, const void* source) = nullptr; // destination = source
	};

	template<typename T>
	const ScriptDataOps& GetScriptDataOps()
	{
		static const ScriptDataOps ops = []()
		{
			ScriptDataOps result;
			result.Type = MakeDataValueType<T>();
			result.Construct = []() -> void*
			{
				return new T();
			};
			result.Destroy = [](void* value)
			{
				delete static_cast<T*>(value);
			};
			result.Copy = [](void* destination, const void* source)
			{
				*static_cast<T*>(destination) = *static_cast<const T*>(source);
			};
			return result;
		}();
		return ops;
	}

	// A script property of a data type (an HY_ENUM, an HY_DATA struct or a std::vector, EntityRef included) as an owned
	// deep copy of the C++ value, saved and drawn like a definition's field. Empty before a default is put in.
	class ScriptDataValue
	{
	public:
		ScriptDataValue() = default;

		template<typename T>
		static ScriptDataValue Make(const T& value)
		{
			ScriptDataValue result;
			result.m_Ops = &GetScriptDataOps<T>();
			result.m_Value = result.m_Ops->Construct();
			result.m_Ops->Copy(result.m_Value, &value);
			return result;
		}

		ScriptDataValue(const ScriptDataValue& other)
		{
			CopyFrom(other);
		}

		ScriptDataValue(ScriptDataValue&& other) noexcept
			: m_Ops(other.m_Ops), m_Value(other.m_Value)
		{
			other.m_Ops = nullptr;
			other.m_Value = nullptr;
		}

		ScriptDataValue& operator=(const ScriptDataValue& other)
		{
			if (this != &other)
			{
				Reset();
				CopyFrom(other);
			}
			return *this;
		}

		ScriptDataValue& operator=(ScriptDataValue&& other) noexcept
		{
			if (this != &other)
			{
				Reset();
				std::swap(m_Ops, other.m_Ops);
				std::swap(m_Value, other.m_Value);
			}
			return *this;
		}

		~ScriptDataValue()
		{
			Reset();
		}

		bool IsSet() const { return m_Ops != nullptr; }
		// nullptr when empty.
		const DataValueType* GetType() const { return m_Ops != nullptr ? &m_Ops->Type : nullptr; }
		void* Get() { return m_Value; }
		const void* Get() const { return m_Value; }

		// The value as a T; nullptr when empty or of another type.
		template<typename T>
		const T* As() const
		{
			if (m_Ops == nullptr || m_Ops->Type.CppType != std::type_index(typeid(T)))
			{
				return nullptr;
			}
			return static_cast<const T*>(m_Value);
		}

		// Whether both hold the same C++ type (both empty counts).
		bool HasSameType(const ScriptDataValue& other) const
		{
			if (m_Ops == nullptr || other.m_Ops == nullptr)
			{
				return m_Ops == other.m_Ops;
			}
			return m_Ops->Type.CppType == other.m_Ops->Type.CppType;
		}

	private:
		void CopyFrom(const ScriptDataValue& other)
		{
			if (other.m_Ops == nullptr)
			{
				return;
			}
			m_Ops = other.m_Ops;
			m_Value = m_Ops->Construct();
			m_Ops->Copy(m_Value, other.m_Value);
		}

		void Reset()
		{
			if (m_Ops != nullptr && m_Value != nullptr)
			{
				m_Ops->Destroy(m_Value);
			}
			m_Ops = nullptr;
			m_Value = nullptr;
		}

		const ScriptDataOps* m_Ops = nullptr;
		void* m_Value = nullptr;
	};
}
