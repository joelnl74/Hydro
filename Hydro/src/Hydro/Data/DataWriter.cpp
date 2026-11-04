#include "hypch.h"
#include "DataSerializer.h"
#include "DataText.h"

#include "Hydro/Asset/AssetMetaSerializer.h"
#include "Hydro/Data/DataTypeRegistry.h"
#include "Hydro/Utils/YAMLConversions.h"

#include <vector>

namespace Hydro
{
	static void WriteStruct(YAML::Emitter& out, const DataTypeInfo& type, const void* object, const DataKeptValues* kept);

	static const DataKeptValues* FindNested(const DataKeptValues* kept, const std::string& key)
	{
		if (kept == nullptr)
		{
			return nullptr;
		}
		auto it = kept->Nested.find(key);
		return it == kept->Nested.end() ? nullptr : &it->second;
	}

	static void WriteFloats(YAML::Emitter& out, const float* components, int count)
	{
		out << YAML::Flow << YAML::BeginSeq;
		for (int i = 0; i < count; i++)
		{
			out << DataText::FormatFloat(components[i]);
		}
		out << YAML::EndSeq;
	}

	static void WriteValue(YAML::Emitter& out, const DataValueType& valueType, const void* value, const DataKeptValues* kept,
		const std::string& keptKey)
	{
		switch (valueType.Kind)
		{
			case DataFieldKind::Bool:
				out << *static_cast<const bool*>(value);
				break;
			case DataFieldKind::Int:
				out << *static_cast<const int*>(value);
				break;
			case DataFieldKind::Float:
				out << DataText::FormatFloat(*static_cast<const float*>(value));
				break;
			case DataFieldKind::Vec2:
				WriteFloats(out, static_cast<const float*>(value), 2);
				break;
			case DataFieldKind::Vec3:
				WriteFloats(out, static_cast<const float*>(value), 3);
				break;
			case DataFieldKind::Vec4:
				WriteFloats(out, static_cast<const float*>(value), 4);
				break;
			case DataFieldKind::String:
				out << *static_cast<const std::string*>(value);
				break;
			case DataFieldKind::Asset:
				out << (uint64_t)*static_cast<const AssetHandle*>(value);
				break;
			case DataFieldKind::Enum:
			{
				const int64_t number = valueType.ReadEnum(value);
				const DataEnumInfo* enumInfo = DataTypeRegistry::FindEnum(valueType.CppType);
				const char* name = enumInfo != nullptr ? enumInfo->FindName(number) : nullptr;
				if (name != nullptr)
				{
					out << name;
				}
				else
				{
					out << number; // not an enumerator: reported as unreadable when read back
				}
				break;
			}
			case DataFieldKind::Struct:
			{
				const DataTypeInfo* nestedType = DataTypeRegistry::Find(valueType.CppType);
				if (nestedType == nullptr)
				{
					out << YAML::Null;
					break;
				}
				WriteStruct(out, *nestedType, value, FindNested(kept, keptKey));
				break;
			}
			case DataFieldKind::List:
			{
				const DataListOps& list = *valueType.List;
				const size_t size = list.Size(value);
				if (size == 0)
				{
					out << YAML::Flow << YAML::BeginSeq << YAML::EndSeq;
					break;
				}
				// Lists of numbers on one line ([10, 20]); text, enums, structs and references one entry per line.
				const DataFieldKind element = list.Element.Kind;
				const bool oneLine = element == DataFieldKind::Bool || element == DataFieldKind::Int || element == DataFieldKind::Float
					|| element == DataFieldKind::Asset
					|| element == DataFieldKind::EntityRef;
				if (oneLine)
				{
					out << YAML::Flow;
				}
				out << YAML::BeginSeq;
				for (size_t i = 0; i < size; i++)
				{
					WriteValue(out, list.Element, list.AtConst(value, i), kept, DataText::IndexKey(keptKey, i));
				}
				out << YAML::EndSeq;
				break;
			}
			case DataFieldKind::RowRef:
			{
				const DataRowRef& ref = *static_cast<const DataRowRef*>(value);
				out << YAML::Flow << YAML::BeginMap;
				out << YAML::Key << "Table" << YAML::Value << (uint64_t)ref.Table;
				out << YAML::Key << "Row" << YAML::Value << ref.Row;
				out << YAML::EndMap;
				break;
			}
			case DataFieldKind::EntityRef:
				out << (uint64_t)static_cast<const EntityRef*>(value)->GetId();
				break;
		}
	}

