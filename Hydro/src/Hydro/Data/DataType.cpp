#include "hypch.h"
#include "DataType.h"

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
		}
		return "Unknown";
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
