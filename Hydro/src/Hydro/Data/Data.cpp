#include "hypch.h"
#include "Data.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Data/DataSerializer.h"
#include "Hydro/Data/DataTableAsset.h"
#include "Hydro/Data/DataTypeRegistry.h"
#include "Hydro/Data/DefinitionAsset.h"

#include <fstream>
#include <iterator>
#include <unordered_set>

namespace Hydro
{
	// ---- Lookups -----------------------------------------------------------------------------------------

	// Each problem a lookup finds is logged once per run, not every time a script asks.
	static void WarnOnce(const std::string& key, const std::string& text)
	{
		static std::unordered_set<std::string> s_Warned;
		if (s_Warned.insert(key).second)
		{
			HY_CORE_WARN("Data: {}", text);
		}
	}

	static std::string HandleText(AssetHandle handle)
	{
		return std::to_string((uint64_t)handle);
	}

	static std::string TypeNameOf(std::type_index cppType)
	{
		const DataTypeInfo* type = DataTypeRegistry::Find(cppType);
		return type != nullptr ? type->Name : std::string("an unregistered type (is it marked HY_DATA?)");
	}

	// The registered asset behind `handle` if it is of `expected` type; warned once (keyed by `what`) otherwise.
	static const AssetMetadata* FindDataAsset(AssetHandle handle, AssetType expected, const char* what)
	{
		const AssetMetadata* metadata = AssetManager::GetMetadata(handle);
		if (metadata == nullptr)
		{
			WarnOnce(std::string("unknown:") + HandleText(handle),
				std::string(what) + ": asset " + HandleText(handle) + " is not in the project (deleted, or from another project?)");
			return nullptr;
		}
		const std::string path = metadata->Path.generic_u8string();
		if (metadata->IsMissing)
		{
			WarnOnce("missing:" + HandleText(handle), std::string(what) + ": the file of '" + path + "' is missing");
			return nullptr;
		}
		if (metadata->Type != expected)
		{
			WarnOnce(std::string("kind:") + what + ":" + HandleText(handle),
				std::string(what) + ": '" + path + "' is a " + AssetTypeToString(metadata->Type) + ", not a "
					+ AssetTypeToString(expected));
			return nullptr;
		}
		return metadata;
	}

	Ref<DefinitionAsset> Data::GetDefinitionAsset(AssetHandle handle)
	{
		if ((uint64_t)handle == 0)
		{
			return nullptr;
		}
		const AssetMetadata* metadata = AssetManager::GetMetadata(handle);
		if (metadata == nullptr || metadata->IsMissing || metadata->Type != AssetType::Definition)
		{
			return nullptr;
		}
		return AssetManager::GetAsset<DefinitionAsset>(handle);
	}

	Ref<DataTableAsset> Data::GetTableAsset(AssetHandle handle)
	{
		if ((uint64_t)handle == 0)
		{
			return nullptr;
		}
		const AssetMetadata* metadata = AssetManager::GetMetadata(handle);
		if (metadata == nullptr || metadata->IsMissing || metadata->Type != AssetType::DataTable)
		{
			return nullptr;
		}
		return AssetManager::GetAsset<DataTableAsset>(handle);
	}

	const DataTypeInfo* Data::GetDataType(AssetHandle handle)
	{
		const AssetMetadata* metadata = AssetManager::GetMetadata(handle);
		if (metadata == nullptr)
		{
			return nullptr;
		}
		if (metadata->Type == AssetType::Definition)
		{
			const Ref<DefinitionAsset> definition = GetDefinitionAsset(handle);
			return definition != nullptr ? definition->GetDataType() : nullptr;
		}
		if (metadata->Type == AssetType::DataTable)
		{
			const Ref<DataTableAsset> table = GetTableAsset(handle);
			return table != nullptr ? table->GetRowType() : nullptr;
		}
		return nullptr;
	}

	const void* Data::FindDefinition(AssetHandle handle, std::type_index cppType, Ref<Asset>& owner)
	{
		if ((uint64_t)handle == 0)
		{
			return nullptr; // "none" is a valid choice, not a problem
		}
		const AssetMetadata* metadata = FindDataAsset(handle, AssetType::Definition, "Data::Get");
		if (metadata == nullptr)
		{
			return nullptr;
		}
		const Ref<DefinitionAsset> definition = AssetManager::GetAsset<DefinitionAsset>(handle); // logs a failed load itself
		if (definition == nullptr)
		{
			return nullptr;
		}
		if (definition->GetDataType()->CppType != cppType)
		{
			WarnOnce("type:" + HandleText(handle) + ":" + TypeNameOf(cppType),
				"Data::Get: '" + metadata->Path.generic_u8string() + "' holds a " + definition->GetDataType()->Name + ", not a "
					+ TypeNameOf(cppType));
			return nullptr;
		}
		owner = definition;
		return definition->GetData().Values.Get();
	}

