#include "hypch.h"
#include "DataReader.h"
#include "DataSerializer.h"
#include "DataText.h"

#include "Hydro/Data/DataTypeRegistry.h"
#include "Hydro/Utils/YAMLConversions.h"

namespace Hydro
{
	// Checks on values in memory: the same for a record just read and one being edited.

	static void CheckRange(const DataField& field, const float* components, int count, const std::string& path,
		const std::string& row, DataReport& report)
	{
		for (int i = 0; i < count; i++)
		{
			if (components[i] < field.Min || components[i] > field.Max)
			{
				const std::string number = DataText::FormatFloat(components[i]);
				std::string text = (count == 1 ? number : "a component (" + number + ")") +
					" is outside " + DataText::FormatNumber(field.Min) + ".." + DataText::FormatNumber(field.Max) + " (kept as written)";
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
			CheckValue(field, field.Type, field.Get(object), DataText::JoinPath(path, field.Name), row, report);
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
				report.AddWarning(row, DataText::JoinPath(path, key), renamed != nullptr
					? "a former name of " + renamed->Name + ", which has its own value (this one is kept in the file, unused)"
					: "unknown field (kept in the file; " + type.Name + " has no field '" + key + "' now)");
			}
		}

		if (kept.Unreadable.IsMap())
		{
			for (const auto& entry : kept.Unreadable)
			{
				const std::string key = entry.first.Scalar();
				const std::string fieldPath = DataText::JoinPath(path, key);
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
				const bool readable = DataReader::ReadValue(entry.second, field->Type, field->Get(scratch), scratchKept, key, fieldPath,
					context);
				type.Destroy(scratch);
				if (readable && report.GetMessages().size() == before)
				{
					// It reads now (an enumerator was added back, say): it is used once the file is loaded again.
					report.AddWarning(row, fieldPath, "the file's value " + DataText::Describe(entry.second)
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
				CheckKept(nested, *nestedType, DataText::JoinPath(path, key), row, report);
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
}
