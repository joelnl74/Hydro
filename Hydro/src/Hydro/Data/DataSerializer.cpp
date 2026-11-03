#include "hypch.h"
#include "DataSerializer.h"

#include "Hydro/Asset/AssetMetaSerializer.h"
#include "Hydro/Data/DataTypeRegistry.h"
#include "Hydro/Utils/YAMLConversions.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <vector>

namespace Hydro
{
	// ---- Paths and text --------------------------------------------------------------------------------

	static std::string JoinPath(const std::string& parent, const std::string& child)
	{
		if (parent.empty())
		{
			return child;
		}
		return parent + "." + child;
	}

	static std::string IndexKey(const std::string& key, size_t index)
	{
		return key + "[" + std::to_string(index) + "]";
	}

	// What a node looks like, for messages: the scalar itself, or "a list" / "a map".
	static std::string Describe(const YAML::Node& node)
	{
		if (node.IsScalar())
		{
			return "'" + node.Scalar() + "'";
		}
		if (node.IsSequence())
		{
			return "a list";
		}
		if (node.IsMap())
		{
			return "a map";
		}
		return "nothing";
	}

	// The shortest text that reads back as exactly `value` (0.1f -> "0.1", not yaml-cpp's "0.100000001").
	static std::string FormatFloat(float value)
	{
		if (std::isnan(value))
		{
			return ".nan";
		}
		if (std::isinf(value))
		{
			return value > 0.0f ? ".inf" : "-.inf";
		}

		char buffer[32] = {};
		for (int precision = 6; precision <= 9; precision++)
		{
			std::snprintf(buffer, sizeof(buffer), "%.*g", precision, (double)value);
			if (std::strtof(buffer, nullptr) == value)
			{
				break;
			}
		}
		return buffer;
	}

	static std::string FormatNumber(float value)
	{
		// Range limits come in as floats; show whole numbers without a fraction.
		if (value == std::floor(value) && std::fabs(value) < 1.0e9f)
		{
			return std::to_string((long long)value);
		}
		return FormatFloat(value);
	}

	static const char* ExpectedText(DataFieldKind kind)
	{
		switch (kind)
		{
			case DataFieldKind::Bool: return "true or false";
			case DataFieldKind::Int: return "a whole number";
			case DataFieldKind::Float: return "a number";
			case DataFieldKind::Vec2: return "[x, y]";
			case DataFieldKind::Vec3: return "[x, y, z]";
			case DataFieldKind::Vec4: return "[x, y, z, w]";
			case DataFieldKind::String: return "text";
			case DataFieldKind::Asset: return "an asset handle (a number, 0 = none)";
			case DataFieldKind::Enum: return "an enumerator's name";
			case DataFieldKind::Struct: return "a map of fields";
			case DataFieldKind::List: return "a list";
			case DataFieldKind::RowRef: return "{ Table: <handle>, Row: <id> }";
			case DataFieldKind::EntityRef: return "an entity's UUID (a number, 0 = none)";
		}
		return "a value";
	}

	// ---- Reading ---------------------------------------------------------------------------------------

	struct DataReadContext
	{
		DataReport* Report = nullptr;
		std::string Row;
		// Duplicate keys or a row that isn't a map: writing would lose or merge something, so the file is read-only.
		bool StructureBroken = false;
		// Value problems (unreadable values, unknown keys) are reported by CheckRecord, not while loading, so the
		// same check serves a record in memory; it reads the kept value again with this on to say why.
		bool ReportValues = false;
		// A record keeps what it can't read (DataKeptValues); a value read on its own (ReadValue) doesn't.
		bool KeepsValues = true;
	};

	static void ReadStruct(const YAML::Node& node, const DataTypeInfo& type, void* object, DataKeptValues& kept,
		const std::string& path, DataReadContext& context);

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

