// Hydro/Data/DataEditing.h
#pragma once

#include "Hydro/Data/DataSerializer.h"
#include "Hydro/Data/DataType.h"

#include <cstddef>
#include <string>

namespace Hydro
{
	// The changes an editor makes to data in memory, kept here (not in the editor) so they behave the same
	// everywhere and can be tested: list entries that take their kept values with them, and table rows by ID.
	// Nothing here writes files; the caller saves (DefinitionAsset::Save / DataTableAsset::Save).
	class DataEditing
	{
	public:
		// ---- Lists (a std::vector field) ----------------------------------------------------------------
		// `list` is the field's value in its record, `kept` that record's kept values and `fieldName` the field's
		// name: the kept values of struct entries ("Lines[2]") move with their entry. The field's own unreadable
		// value (a list that couldn't be read) is dropped: the list has been set.
		static void InsertListEntry(const DataValueType& listType, void* list, DataKeptValues& kept, const std::string& fieldName, size_t index);
		static void RemoveListEntry(const DataValueType& listType, void* list, DataKeptValues& kept, const std::string& fieldName, size_t index);
		static void SwapListEntries(const DataValueType& listType, void* list, DataKeptValues& kept, const std::string& fieldName, size_t first, size_t second);

		// ---- Table rows ---------------------------------------------------------------------------------
		// A row with the type's defaults under a free ID ("NewItem", "NewItem2", ...). Returns its ID.
		static std::string AddRow(DataTableData& table);
		// A copy of row `id` (values and kept values) under a free ID ("PotionCopy", "PotionCopy2"). Returns its
		// ID; empty when there is no such row.
		static std::string DuplicateRow(DataTableData& table, const std::string& id);
		static bool RemoveRow(DataTableData& table, const std::string& id);
		// Gives row `id` a new ID (a migration: scenes, rows and saves that refer to the old one then show it as
		// missing). false, with why in `problem`, when `newId` isn't a valid ID or is taken.
		static bool ChangeRowId(DataTableData& table, const std::string& id, const std::string& newId, std::string& problem);
		// Why `newId` can't be the ID of row `currentId` (empty: it can). `currentId` may be empty for a new row.
		static std::string CheckRowId(const DataTableData& table, const std::string& currentId, const std::string& newId);
		// `base` if it is free, else `base` followed by the first free number from 2.
		static std::string MakeFreeRowId(const DataTableData& table, const std::string& base);

		// What a row is called in lists and pickers: its display field (an item's Name) when it has one and it
		// isn't empty, else its ID.
		static std::string GetRowLabel(const DataRecord& record, const std::string& id);
	};
}
