#include "hypch.h"
#include "DataReader.h"
#include "DataSerializer.h"
#include "DataText.h"

#include "Hydro/Asset/AssetMetaSerializer.h"
#include "Hydro/Data/DataTypeRegistry.h"
#include "Hydro/Utils/YAMLConversions.h"

#include <algorithm>
#include <set>
#include <vector>

namespace Hydro
{
	// ---- Values ----------------------------------------------------------------------------------------

	template<typename T>
	static bool ReadScalar(const YAML::Node& node, T& result)
	{
		if (node.IsScalar() == false)
		{
			return false;
		}
		try
		{
			result = node.as<T>();
			return true;
		}
		catch (const YAML::Exception&)
		{
			return false;
		}
	}

	template<typename T>
	static bool ReadVector(const YAML::Node& node, T& result)
	{
		if (node.IsSequence() == false || node.size() != (size_t)T::length())
		{
			return false;
		}
		try
		{
			result = node.as<T>();
			return true;
		}
		catch (const YAML::Exception&)
		{
			return false;
		}
	}

	bool DataReader::ReadValue(const YAML::Node& node, const DataValueType& valueType, void* value, DataKeptValues& kept,
		const std::string& keptKey, const std::string& path, DataReadContext& context)
	{
		auto fail = [&](const std::string& reason) -> bool
		{
			if (context.ReportValues)
			{
				context.Report->AddError(context.Row, path, reason
					+ (context.KeepsValues ? " (the file's value is kept until the field is set)" : ""));
			}
			return false;
		};
		auto expected = [&]() -> bool
		{
			return fail(std::string("expected ") + DataText::ExpectedText(valueType.Kind) + ", got " + DataText::Describe(node));
		};

		switch (valueType.Kind)
		{
			case DataFieldKind::Bool:
			{
				bool result = false;
				if (ReadScalar(node, result) == false)
				{
					return expected();
				}
				*static_cast<bool*>(value) = result;
				return true;
			}
			case DataFieldKind::Int:
			{
				int result = 0;
				if (ReadScalar(node, result) == false)
				{
					return expected();
				}
				*static_cast<int*>(value) = result;
				return true;
			}
			case DataFieldKind::Float:
			{
				float result = 0.0f;
				if (ReadScalar(node, result) == false)
				{
					return expected();
				}
				*static_cast<float*>(value) = result;
				return true;
			}
			case DataFieldKind::Vec2:
			{
				glm::vec2 result{ 0.0f };
				if (ReadVector(node, result) == false)
				{
					return expected();
				}
				*static_cast<glm::vec2*>(value) = result;
				return true;
			}
			case DataFieldKind::Vec3:
			{
				glm::vec3 result{ 0.0f };
				if (ReadVector(node, result) == false)
				{
					return expected();
				}
				*static_cast<glm::vec3*>(value) = result;
				return true;
			}
			case DataFieldKind::Vec4:
			{
				glm::vec4 result{ 0.0f };
				if (ReadVector(node, result) == false)
				{
					return expected();
				}
				*static_cast<glm::vec4*>(value) = result;
				return true;
			}
			case DataFieldKind::String:
			{
				std::string result;
				if (ReadScalar(node, result) == false)
				{
					return expected();
				}
				*static_cast<std::string*>(value) = std::move(result);
				return true;
			}
			case DataFieldKind::Asset:
			{
				uint64_t result = 0;
				if (ReadScalar(node, result) == false)
				{
					return expected();
				}
				*static_cast<AssetHandle*>(value) = AssetHandle(result);
				return true;
			}
			case DataFieldKind::Enum:
			{
				const DataEnumInfo* enumInfo = DataTypeRegistry::FindEnum(valueType.CppType);
				if (enumInfo == nullptr)
				{
					return fail("the field's enum is not registered (is it marked HY_ENUM()?)");
				}
				if (node.IsScalar() == false)
				{
					return expected();
				}
				int64_t number = 0;
				if (enumInfo->FindValue(node.Scalar(), number) == false)
				{
					std::string names;
					for (const DataEnumValue& entry : enumInfo->Values)
					{
						names += (names.empty() ? "" : ", ") + entry.Name;
					}
					return fail("unknown " + enumInfo->Name + " '" + node.Scalar() + "' (one of " + names + ")");
				}
				valueType.WriteEnum(value, number);
				return true;
			}
			case DataFieldKind::Struct:
			{
				const DataTypeInfo* nestedType = DataTypeRegistry::Find(valueType.CppType);
				if (nestedType == nullptr)
				{
					return fail("the field's struct is not registered (is it marked HY_DATA()?)");
				}
				if (node.IsMap() == false)
				{
					return expected();
				}
				DataKeptValues& nested = kept.Nested[keptKey];
				ReadStruct(node, *nestedType, value, nested, path, context);
				if (nested.IsEmpty())
				{
					kept.Nested.erase(keptKey);
				}
				return true;
			}
			case DataFieldKind::List:
			{
				if (node.IsSequence() == false)
				{
					return expected();
				}
				const DataListOps& list = *valueType.List;
				list.Resize(value, node.size());

				// An entry that can't be read makes the whole list unreadable (an entry can't be left out without
				// shifting the rest); struct entries keep their own unreadable fields instead.
				bool readable = true;
				for (size_t i = 0; i < node.size(); i++)
				{
					const YAML::Node entry = node[i];
					if (entry.IsNull())
					{
						continue;
					}
					const std::string entryKey = DataText::IndexKey(keptKey, i);
					const std::string entryPath = path + "[" + std::to_string(i) + "]";
					if (ReadValue(entry, list.Element, list.At(value, i), kept, entryKey, entryPath, context) == false)
					{
						readable = false;
					}
				}
				return readable;
			}
			case DataFieldKind::RowRef:
			{
				if (node.IsMap() == false)
				{
					return expected();
				}
				DataRowRef result;
				const YAML::Node table = node["Table"];
				const YAML::Node row = node["Row"];
				uint64_t handle = 0;
				if (table && table.IsNull() == false && ReadScalar(table, handle) == false)
				{
					return expected();
				}
				if (row && row.IsNull() == false && ReadScalar(row, result.Row) == false)
				{
					return expected();
				}
				result.Table = AssetHandle(handle);
				*static_cast<DataRowRef*>(value) = std::move(result);
				return true;
			}
			case DataFieldKind::EntityRef:
			{
				uint64_t id = 0;
				if (ReadScalar(node, id) == false)
				{
					return expected();
				}
				// The scene is set when the value is applied to a script (ForEachEntityRef).
				*static_cast<EntityRef*>(value) = EntityRef(UUID(id));
				return true;
			}
		}
		return expected();
	}