	// False when unreadable (reported here); the caller keeps the file's value and the field its default. `kept` is
	// the enclosing record's kept values, `keptKey` this value's key in it.
	static bool ReadValue(const YAML::Node& node, const DataValueType& valueType, void* value, DataKeptValues& kept,
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
			return fail(std::string("expected ") + ExpectedText(valueType.Kind) + ", got " + Describe(node));
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
					const std::string entryKey = IndexKey(keptKey, i);
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

	static void ReadStruct(const YAML::Node& node, const DataTypeInfo& type, void* object, DataKeptValues& kept,
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
			const std::string fieldPath = JoinPath(path, key);
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
			if (ReadValue(entry.second, field->Type, field->Get(object), kept, field->Name, JoinPath(path, field->Name), context) == false)
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

	// ---- Checks on values in memory ---------------------------------------------------------------------

	static void CheckRange(const DataField& field, const float* components, int count, const std::string& path,
		const std::string& row, DataReport& report)
	{
		for (int i = 0; i < count; i++)
		{
			if (components[i] < field.Min || components[i] > field.Max)
			{
				std::string text = (count == 1 ? FormatFloat(components[i]) : "a component (" + FormatFloat(components[i]) + ")") +
					" is outside " + FormatNumber(field.Min) + ".." + FormatNumber(field.Max) + " (kept as written)";
				report.AddWarning(row, path, text);
				return;
			}
		}
	}

	static void CheckStruct(const DataTypeInfo& type, const void* object, const std::string& path, const std::string& row,
		DataReport& report);

	static void CheckValue(const DataField& field, const DataValueType& valueType, const void* value, const std::string& path,
		const std::string& row, DataReport& report)
	{
		switch (valueType.Kind)
		{
			case DataFieldKind::Int:
			{
				if (field.HasRange())
				{
					const float number = (float)*static_cast<const int*>(value);
					CheckRange(field, &number, 1, path, row, report);
				}
				break;
			}
			case DataFieldKind::Float:
			{
				if (field.HasRange())
				{
					CheckRange(field, static_cast<const float*>(value), 1, path, row, report);
				}
				break;
			}
			case DataFieldKind::Vec2:
			case DataFieldKind::Vec3:
			case DataFieldKind::Vec4:
			{
				if (field.HasRange())
				{
					const int count = valueType.Kind == DataFieldKind::Vec2 ? 2 : (valueType.Kind == DataFieldKind::Vec3 ? 3 : 4);
					CheckRange(field, static_cast<const float*>(value), count, path, row, report);
				}
				break;
			}
			case DataFieldKind::Struct:
			{
				const DataTypeInfo* nestedType = DataTypeRegistry::Find(valueType.CppType);
				if (nestedType != nullptr)
				{
					CheckStruct(*nestedType, value, path, row, report);
				}
				break;
			}
			case DataFieldKind::List:
			{
				const DataListOps& list = *valueType.List;
				const size_t size = list.Size(value);
				for (size_t i = 0; i < size; i++)
				{
					CheckValue(field, list.Element, list.AtConst(value, i), path + "[" + std::to_string(i) + "]", row, report);
				}
				break;
			}
			default:
				break;
		}
	}

	static void CheckStruct(const DataTypeInfo& type, const void* object, const std::string& path, const std::string& row,
		DataReport& report)
	{
		for (const DataField& field : type.Fields)
		{
			CheckValue(field, field.Type, field.Get(object), JoinPath(path, field.Name), row, report);
		}
	}

	static void CheckDisplayField(const DataRecord& record, const std::string& row, DataReport& report)
	{
		const DataTypeInfo& type = *record.GetType();
		if (type.DisplayField.empty())
		{
			return;
		}
		const DataField* field = type.FindField(type.DisplayField);
		if (field == nullptr || field->Type.Kind != DataFieldKind::String)
		{
			return;
		}
		if (static_cast<const std::string*>(field->Get(record.Get()))->empty())
		{
			report.AddWarning(row, field->Name, "empty; lists show the row ID instead");
		}
	}

	// What a record kept from its file, at every level: unknown keys (warnings) and unreadable values (errors, with
	// the reason found by reading them again into a scratch instance).
	static void CheckKept(const DataKeptValues& kept, const DataTypeInfo& type, const std::string& path, const std::string& row,
		DataReport& report)
	{
		if (kept.Unknown.IsMap())
		{
			for (const auto& entry : kept.Unknown)
			{
				const std::string key = entry.first.Scalar();
				const DataField* renamed = type.FindFieldByFormerName(key);
				report.AddWarning(row, JoinPath(path, key), renamed != nullptr
					? "a former name of " + renamed->Name + ", which has its own value (this one is kept in the file, unused)"
					: "unknown field (kept in the file; " + type.Name + " has no field '" + key + "' now)");
			}
		}

		if (kept.Unreadable.IsMap())
		{
			for (const auto& entry : kept.Unreadable)
			{
				const std::string key = entry.first.Scalar();
				const std::string fieldPath = JoinPath(path, key);
				const DataField* field = type.FindField(key);
				if (field == nullptr)
				{
					report.AddWarning(row, fieldPath, "a value kept from the file for a field " + type.Name + " doesn't have now");
					continue;
				}

				void* scratch = type.Construct();
				DataKeptValues scratchKept;
				DataReadContext context;
				context.Report = &report;
				context.Row = row;
				context.ReportValues = true;
				const size_t before = report.GetMessages().size();
				const bool readable = ReadValue(entry.second, field->Type, field->Get(scratch), scratchKept, key, fieldPath, context);
				type.Destroy(scratch);
				if (readable && report.GetMessages().size() == before)
				{
					// It reads now (an enumerator was added back, say): it is used once the file is loaded again.
					report.AddWarning(row, fieldPath, "the file's value " + Describe(entry.second)
						+ " is kept but not used until the file is loaded again");
				}
			}
		}

		// "Reward" (a struct field) and "Lines[2]" (a struct in a list) have kept values of their own.
		for (const auto& [key, nested] : kept.Nested)
		{
			const DataField* field = type.FindField(key.substr(0, key.find('[')));
			if (field == nullptr)
			{
				continue;
			}
			const DataTypeInfo* nestedType = nullptr;
			if (field->Type.Kind == DataFieldKind::Struct)
			{
				nestedType = DataTypeRegistry::Find(field->Type.CppType);
			}
			else if (field->Type.Kind == DataFieldKind::List && field->Type.List->Element.Kind == DataFieldKind::Struct)
			{
				nestedType = DataTypeRegistry::Find(field->Type.List->Element.CppType);
			}
			if (nestedType != nullptr)
			{
				CheckKept(nested, *nestedType, JoinPath(path, key), row, report);
			}
		}
	}

	void DataSerializer::CheckRecord(const DataRecord& record, const std::string& row, bool isTableRow, DataReport& report)
	{
		if (record.IsValid() == false)
		{
			report.AddError(row, "", "no data");
			return;
		}
		CheckKept(record.GetKept(), *record.GetType(), "", row, report);
		CheckStruct(*record.GetType(), record.Get(), "", row, report);
		if (isTableRow)
		{
			CheckDisplayField(record, row, report);
		}
	}

	bool DataSerializer::IsValidRowId(const std::string& id)
	{
		if (id.empty())
		{
			return false;
		}
		for (size_t i = 0; i < id.size(); i++)
		{
			const char ch = id[i];
			const bool letter = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_';
			const bool digit = ch >= '0' && ch <= '9';
			if (letter == false && (digit == false || i == 0))
			{
				return false;
			}
		}
		return true;
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
			report.AddError("", AssetMetaSerializer::HandleKey, "not a handle: " + Describe(handle));
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
				report.AddWarning("", JoinPath(where, key), "not part of the format (ignored, not written back)");
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
			report.AddError("", "Version", "not a version: " + Describe(node) + " (read-only)");
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
			ReadStruct(values, *type, data.Values.Get(), data.Values.GetKept(), "", context);
		}
		else if (values.IsNull() == false)
		{
			report.AddError("", "Values", "expected a map of fields, got " + Describe(values) + " (read-only)");
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
					ReadStruct(entry.second, type, record.Get(), record.GetKept(), "", context);
					structureBroken = structureBroken || context.StructureBroken;
				}
				else if (entry.second.IsNull() == false)
				{
					report.AddError(id, "", "expected a map of fields, got " + Describe(entry.second) + " (read-only)");
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
			report.AddError("", "Rows", "expected a map of rows by ID, got " + Describe(rows) + " (read-only)");
			structureBroken = true;
		}

		if (structureBroken)
		{
			data.ReadOnly = true;
		}
		return true;
	}

	// ---- Writing ---------------------------------------------------------------------------------------

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
			out << FormatFloat(components[i]);
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
				out << FormatFloat(*static_cast<const float*>(value));
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
					WriteValue(out, list.Element, list.AtConst(value, i), kept, IndexKey(keptKey, i));
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

	bool DataSerializer::ReadValue(const YAML::Node& node, const DataValueType& type, void* value, DataReport& report,
		const std::string& path)
	{
		// A value outside a record: what can't be read is reported, not kept, and so are a struct's unknown keys.
		DataKeptValues kept;
		DataReadContext context;
		context.Report = &report;
		context.ReportValues = true;
		context.KeepsValues = false;
		const bool read = ::Hydro::ReadValue(node, type, value, kept, "", path, context);
		if (kept.Unknown.IsMap() && kept.Unknown.size() > 0)
		{
			for (const auto& entry : kept.Unknown)
			{
				report.AddWarning("", JoinPath(path, entry.first.Scalar()), "unknown field (dropped when saved)");
			}
		}
		return read;
	}

	void DataSerializer::WriteValue(YAML::Emitter& out, const DataValueType& type, const void* value)
	{
		::Hydro::WriteValue(out, type, value, nullptr, "");
	}
}