	static void WriteStruct(YAML::Emitter& out, const DataTypeInfo& type, const void* object, const DataKeptValues* kept)
	{
		if (type.Fields.empty() && (kept == nullptr || kept->Unknown.size() == 0))
		{
			out << YAML::Flow << YAML::BeginMap << YAML::EndMap;
			return;
		}

		out << YAML::BeginMap;
		for (const DataField& field : type.Fields)
		{
			out << YAML::Key << field.Name << YAML::Value;
			if (kept != nullptr && kept->Unreadable.IsMap() && kept->Unreadable[field.Name])
			{
				out << kept->Unreadable[field.Name];
			}
			else
			{
				WriteValue(out, field.Type, field.Get(object), kept, field.Name);
			}
		}

		// Unknown keys after the fields, sorted, so they don't move around between saves.
		if (kept != nullptr && kept->Unknown.IsMap())
		{
			std::vector<std::string> keys;
			for (const auto& entry : kept->Unknown)
			{
				keys.push_back(entry.first.Scalar());
			}
			std::sort(keys.begin(), keys.end());
			for (const std::string& key : keys)
			{
				out << YAML::Key << key << YAML::Value << kept->Unknown[key];
			}
		}
		out << YAML::EndMap;
	}

	static void BeginFile(YAML::Emitter& out, AssetHandle handle, const char* sectionKey, uint32_t version, const std::string& typeName)
	{
		out << YAML::BeginMap;
		if ((uint64_t)handle != 0)
		{
			out << YAML::Key << AssetMetaSerializer::HandleKey << YAML::Value << (uint64_t)handle;
		}
		out << YAML::Key << sectionKey << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << version;
		out << YAML::Key << "Type" << YAML::Value << typeName;
	}

	static bool EndFile(YAML::Emitter& out, std::string& text)
	{
		out << YAML::EndMap; // the section
		out << YAML::EndMap; // the file
		if (out.good() == false)
		{
			HY_CORE_ERROR("DataSerializer: can't write the data: {}", out.GetLastError());
			return false;
		}
		text = out.c_str();
		text += "\n";
		return true;
	}

	bool DataSerializer::WriteDefinition(const DefinitionData& data, std::string& text)
	{
		if (data.ReadOnly || data.Values.IsValid() == false)
		{
			return false;
		}

		YAML::Emitter out;
		BeginFile(out, data.Handle, DefinitionKey, DataFormatVersion, data.Values.GetType()->Name);
		out << YAML::Key << "Values" << YAML::Value;
		WriteStruct(out, *data.Values.GetType(), data.Values.Get(), &data.Values.GetKept());
		return EndFile(out, text);
	}

	bool DataSerializer::WriteTable(const DataTableData& data, std::string& text)
	{
		if (data.ReadOnly || data.RowType == nullptr)
		{
			return false;
		}

		YAML::Emitter out;
		BeginFile(out, data.Handle, TableKey, DataFormatVersion, data.RowType->Name);
		out << YAML::Key << "Rows" << YAML::Value;
		if (data.Rows.empty())
		{
			out << YAML::Flow << YAML::BeginMap << YAML::EndMap;
		}
		else
		{
			out << YAML::BeginMap;
			for (const auto& [id, record] : data.Rows) // std::map: sorted by ID
			{
				out << YAML::Key << id << YAML::Value;
				WriteStruct(out, *record.GetType(), record.Get(), &record.GetKept());
			}
			out << YAML::EndMap;
		}
		return EndFile(out, text);
	}

	void DataSerializer::WriteValue(YAML::Emitter& out, const DataValueType& type, const void* value)
	{
		::Hydro::WriteValue(out, type, value, nullptr, "");
	}
}
