#pragma once
#include <Hydro/Asset/Asset.h>
#include <Hydro/Data/DataRecord.h>
#include <Hydro/Data/DataReport.h>
#include <Hydro/Data/DataRowRef.h>
#include <Hydro/Data/DataType.h>

#include <functional>
#include <string>

namespace Hydro
{
	// Draws and edits game data in the inspector (definitions, table rows, nested structs, list entries, #170).
	// The caller saves when a draw returns true; values kept from the file are shown where they are.
	class DataFieldRenderer
	{
	public:
		static bool DrawFields(const DataTypeInfo& type, void* object, DataKeptValues& kept, bool readOnly = false);
		// One field on its own, its value at field.Get(object); script data properties are drawn with it too.
		static bool DrawField(const DataField& field, void* object, DataKeptValues& kept, bool readOnly = false);
		// How an EntityRef is drawn. The inspector sets it around a script's fields; without one the UUID is shown.
		using EntityRefWidget = std::function<bool(EntityRef& reference)>;
		static void SetEntityRefWidget(EntityRefWidget widget);

		// A row picker: the rows of every table whose rows are `rowType` (all tables when empty), searchable.
		static bool DrawRowRef(const char* label, DataRowRef& ref, const std::string& rowType);
		// The same picker without its label column. `width` 0 fills the line.
		static bool DrawRowRefWidget(DataRowRef& ref, const std::string& rowType, float width = 0.0f);

		// "Valid", "2 warnings" or "1 error, 2 warnings", unfolding into the messages.
		static void DrawValidation(const DataReport& report);
	};
}