	const std::map<std::string, DataRecord>* Data::FindTable(AssetHandle handle, std::type_index cppType, Ref<Asset>& owner)
	{
		if ((uint64_t)handle == 0)
		{
			return nullptr;
		}
		const AssetMetadata* metadata = FindDataAsset(handle, AssetType::DataTable, "Data::GetTable");
		if (metadata == nullptr)
		{
			return nullptr;
		}
		const Ref<DataTableAsset> table = AssetManager::GetAsset<DataTableAsset>(handle);
		if (table == nullptr)
		{
			return nullptr;
		}
		if (table->GetRowType()->CppType != cppType)
		{
			WarnOnce("type:" + HandleText(handle) + ":" + TypeNameOf(cppType),
				"Data::GetTable: '" + metadata->Path.generic_u8string() + "' has rows of " + table->GetRowType()->Name + ", not "
					+ TypeNameOf(cppType));
			return nullptr;
		}
		owner = table;
		return &table->GetData().Rows;
	}

	const void* Data::FindRow(const DataRowRef& row, std::type_index cppType, Ref<Asset>& owner)
	{
		if (row.IsSet() == false)
		{
			return nullptr;
		}
		Ref<Asset> table;
		const std::map<std::string, DataRecord>* rows = FindTable(row.Table, cppType, table);
		if (rows == nullptr)
		{
			return nullptr;
		}
		auto it = rows->find(row.Row);
		if (it == rows->end())
		{
			const AssetMetadata* metadata = AssetManager::GetMetadata(row.Table);
			WarnOnce("row:" + HandleText(row.Table) + ":" + row.Row,
				"Data::GetRow: '" + (metadata != nullptr ? metadata->Path.generic_u8string() : HandleText(row.Table)) + "' has no row '"
					+ row.Row + "'");
			return nullptr;
		}
		owner = std::move(table);
		return it->second.Get();
	}

	// ---- Validation --------------------------------------------------------------------------------------

	static std::string JoinPath(const std::string& parent, const std::string& child)
	{
		return parent.empty() ? child : parent + "." + child;
	}

	static void CheckReferences(const DataTypeInfo& type, const void* object, const std::string& path, const std::string& row,
		DataReport& report);

	// An asset reference: registered, its file there, of the field's asset type and, for data, of its data type.
	static void CheckAssetReference(const DataField& field, AssetHandle handle, const std::string& path, const std::string& row,
		DataReport& report)
	{
		if ((uint64_t)handle == 0)
		{
			return;
		}
		const AssetMetadata* metadata = AssetManager::GetMetadata(handle);
		if (metadata == nullptr)
		{
			report.AddError(row, path, "asset " + HandleText(handle) + " is not in the project");
			return;
		}
		const std::string file = metadata->Path.generic_u8string();
		if (metadata->IsMissing)
		{
			report.AddError(row, path, "Missing: '" + file + "'");
			return;
		}
		if (field.AssetFilter != AssetType::None && metadata->Type != field.AssetFilter)
		{
			report.AddError(row, path, "'" + file + "' is a " + AssetTypeToString(metadata->Type) + ", not a "
				+ AssetTypeToString(field.AssetFilter));
			return;
		}
		if (field.DataTypeFilter.empty() == false && (metadata->Type == AssetType::Definition || metadata->Type == AssetType::DataTable))
		{
			const DataTypeInfo* dataType = Data::GetDataType(handle);
			if (dataType == nullptr)
			{
				report.AddError(row, path, "'" + file + "' can't be read");
			}
			else if (dataType->Name != field.DataTypeFilter)
			{
				report.AddError(row, path, "'" + file + "' holds " + dataType->Name + ", not " + field.DataTypeFilter);
			}
		}
	}