	void DataReader::ReadStruct(const YAML::Node& node, const DataTypeInfo& type, void* object, DataKeptValues& kept,
		const std::string& path, DataReadContext& context)
	{
		std::set<std::string> seen;
		std::set<const DataField*> read; // a field read under one key isn't read again under a former name
		for (const auto& entry : node)
		{
			if (entry.first.IsScalar() == false)
			{
				context.Report->AddError(context.Row, path, "a key that isn't a name");
				context.StructureBroken = true;
				continue;
			}

			const std::string key = entry.first.Scalar();
			const std::string fieldPath = DataText::JoinPath(path, key);
			if (seen.insert(key).second == false)
			{
				context.Report->AddError(context.Row, fieldPath, "the key appears twice (the second is ignored; fix it in a text editor)");
				context.StructureBroken = true;
				continue;
			}

			const DataField* field = type.FindField(key);
			if (field == nullptr)
			{
				// A former name: its value is the field's unless the record also has one under the field's name (or another
				// former name), which then wins; this one is kept as an unknown key.
				field = type.FindFieldByFormerName(key);
				if (field != nullptr && node[field->Name].IsDefined() == false && read.count(field) == 0)
				{
					context.Report->AddWarning(context.Row, fieldPath, "saved under " + field->Name
						+ "'s former name; saving the file writes '" + field->Name + "'");
				}
				else
				{
					kept.Unknown[key] = YAML::Clone(entry.second);
					if (context.ReportValues)
					{
						context.Report->AddWarning(context.Row, fieldPath, field != nullptr
							? "a former name of " + field->Name + ", which has its own value (this one is kept in the file, unused)"
							: "unknown field (kept in the file; " + type.Name + " has no field '" + key + "' now)");
					}
					continue;
				}
			}
			read.insert(field);

			if (entry.second.IsNull())
			{
				continue; // "Speaker:" with nothing after it: the default
			}

			// What can't be used is kept under the field's current name, where saving writes the field.
			const std::string valuePath = DataText::JoinPath(path, field->Name);
			if (ReadValue(entry.second, field->Type, field->Get(object), kept, field->Name, valuePath, context) == false)
			{
				// The field keeps its default; a list may have been half read, so put its default back.
				if (field->Type.Kind == DataFieldKind::List)
				{
					void* defaults = type.Construct();
					field->Type.List->Assign(field->Get(object), field->Get(static_cast<const void*>(defaults)));
					type.Destroy(defaults);
				}
				kept.Forget(field->Name);
				kept.Unreadable[field->Name] = YAML::Clone(entry.second);
			}
		}
	}

