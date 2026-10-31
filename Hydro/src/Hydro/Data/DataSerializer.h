// Hydro/Data/DataSerializer.h
#pragma once

#include "Hydro/Asset/Asset.h"
#include "Hydro/Data/DataRecord.h"
#include "Hydro/Data/DataReport.h"

#include <cstdint>
#include <map>
#include <string>

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
		// Sorted by ID, which is also the order they are written in. Row order means nothing to gameplay: where
		// order matters (a shop's stock), it is stored explicitly, as a list of DataRowRef.
		std::map<std::string, DataRecord> Rows;
	};

	// Reads and writes definitions and data tables as YAML text (files are the asset loaders' job).
	//
	// Writing is canonical: Handle, then the root section (Definition / DataTable) with Version, Type, then
	// Values / Rows; every field of a record in declaration order (defaults too); rows sorted by ID; enums by
	// name; floats in the shortest form that reads back exactly. So reading what was written and writing it
	// again gives the same bytes. Formatting and comments of hand-edited files are not kept.
	//
	// Nothing else is lost: keys the type doesn't have and values that can't be read are reported and kept
	// (DataKeptValues), and written back where they were. Missing keys keep the field's default.
	//
	//     Handle: 7104338611223409153
	//     Definition:
	//       Version: 1
	//       Type: DialogueDefinition
	//       Values:
	//         Speaker: Innkeeper
	//         Lines:
	//           - Speaker: ""
	//             Text: Welcome to the Golden Sheaf.
	class DataSerializer
	{
	public:
		static constexpr const char* DefinitionKey = "Definition";
		static constexpr const char* TableKey = "DataTable";

		// false when the text can't be used at all (not YAML, not a definition, an unknown type). Problems with
		// single values go into `report` and the rest is still read.
		static bool ReadDefinition(const std::string& text, DefinitionData& data, DataReport& report);
		static bool ReadTable(const std::string& text, DataTableData& data, DataReport& report);

		// false (and nothing written) for read-only data.
		static bool WriteDefinition(const DefinitionData& data, std::string& text);
		static bool WriteTable(const DataTableData& data, std::string& text);

		// Problems a record in memory has, as if it had just been read: values outside a field's Range, an empty
		// display field (table rows), values kept from the file. `row` is the row ID (empty for a definition).
		static void CheckRecord(const DataRecord& record, const std::string& row, bool isTableRow, DataReport& report);

		// A row ID: a letter or '_', then letters, digits or '_'. PascalCase by convention (Potion, IronSword).
		static bool IsValidRowId(const std::string& id);
	};
}
