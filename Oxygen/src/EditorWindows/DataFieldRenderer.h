#pragma once
#include <Hydro/Asset/Asset.h>
#include <Hydro/Data/DataRecord.h>
#include <Hydro/Data/DataReport.h>
#include <Hydro/Data/DataRowRef.h>
#include <Hydro/Data/DataType.h>

#include <string>

namespace Hydro
{
	// Draws and edits game data (definitions and data tables, #170) in the inspector: one renderer for a
	// definition's values, a table row, a nested struct and a list entry, so they all look and behave the same.
	// Edits change the data in memory; the caller saves when this returns true (once no field is active).
	//
	// Values kept from the file (DataKeptValues) are shown where they are: an unreadable value above its field
	// (setting the field replaces it), unknown keys at the end of their record (with Remove).
	class DataFieldRenderer
	{
	public:
		// Every field of `object` (an instance of `type`) that isn't Hidden. True when something changed.
		static bool DrawFields(const DataTypeInfo& type, void* object, DataKeptValues& kept, bool readOnly = false);

		// A row reference: one list of the rows of every table whose rows are `rowType` (all tables when empty),
		// grouped by table, searchable, by display name. A row that is gone shows as "Missing: <id>".
		static bool DrawRowRef(const char* label, DataRowRef& ref, const std::string& rowType);
		// The same field without its label column (inside a row someone else draws). `width` 0 fills the line.
		static bool DrawRowRefWidget(DataRowRef& ref, const std::string& rowType, float width = 0.0f);

		// The validation line: "Valid", "2 warnings" or "1 error, 2 warnings", unfolding into the messages.
		static void DrawValidation(const DataReport& report);
	};
}