	// ---- The file around the records ------------------------------------------------------------------

	static bool LoadRoot(const std::string& text, YAML::Node& root, DataReport& report)
	{
		try
		{
			root = YAML::Load(text);
		}
		catch (const YAML::Exception& e)
		{
			report.AddError("", "", std::string("not valid YAML: ") + e.what());
			return false;
		}
		if (root.IsMap() == false)
		{
			report.AddError("", "", "the file is not a YAML map");
			return false;
		}
		return true;
	}

	static AssetHandle ReadHandle(const YAML::Node& root, DataReport& report)
	{
		const YAML::Node handle = root[AssetMetaSerializer::HandleKey];
		uint64_t value = 0;
		if (handle && ReadScalar(handle, value) == false)
		{
			report.AddError("", AssetMetaSerializer::HandleKey, "not a handle: " + DataText::Describe(handle));
		}
		return AssetHandle(value);
	}

	// Keys the format doesn't know, at the top of the file or in its section: reported, not kept.
	static void ReportUnknownKeys(const YAML::Node& map, const std::vector<std::string>& known, const std::string& where,
		DataReport& report)
	{
		for (const auto& entry : map)
		{
			const std::string key = entry.first.IsScalar() ? entry.first.Scalar() : std::string("?");
			if (std::find(known.begin(), known.end(), key) == known.end())
			{
				report.AddWarning("", DataText::JoinPath(where, key), "not part of the format (ignored, not written back)");
			}
		}
	}

	static void ReadVersion(const YAML::Node& section, uint32_t& version, bool& readOnly, DataReport& report)
	{
		const YAML::Node node = section["Version"];
		version = DataFormatVersion;
		if (node.IsDefined() == false || node.IsNull())
		{
			report.AddWarning("", "Version", "missing; read as version " + std::to_string(DataFormatVersion));
			return;
		}

		uint32_t value = 0;
		if (ReadScalar(node, value) == false || value == 0)
		{
			report.AddError("", "Version", "not a version: " + DataText::Describe(node) + " (read-only)");
			readOnly = true;
			return;
		}
		version = value;
		if (version > DataFormatVersion)
		{
			report.AddError("", "Version", "version " + std::to_string(version) + " was written by a newer Hydro (this build reads up to " +
				std::to_string(DataFormatVersion) + "); read-only so it isn't overwritten");
			readOnly = true;
		}
	}

	// The data type a section names; nullptr (reported) when it is missing or unknown.
	static const DataTypeInfo* ReadType(const YAML::Node& section, DataReport& report)
	{
		const YAML::Node node = section["Type"];
		if (node.IsDefined() == false || node.IsScalar() == false)
		{
			report.AddError("", "Type", "missing: the file doesn't say which data type it holds");
			return nullptr;
		}
		const DataTypeInfo* type = DataTypeRegistry::Find(node.Scalar());
		if (type == nullptr)
		{
			type = DataTypeRegistry::FindByFormerName(node.Scalar());
			if (type != nullptr)
			{
				report.AddWarning("", "Type", "'" + node.Scalar() + "' is " + type->Name + "'s former name; saving the file writes '"
					+ type->Name + "'");
			}
		}
		if (type == nullptr)
		{
			report.AddError("", "Type", "unknown data type '" + node.Scalar()
				+ "' (was the struct renamed without a FormerName, or its HY_DATA removed?)");
		}
		return type;
	}

	bool DataSerializer::ReadDefinition(const std::string& text, DefinitionData& data, DataReport& report, bool checkRecords)
	{
		data = DefinitionData();

		YAML::Node root;
		if (LoadRoot(text, root, report) == false)
		{
			data.ReadOnly = true;
			return false;
		}

		const YAML::Node section = root[DefinitionKey];
		if (section.IsDefined() == false || section.IsMap() == false)
		{
			report.AddError("", "", root[TableKey] ? "this is a data table, not a definition" : "no Definition section");
			data.ReadOnly = true;
			return false;
		}

		data.Handle = ReadHandle(root, report);
		ReportUnknownKeys(root, { AssetMetaSerializer::HandleKey, DefinitionKey }, "", report);
		ReportUnknownKeys(section, { "Version", "Type", "Values" }, DefinitionKey, report);
		ReadVersion(section, data.Version, data.ReadOnly, report);

		const DataTypeInfo* type = ReadType(section, report);
		if (type == nullptr)
		{
			data.ReadOnly = true;
			return false;
		}
		if (type->CanBeDefinition == false)
		{
			report.AddError("", "Type", "'" + type->Name + "' is not a definition type (mark it HY_DATA(Definition))");
		}

		data.Values = DataRecord(*type);
		DataReadContext context;
		context.Report = &report;

		const YAML::Node values = section["Values"];
		if (values.IsDefined() == false)
		{
			report.AddWarning("", "Values", "missing; every field has its default");
		}
		else if (values.IsMap())
		{
			DataReader::ReadStruct(values, *type, data.Values.Get(), data.Values.GetKept(), "", context);
		}
		else if (values.IsNull() == false)
		{
			report.AddError("", "Values", "expected a map of fields, got " + DataText::Describe(values) + " (read-only)");
			context.StructureBroken = true;
		}

		if (checkRecords)
		{
			CheckRecord(data.Values, "", false, report);
		}

		if (context.StructureBroken)
		{
			data.ReadOnly = true;
		}
		return true;
	}

