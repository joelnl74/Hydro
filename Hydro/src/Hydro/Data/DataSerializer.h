// Hydro/Data/DataSerializer.h
#pragma once

#include "Hydro/Asset/Asset.h"
#include "Hydro/Data/DataRecord.h"
#include "Hydro/Data/DataReport.h"

#include <cstdint>
#include <map>
#include <string>

namespace YAML
{
	class Emitter;
	class Node;
}

namespace Hydro
{
	// The data file format's version. Every file has it; a file with a newer one is read-only in this build.
	inline constexpr uint32_t DataFormatVersion = 1;

	// A definition file (.hydef) in memory: one record of one data type.
	struct DefinitionData
	{
		AssetHandle Handle = 0;
		uint32_t Version = DataFormatVersion;
		// Never written back: the file is newer than this build, or its structure is broken (duplicate keys).
		bool ReadOnly = false;
		DataRecord Values;
	};

	// A data table file (.hytable) in memory: many records of one type, by row ID.
	struct DataTableData
	{
		AssetHandle Handle = 0;
		uint32_t Version = DataFormatVersion;
		bool ReadOnly = false;
		const DataTypeInfo* RowType = nullptr;
		// Sorted by ID, which is also the write order. Row order means nothing to gameplay: store order explicitly
		// (a list of DataRowRef) where it matters.
		std::map<std::string, DataRecord> Rows;
	};

	// Reads and writes definitions and data tables as YAML text: canonical output, and unknown or unreadable values
	// are kept and written back (docs/Architecture.md#definitions-and-data-tables).
	class DataSerializer
	{
	public:
		static constexpr const char* DefinitionKey = "Definition";
		static constexpr const char* TableKey = "DataTable";

		// False when the text can't be used at all. Problems with single values go into `report` and the rest is read.
		// With `checkRecords` false only the file's own problems are reported (see CheckRecord).
		static bool ReadDefinition(const std::string& text, DefinitionData& data, DataReport& report, bool checkRecords = true);
		static bool ReadTable(const std::string& text, DataTableData& data, DataReport& report, bool checkRecords = true);

		// False, writing nothing, for read-only data.
		static bool WriteDefinition(const DefinitionData& data, std::string& text);
		static bool WriteTable(const DataTableData& data, std::string& text);

		// What is wrong with a record's values: kept values, values outside a field's Range, an empty display field.
		// `row` is the row ID (empty for a definition).
		static void CheckRecord(const DataRecord& record, const std::string& row, bool isTableRow, DataReport& report);

		// A letter or '_', then letters, digits or '_'. PascalCase by convention (Potion, IronSword).
		static bool IsValidRowId(const std::string& id);

		// One value of any kind on its own (a script's data-typed property). False when unreadable; problems go into
		// `report`, named by `path`. A value read halfway keeps what was read.
		static bool ReadValue(const YAML::Node& node, const DataValueType& type, void* value, DataReport& report, const std::string& path);
		static void WriteValue(YAML::Emitter& out, const DataValueType& type, const void* value);
	};
}
