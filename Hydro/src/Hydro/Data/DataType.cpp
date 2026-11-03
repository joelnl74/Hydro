#include "hypch.h"
#include "DataType.h"
#include "DataTypeRegistry.h"

#include <algorithm>

namespace Hydro
{
	const char* DataFieldKindToString(DataFieldKind kind)
	{
		switch (kind)
		{
			case DataFieldKind::Bool: return "Bool";
			case DataFieldKind::Int: return "Int";
			case DataFieldKind::Float: return "Float";
			case DataFieldKind::Vec2: return "Vec2";
			case DataFieldKind::Vec3: return "Vec3";
			case DataFieldKind::Vec4: return "Vec4";
			case DataFieldKind::String: return "String";
			case DataFieldKind::Asset: return "Asset";
			case DataFieldKind::Enum: return "Enum";
			case DataFieldKind::Struct: return "Struct";
			case DataFieldKind::List: return "List";
			case DataFieldKind::RowRef: return "RowRef";
			case DataFieldKind::EntityRef: return "EntityRef";
		}
		return "Unknown";
	}

	void ForEachEntityRef(const DataValueType& type, void* value, const std::function<void(EntityRef&)>& visit)
	{
		switch (type.Kind)
		{
			case DataFieldKind::EntityRef:
				visit(*static_cast<EntityRef*>(value));
				break;
			case DataFieldKind::Struct:
			{
				const DataTypeInfo* nested = DataTypeRegistry::Find(type.CppType);
				if (nested != nullptr)
				{
					for (const DataField& field : nested->Fields)
					{
						ForEachEntityRef(field.Type, field.Get(value), visit);
					}
				}
				break;
			}
			case DataFieldKind::List:
			{
				const DataListOps& list = *type.List;
				const size_t size = list.Size(value);
				for (size_t i = 0; i < size; i++)
				{
					ForEachEntityRef(list.Element, list.At(value, i), visit);
				}
				break;
			}
			default:
				break;
		}
	}

	const DataField* DataTypeInfo::FindField(const std::string& name) const
	{
		for (const DataField& field : Fields)
		{
			if (field.Name == name)
			{
				return &field;
			}
		}
		return nullptr;
	}

	const DataField* DataTypeInfo::FindFieldByFormerName(const std::string& name) const
	{
		for (const DataField& field : Fields)
		{
			if (std::find(field.FormerNames.begin(), field.FormerNames.end(), name) != field.FormerNames.end())
			{
				return &field;
			}
		}
		return nullptr;
	}

	const char* DataEnumInfo::FindName(int64_t value) const
	{
		for (const DataEnumValue& entry : Values)
		{
			if (entry.Value == value)
			{
				return entry.Name.c_str();
			}
		}
		return nullptr;
	}

	bool DataEnumInfo::FindValue(const std::string& name, int64_t& value) const
	{
		for (const DataEnumValue& entry : Values)
		{
			if (entry.Name == name)
			{
				value = entry.Value;
				return true;
			}
		}
		return false;
	}
}