	bool DataSerializer::ReadTable(const std::string& text, DataTableData& data, DataReport& report, bool checkRecords)
	{
		data = DataTableData();

		YAML::Node root;
		if (LoadRoot(text, root, report) == false)
		{
			data.ReadOnly = true;
			return false;
		}

		const YAML::Node section = root[TableKey];
		if (section.IsDefined() == false || section.IsMap() == false)
		{
			report.AddError("", "", root[DefinitionKey] ? "this is a definition, not a data table" : "no DataTable section");
			data.ReadOnly = true;
			return false;
		}

		data.Handle = ReadHandle(root, report);
		ReportUnknownKeys(root, { AssetMetaSerializer::HandleKey, TableKey }, "", report);
		ReportUnknownKeys(section, { "Version", "Type", "Rows" }, TableKey, report);
		ReadVersion(section, data.Version, data.ReadOnly, report);

		data.RowType = ReadType(section, report);
		if (data.RowType == nullptr)
		{
			data.ReadOnly = true;
			return false;
		}
		const DataTypeInfo& type = *data.RowType;
		if (type.CanBeTable == false)
		{
			report.AddError("", "Type", "'" + type.Name + "' is not a table type (mark it HY_DATA(Table))");
		}

		bool structureBroken = false;
		const YAML::Node rows = section["Rows"];
		if (rows.IsDefined() == false)
		{
			report.AddWarning("", "Rows", "missing; the table is empty");
		}
		else if (rows.IsMap())
		{
			// Duplicates are caught here, before rows go into the map, so the second can't replace the first.
			for (const auto& entry : rows)
			{
				if (entry.first.IsScalar() == false)
				{
					report.AddError("", "Rows", "a row ID that isn't a name (read-only)");
					structureBroken = true;
					continue;
				}

				const std::string id = entry.first.Scalar();
				if (data.Rows.find(id) != data.Rows.end())
				{
					report.AddError(id, "",
						"duplicate row ID: the second row is ignored and the file is read-only until it is fixed in a text editor");
					structureBroken = true;
					continue;
				}
				if (IsValidRowId(id) == false)
				{
					report.AddError(id, "", "not a valid row ID (letters, digits and '_', not starting with a digit)");
				}

				DataRecord record(type);
				if (entry.second.IsMap())
				{
					DataReadContext context;
					context.Report = &report;
					context.Row = id;
					DataReader::ReadStruct(entry.second, type, record.Get(), record.GetKept(), "", context);
					structureBroken = structureBroken || context.StructureBroken;
				}
				else if (entry.second.IsNull() == false)
				{
					report.AddError(id, "", "expected a map of fields, got " + DataText::Describe(entry.second) + " (read-only)");
					structureBroken = true;
				}

				if (checkRecords)
				{
					CheckRecord(record, id, true, report);
				}
				data.Rows.emplace(id, std::move(record));
			}
		}
		else if (rows.IsNull() == false)
		{
			report.AddError("", "Rows", "expected a map of rows by ID, got " + DataText::Describe(rows) + " (read-only)");
			structureBroken = true;
		}

		if (structureBroken)
		{
			data.ReadOnly = true;
		}
		return true;
	}

	bool DataSerializer::ReadValue(const YAML::Node& node, const DataValueType& type, void* value, DataReport& report,
		const std::string& path)
	{
		// A value outside a record: what can't be read is reported, not kept, and so are a struct's unknown keys.
		DataKeptValues kept;
		DataReadContext context;
		context.Report = &report;
		context.ReportValues = true;
		context.KeepsValues = false;
		const bool read = DataReader::ReadValue(node, type, value, kept, "", path, context);
		if (kept.Unknown.IsMap() && kept.Unknown.size() > 0)
		{
			for (const auto& entry : kept.Unknown)
			{
				report.AddWarning("", DataText::JoinPath(path, entry.first.Scalar()), "unknown field (dropped when saved)");
			}
		}
		return read;
	}
}