	// A row reference: both halves set, the table there and of the field's row type, the row in it.
	static void CheckRowReference(const DataField& field, const DataRowRef& ref, const std::string& path, const std::string& row,
		DataReport& report)
	{
		const bool hasTable = (uint64_t)ref.Table != 0;
		const bool hasRow = ref.Row.empty() == false;
		if (hasTable == false && hasRow == false)
		{
			return;
		}
		if (hasTable == false || hasRow == false)
		{
			report.AddError(row, path, hasTable ? "a table without a row" : "row '" + ref.Row + "' without a table");
			return;
		}
		const AssetMetadata* metadata = AssetManager::GetMetadata(ref.Table);
		if (metadata == nullptr || metadata->IsMissing)
		{
			report.AddError(row, path, "Missing: " + ref.Row + " (its table is not in the project)");
			return;
		}
		const std::string file = metadata->Path.generic_u8string();
		if (metadata->Type != AssetType::DataTable)
		{
			report.AddError(row, path, "'" + file + "' is a " + AssetTypeToString(metadata->Type) + ", not a data table");
			return;
		}
		const Ref<DataTableAsset> table = Data::GetTableAsset(ref.Table);
		if (table == nullptr)
		{
			report.AddError(row, path, "'" + file + "' can't be read");
			return;
		}
		if (field.DataTypeFilter.empty() == false && table->GetRowType()->Name != field.DataTypeFilter)
		{
			report.AddError(row, path, "'" + file + "' has rows of " + table->GetRowType()->Name + ", not " + field.DataTypeFilter);
			return;
		}
		if (table->FindRow(ref.Row) == nullptr)
		{
			report.AddError(row, path, "Missing: " + ref.Row + " ('" + file + "' has no such row)");
		}
	}

	static void CheckReferenceValue(const DataField& field, const DataValueType& valueType, const void* value, const std::string& path,
		const std::string& row, DataReport& report)
	{
		switch (valueType.Kind)
		{
			case DataFieldKind::Asset:
				CheckAssetReference(field, *static_cast<const AssetHandle*>(value), path, row, report);
				break;
			case DataFieldKind::RowRef:
				CheckRowReference(field, *static_cast<const DataRowRef*>(value), path, row, report);
				break;
			case DataFieldKind::Struct:
			{
				const DataTypeInfo* nestedType = DataTypeRegistry::Find(valueType.CppType);
				if (nestedType != nullptr)
				{
					CheckReferences(*nestedType, value, path, row, report);
				}
				break;
			}
			case DataFieldKind::List:
			{
				const DataListOps& list = *valueType.List;
				const size_t size = list.Size(value);
				for (size_t i = 0; i < size; i++)
				{
					CheckReferenceValue(field, list.Element, list.AtConst(value, i), path + "[" + std::to_string(i) + "]", row, report);
				}
				break;
			}
			default:
				break;
		}
	}

	static void CheckReferences(const DataTypeInfo& type, const void* object, const std::string& path, const std::string& row,
		DataReport& report)
	{
		for (const DataField& field : type.Fields)
		{
			CheckReferenceValue(field, field.Type, field.Get(object), JoinPath(path, field.Name), row, report);
		}
	}

	// Why a data file doesn't load: read it again for the report (the loader has only logged it).
	static void ReportUnreadable(AssetHandle handle, AssetType type, DataReport& report)
	{
		std::ifstream in(AssetManager::GetAbsolutePath(handle), std::ios::binary);
		const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		const size_t before = report.GetMessages().size();
		if (type == AssetType::Definition)
		{
			DefinitionData scratch;
			DataSerializer::ReadDefinition(text, scratch, report);
		}
		else
		{
			DataTableData scratch;
			DataSerializer::ReadTable(text, scratch, report);
		}
		if (report.GetMessages().size() == before)
		{
			report.AddError("", "", "the file can't be read");
		}
	}

	DataReport Data::Validate(AssetHandle handle)
	{
		DataReport report;
		const AssetMetadata* metadata = AssetManager::GetMetadata(handle);
		if (metadata == nullptr)
		{
			report.AddError("", "", "asset " + HandleText(handle) + " is not in the project");
			return report;
		}
		if (metadata->IsMissing)
		{
			report.AddError("", "", "the file of '" + metadata->Path.generic_u8string() + "' is missing");
			return report;
		}

		if (metadata->Type == AssetType::Definition)
		{
			const Ref<DefinitionAsset> definition = GetDefinitionAsset(handle);
			if (definition == nullptr)
			{
				ReportUnreadable(handle, metadata->Type, report);
				return report;
			}
			const DataRecord& values = definition->GetData().Values;
			report.Append(definition->GetFileReport());
			DataSerializer::CheckRecord(values, "", false, report);
			CheckReferences(*values.GetType(), values.Get(), "", "", report);
			return report;
		}

		if (metadata->Type == AssetType::DataTable)
		{
			const Ref<DataTableAsset> table = GetTableAsset(handle);
			if (table == nullptr)
			{
				ReportUnreadable(handle, metadata->Type, report);
				return report;
			}
			report.Append(table->GetFileReport());
			for (const auto& [id, record] : table->GetData().Rows)
			{
				DataSerializer::CheckRecord(record, id, true, report);
				CheckReferences(*record.GetType(), record.Get(), "", id, report);
			}
			return report;
		}

		report.AddError("", "", "'" + metadata->Path.generic_u8string() + "' is a " + AssetTypeToString(metadata->Type)
			+ ", not a definition or data table");
		return report;
	}
}
